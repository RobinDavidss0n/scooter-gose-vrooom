#!/usr/bin/env python3
# =============================================================================
# vesc_poc.py — Minimal standalone POC to drive the VESC MINI 6.7 from a PC.
#
# Purpose: prove the VESC + motor + battery loop works WITHOUT the ESP32.
# Plug a USB-C cable from your laptop into the VESC and run this script.
#
#   g  -> GAS    (send 3 A motor current, only if speed < 5 km/h)
#   b  -> BRAKE  (send 2 A regen brake current)
#   s  -> STOP   (send 0 A — motor freewheels)
#   q  -> QUIT   (stops motor and exits)
#
# Live speed is printed once a second.
#
# Why COMM_SET_CURRENT instead of COMM_SET_RPM?
#   COMM_SET_RPM (ID=8) uses the VESC's internal PID speed controller.
#   That controller requires the motor-detection wizard to have been run AND
#   properly tuned PID gains.  Without both, it can oscillate or fail to start
#   at low speeds (especially with Hall sensors and a hub motor).
#
#   COMM_SET_CURRENT (ID=6) just sends a target current to the already-tuned
#   current controller (which IS set up by motor detection).  It is reliable
#   from the very first run.  Source — vedderb/bldc commands.c:
#     case COMM_SET_CURRENT:
#       mc_interface_set_current((float)buffer_get_int32(data,&ind) / 1000.0)
#   So multiply amps × 1000 and send as big-endian int32.
#
# COMM_GET_VALUES payload byte map (verified against vedderb/bldc commands.c,
# full 0xFFFFFFFF mask — i.e. plain COMM_GET_VALUES, not SELECTIVE):
#   [0]     cmd echo (0x04)
#   [1..2]  temp_fet     float16/10
#   [3..4]  temp_motor   float16/10
#   [5..8]  avg_motor_current  float32/100
#   [9..12] avg_input_current  float32/100
#   [13..16] avg_id   float32/100
#   [17..20] avg_iq   float32/100
#   [21..22] duty     float16/1000
#   [23..26] rpm      float32/1.0 stored as big-endian int32 (ERPM, signed)
#   [27..28] v_in     float16/10
#   ... (more fields follow, but we only need RPM)
#
# Dependencies:  pip install pyserial
# Usage:         python3 vesc_poc.py [serial_port]
#                e.g. python3 vesc_poc.py /dev/ttyACM0
#                     python3 vesc_poc.py COM5
#
# This script is self-contained — it does NOT import anything from this repo.
# =============================================================================

import sys
import time
import struct
import threading
import serial
import serial.tools.list_ports

# ---- Safety limits ----------------------------------------------------------
# Xiaomi Elite NE: 15 pole pairs, 8.5" wheel (~0.6786 m circ)
# speed_kmh = |erpm| / POLE_PAIRS * WHEEL_CIRC_M * 60 / 1000
POLE_PAIRS   = 15
WHEEL_CIRC_M = 0.216 * 3.14159   # 0.6786 m
MAX_KMH      = 5.0                # software speed cap
GAS_AMPS     = 3.0                # motor current in GAS state
BRAKE_AMPS   = 2.0                # regen current in BRAKE state
TICK_S       = 0.05               # main loop tick rate

# ---- VESC command IDs (from vedderb/bldc comm/commands.h) -------------------
COMM_GET_VALUES        = 4
COMM_SET_CURRENT       = 6    # int32 = amps × 1000
COMM_SET_CURRENT_BRAKE = 7    # int32 = amps × 1000 (positive only)
COMM_ALIVE             = 30


# ---- CRC16-CCITT (poly 0x1021, init 0x0000) — matches vedderb/bldc crc.c ----
def crc16(data: bytes) -> int:
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


# ---- Packet framing — short frames only (payload ≤ 255 bytes) ---------------
# Format: [0x02][LEN][PAYLOAD...][CRC_H][CRC_L][0x03]
# Source: vedderb/bldc comm/packet.c  packet_send_packet()
def make_frame(payload: bytes) -> bytes:
    assert 1 <= len(payload) <= 255
    c = crc16(payload)
    return bytes([0x02, len(payload)]) + payload + bytes([(c >> 8) & 0xFF, c & 0xFF, 0x03])


def vesc_send(ser: serial.Serial, payload: bytes) -> None:
    ser.write(make_frame(payload))


def vesc_recv(ser: serial.Serial, timeout: float = 0.12) -> bytes | None:
    """
    Read one short VESC reply frame.  Returns the payload bytes or None.

    Key design decisions:
    - We only accept start byte 0x02 (short frame, payload ≤ 255 bytes).
      The byte 0x03 is the END byte of every frame.  If we naively read the
      first available byte and treat 0x03 as a long-frame start, a leftover
      end-byte from the previous transaction (e.g. after a timeout) corrupts
      the entire subsequent stream.
    - We drain any non-0x02 bytes before attempting to parse a frame, so
      stale garbage from previous calls does not propagate.
    """
    deadline = time.monotonic() + timeout

    # Drain bytes until we see 0x02 or the deadline passes
    start = None
    while time.monotonic() < deadline:
        ser.timeout = max(0.005, deadline - time.monotonic())
        b = ser.read(1)
        if not b:
            break
        if b[0] == 0x02:
            start = b[0]
            break
        # Skip non-0x02 bytes (including stray 0x03 end-bytes)

    if start != 0x02:
        return None

    # Read length
    ser.timeout = max(0.005, deadline - time.monotonic())
    ln = ser.read(1)
    if not ln:
        return None
    n = ln[0]
    if n == 0:
        return None

    # Read payload
    ser.timeout = max(0.005, deadline - time.monotonic())
    payload = ser.read(n)
    if len(payload) != n:
        return None

    # Read [CRC_H][CRC_L][0x03]
    ser.timeout = max(0.005, deadline - time.monotonic())
    trailer = ser.read(3)
    if len(trailer) != 3 or trailer[2] != 0x03:
        return None

    rx_crc = (trailer[0] << 8) | trailer[1]
    if rx_crc != crc16(payload):
        return None

    return payload


# ---- VESC command helpers ---------------------------------------------------

def set_current(ser: serial.Serial, amps: float) -> None:
    """COMM_SET_CURRENT: positive = drive, 0 = freewheel/coast."""
    v = int(amps * 1000)
    vesc_send(ser, bytes([COMM_SET_CURRENT]) + struct.pack(">i", v))


def set_brake(ser: serial.Serial, amps: float) -> None:
    """COMM_SET_CURRENT_BRAKE: regen braking.  Firmware expects positive int."""
    v = int(abs(amps) * 1000)
    vesc_send(ser, bytes([COMM_SET_CURRENT_BRAKE]) + struct.pack(">i", v))


def alive(ser: serial.Serial) -> None:
    """COMM_ALIVE: resets VESC UART watchdog (must arrive < timeout, default 500 ms)."""
    vesc_send(ser, bytes([COMM_ALIVE]))


def poll_speed_kmh(ser: serial.Serial) -> float | None:
    """
    Request COMM_GET_VALUES and extract speed from the verified byte offsets.

    RPM is at payload bytes [23..26] as big-endian int32 with scale 1.0.
    Verified against vedderb/bldc commands.c:
      buffer_append_float32(send_buffer, mc_interface_get_rpm(), 1e0, &ind)
    where buffer_append_float32 writes (int32_t)(val * scale) big-endian.
    """
    ser.reset_input_buffer()   # discard stale bytes before fresh request
    vesc_send(ser, bytes([COMM_GET_VALUES]))
    p = vesc_recv(ser, timeout=0.15)
    if p is None or len(p) < 27 or p[0] != COMM_GET_VALUES:
        return None
    erpm = struct.unpack(">i", p[23:27])[0]
    mech_rpm = erpm / POLE_PAIRS
    return abs(mech_rpm) * WHEEL_CIRC_M * 60.0 / 1000.0


# ---- Non-blocking keypress (cross-platform POSIX + Windows) -----------------
class KeyReader:
    def __init__(self) -> None:
        self._key: str | None = None
        self._lock = threading.Lock()
        self._stop = False
        threading.Thread(target=self._run, daemon=True).start()

    def _run(self) -> None:
        try:
            import msvcrt  # Windows
            while not self._stop:
                if msvcrt.kbhit():
                    with self._lock:
                        self._key = msvcrt.getwch()
                time.sleep(0.02)
        except ImportError:
            import termios, tty, select  # noqa: E401
            fd = sys.stdin.fileno()
            old = termios.tcgetattr(fd)
            try:
                tty.setcbreak(fd)
                while not self._stop:
                    r, _, _ = select.select([sys.stdin], [], [], 0.05)
                    if r:
                        with self._lock:
                            self._key = sys.stdin.read(1)
            finally:
                termios.tcsetattr(fd, termios.TCSADRAIN, old)

    def get(self) -> str | None:
        with self._lock:
            k, self._key = self._key, None
            return k

    def stop(self) -> None:
        self._stop = True


# ---- Serial port auto-detect ------------------------------------------------
def pick_port() -> str:
    if len(sys.argv) > 1:
        return sys.argv[1]
    ports = list(serial.tools.list_ports.comports())
    # VESC MINI 6.7 runs ChibiOS and appears as a USB CDC ACM device.
    # Try to match known identifiers before falling back.
    for p in ports:
        desc = (p.description or "").lower() + (p.manufacturer or "").lower()
        if any(kw in desc for kw in ("vesc", "stm", "chibios", "vedder")):
            return p.device
    if ports:
        print("Available serial ports:")
        for p in ports:
            print(f"  {p.device}  —  {p.description}")
    print("\nCould not auto-detect VESC port.")
    print("Pass the port explicitly:  python3 vesc_poc.py /dev/ttyACM0")
    sys.exit(1)


# ---- Main control loop ------------------------------------------------------
def main() -> None:
    port = pick_port()
    print(f"[POC] opening {port} @ 115200 baud")
    ser = serial.Serial(port, 115200, timeout=0.05)
    time.sleep(0.3)           # allow USB-CDC enumeration to settle
    ser.reset_input_buffer()

    # Ensure motor is stopped before entering the loop
    set_current(ser, 0.0)
    alive(ser)
    time.sleep(0.1)

    max_erpm_approx = int(MAX_KMH * 1000 / 60 / WHEEL_CIRC_M * POLE_PAIRS)
    print(f"Controls:  g=gas ({GAS_AMPS} A)   b=brake ({BRAKE_AMPS} A regen)   "
          f"s=stop (0 A)   q=quit")
    print(f"Speed cap: {MAX_KMH} km/h  (~{max_erpm_approx} ERPM)  — "
          f"enforced by telemetry poll")
    print("Tip: set l_max_erpm = ~2000 in VESC Tool as a hardware backstop.")

    keys      = KeyReader()
    state     = "STOP"
    last_speed = 0.0
    last_alive = 0.0
    last_print = 0.0

    try:
        while True:
            tick_start = time.monotonic()

            # --- Handle keypress ---
            k = keys.get()
            if k:
                k = k.lower()
                if   k == "q": break
                elif k == "g": state = "GAS"
                elif k == "b": state = "BRAKE"
                elif k == "s": state = "STOP"

            # --- Apply state (with speed cap enforcement) ---
            if state == "GAS" and last_speed < MAX_KMH:
                set_current(ser, GAS_AMPS)
            elif state == "GAS":   # speed cap reached
                set_current(ser, 0.0)
            elif state == "BRAKE":
                set_brake(ser, BRAKE_AMPS)
            else:  # STOP / coast
                set_current(ser, 0.0)

            # --- Heartbeat (belt-and-suspenders; motor cmds already reset it) ---
            now = time.monotonic()
            if now - last_alive > 0.15:
                alive(ser)
                last_alive = now

            # --- Poll speed + status print once per second ---
            if now - last_print > 1.0:
                s = poll_speed_kmh(ser)
                if s is not None:
                    last_speed = s
                capped = " [CAPPED]" if state == "GAS" and last_speed >= MAX_KMH else ""
                print(f"[{state:5}]  speed={last_speed:4.1f} km/h{capped}",
                      flush=True)
                last_print = now

            # --- Maintain tick rate ---
            elapsed = time.monotonic() - tick_start
            if elapsed < TICK_S:
                time.sleep(TICK_S - elapsed)

    except KeyboardInterrupt:
        pass
    finally:
        print("\n[POC] stopping motor...")
        try:
            set_current(ser, 0.0)
            alive(ser)
            time.sleep(0.15)
        except Exception:
            pass
        ser.close()
        keys.stop()
        print("[POC] done")


if __name__ == "__main__":
    main()
