// ==============================================================================
// app.js — ScooterCtrl web dashboard
// Connects via WebSocket, updates gauges and stats in real time.
// ==============================================================================

// --- Constants ----------------------------------------------------------------
const MAX_SPEED    = 35;      // km/h at 100% arc
const R_SPEED      = 85;      // SVG circle radius (speed arc)
const R_BAT        = 68;      // SVG circle radius (battery arc)
const ARC_DEG      = 270;     // degrees of arc sweep
const RECONNECT_MS = 2000;    // WebSocket reconnect interval

// Arc circumferences
const C_SPEED = 2 * Math.PI * R_SPEED;   // ≈ 534
const C_BAT   = 2 * Math.PI * R_BAT;     // ≈ 427

// The visible arc is 270° of the full circle.
const ARC_FRAC = ARC_DEG / 360;
const ARC_LEN_SPEED = C_SPEED * ARC_FRAC;  // ≈ 401
const ARC_LEN_BAT   = C_BAT   * ARC_FRAC;  // ≈ 320

// --- DOM refs -----------------------------------------------------------------
const $ = id => document.getElementById(id);

const D = {
  connDot:      $('conn-dot'),
  speedVal:     $('speed-val'),
  speedArc:     $('speed-arc'),
  batArc:       $('bat-arc'),
  batPct:       $('bat-pct'),
  powerVal:     $('power-val'),
  tempVal:      $('temp-val'),
  currentVal:   $('current-val'),
  modeLabel:    $('mode-label'),
  faultBanner:  $('fault-banner'),
  faultText:    $('fault-text'),
  lastUpdate:   $('last-update'),

  // Stats tab
  statVoltage:  $('stat-voltage'),
  statMotorA:   $('stat-motor-a'),
  statInputA:   $('stat-input-a'),
  statDuty:     $('stat-duty'),
  statRpm:      $('stat-rpm'),
  statTempMotor:$('stat-temp-motor'),
  statAh:       $('stat-ah'),
  statWh:       $('stat-wh'),
  statWhRegen:  $('stat-wh-regen'),

  // Settings tab
  wsClients:    $('ws-clients'),
  vescStatus:   $('vesc-status'),
};

// --- Speedometer arc helper ---------------------------------------------------
/**
 * Sets an SVG <circle> element's stroke-dasharray to represent a filled arc.
 * @param {SVGCircleElement} el  - The circle element
 * @param {number} fraction      - 0.0 … 1.0
 * @param {number} fullArcLen    - Total arc length (pre-computed)
 * @param {number} circumference - Full circumference of the circle
 */
function setArc(el, fraction, fullArcLen, circumference) {
  const filled = Math.max(0, Math.min(1, fraction)) * fullArcLen;
  el.style.strokeDasharray = `${filled.toFixed(1)} ${circumference.toFixed(1)}`;
}

// Arc colour helpers
function speedColour(kmh) {
  if (kmh < 15) return '#00c48c';
  if (kmh < 25) return '#00aaff';
  if (kmh < 30) return '#ffaa00';
  return '#ff4444';
}

function batColour(pct) {
  if (pct > 40) return '#44dd88';
  if (pct > 15) return '#ffaa00';
  return '#ff4444';
}

// --- Update UI from telemetry JSON -------------------------------------------
function applyData(d) {
  const speed = parseFloat(d.speed_kmh) || 0;
  const bat   = parseInt(d.battery_pct) || 0;
  const power = parseFloat(d.power_w)   || 0;
  const temp  = parseFloat(d.temp_fet)  || 0;
  const amps  = parseFloat(d.input_amps)|| 0;

  // Speed
  D.speedVal.textContent = speed.toFixed(0);
  setArc(D.speedArc, speed / MAX_SPEED, ARC_LEN_SPEED, C_SPEED);
  D.speedArc.style.stroke = speedColour(speed);

  // Battery
  setArc(D.batArc, bat / 100, ARC_LEN_BAT, C_BAT);
  D.batArc.style.stroke = batColour(bat);
  D.batPct.textContent  = `${bat}%`;

  // Cards
  D.powerVal.textContent   = `${power.toFixed(0)} W`;
  D.tempVal.textContent    = `${temp.toFixed(0)} °C`;
  D.currentVal.textContent = `${amps.toFixed(1)} A`;

  // Fault
  const hasFault = d.fault && d.fault !== 'OK';
  D.faultBanner.classList.toggle('hidden', !hasFault);
  if (hasFault) D.faultText.textContent = `⚠ ${d.fault}`;

  // Connection status
  D.connDot.className = d.connected ? 'dot dot-green' : 'dot dot-red';
  D.vescStatus.textContent = d.connected ? '✓ Connected' : '✗ Offline';
  D.vescStatus.style.color = d.connected ? 'var(--good)' : 'var(--turbo)';

  // Stats tab
  D.statVoltage.textContent   = `${parseFloat(d.v_in).toFixed(2)} V`;
  D.statMotorA.textContent    = `${parseFloat(d.motor_amps).toFixed(2)} A`;
  D.statInputA.textContent    = `${amps.toFixed(2)} A`;
  D.statDuty.textContent      = `${parseFloat(d.duty).toFixed(1)} %`;
  D.statRpm.textContent       = `${parseInt(d.rpm)} ERPM`;
  D.statTempMotor.textContent = `${parseFloat(d.temp_motor).toFixed(1)} °C`;
  D.statAh.textContent        = `${parseFloat(d.amp_hours).toFixed(3)} Ah`;
  D.statWh.textContent        = `${parseFloat(d.watt_hours).toFixed(2)} Wh`;
  D.statWhRegen.textContent   = `${parseFloat(d.wh_regen).toFixed(2)} Wh`;

  // Sync mode buttons from server state (so BLE/display mode changes reflect here)
  if (d.mode_idx !== undefined) {
    const MODES = ['eco', 'sport', 'turbo'];
    const modeName = MODES[d.mode_idx] || 'sport';
    document.querySelectorAll('.mode-btn').forEach(b => {
      b.classList.toggle('active', b.dataset.mode === modeName);
    });
    D.modeLabel.textContent = modeName.toUpperCase();
    D.modeLabel.className   = `mode-badge ${modeName}`;
  }

  // Footer
  D.lastUpdate.textContent = `Updated ${new Date().toLocaleTimeString()}`;
}

// --- WebSocket ---------------------------------------------------------------
let ws = null;
let reconnectTimer = null;

function connect() {
  const url = `ws://${location.host}/ws`;
  ws = new WebSocket(url);

  ws.onopen = () => {
    console.log('[WS] Connected');
    D.connDot.className = 'dot dot-green';
    D.lastUpdate.textContent = 'Connected — waiting for data…';
    clearTimeout(reconnectTimer);
  };

  ws.onmessage = evt => {
    try {
      const data = JSON.parse(evt.data);
      applyData(data);
    } catch (e) {
      console.warn('[WS] JSON parse error:', e);
    }
  };

  ws.onerror = err => {
    console.warn('[WS] Error', err);
  };

  ws.onclose = () => {
    D.connDot.className = 'dot dot-red';
    D.lastUpdate.textContent = 'Disconnected — reconnecting…';
    reconnectTimer = setTimeout(connect, RECONNECT_MS);
  };
}

// --- Tab switching -----------------------------------------------------------
document.querySelectorAll('.tab').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
    document.querySelectorAll('.tab-content').forEach(s => s.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById(`tab-${btn.dataset.tab}`).classList.add('active');
  });
});

// --- Mode buttons ------------------------------------------------------------
const MODE_COLORS = { eco: '#00c48c', sport: '#00aaff', turbo: '#ff4444' };

document.querySelectorAll('.mode-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    const mode = btn.dataset.mode;

    document.querySelectorAll('.mode-btn').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');

    // Update mode badge
    D.modeLabel.textContent = mode.toUpperCase();
    D.modeLabel.className = `mode-badge ${mode}`;

    // Send to ESP32
    fetch(`/api/mode?m=${mode}`).catch(() => {});

    // Also send over WebSocket if open
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(`mode:${mode}`);
    }
  });
});

// --- Fetch initial state (fallback if WS slow) --------------------------------
async function fetchStatus() {
  try {
    const r = await fetch('/api/status');
    if (r.ok) {
      const data = await r.json();
      applyData(data);
    }
  } catch (_) {}
}

// --- Boot --------------------------------------------------------------------
fetchStatus();
connect();
