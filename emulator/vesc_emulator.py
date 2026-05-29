#!/usr/bin/env python3
"""
VESC Hardware Emulator — scooter-gose-vrooom
=============================================

Emulates the entire hardware layer so the ESP32 firmware can be tested
without any physical hardware connected:

  • VESC MINI 6.7 UART protocol (115200 baud, VESC packet framing + CRC16)
      – COMM_FW_VERSION response (FW 5.3, HW "MINI 6.7")
      – COMM_GET_VALUES response with full 57-byte telemetry payload
      – COMM_SET_CURRENT / SET_DUTY / SET_CURRENT_BRAKE / SET_RPM / SET_HANDBRAKE
      – COMM_ALIVE heartbeat (resets UART timeout; emulator cuts motor if it expires)
      – COMM_GET_DECODED_ADC / GET_DECODED_PPM
      – COMM_TERMINAL_CMD echo
  • Xiaomi Elite NE hub-motor physics simulation
      – Speed, ERPM, duty cycle derived from commanded current
      – BEMF back-calculation for duty cycle estimate
      – Thermal model: FET and motor temperature rise with I²·R losses
      – 10S LiPo battery model: voltage sag, SOC drain, regen charging
  • Simulated input hardware (TUI-controlled):
      – Throttle (0–100%) — represents Hall sensor position at ESP32 ADC
      – Brake (0–100%)   — represents lever angle at ESP32 ADC/GPIO
      – Turn signal L/R  — button GPIOs into ESP32
      – Power on/off     — power button GPIO into ESP32
  • Interactive curses TUI with live telemetry display
  • Creates a virtual PTY at /tmp/vesc_emu → connect your serial client there

Usage
-----
    python3 emulator/vesc_emulator.py          # full TUI
    python3 emulator/vesc_emulator.py --no-tui # headless, prints PTY path + log

Hardware Architecture (best-practice wiring)
--------------------------------------------
  Throttle hall sensor  → ESP32 ADC pin  (0.8–4.2 V, voltage-divided to 3.3 V)
                          ESP32 maps → COMM_SET_CURRENT(mode.motor_amps × frac)

  Brake lever switch    → ESP32 GPIO     (active LOW, pull-up)
                          ESP32 maps → COMM_SET_CURRENT_BRAKE(regen_amps)

  Turn L / Turn R btns  → ESP32 GPIO     (active LOW, pull-up)
                          ESP32 drives indicator LEDs — no VESC involvement

  Power button          → ESP32 GPIO     (active LOW, long press detection)
                          ESP32 sends COMM_SET_CURRENT(0) then enters deep-sleep

  VESC UART TX/RX       ↔ ESP32 UART1 pins 17/18 @ 115200 baud

Dependencies: only Python 3 stdlib (os, pty, struct, threading, curses …)
"""

import os
import sys
import pty
import tty
import select
import struct
import threading
import time
import math
import curses
import argparse
import signal
import random
from typing import Optional


# ============================================================================
# Configuration — mirrors esp32/include/config.h
# ============================================================================

MOTOR_POLE_PAIRS = 15
WHEEL_DIAMETER_MM = 216
WHEEL_CIRC_M = WHEEL_DIAMETER_MM * math.pi / 1000.0  # ≈ 0.6786 m

BATT_CELLS = 10
CELL_V_MAX = 4.20
CELL_V_MIN = 3.30
BATT_V_MAX = BATT_CELLS * CELL_V_MAX   # 42.0 V
BATT_V_MIN = BATT_CELLS * CELL_V_MIN   # 33.0 V
BATT_CAPACITY_AH = 7.8                  # 280 Wh ÷ 36 V ≈ 7.8 Ah
BATT_R_INT = 0.08                       # Ω internal resistance

# Physics — Xiaomi Elite NE (~14 kg scooter + 75 kg rider at 90 kg total)
# Empirical torque constant: at 20 A the scooter reaches ~25 km/h in ~4 s
# → a_peak ≈ 7 m/s / 4 s = 1.75 m/s² → k = 1.75 / 20 ≈ 0.088
VEHICLE_MASS_KG = 90.0
MOTOR_K_TORQUE = 0.088   # (m/s²) / A  — at zero speed, pure torque contribution
K_ROLLING = 0.015        # rolling resistance coefficient (unitless)
K_AERO = 0.0006          # aero drag: a_drag = K_AERO * v² [m/s² per (m/s)²]
MAX_SPEED_MS = 9.5        # ≈ 34 km/h hard physics cap

# Thermal model
TEMP_AMBIENT = 25.0
K_HEAT_FET = 0.055        # °C / (A² · s)
K_COOL_FET = 0.018        # °C / (ΔT · s)
K_HEAT_MOT = 0.022
K_COOL_MOT = 0.007

# VESC UART timeout (same as real VESC default)
VESC_UART_TIMEOUT_S = 0.5

# Simulated VESC firmware version
FW_MAJOR = 5
FW_MINOR = 3
HW_NAME = b"MINI 6.7"

# TUI refresh
TUI_FPS = 15

# Physics step
PHYSICS_DT_S = 0.040   # 25 Hz


# ============================================================================
# CRC16-CCITT (polynomial 0x1021, init 0x0000) — matches VESC firmware
# ============================================================================

_CRC16_TAB: list[int] = []


def _init_crc16() -> None:
    for i in range(256):
        crc = i << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if (crc & 0x8000) else (crc << 1)
        _CRC16_TAB.append(crc & 0xFFFF)


_init_crc16()


def crc16(data: bytes) -> int:
    crc = 0
    for b in data:
        crc = _CRC16_TAB[((crc >> 8) ^ b) & 0xFF] ^ ((crc << 8) & 0xFFFF)
    return crc


# ============================================================================
# VESC packet framing
#   Short  (payload ≤ 255 B): 0x02 | LEN | PAYLOAD | CRC_H | CRC_L | 0x03
#   Long   (payload > 255 B): 0x03 | LEN_H | LEN_L | PAYLOAD | CRC_H | CRC_L | 0x03
# ============================================================================

def pack_frame(payload: bytes) -> bytes:
    n = len(payload)
    c = crc16(payload)
    trailer = bytes([c >> 8, c & 0xFF, 0x03])
    if n <= 255:
        return bytes([0x02, n]) + payload + trailer
    return bytes([0x03, n >> 8, n & 0xFF]) + payload + trailer


class FrameParser:
    """Streaming parser — feed raw bytes, iterate to get complete payloads."""

    def __init__(self) -> None:
        self._buf = bytearray()

    def feed(self, data: bytes) -> None:
        self._buf.extend(data)

    def __iter__(self):
        return self

    def __next__(self) -> bytes:
        buf = self._buf
        while buf:
            if buf[0] == 0x02:
                if len(buf) < 4:
                    raise StopIteration
                plen = buf[1]
                need = 2 + plen + 3
                if len(buf) < need:
                    raise StopIteration
                payload = bytes(buf[2:2 + plen])
                crc_rx = (buf[2 + plen] << 8) | buf[2 + plen + 1]
                end = buf[2 + plen + 2]
                if end != 0x03 or crc16(payload) != crc_rx:
                    del buf[:1]       # discard only the start byte; rescan from next
                    continue
                del buf[:need]        # validated — consume the frame
                return payload

            elif buf[0] == 0x03:
                if len(buf) < 5:
                    raise StopIteration
                plen = (buf[1] << 8) | buf[2]
                need = 3 + plen + 3
                if len(buf) < need:
                    raise StopIteration
                payload = bytes(buf[3:3 + plen])
                crc_rx = (buf[3 + plen] << 8) | buf[3 + plen + 1]
                end = buf[3 + plen + 2]
                if end != 0x03 or crc16(payload) != crc_rx:
                    del buf[:1]       # discard only the start byte; rescan
                    continue
                del buf[:need]        # validated — consume the frame
                return payload

            else:
                del buf[0]    # discard unknown start byte

        raise StopIteration


# ============================================================================
# VESC command IDs — subset used in this firmware
# ============================================================================

class COMM:
    FW_VERSION        =  0
    GET_VALUES        =  4
    SET_DUTY          =  5
    SET_CURRENT       =  6
    SET_CURRENT_BRAKE =  7
    SET_RPM           =  8
    SET_POS           =  9
    SET_HANDBRAKE     = 10
    PRINT             = 15
    TERMINAL_CMD      = 16
    GET_DECODED_PPM   = 18
    GET_DECODED_ADC   = 19
    GET_DECODED_CHUK  = 20
    REBOOT            = 29
    ALIVE             = 30
    SET_CURRENT_LIMIT = 93


_COMM_NAMES: dict[int, str] = {
    v: k for k, v in COMM.__dict__.items() if not k.startswith('_')
}


def comm_name(cmd: int) -> str:
    return _COMM_NAMES.get(cmd, f"0x{cmd:02X}")


# ============================================================================
# Fault codes
# ============================================================================

_FAULT_NAMES = {
    0: "NONE",
    1: "OV",
    2: "UV",
    3: "DRV",
    4: "OC",
    5: "OT_FET",
    6: "OT_MOT",
    7: "GD_OV",
    8: "GD_UV",
    9: "MCU_UV",
}


def fault_name(code: int) -> str:
    return _FAULT_NAMES.get(code, f"FAULT_{code}")


# ============================================================================
# Shared state
# ============================================================================

class State:
    """All mutable emulator state. Always hold .lock when reading/writing."""

    def __init__(self) -> None:
        self.lock = threading.Lock()

        # ── VESC telemetry (what COMM_GET_VALUES returns) ──────────────────
        self.temp_mos           = 35.0    # °C
        self.temp_motor         = 28.0    # °C
        self.motor_current      = 0.0     # A
        self.input_current      = 0.0     # A
        self.d_current          = 0.0     # A (field-weakening component)
        self.q_current          = 0.0     # A (torque component)
        self.duty_cycle         = 0.0     # 0.0–1.0
        self.rpm                = 0.0     # electrical RPM
        self.v_in               = 40.5    # V (start at ~90 % charge)
        self.amp_hours          = 0.0     # Ah consumed
        self.amp_hours_charged  = 0.0     # Ah regenerated
        self.watt_hours         = 0.0     # Wh consumed
        self.watt_hours_charged = 0.0     # Wh regenerated
        self.tachometer         = 0       # electrical counts
        self.tachometer_abs     = 0
        self.fault_code         = 0       # mc_fault_code

        # ── Control targets — set by incoming ESP32 UART commands ──────────
        self.target_current       = 0.0   # A  (COMM_SET_CURRENT)
        self.target_brake_current = 0.0   # A  (COMM_SET_CURRENT_BRAKE)
        self.target_duty          = 0.0   # 0–1 (COMM_SET_DUTY)
        self.target_erpm          = None  # int or None (COMM_SET_RPM)
        self.control_mode         = "idle"  # "current"|"brake"|"duty"|"erpm"|"idle"

        # ── UART health ────────────────────────────────────────────────────
        self.last_alive_t     = time.monotonic()
        self.alive_timed_out  = False

        # ── Simulated input hardware (TUI-controlled) ──────────────────────
        # These represent the physical hardware signals wired into the ESP32.
        # In a real system the ESP32 reads these and sends COMM_SET_CURRENT etc.
        # In standalone mode (no ESP32 connected), physics uses these directly.
        self.throttle   = 0.0    # 0.0–1.0 (Hall throttle, ESP32 ADC)
        self.brake      = 0.0    # 0.0–1.0 (lever angle, ESP32 ADC/GPIO)
        self.turn_left  = False  # button, ESP32 GPIO
        self.turn_right = False  # button, ESP32 GPIO
        self.power_on   = True   # power button state

        # ── Internal physics state ─────────────────────────────────────────
        self.speed_ms   = 0.0    # m/s vehicle speed
        self.soc        = 0.90   # battery state of charge 0–1

        # ── Fault injection ────────────────────────────────────────────────
        self._inject_fault  = 0     # set nonzero from TUI to inject a fault
        self._fault_latched = False  # True while an injected fault is held

        # ── Statistics ────────────────────────────────────────────────────
        self.distance_m   = 0.0
        self.runtime_s    = 0.0
        self.cmd_count    = 0
        self.alive_count  = 0
        self.last_cmd     = "—"
        self.slave_path   = "?"

        # ── Log ────────────────────────────────────────────────────────────
        self._log: list[str] = []
        self._log_lock = threading.Lock()

    def log(self, msg: str) -> None:
        ts = time.strftime("%H:%M:%S")
        line = f"[{ts}] {msg}"
        with self._log_lock:
            self._log.append(line)
            if len(self._log) > 200:
                self._log = self._log[-200:]
        if not sys.stdout.isatty():
            print(line)

    def get_log(self, n: int = 10) -> list[str]:
        with self._log_lock:
            return list(self._log[-n:])


# ============================================================================
# Physics simulation
# ============================================================================

def physics_step(s: State, dt: float) -> None:
    """Advance the physics model by dt seconds. Must be called from physics thread."""
    with s.lock:
        # ── VESC UART timeout ──────────────────────────────────────────────
        uart_age = time.monotonic() - s.last_alive_t
        if uart_age > VESC_UART_TIMEOUT_S:
            # Real VESC cuts all output when UART times out
            s.target_current       = 0.0
            s.target_brake_current = 0.0
            s.control_mode         = "idle"
            s.alive_timed_out      = True
        else:
            s.alive_timed_out = False

        # ── Determine effective motor current ─────────────────────────────
        # If ESP32 is connected and fresh, use its commanded values.
        # Otherwise fall back to TUI-controlled inputs (standalone sim mode).
        esp32_connected = uart_age < VESC_UART_TIMEOUT_S

        if esp32_connected and s.control_mode == "current":
            i_cmd = s.target_current
        elif esp32_connected and s.control_mode == "brake":
            i_cmd = -abs(s.target_brake_current)
        elif esp32_connected and s.control_mode == "duty":
            # Estimate current from duty cycle and back-EMF
            v_bemf = (s.speed_ms / WHEEL_CIRC_M) * (1 / 60.0) * MOTOR_POLE_PAIRS * 0.12
            v_applied = s.target_duty * s.v_in
            i_cmd = (v_applied - v_bemf) / 0.15
            i_cmd = max(-20.0, min(20.0, i_cmd))
        elif esp32_connected and s.control_mode == "erpm":
            # Simple P-controller simulating VESC internal speed controller
            err = (s.target_erpm or 0) - s.rpm
            i_cmd = max(-20.0, min(20.0, err * 0.008))
        else:
            # Standalone mode: TUI throttle/brake → current
            # Mimic what the ESP32 firmware would do (sport mode ≈ 20 A max)
            i_cmd = s.throttle * 20.0 - s.brake * 12.0

        # Clamp to motor limits (VESC MINI 6.7 limit: 50 A abs, we use 25 A)
        i_cmd = max(-20.0, min(25.0, i_cmd))

        # ── Vehicle dynamics ───────────────────────────────────────────────
        # Effective torque contribution (drops as speed approaches V_max via
        # back-EMF saturation — simplified with a linear derating term)
        bemf_derating = max(0.0, 1.0 - s.speed_ms / MAX_SPEED_MS)

        if i_cmd > 0.0:
            a_drive   = i_cmd * MOTOR_K_TORQUE * bemf_derating
            a_regen   = 0.0
            a_resist  = (K_ROLLING * 9.81) + K_AERO * s.speed_ms ** 2
            a_net     = a_drive - a_resist
        elif i_cmd < 0.0:
            a_drive   = 0.0
            # Regen braking is more effective at higher speeds (more BEMF available)
            regen_eff = min(1.0, s.speed_ms / 2.0)
            a_regen   = abs(i_cmd) * MOTOR_K_TORQUE * regen_eff
            a_resist  = (K_ROLLING * 9.81) + K_AERO * s.speed_ms ** 2
            a_net     = -(a_regen + a_resist)
        else:
            a_resist  = (K_ROLLING * 9.81 + K_AERO * s.speed_ms ** 2)
            a_net     = -a_resist if s.speed_ms > 0.1 else 0.0

        s.speed_ms = max(0.0, min(MAX_SPEED_MS, s.speed_ms + a_net * dt))

        # ── ERPM and tachometer ────────────────────────────────────────────
        mech_rps = s.speed_ms / WHEEL_CIRC_M          # mechanical rev/s
        s.rpm    = mech_rps * 60.0 * MOTOR_POLE_PAIRS  # ERPM

        d_tach = int(mech_rps * dt * MOTOR_POLE_PAIRS * 6)  # 6 elec. counts / rev / pair
        s.tachometer     += d_tach
        s.tachometer_abs += abs(d_tach)

        # ── Duty cycle ────────────────────────────────────────────────────
        if s.v_in > 1.0:
            v_bemf  = mech_rps * 0.15   # back-EMF constant (V per mech rev/s)
            v_motor = v_bemf + max(0.0, i_cmd) * 0.05
            s.duty_cycle = max(0.0, min(1.0, v_motor / s.v_in))
        else:
            s.duty_cycle = 0.0

        # ── Currents ──────────────────────────────────────────────────────
        s.motor_current = abs(i_cmd)
        s.q_current     = i_cmd * 0.99      # torque current ≈ motor current
        s.d_current     = i_cmd * 0.05      # field-weakening (small at low speed)
        # Input (battery) current: positive = drawing, negative = regen charging.
        # This matches the real VESC avg_input_current sign convention.
        if i_cmd > 0:
            s.input_current =  i_cmd * s.duty_cycle * 0.90   # motoring (~90% eff)
        else:
            s.input_current =  i_cmd * 0.85                   # regen (i_cmd<0 → negative)

        # ── Battery ───────────────────────────────────────────────────────
        v_oc   = BATT_V_MIN + (BATT_V_MAX - BATT_V_MIN) * s.soc
        # During regen input_current is negative → terminal voltage rises slightly
        s.v_in = max(BATT_V_MIN - 2.0, min(BATT_V_MAX, v_oc - s.input_current * BATT_R_INT))

        ah_delta = s.input_current * dt / 3600.0
        wh_delta = s.input_current * s.v_in * dt / 3600.0
        if ah_delta >= 0.0:
            s.amp_hours  += ah_delta        # battery discharge
            s.watt_hours += wh_delta
        else:
            s.amp_hours_charged  -= ah_delta   # regen charge (stored as positive)
            s.watt_hours_charged -= wh_delta
        s.soc = max(0.0, min(1.0, s.soc - ah_delta / BATT_CAPACITY_AH))

        # ── Thermal model ─────────────────────────────────────────────────
        i2 = i_cmd ** 2
        s.temp_mos   += (K_HEAT_FET * i2 - K_COOL_FET * (s.temp_mos   - TEMP_AMBIENT)) * dt
        s.temp_motor += (K_HEAT_MOT * i2 - K_COOL_MOT * (s.temp_motor - TEMP_AMBIENT)) * dt
        s.temp_mos   = max(TEMP_AMBIENT, s.temp_mos)
        s.temp_motor = max(TEMP_AMBIENT, s.temp_motor)

        # ── Fault logic ───────────────────────────────────────────────────
        if s._inject_fault:
            s.fault_code     = s._inject_fault
            s._inject_fault  = 0
            s._fault_latched = True   # hold until user presses [0]
        elif s._fault_latched:
            pass   # keep injected fault code until explicitly cleared
        elif s.temp_mos > 85.0:
            s.fault_code = 5   # OVER_TEMP_FET
        elif s.temp_motor > 95.0:
            s.fault_code = 6   # OVER_TEMP_MOTOR
        elif s.v_in < BATT_V_MIN:
            s.fault_code = 2   # UNDER_VOLTAGE
        elif s.v_in > BATT_V_MAX + 1.0:
            s.fault_code = 1   # OVER_VOLTAGE
        else:
            s.fault_code = 0

        # ── Distance and runtime ──────────────────────────────────────────
        s.distance_m += s.speed_ms * dt
        s.runtime_s  += dt


# ============================================================================
# VESC response builders
# ============================================================================

def _f16(val: float, scale: float) -> bytes:
    """Big-endian int16 from float × scale."""
    return struct.pack(">h", int(round(max(-32768, min(32767, val * scale)))))


def _f32(val: float, scale: float) -> bytes:
    """Big-endian int32 from float × scale."""
    return struct.pack(">i", int(round(max(-2**31, min(2**31 - 1, val * scale)))))


def _i32(val: int) -> bytes:
    return struct.pack(">i", int(val))


def build_fw_version() -> bytes:
    """COMM_FW_VERSION response (matches bldc_interface.c decoder)."""
    p = bytearray([COMM.FW_VERSION, FW_MAJOR, FW_MINOR])
    p.extend(HW_NAME)
    p.append(0)          # null-terminate hw_name
    p.extend(b"\x00" * 12)  # UUID placeholder
    p.append(0)          # hw_type = VESC_BMS_HW_TYPE_VESC (0)
    p.append(1)          # has_phase_filters: yes (cosmetic)
    return bytes(p)


def build_get_values(s: State) -> bytes:
    """
    COMM_GET_VALUES response — exact byte layout matching bldc_interface.c:

    Offset  Size  Scale  Field
    ------  ----  -----  -----
      0      1     —     COMM_GET_VALUES (0x04)
      1      2    ×10    temp_mos      (int16, °C)
      3      2    ×10    temp_motor    (int16, °C)
      5      4   ×100    avg_motor_current (int32, A)
      9      4   ×100    avg_input_current (int32, A)
     13      4   ×100    avg_id  (int32, A)
     17      4   ×100    avg_iq  (int32, A)
     21      2   ×1000   duty_cycle (int16)
     23      4    ×1     rpm    (int32, ERPM — NOTE: scale 1.0)
     27      2    ×10    v_in   (int16, V)
     29      4  ×10000   amp_hours          (int32, Ah)
     33      4  ×10000   amp_hours_charged  (int32, Ah)
     37      4  ×10000   watt_hours         (int32, Wh)
     41      4  ×10000   watt_hours_charged (int32, Wh)
     45      4    —      tachometer         (int32, counts)
     49      4    —      tachometer_abs     (int32, counts)
     53      1    —      fault_code         (uint8)
     54      4  ×1e6     pid_pos            (int32, deg)  ← optional in older FW
     58      1    —      vesc_id            (uint8)       ← optional
    """
    with s.lock:
        p = bytearray([COMM.GET_VALUES])
        p += _f16(s.temp_mos,           10.0)
        p += _f16(s.temp_motor,         10.0)
        p += _f32(s.motor_current,     100.0)
        p += _f32(s.input_current,     100.0)
        p += _f32(s.d_current,         100.0)
        p += _f32(s.q_current,         100.0)
        p += _f16(s.duty_cycle,       1000.0)
        p += _f32(s.rpm,                 1.0)
        p += _f16(s.v_in,               10.0)
        p += _f32(s.amp_hours,       10000.0)
        p += _f32(s.amp_hours_charged,10000.0)
        p += _f32(s.watt_hours,      10000.0)
        p += _f32(s.watt_hours_charged,10000.0)
        p += _i32(s.tachometer)
        p += _i32(s.tachometer_abs)
        p.append(s.fault_code & 0xFF)
        p += _f32(0.0, 1_000_000.0)    # pid_pos = 0
        p.append(0)                    # vesc_id = 0
    return bytes(p)


def build_decoded_adc() -> bytes:
    """COMM_GET_DECODED_ADC — dec_adc (float32, ×1e6), adc_voltage (float32, ×1e6)."""
    p = bytearray([COMM.GET_DECODED_ADC])
    p += struct.pack(">i", 0)    # decoded ADC value 0.0
    p += struct.pack(">i", 0)    # ADC voltage 0.0
    return bytes(p)


def build_decoded_ppm() -> bytes:
    """COMM_GET_DECODED_PPM — dec_ppm (float32, ×1e6), pulse_ms (float32, ×1e6)."""
    p = bytearray([COMM.GET_DECODED_PPM])
    p += struct.pack(">i", 0)                          # decoded PPM 0.0
    p += struct.pack(">i", int(1.5 * 1_000_000))      # 1.5 ms neutral pulse (÷1e6 on decode)
    return bytes(p)


# ============================================================================
# Command processor
# ============================================================================

def process_command(payload: bytes, s: State) -> Optional[bytes]:
    """
    Decode one VESC command payload, update emulator state, return response frame
    or None if no response is required (SET_* commands have no ACK in VESC protocol).
    """
    if not payload:
        return None

    cmd  = payload[0]
    name = comm_name(cmd)

    with s.lock:
        s.cmd_count    += 1
        s.last_cmd      = name
        s.last_alive_t  = time.monotonic()   # any valid packet resets timeout

        if cmd == COMM.ALIVE:
            s.alive_count += 1

    # ── Requests that need a response ─────────────────────────────────────
    if cmd == COMM.FW_VERSION:
        return pack_frame(build_fw_version())

    if cmd == COMM.GET_VALUES:
        return pack_frame(build_get_values(s))

    if cmd == COMM.GET_DECODED_ADC:
        return pack_frame(build_decoded_adc())

    if cmd == COMM.GET_DECODED_PPM:
        return pack_frame(build_decoded_ppm())

    if cmd == COMM.TERMINAL_CMD:
        msg = b"[EMULATOR] command received\r\n"
        resp = bytearray([COMM.PRINT]) + msg
        return pack_frame(bytes(resp))

    # ── Control commands — no ACK ──────────────────────────────────────────
    if cmd == COMM.SET_CURRENT and len(payload) >= 5:
        val = struct.unpack_from(">i", payload, 1)[0] / 1000.0
        with s.lock:
            s.target_current = val
            s.control_mode   = "current"
        return None

    if cmd == COMM.SET_CURRENT_BRAKE and len(payload) >= 5:
        val = abs(struct.unpack_from(">i", payload, 1)[0] / 1000.0)
        with s.lock:
            s.target_brake_current = val
            s.control_mode         = "brake"
        return None

    if cmd == COMM.SET_DUTY and len(payload) >= 5:
        val = struct.unpack_from(">i", payload, 1)[0] / 100_000.0
        with s.lock:
            s.target_duty  = max(-1.0, min(1.0, val))
            s.control_mode = "duty"
        return None

    if cmd == COMM.SET_RPM and len(payload) >= 5:
        val = struct.unpack_from(">i", payload, 1)[0]
        with s.lock:
            s.target_erpm  = val
            s.control_mode = "erpm"
        return None

    if cmd == COMM.SET_HANDBRAKE and len(payload) >= 5:
        val = abs(struct.unpack_from(">i", payload, 1)[0] / 1000.0)
        with s.lock:
            s.target_brake_current = val
            s.control_mode         = "brake"
        return None

    if cmd == COMM.SET_POS and len(payload) >= 5:
        return None    # position control not implemented in this firmware

    if cmd == COMM.REBOOT:
        return None

    if cmd == COMM.ALIVE:
        return None    # no response expected

    if cmd == COMM.SET_CURRENT_LIMIT:
        return None    # ignore runtime current limit changes

    # Unknown — silently discard
    return None


# ============================================================================
# VESC Emulator — virtual PTY + threads
# ============================================================================

class VescEmulator:

    def __init__(self, state: State) -> None:
        self._s          = state
        self._running    = False
        self._master_fd  = -1
        self._slave_fd   = -1
        self._uart_th    : Optional[threading.Thread] = None
        self._phys_th    : Optional[threading.Thread] = None
        self._symlink    = "/tmp/vesc_emu"

    def start(self) -> None:
        self._master_fd, self._slave_fd = pty.openpty()
        # Keep master side in raw mode (no echo, no newline translation)
        tty.setraw(self._master_fd)

        slave_path = os.ttyname(self._slave_fd)
        with self._s.lock:
            self._s.slave_path = slave_path

        # Create a stable symlink for convenience
        try:
            if os.path.islink(self._symlink):
                os.unlink(self._symlink)
            os.symlink(slave_path, self._symlink)
        except OSError:
            pass

        self._running = True
        self._uart_th = threading.Thread(target=self._uart_loop, daemon=True, name="uart")
        self._phys_th = threading.Thread(target=self._phys_loop, daemon=True, name="phys")
        self._uart_th.start()
        self._phys_th.start()

        self._s.log(f"Virtual UART slave: {slave_path}  (symlink: {self._symlink})")
        self._s.log(f"Configure ESP32/client to connect at 115200 8N1")

    def stop(self) -> None:
        self._running = False
        # Wait for threads to exit before closing FDs they may be using
        for th in (self._uart_th, self._phys_th):
            if th is not None and th.is_alive():
                th.join(timeout=1.0)
        try:
            if os.path.islink(self._symlink):
                os.unlink(self._symlink)
        except OSError:
            pass
        for fd in (self._slave_fd, self._master_fd):
            if fd >= 0:
                try:
                    os.close(fd)
                except OSError:
                    pass

    @property
    def slave_path(self) -> str:
        return os.ttyname(self._slave_fd) if self._slave_fd >= 0 else "?"

    def _uart_loop(self) -> None:
        parser = FrameParser()
        while self._running:
            try:
                r, _, _ = select.select([self._master_fd], [], [], 0.05)
                if r:
                    try:
                        raw = os.read(self._master_fd, 512)
                    except OSError:
                        break
                    parser.feed(raw)
                    for payload in parser:
                        resp = process_command(payload, self._s)
                        if resp:
                            try:
                                os.write(self._master_fd, resp)
                            except OSError:
                                pass
            except OSError:
                break

    def _phys_loop(self) -> None:
        last_t = time.monotonic()
        while self._running:
            step_start = time.monotonic()
            dt         = step_start - last_t
            last_t     = step_start
            physics_step(self._s, min(dt, 0.2))   # cap dt to avoid explosion on pause
            elapsed = time.monotonic() - step_start
            time.sleep(max(0.0, PHYSICS_DT_S - elapsed))  # compensate for processing time


# ============================================================================
# Curses TUI
# ============================================================================

def _bar(width: int, value: float, max_val: float = 1.0) -> str:
    filled = int(value / max_val * width) if max_val > 0 else 0
    filled = max(0, min(width, filled))
    return "█" * filled + "░" * (width - filled)


def _safe(win, y: int, x: int, text: str, attr: int = 0) -> None:
    h, w = win.getmaxyx()
    if y < 0 or y >= h - 1 or x < 0 or x >= w:
        return
    text = text[:max(0, w - x - 1)]
    try:
        win.addstr(y, x, text, attr)
    except curses.error:
        pass


def run_tui(stdscr, emu: VescEmulator, state: State) -> None:
    curses.curs_set(0)
    stdscr.nodelay(True)
    curses.start_color()
    curses.use_default_colors()

    curses.init_pair(1, curses.COLOR_GREEN,   -1)   # OK
    curses.init_pair(2, curses.COLOR_YELLOW,  -1)   # warn
    curses.init_pair(3, curses.COLOR_RED,     -1)   # fault/danger
    curses.init_pair(4, curses.COLOR_CYAN,    -1)   # section headers
    curses.init_pair(5, curses.COLOR_WHITE,   -1)   # normal
    curses.init_pair(6, curses.COLOR_MAGENTA, -1)   # highlight

    GREEN   = curses.color_pair(1)
    YELLOW  = curses.color_pair(2)
    RED     = curses.color_pair(3)
    CYAN    = curses.color_pair(4)
    BOLD    = curses.A_BOLD
    DIM     = curses.A_DIM

    frame_dt = 1.0 / TUI_FPS
    last_draw = 0.0

    while True:
        key = stdscr.getch()
        if key != -1:
            with state.lock:
                if key in (ord('q'), ord('Q')):
                    break
                elif key == ord('t'):
                    state.throttle = min(1.0, round(state.throttle + 0.05, 2))
                elif key == ord('T'):
                    state.throttle = max(0.0, round(state.throttle - 0.05, 2))
                elif key == ord('b'):
                    state.brake = min(1.0, round(state.brake + 0.05, 2))
                elif key == ord('B'):
                    state.brake = max(0.0, round(state.brake - 0.05, 2))
                elif key in (ord('l'), ord('L')):
                    state.turn_left  = not state.turn_left
                    if state.turn_left:
                        state.turn_right = False
                elif key in (ord('r'), ord('R')):
                    state.turn_right = not state.turn_right
                    if state.turn_right:
                        state.turn_left = False
                elif key in (ord('p'), ord('P')):
                    state.power_on = not state.power_on
                elif key in (ord('f'), ord('F')):
                    state._inject_fault = random.choice([1, 2, 3, 4, 5, 6])
                elif key == ord('0'):
                    state.fault_code     = 0
                    state._inject_fault  = 0
                    state._fault_latched = False
                elif key == ord(' '):          # full brake
                    state.brake = 1.0
                    state.throttle = 0.0
                elif key == curses.KEY_UP:
                    state.throttle = min(1.0, round(state.throttle + 0.10, 2))
                elif key == curses.KEY_DOWN:
                    state.throttle = max(0.0, round(state.throttle - 0.10, 2))

        now = time.monotonic()
        if now - last_draw < frame_dt:
            time.sleep(0.005)
            continue
        last_draw = now

        # ── Snapshot state (minimise lock hold time) ──────────────────────
        with state.lock:
            snap = {
                "temp_mos":       state.temp_mos,
                "temp_motor":     state.temp_motor,
                "motor_current":  state.motor_current,
                "input_current":  state.input_current,
                "duty":           state.duty_cycle,
                "rpm":            state.rpm,
                "v_in":           state.v_in,
                "fault":          state.fault_code,
                "throttle":       state.throttle,
                "brake":          state.brake,
                "turn_left":      state.turn_left,
                "turn_right":     state.turn_right,
                "power_on":       state.power_on,
                "speed_ms":       state.speed_ms,
                "soc":            state.soc,
                "distance_m":     state.distance_m,
                "runtime_s":      state.runtime_s,
                "cmd_count":      state.cmd_count,
                "alive_count":    state.alive_count,
                "last_cmd":       state.last_cmd,
                "slave_path":     state.slave_path,
                "control_mode":   state.control_mode,
                "target_current": state.target_current,
                "target_brake":   state.target_brake_current,
                "target_duty":    state.target_duty,
                "timed_out":      state.alive_timed_out,
                "amp_hours":      state.amp_hours,
                "watt_hours":     state.watt_hours,
                "amp_hours_regen": state.amp_hours_charged,
            }

        # Derived values
        speed_kmh = snap["speed_ms"] * 3.6
        batt_pct  = int(snap["soc"] * 100)
        rt        = int(snap["runtime_s"])
        rm, rs    = divmod(rt, 60)
        dist_km   = snap["distance_m"] / 1000.0

        stdscr.erase()
        h, w = stdscr.getmaxyx()

        # ── Title ──────────────────────────────────────────────────────────
        title = "  VESC Hardware Emulator — scooter-gose-vrooom  "
        tx    = max(0, (w - len(title)) // 2)
        _safe(stdscr, 0, tx, title, BOLD | CYAN)
        _safe(stdscr, 1, 0, "─" * min(w - 1, 80))

        C1 = 1       # column 1 x
        C2 = 42      # column 2 x

        # ── HARDWARE INPUTS (column 1) ────────────────────────────────────
        _safe(stdscr, 2, C1, "[ HARDWARE INPUTS ]", BOLD | CYAN)

        throttle_color = GREEN if snap["throttle"] > 0.05 else DIM
        brake_color    = RED   if snap["brake"]    > 0.05 else DIM

        _safe(stdscr, 3, C1,
              f"Throttle  [{_bar(18, snap['throttle'])}] {snap['throttle']*100:4.0f}%",
              throttle_color | BOLD)
        _safe(stdscr, 4, C1,
              f"Brake     [{_bar(18, snap['brake'])}] {snap['brake']*100:4.0f}%",
              brake_color | BOLD)

        tl_attr = GREEN | BOLD if snap["turn_left"]  else DIM
        tr_attr = GREEN | BOLD if snap["turn_right"] else DIM
        pw_attr = GREEN | BOLD if snap["power_on"]   else RED | BOLD

        _safe(stdscr, 5, C1, f"Turn L:   {'◄ BLINK' if snap['turn_left']  else '  off  '}", tl_attr)
        _safe(stdscr, 6, C1, f"Turn R:   {'BLINK ►' if snap['turn_right'] else '  off  '}", tr_attr)
        _safe(stdscr, 7, C1, f"Power:    {'[ ON  ]' if snap['power_on']   else '[ OFF ]'}", pw_attr)

        # ── ESP32 → VESC (column 1, mid) ─────────────────────────────────
        _safe(stdscr, 9, C1, "[ ESP32 → VESC COMMANDS ]", BOLD | CYAN)

        uart_color = GREEN if not snap["timed_out"] else RED | BOLD
        uart_label = "OK — UART active" if not snap["timed_out"] else "TIMEOUT — motor cut"
        _safe(stdscr, 10, C1, f"UART:   {uart_label}", uart_color)
        _safe(stdscr, 11, C1, f"Mode:   {snap['control_mode'].upper():<10}")
        _safe(stdscr, 12, C1, f"Cmd I:  {snap['target_current']:>7.2f} A")
        _safe(stdscr, 13, C1, f"Brk I:  {snap['target_brake']:>7.2f} A")
        _safe(stdscr, 14, C1, f"Duty:   {snap['target_duty']:>7.3f}")

        # ── VESC TELEMETRY (column 2) ─────────────────────────────────────
        _safe(stdscr, 2, C2, "[ VESC TELEMETRY ]", BOLD | CYAN)

        s_color = GREEN if speed_kmh < 20 else (YELLOW if speed_kmh < 28 else RED)
        _safe(stdscr, 3, C2, f"Speed:    {speed_kmh:6.1f} km/h", s_color | BOLD)
        _safe(stdscr, 4, C2, f"ERPM:     {snap['rpm']:7.0f}")
        _safe(stdscr, 5, C2, f"Duty:     {snap['duty']:6.3f}")

        i_color = GREEN if snap["motor_current"] < 12 else (YELLOW if snap["motor_current"] < 20 else RED)
        _safe(stdscr, 6, C2, f"Motor I:  {snap['motor_current']:5.1f} A", i_color)
        _safe(stdscr, 7, C2, f"Input I:  {snap['input_current']:5.1f} A")

        v_color = GREEN if snap["v_in"] > 36.0 else (YELLOW if snap["v_in"] > 34.0 else RED)
        _safe(stdscr, 8, C2, f"V_in:     {snap['v_in']:5.1f} V", v_color)

        b_color = GREEN if batt_pct > 40 else (YELLOW if batt_pct > 20 else RED)
        _safe(stdscr, 9, C2, f"Battery:  {batt_pct:4d} %  [{_bar(12, snap['soc'])}]", b_color | BOLD)

        m_color = GREEN if snap["temp_mos"] < 60 else (YELLOW if snap["temp_mos"] < 75 else RED)
        t_color = GREEN if snap["temp_motor"] < 70 else (YELLOW if snap["temp_motor"] < 85 else RED)
        _safe(stdscr, 10, C2, f"Temp FET: {snap['temp_mos']:5.1f} °C", m_color)
        _safe(stdscr, 11, C2, f"Temp Mot: {snap['temp_motor']:5.1f} °C", t_color)

        f_color = GREEN if snap["fault"] == 0 else RED | BOLD
        _safe(stdscr, 12, C2, f"Fault:    {fault_name(snap['fault']):<12}", f_color)

        _safe(stdscr, 13, C2, f"Ah used:  {snap['amp_hours']:5.3f} Ah")
        _safe(stdscr, 14, C2, f"Ah regen: {snap['amp_hours_regen']:5.3f} Ah")

        # ── Stats bar ─────────────────────────────────────────────────────
        _safe(stdscr, 16, 0, "─" * min(w - 1, 80))
        _safe(stdscr, 17, C1,
              f"PTY: {snap['slave_path']:<16}  Cmds recv: {snap['cmd_count']:<7}  ALIVEs: {snap['alive_count']}")
        _safe(stdscr, 18, C1,
              f"Last cmd: {snap['last_cmd']:<16}  Dist: {dist_km:5.2f} km   Runtime: {rm:02d}m{rs:02d}s")

        # ── Key help ──────────────────────────────────────────────────────
        _safe(stdscr, 20, 0, "─" * min(w - 1, 80))
        _safe(stdscr, 21, C1,
              "[t/T] or [↑/↓] Throttle ±5/10%   [b/B] Brake ±5%   [SPACE] Full-brake",
              YELLOW)
        _safe(stdscr, 22, C1,
              "[l] Turn-L  [r] Turn-R  [p] Power  [f] Inject fault  [0] Clear fault  [q] Quit",
              YELLOW)

        stdscr.refresh()


# ============================================================================
# Headless mode — print log to stdout
# ============================================================================

def run_headless(emu: VescEmulator, state: State) -> None:
    print(f"\nVESC Emulator running (headless)")
    print(f"  Slave PTY : {state.slave_path}")
    print(f"  Symlink   : /tmp/vesc_emu")
    print(f"  Protocol  : 115200 8N1 (VESC packet framing)")
    print(f"\nPress Ctrl+C to stop.\n")
    try:
        prev_cmds = 0
        while True:
            time.sleep(1.0)
            with state.lock:
                if state.cmd_count != prev_cmds:
                    speed_kmh = state.speed_ms * 3.6
                    print(
                        f"cmds={state.cmd_count:5d}  last={state.last_cmd:<18s}"
                        f"  speed={speed_kmh:4.1f} km/h  V={state.v_in:.1f}V"
                        f"  I={state.motor_current:.1f}A  soc={state.soc*100:.0f}%"
                        f"  fault={fault_name(state.fault_code)}"
                    )
                    prev_cmds = state.cmd_count
    except KeyboardInterrupt:
        pass


# ============================================================================
# Entry point
# ============================================================================

def main() -> None:
    ap = argparse.ArgumentParser(
        description="VESC Hardware Emulator for scooter-gose-vrooom",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    ap.add_argument(
        "--no-tui", action="store_true",
        help="Run headless — print PTY path and log to stdout",
    )
    ap.add_argument(
        "--soc", type=float, default=0.9, metavar="0-1",
        help="Initial battery state of charge (default 0.9 = 90%%)",
    )
    args = ap.parse_args()

    state = State()
    state.soc = max(0.0, min(1.0, args.soc))

    emu = VescEmulator(state)
    emu.start()

    def _shutdown(sig, frame):
        emu.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT,  _shutdown)
    signal.signal(signal.SIGTERM, _shutdown)

    if args.no_tui:
        run_headless(emu, state)
    else:
        try:
            curses.wrapper(run_tui, emu, state)
        except KeyboardInterrupt:
            pass
        finally:
            emu.stop()


if __name__ == "__main__":
    main()
