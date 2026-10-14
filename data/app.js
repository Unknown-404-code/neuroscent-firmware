/* ============================================================
   NeuroScent Dashboard — app.js
   Single-page application for ESP32-hosted web management
   ============================================================ */
'use strict';

// ---- Global State ----
const State = {
  page: 'dashboard',
  role: 'guest',          // 'guest' | 'admin'
  status: {},             // from /api/status
  network: {},            // from /api/network
  pumps: [],              // from /api/pumps
  accords: [],            // from /api/accords
  calibration: {},        // from /api/calibration
  formula: {},            // from /api/formula/status
  settings: {},           // from /api/settings
  pollingTimer: null,
  purgeRunning: false,
  calibRunning: false,
  activePump: null,       // for web operations
  calibStartMs: 0,
};

// ---- API helper ----
async function api(method, path, body) {
  const opts = { method, headers: {} };
  if (body !== undefined) {
    opts.headers['Content-Type'] = 'application/json';
    opts.body = JSON.stringify(body);
  }
  const res = await fetch(path, opts);
  let data;
  try { data = await res.json(); } catch { data = { status: 'error', message: 'Invalid response' }; }
  if (!res.ok && data.status !== 'ok') throw new Error(data.message || `HTTP ${res.status}`);
  return data;
}

// ---- Toast notifications ----
function toast(msg, type = 'info', duration = 4000) {
  const c = document.getElementById('toast-container');
  const icons = { success: '✓', error: '✕', info: 'ℹ' };
  const t = document.createElement('div');
  t.className = `toast ${type}`;
  t.innerHTML = `<span class="toast-icon">${icons[type] || 'ℹ'}</span><span>${msg}</span>`;
  c.appendChild(t);
  setTimeout(() => {
    t.classList.add('fadeout');
    setTimeout(() => t.remove(), 350);
  }, duration);
}

// ---- Confirm modal ----
let _confirmResolve = null;
function confirm(title, body, danger = true) {
  return new Promise(resolve => {
    _confirmResolve = resolve;
    document.getElementById('confirm-title').textContent = title;
    document.getElementById('confirm-body').textContent = body;
    const ok = document.getElementById('confirm-ok');
    ok.className = `btn ${danger ? 'btn-danger' : 'btn-primary'}`;
    document.getElementById('confirm-modal').classList.remove('hidden');
  });
}
function closeConfirm(yes) {
  document.getElementById('confirm-modal').classList.add('hidden');
  if (_confirmResolve) { _confirmResolve(yes); _confirmResolve = null; }
}

// ---- Sidebar (mobile) ----
function toggleSidebar() {
  document.getElementById('sidebar').classList.toggle('open');
}

// ---- Navigation ----
function navigate(page) {
  State.page = page;
  document.getElementById('sidebar').classList.remove('open');
  document.querySelectorAll('.nav-item').forEach(el => {
    el.classList.toggle('active', el.dataset.page === page);
  });
  renderPage();
}

// ---- Auth ----
function showLoginModal() {
  document.getElementById('login-modal').classList.remove('hidden');
  document.getElementById('login-pass').value = '';
  document.getElementById('login-error').classList.add('hidden');
  setTimeout(() => document.getElementById('login-pass').focus(), 50);
}
function hideLoginModal() {
  document.getElementById('login-modal').classList.add('hidden');
}
async function submitLogin() {
  const pass = document.getElementById('login-pass').value;
  const btn  = document.getElementById('login-btn');
  const err  = document.getElementById('login-error');
  btn.disabled = true; btn.textContent = 'Logging in…';
  err.classList.add('hidden');
  try {
    const d = await api('POST', '/api/auth/login', { password: pass });
    State.role = 'admin';
    updateAuthUI();
    hideLoginModal();
    toast('Logged in as administrator', 'success');
    renderPage();
  } catch (e) {
    err.textContent = e.message;
    err.classList.remove('hidden');
  } finally {
    btn.disabled = false; btn.textContent = 'Login';
  }
}
async function logout() {
  await api('POST', '/api/auth/logout').catch(() => {});
  State.role = 'guest';
  updateAuthUI();
  toast('Logged out', 'info');
  renderPage();
}
async function checkAuth() {
  try {
    const d = await api('GET', '/api/auth/status');
    State.role = d.role || 'guest';
  } catch { State.role = 'guest'; }
  updateAuthUI();
}
function updateAuthUI() {
  const badge  = document.getElementById('role-badge');
  const btnL   = document.getElementById('btn-login');
  const btnOut = document.getElementById('btn-logout');
  badge.textContent = State.role;
  badge.className   = `role-badge ${State.role}`;
  btnL.style.display   = State.role === 'admin' ? 'none'  : 'inline-flex';
  btnOut.style.display = State.role === 'admin' ? 'inline-flex' : 'none';
}
function requireAdmin(action) {
  if (State.role !== 'admin') { showLoginModal(); return false; }
  return true;
}

// ---- Polling ----
async function pollStatus() {
  try {
    const [status, network, pumps, formula] = await Promise.all([
      api('GET', '/api/status'),
      api('GET', '/api/network'),
      api('GET', '/api/pumps'),
      api('GET', '/api/formula/status'),
    ]);
    State.status  = status;
    State.network = network;
    State.pumps   = pumps.pumps || [];
    State.formula = formula;
    updateTopbar();
    if (['dashboard', 'pumps', 'accords'].includes(State.page)) {
      updatePageLive();
    }
  } catch (e) {
    console.warn('Poll error:', e.message);
    updateTopbarDisconnected();
  }
}

function updateTopbar() {
  const wDot   = document.getElementById('wifi-dot');
  const wLabel = document.getElementById('wifi-label');
  const sDot   = document.getElementById('sys-dot');
  const sLabel = document.getElementById('sys-label');

  const wState = State.network.wifi_state || 'disconnected';
  wDot.className   = 'status-dot ' + (wState === 'connected' ? 'connected' : wState === 'ap_mode' ? 'warn' : 'error');
  wLabel.textContent = wState === 'connected' ? (State.network.ssid || 'Wi-Fi') :
                       wState === 'ap_mode'   ? 'Setup Mode' : 'Disconnected';

  const sState = (State.status.state || 'idle').toLowerCase();
  const sClass = sState === 'idle' ? '' : sState.includes('error') ? 'error' : 'connected';
  sDot.className   = 'status-dot ' + sClass;
  sLabel.textContent = sState.replace(/_/g, ' ');
}

function updateTopbarDisconnected() {
  document.getElementById('wifi-dot').className   = 'status-dot error';
  document.getElementById('wifi-label').textContent = 'Offline';
  document.getElementById('sys-dot').className    = 'status-dot error';
  document.getElementById('sys-label').textContent = 'No connection';
}

// ---- Page rendering (full re-render on navigate) ----
function renderPage() {
  const pages = { dashboard, pumps, calibration, accords, formula, logs, settings };
  const fn = pages[State.page];
  if (fn) fn();
}

// ---- Live updates (partial re-renders during polling) ----
function updatePageLive() {
  if (State.page === 'dashboard') dashboard_update();
  if (State.page === 'pumps')     pumps_update();
}

// ==============================================================
// PAGE: DASHBOARD
// ==============================================================
async function dashboard() {
  // Fetch extra data needed by dashboard
  try {
    const [ac, status] = await Promise.all([
      api('GET', '/api/accords'),
      api('GET', '/api/status'),
    ]);
    State.accords = ac.accords || [];
    State.status  = status;
  } catch {}

  const p = document.getElementById('page-content');
  p.innerHTML = `
    <div class="page-header">
      <div>
        <h1 class="page-title">Dashboard</h1>
        <p class="page-sub">Live system monitoring</p>
      </div>
      <span class="state-badge ${stateClass(State.status.state)}" id="dash-state">
        ${dot(State.status.state)} ${(State.status.state || 'idle').replace(/_/g,' ')}
      </span>
    </div>

    <div class="cards-grid">
      <div class="stat-card">
        <div class="stat-label">System State</div>
        <div class="stat-value" id="d-state" style="font-size:1.1rem">${(State.status.state||'idle').replace(/_/g,' ')}</div>
        <div class="stat-sub">Firmware ${State.status.firmware||'—'}</div>
      </div>
      <div class="stat-card">
        <div class="stat-label">Uptime</div>
        <div class="stat-value" id="d-uptime">${formatUptime(State.status.uptime_s)}</div>
        <div class="stat-sub">Since last boot</div>
      </div>
      <div class="stat-card">
        <div class="stat-label">Wi-Fi</div>
        <div class="stat-value" style="font-size:1rem" id="d-wifi">${State.network.ssid || State.network.wifi_state || '—'}</div>
        <div class="stat-sub" id="d-ip">${State.network.ip || ''}</div>
      </div>
      <div class="stat-card">
        <div class="stat-label">Signal</div>
        <div class="stat-value" id="d-rssi">${State.network.rssi ? State.network.rssi + ' dBm' : '—'}</div>
        <div class="stat-sub">${rssiLabel(State.network.rssi)}</div>
      </div>
    </div>

    <!-- Formula progress (shown when active) -->
    <div id="formula-progress-section" class="${State.formula.formula_state === 'idle' ? 'hidden' : ''}">
      <div class="card">
        <div class="card-title">Active Formula</div>
        <div style="display:flex;align-items:center;justify-content:space-between;margin-bottom:12px;">
          <span class="state-badge ${stateClass(State.formula.formula_state)}">
            ${dot(State.formula.formula_state)} ${(State.formula.formula_state||'idle').replace(/_/g,' ')}
          </span>
          <span class="text-muted" style="font-size:0.85rem" id="d-formula-pump">
            ${State.formula.running_pump >= 0 ? 'Pump ' + (State.formula.running_pump + 1) : ''}
          </span>
        </div>
        <div class="progress-bar"><div class="fill" id="d-formula-bar" style="width:${State.formula.step_progress||0}%"></div></div>
        <div style="margin-top:8px;font-size:0.82rem;color:var(--text-muted)">Step progress: <span id="d-formula-pct">${State.formula.step_progress||0}</span>%</div>
        ${State.formula.formula_state === 'awaiting_confirmation'
          ? '<p style="margin-top:12px;color:var(--warn);font-size:0.88rem;">⚠ Confirm the formula using the physical NeuroScent device buttons.</p>'
          : ''}
      </div>
    </div>

    <!-- Pump overview -->
    <div class="card">
      <div class="card-title">Pump Overview</div>
      <div class="pumps-grid" id="dash-pumps">
        ${State.pumps.map(renderPumpCard).join('')}
      </div>
    </div>

    <!-- Accord levels -->
    <div class="card">
      <div class="card-title">Accord Levels</div>
      <div id="dash-accords">
        ${State.accords.length ? renderAccordBars(State.accords) : '<div class="text-muted">Loading…</div>'}
      </div>
    </div>
  `;
}

function dashboard_update() {
  const dState = document.getElementById('d-state');
  const dBadge = document.getElementById('dash-state');
  const dUp    = document.getElementById('d-uptime');
  const dWifi  = document.getElementById('d-wifi');
  const dIp    = document.getElementById('d-ip');
  const dRssi  = document.getElementById('d-rssi');
  const dPumps = document.getElementById('dash-pumps');
  const fSec   = document.getElementById('formula-progress-section');
  const fBar   = document.getElementById('d-formula-bar');
  const fPct   = document.getElementById('d-formula-pct');
  const fPump  = document.getElementById('d-formula-pump');

  if (dState) dState.textContent = (State.status.state||'idle').replace(/_/g,' ');
  if (dBadge) { dBadge.className = `state-badge ${stateClass(State.status.state)}`; dBadge.innerHTML = `${dot(State.status.state)} ${(State.status.state||'idle').replace(/_/g,' ')}`; }
  if (dUp)    dUp.textContent = formatUptime(State.status.uptime_s);
  if (dWifi)  dWifi.textContent = State.network.ssid || State.network.wifi_state || '—';
  if (dIp)    dIp.textContent = State.network.ip || '';
  if (dRssi)  dRssi.textContent = State.network.rssi ? State.network.rssi + ' dBm' : '—';
  if (dPumps) dPumps.innerHTML = State.pumps.map(renderPumpCard).join('');

  if (fSec) fSec.classList.toggle('hidden', State.formula.formula_state === 'idle');
  if (fBar) fBar.style.width = (State.formula.step_progress || 0) + '%';
  if (fPct) fPct.textContent = State.formula.step_progress || 0;
  if (fPump && State.formula.running_pump >= 0)
    fPump.textContent = 'Pump ' + (State.formula.running_pump + 1);
}

function renderPumpCard(p) {
  const pct  = Math.round(p.percent);
  const cls  = p.running ? 'running' : p.low_liquid ? 'low' : '';
  const indCls = p.running ? 'on' : p.low_liquid ? 'low' : '';
  const fillCls = pct <= 20 ? 'error' : pct <= 40 ? 'warn' : '';
  return `<div class="pump-card ${cls}">
    <div class="pump-header">
      <div>
        <div class="pump-num">Pump ${p.pump}</div>
        <div class="pump-name">${p.name || 'Unassigned'}</div>
      </div>
      <div class="pump-indicator ${indCls}"></div>
    </div>
    <div class="pump-ml">${p.remaining_ml.toFixed(1)} / ${p.capacity_ml.toFixed(0)} mL</div>
    <div class="progress-bar"><div class="fill ${fillCls}" style="width:${pct}%"></div></div>
    ${p.low_liquid ? '<div style="font-size:0.75rem;color:var(--warn);margin-top:4px;">⚠ Low liquid</div>' : ''}
  </div>`;
}

function renderAccordBars(accords) {
  return accords.map(a => {
    const pct = Math.round(a.percent);
    const fillCls = pct <= 20 ? 'error' : pct <= 40 ? 'warn' : '';
    return `<div style="display:flex;align-items:center;gap:12px;margin-bottom:10px;">
      <div style="width:72px;font-size:0.82rem;color:var(--text-muted);flex-shrink:0">
        P${a.pump} ${a.name ? '<span style="color:var(--text-dim)">'+a.name+'</span>' : ''}
      </div>
      <div class="progress-bar" style="flex:1"><div class="fill ${fillCls}" style="width:${pct}%"></div></div>
      <div style="width:56px;text-align:right;font-size:0.82rem;color:var(--text-dim)">${a.remaining_ml.toFixed(1)} mL</div>
      ${a.low_liquid ? '<span style="color:var(--warn);font-size:0.8rem">⚠</span>' : '<span style="width:16px"></span>'}
    </div>`;
  }).join('');
}

// ==============================================================
// PAGE: PUMPS
// ==============================================================
async function pumps() {
  const p = document.getElementById('page-content');
  p.innerHTML = `
    <div class="page-header">
      <div>
        <h1 class="page-title">Pumps</h1>
        <p class="page-sub">Purge and test individual pumps</p>
      </div>
    </div>
    <div class="card" style="margin-bottom:0">
      <div class="card-title">Web Purge Control</div>
      <p style="color:var(--text-muted);font-size:0.85rem;margin-bottom:16px;">
        Select a pump and press Start Purging. The pump will run until you stop it or the maximum duration is reached (firmware-enforced).
        <span style="color:var(--warn)"> Admin only.</span>
      </p>
      <div style="display:flex;gap:12px;flex-wrap:wrap;margin-bottom:20px;">
        <select class="form-select" id="purge-pump-select" style="max-width:180px;">
          ${Array.from({length:8},(_,i)=>`<option value="${i+1}">Pump ${i+1}</option>`).join('')}
        </select>
        <button id="purge-start-btn" class="btn btn-success" onclick="startPurge()">▶ Start Purging</button>
        <button id="purge-stop-btn"  class="btn btn-danger hidden" onclick="stopPurge()">■ Stop Purging</button>
      </div>
      <div id="purge-status" class="hidden" style="padding:12px;background:var(--surface2);border-radius:var(--radius-sm);font-size:0.88rem;">
        Running Pump <span id="purge-pump-num">—</span> &nbsp;·&nbsp;
        Elapsed: <span id="purge-elapsed">0s</span>
      </div>
    </div>
    <div class="card" style="margin-top:18px">
      <div class="card-title">Pump Status</div>
      <div class="pumps-grid" id="pumps-grid">
        ${State.pumps.map(renderPumpCard).join('')}
      </div>
    </div>
  `;
  updatePurgeUI();
}

function pumps_update() {
  const grid = document.getElementById('pumps-grid');
  if (grid) grid.innerHTML = State.pumps.map(renderPumpCard).join('');
  updatePurgeElapsed();
}

function updatePurgeUI() {
  const start = document.getElementById('purge-start-btn');
  const stop  = document.getElementById('purge-stop-btn');
  const sts   = document.getElementById('purge-status');
  if (!start) return;
  if (State.purgeRunning) {
    start.classList.add('hidden');
    stop.classList.remove('hidden');
    sts.classList.remove('hidden');
    document.getElementById('purge-pump-num').textContent = State.activePump;
  } else {
    start.classList.remove('hidden');
    stop.classList.add('hidden');
    sts.classList.add('hidden');
  }
}

function updatePurgeElapsed() {
  if (!State.purgeRunning) return;
  const el = document.getElementById('purge-elapsed');
  if (el) el.textContent = Math.round((Date.now() - State.calibStartMs) / 1000) + 's';
}

async function startPurge() {
  if (!requireAdmin()) return;
  const pump = parseInt(document.getElementById('purge-pump-select').value);
  try {
    await api('POST', '/api/pumps/purge/start', { pump });
    State.purgeRunning = true;
    State.activePump   = pump;
    State.calibStartMs = Date.now();
    updatePurgeUI();
    toast(`Purge started — Pump ${pump}`, 'success');
  } catch(e) { toast(e.message, 'error'); }
}
async function stopPurge() {
  try {
    await api('POST', '/api/pumps/purge/stop');
    State.purgeRunning = false;
    State.activePump   = null;
    updatePurgeUI();
    toast('Purge stopped', 'info');
  } catch(e) { toast(e.message, 'error'); }
}

// ==============================================================
// PAGE: CALIBRATION
// ==============================================================
async function calibration() {
  try {
    const d = await api('GET', '/api/calibration');
    State.calibration = d;
  } catch {}

  const target = State.calibration.target_ml || 100;
  const rows = (State.calibration.calibrations || []).map(c => `
    <tr>
      <td>Pump ${c.pump}</td>
      <td class="font-mono">${c.ml_per_ms.toFixed(6)}</td>
      <td class="font-mono">${c.ml_per_s.toFixed(4)} mL/s</td>
      <td>
        <button class="btn btn-success btn-sm" onclick="startCalib(${c.pump})">▶ Start</button>
        <button class="btn btn-warn btn-sm" onclick="stopCalib()" id="calib-stop-${c.pump}" style="display:none">■ Stop</button>
        <button class="btn btn-ghost btn-sm" onclick="resetCalib(${c.pump})">↺ Reset</button>
      </td>
    </tr>`).join('');

  document.getElementById('page-content').innerHTML = `
    <div class="page-header">
      <div>
        <h1 class="page-title">Calibration</h1>
        <p class="page-sub">Calibrate pump flow rates. Target volume: <strong>${target} mL</strong> (change in Settings)</p>
      </div>
    </div>
    <div class="card">
      <div id="calib-active-msg" class="hidden" style="padding:14px;background:rgba(245,158,11,0.08);border:1px solid rgba(245,158,11,0.3);border-radius:var(--radius-sm);margin-bottom:16px;">
        <strong style="color:var(--warn)">🔬 Calibrating Pump <span id="calib-active-pump">—</span></strong>
        &nbsp; Elapsed: <span id="calib-elapsed">0s</span>
        <p style="font-size:0.82rem;color:var(--text-muted);margin-top:6px;">
          Collect <strong>${target} mL</strong> of liquid, then press ■ Stop to save the calibration.
        </p>
      </div>
      <div class="card-title">Calibration Values</div>
      <div style="overflow-x:auto;">
        <table class="data-table">
          <thead><tr>
            <th>Pump</th>
            <th>mL/ms (raw)</th>
            <th>mL/s (display)</th>
            <th>Actions</th>
          </tr></thead>
          <tbody id="calib-table">${rows}</tbody>
        </table>
      </div>
    </div>`;
  updateCalibTimer();
}

let _calibTimerHandle = null;
function updateCalibTimer() {
  clearInterval(_calibTimerHandle);
  if (!State.calibRunning) return;
  _calibTimerHandle = setInterval(() => {
    const el = document.getElementById('calib-elapsed');
    if (el) el.textContent = Math.round((Date.now() - State.calibStartMs) / 1000) + 's';
  }, 500);
}

async function startCalib(pump) {
  if (!requireAdmin()) return;
  try {
    await api('POST', '/api/calibration/start', { pump });
    State.calibRunning = true;
    State.activePump   = pump;
    State.calibStartMs = Date.now();
    const msg = document.getElementById('calib-active-msg');
    const pumpEl = document.getElementById('calib-active-pump');
    if (msg) { msg.classList.remove('hidden'); }
    if (pumpEl) pumpEl.textContent = pump;
    // Show stop button for this pump
    const stopBtn = document.getElementById(`calib-stop-${pump}`);
    if (stopBtn) stopBtn.style.display = 'inline-flex';
    updateCalibTimer();
    toast(`Calibration started — Pump ${pump}`, 'success');
  } catch(e) { toast(e.message, 'error'); }
}

async function stopCalib() {
  try {
    const d = await api('POST', '/api/calibration/stop');
    State.calibRunning = false;
    clearInterval(_calibTimerHandle);
    toast(`Pump ${d.pump} calibrated: ${d.ml_per_s.toFixed(4)} mL/s`, 'success', 6000);
    calibration(); // Refresh page
  } catch(e) { toast(e.message, 'error'); }
}

async function resetCalib(pump) {
  const ok = await confirm(`Reset Pump ${pump} Calibration?`,
    `This will reset Pump ${pump} to the default calibration value (${(0.005).toFixed(6)} mL/ms). This cannot be undone.`);
  if (!ok) return;
  try {
    await api('POST', '/api/calibration/reset', { pump });
    toast(`Pump ${pump} calibration reset`, 'info');
    calibration();
  } catch(e) { toast(e.message, 'error'); }
}

// ==============================================================
// PAGE: ACCORDS
// ==============================================================
async function accords() {
  try {
    const d = await api('GET', '/api/accords');
    State.accords = d.accords || [];
  } catch {}

  const rows = State.accords.map(a => {
    const pct     = Math.round(a.percent);
    const fillCls = pct <= 20 ? 'error' : pct <= 40 ? 'warn' : '';
    return `<tr>
      <td><strong>Pump ${a.pump}</strong></td>
      <td>${a.name || '<span class="text-muted">—</span>'}</td>
      <td>
        <div style="display:flex;align-items:center;gap:10px;">
          <div class="progress-bar" style="width:100px;"><div class="fill ${fillCls}" style="width:${pct}%"></div></div>
          <span style="font-size:0.82rem;color:var(--text-dim)">${a.remaining_ml.toFixed(1)} / ${a.capacity_ml.toFixed(0)} mL (${pct}%)</span>
          ${a.low_liquid ? '<span style="color:var(--warn);font-size:0.8rem">⚠ Low</span>' : ''}
        </div>
      </td>
      <td>
        <button class="btn btn-ghost btn-sm" onclick="editAccord(${a.pump})">✏ Edit</button>
        <button class="btn btn-success btn-sm" onclick="refillAccord(${a.pump})">↑ Refill</button>
      </td>
    </tr>`;
  }).join('');

  document.getElementById('page-content').innerHTML = `
    <div class="page-header">
      <div><h1 class="page-title">Accords</h1><p class="page-sub">Monitor and manage accord containers</p></div>
    </div>
    <div class="card">
      <div class="card-title">Container Status</div>
      <div style="overflow-x:auto;">
        <table class="data-table">
          <thead><tr><th>Pump</th><th>Accord Name</th><th>Level</th><th>Actions</th></tr></thead>
          <tbody>${rows}</tbody>
        </table>
      </div>
    </div>

    <!-- Edit accordion (hidden by default) -->
    <div id="accord-edit-panel" class="card hidden">
      <div class="card-title">Edit Accord — Pump <span id="edit-pump-num">—</span></div>
      <div style="display:grid;gap:14px;max-width:400px;">
        <div class="form-group">
          <label class="form-label">Accord Name</label>
          <input class="form-input" type="text" id="edit-name" placeholder="e.g. Rose, Sandalwood"/>
        </div>
        <div class="form-group">
          <label class="form-label">Remaining mL</label>
          <input class="form-input" type="number" id="edit-remaining" min="0" step="0.1"/>
        </div>
        <div class="form-group">
          <label class="form-label">Container Capacity (mL)</label>
          <input class="form-input" type="number" id="edit-capacity" min="1" step="1"/>
        </div>
        <div style="display:flex;gap:10px;">
          <button class="btn btn-primary" onclick="saveAccordEdit()">Save Changes</button>
          <button class="btn btn-ghost" onclick="document.getElementById('accord-edit-panel').classList.add('hidden')">Cancel</button>
        </div>
      </div>
    </div>`;
}

function editAccord(pump) {
  const a = State.accords.find(x => x.pump === pump);
  if (!a) return;
  if (!requireAdmin()) return;
  document.getElementById('edit-pump-num').textContent = pump;
  document.getElementById('edit-name').value      = a.name || '';
  document.getElementById('edit-remaining').value = a.remaining_ml.toFixed(1);
  document.getElementById('edit-capacity').value  = a.capacity_ml.toFixed(0);
  document.getElementById('accord-edit-panel').dataset.pump = pump;
  document.getElementById('accord-edit-panel').classList.remove('hidden');
  document.getElementById('accord-edit-panel').scrollIntoView({behavior:'smooth'});
}

async function saveAccordEdit() {
  const pump = parseInt(document.getElementById('accord-edit-panel').dataset.pump);
  const name = document.getElementById('edit-name').value.trim();
  const rem  = parseFloat(document.getElementById('edit-remaining').value);
  const cap  = parseFloat(document.getElementById('edit-capacity').value);
  try {
    if (name !== '') await api('POST', '/api/accords/name',     { pump, name });
    await api('POST', '/api/accords/capacity', { pump, capacity_ml: cap });
    await api('POST', '/api/accords/set',      { pump, remaining_ml: rem });
    toast('Accord updated', 'success');
    accords();
  } catch(e) { toast(e.message, 'error'); }
}

async function refillAccord(pump) {
  if (!requireAdmin()) return;
  const ok = await confirm(`Refill Pump ${pump}?`,
    `This will reset the remaining liquid to the full container capacity for Pump ${pump}.`, false);
  if (!ok) return;
  try {
    await api('POST', '/api/accords/refill', { pump });
    toast(`Pump ${pump} refilled`, 'success');
    accords();
  } catch(e) { toast(e.message, 'error'); }
}

// ==============================================================
// PAGE: FORMULA TESTING
// ==============================================================
async function formula() {
  const p = document.getElementById('page-content');
  p.innerHTML = `
    <div class="page-header">
      <div><h1 class="page-title">Formula Testing</h1>
        <p class="page-sub">Build and submit test formulas. Physical confirmation on the device is required before dispensing.</p>
      </div>
    </div>
    <div style="display:grid;gap:18px;grid-template-columns:1fr 1fr;">
      <div class="card">
        <div class="card-title">Formula Builder</div>
        <div class="form-group">
          <label class="form-label">Formula Name</label>
          <input class="form-input" type="text" id="fm-name" placeholder="e.g. Test Mix"/>
        </div>
        <div class="form-group">
          <label class="form-label">Formula ID</label>
          <input class="form-input" type="number" id="fm-id" value="${Math.floor(Math.random()*900)+100}" min="1"/>
        </div>
        <div class="form-group">
          <label class="form-label">Container Size (mL)</label>
          <input class="form-input" type="number" id="fm-container" value="10" min="1" step="1" oninput="formulaRecalc()"/>
        </div>
        <div class="card-title" style="margin-top:12px">Pump Percentages</div>
        <p style="font-size:0.8rem;color:var(--text-muted);margin-bottom:10px;">Enter % for each pump. Total must equal 100%.</p>
        <div id="formula-pumps">
          ${Array.from({length:8},(_,i)=>`
            <div style="display:flex;align-items:center;gap:10px;margin-bottom:8px;">
              <span style="width:52px;font-size:0.82rem;color:var(--text-muted)">Pump ${i+1}</span>
              <input class="form-input" type="number" id="fp-${i+1}" placeholder="0" min="0" max="100" step="0.1"
                     oninput="formulaRecalc()" style="max-width:90px;"/>
              <span style="font-size:0.8rem;color:var(--text-muted)">%</span>
              <span id="fp-ml-${i+1}" style="font-size:0.78rem;color:var(--text-dim);min-width:50px">0.00 mL</span>
            </div>`).join('')}
        </div>
        <div style="margin-top:12px;padding:10px;background:var(--surface2);border-radius:var(--radius-sm);font-size:0.85rem;">
          Total: <strong id="fm-total" class="text-accent">0%</strong>
          <span id="fm-total-err" class="text-error hidden"> — must equal 100%</span>
        </div>
        <div style="margin-top:16px;display:flex;gap:10px;">
          <button class="btn btn-ghost btn-sm" onclick="formulaClear()">Clear</button>
          <button class="btn btn-primary" onclick="formulaSubmit()">▶ Submit Formula</button>
        </div>
      </div>
      <div class="card">
        <div class="card-title">Formula Status</div>
        <div id="formula-status-panel">
          <div class="empty-state">
            <div class="empty-icon">🧪</div>
            <h3>No active formula</h3>
            <p>Submit a formula using the builder. Confirm on the device when prompted.</p>
          </div>
        </div>
        <div class="card-title" style="margin-top:20px">JSON Preview</div>
        <pre id="formula-json" style="background:var(--surface2);border:1px solid var(--border);border-radius:var(--radius-sm);padding:12px;font-size:0.78rem;overflow-x:auto;color:var(--text-dim);max-height:200px;overflow-y:auto;">{}</pre>
      </div>
    </div>`;
  formulaRecalc();
  formulaStatusPoll();
}

function formulaRecalc() {
  const container = parseFloat(document.getElementById('fm-container')?.value) || 0;
  let total = 0;
  for (let i = 1; i <= 8; i++) {
    const v = parseFloat(document.getElementById(`fp-${i}`)?.value) || 0;
    total += v;
    const mlEl = document.getElementById(`fp-ml-${i}`);
    if (mlEl) mlEl.textContent = (container * v / 100).toFixed(2) + ' mL';
  }
  const totEl = document.getElementById('fm-total');
  const errEl = document.getElementById('fm-total-err');
  if (totEl) { totEl.textContent = total.toFixed(1) + '%'; totEl.className = Math.abs(total-100)<0.1 ? 'text-success' : 'text-error'; }
  if (errEl) errEl.classList.toggle('hidden', Math.abs(total-100)<0.1 || total === 0);
  updateFormulaJson();
}

function buildFormulaJson() {
  const id   = parseInt(document.getElementById('fm-id')?.value) || 0;
  const name = document.getElementById('fm-name')?.value?.trim() || '';
  const container = parseInt(document.getElementById('fm-container')?.value) || 0;
  const steps = [];
  for (let i = 1; i <= 8; i++) {
    const v = parseFloat(document.getElementById(`fp-${i}`)?.value) || 0;
    if (v > 0) steps.push({ pump: i, percent: parseFloat(v.toFixed(2)) });
  }
  return { id, name, container_ml: container, formula: steps };
}

function updateFormulaJson() {
  const el = document.getElementById('formula-json');
  if (el) el.textContent = JSON.stringify(buildFormulaJson(), null, 2);
}

function formulaClear() {
  for (let i = 1; i <= 8; i++) {
    const el = document.getElementById(`fp-${i}`);
    if (el) el.value = '';
  }
  formulaRecalc();
}

async function formulaSubmit() {
  if (!requireAdmin()) return;
  const j = buildFormulaJson();
  // Validate
  if (!j.name) { toast('Formula name is required', 'error'); return; }
  if (!j.id)   { toast('Formula ID is required', 'error'); return; }
  if (!j.container_ml || j.container_ml < 1) { toast('Container size must be ≥ 1 mL', 'error'); return; }
  if (!j.formula.length) { toast('Add at least one pump', 'error'); return; }
  const total = j.formula.reduce((s,x)=>s+x.percent, 0);
  if (Math.abs(total - 100) > 0.1) { toast(`Percentages total ${total.toFixed(1)}% — must be 100%`, 'error'); return; }

  try {
    const d = await api('POST', '/api/formula/submit', j);
    toast('Formula submitted — confirm on the NeuroScent device!', 'success', 8000);
    formulaStatusPoll();
  } catch(e) { toast(e.message, 'error'); }
}

async function formulaStatusPoll() {
  try {
    const d = await api('GET', '/api/formula/status');
    State.formula = d;
    const el = document.getElementById('formula-status-panel');
    if (!el) return;
    if (d.formula_state === 'idle') {
      el.innerHTML = `<div class="empty-state"><div class="empty-icon">🧪</div><h3>No active formula</h3><p>Submit a formula using the builder. Confirm on the device when prompted.</p></div>`;
    } else {
      el.innerHTML = `
        <div style="display:flex;flex-direction:column;gap:12px;">
          <div><span class="state-badge ${stateClass(d.formula_state)}">${dot(d.formula_state)} ${d.formula_state.replace(/_/g,' ')}</span></div>
          ${d.formula_state==='awaiting_confirmation' ? '<p style="color:var(--warn);font-size:0.88rem;">⚠ Press SELECT on the device to confirm dispensing.</p>' : ''}
          ${d.formula_state==='dispensing' ? `
            <div>
              <div style="font-size:0.82rem;color:var(--text-muted);margin-bottom:6px;">Step Progress</div>
              <div class="progress-bar"><div class="fill" style="width:${d.step_progress||0}%"></div></div>
              <div style="font-size:0.78rem;color:var(--text-muted);margin-top:4px;">${d.step_progress||0}% • Pump ${(d.running_pump||0)+1}</div>
            </div>` : ''}
        </div>`;
    }
  } catch {}
}

// ==============================================================
// PAGE: LOGS
// ==============================================================
async function logs() {
  const p = document.getElementById('page-content');
  p.innerHTML = `<div class="page-header">
    <div><h1 class="page-title">Activity Logs</h1><p class="page-sub">System event history (admin only)</p></div>
    <button class="btn btn-danger btn-sm" onclick="clearLogs()">🗑 Clear Logs</button>
  </div>
  <div class="card">
    <div id="logs-content"><div class="flex-center" style="padding:40px;"><div class="spinner"></div></div></div>
  </div>`;
  fetchLogs();
}

async function fetchLogs() {
  const el = document.getElementById('logs-content');
  if (!el) return;
  try {
    const d = await api('GET', '/api/logs');
    if (!d.logs || !d.logs.length) {
      el.innerHTML = `<div class="empty-state"><div class="empty-icon">📋</div><h3>No logs</h3><p>Events will appear here as the system operates.</p></div>`;
      return;
    }
    const sevClass = { INFO: 'log-info', WARN: 'log-warn', ERROR: 'log-error' };
    el.innerHTML = `<div style="overflow-x:auto;"><table class="data-table">
      <thead><tr><th>Time</th><th>Severity</th><th>Event</th><th>Pump</th><th>Description</th></tr></thead>
      <tbody>${d.logs.map(l=>`<tr class="${sevClass[l.severity]||''}">
        <td class="font-mono" style="white-space:nowrap">${l.uptime}</td>
        <td><span class="log-sev ${sevClass[l.severity]||''}">${l.severity}</span></td>
        <td style="font-size:0.8rem">${l.event}</td>
        <td>${l.pump>=0?'P'+(l.pump+1):'—'}</td>
        <td>${l.description}</td>
      </tr>`).join('')}</tbody>
    </table></div>`;
  } catch(e) {
    el.innerHTML = `<div class="empty-state"><div class="empty-icon">🔒</div><h3>Authentication required</h3><p>Log viewing requires admin login.</p><button class="btn btn-primary" style="margin-top:12px" onclick="showLoginModal()">Login</button></div>`;
  }
}

async function clearLogs() {
  if (!requireAdmin()) return;
  const ok = await confirm('Clear All Logs?', 'This will permanently delete all activity logs. This cannot be undone.');
  if (!ok) return;
  try {
    await fetch('/api/logs/clear', {method:'DELETE'});
    toast('Logs cleared', 'info');
    fetchLogs();
  } catch(e) { toast(e.message, 'error'); }
}

// ==============================================================
// PAGE: SETTINGS
// ==============================================================
async function settings() {
  let s = {}, dev = {};
  try { [s, dev] = await Promise.all([api('GET','/api/settings'), api('GET','/api/device')]); State.settings = s; } catch {}

  document.getElementById('page-content').innerHTML = `
    <div class="page-header"><div><h1 class="page-title">Settings</h1><p class="page-sub">Configure device behaviour</p></div></div>

    <!-- Device Info -->
    <div class="card">
      <div class="card-title">Device Information</div>
      <table class="data-table">
        <tbody>
          <tr><td class="text-muted">Device</td><td>${dev.device||'NeuroScent'}</td></tr>
          <tr><td class="text-muted">Chip</td><td>${dev.chip||'ESP32-S3'}</td></tr>
          <tr><td class="text-muted">Firmware</td><td class="font-mono">${dev.firmware||'—'}</td></tr>
          <tr><td class="text-muted">Flash</td><td>${dev.flash_mb||16} MB</td></tr>
          <tr><td class="text-muted">Free Heap</td><td class="font-mono">${dev.free_heap ? (dev.free_heap/1024).toFixed(1)+' KB' : '—'}</td></tr>
          <tr><td class="text-muted">Free PSRAM</td><td class="font-mono">${dev.psram_free ? (dev.psram_free/1024/1024).toFixed(1)+' MB' : '—'}</td></tr>
          <tr><td class="text-muted">Uptime</td><td>${formatUptime(dev.uptime_s)}</td></tr>
          <tr><td class="text-muted">IP Address</td><td class="font-mono">${State.network.ip||'—'}</td></tr>
          <tr><td class="text-muted">mDNS</td><td class="font-mono">${State.network.mdns||'—'}</td></tr>
          <tr><td class="text-muted">RSSI</td><td>${State.network.rssi||'—'} dBm</td></tr>
        </tbody>
      </table>
    </div>

    <!-- Pump Settings -->
    <div class="card">
      <div class="card-title">Pump Settings</div>
      <div class="form-group">
        <label class="form-label">Max Web Purge Duration (seconds)</label>
        <input class="form-input" type="number" id="s-purge" value="${s.max_purge_s||60}" min="5" max="300" style="max-width:180px;"/>
        <div class="form-hint">Firmware-enforced. Range: 5–300 s</div>
      </div>
      <button class="btn btn-primary btn-sm" onclick="saveSetting('pumps',{max_purge_s:+document.getElementById('s-purge').value})">Save</button>
    </div>

    <!-- Calibration Settings -->
    <div class="card">
      <div class="card-title">Calibration Settings</div>
      <div class="form-group">
        <label class="form-label">Calibration Target Volume (mL)</label>
        <input class="form-input" type="number" id="s-calib-ml" value="${s.calib_target_ml||100}" min="5" max="500" style="max-width:180px;"/>
        <div class="form-hint">Volume of liquid to collect during calibration. Applies to future calibrations only.</div>
      </div>
      <div class="form-group">
        <label class="form-label">Calibration Safety Timeout (seconds)</label>
        <input class="form-input" type="number" id="s-calib-to" value="${s.calib_timeout_s||300}" min="30" max="600" style="max-width:180px;"/>
      </div>
      <button class="btn btn-primary btn-sm" onclick="saveSetting('calibration',{calib_target_ml:+document.getElementById('s-calib-ml').value, calib_timeout_s:+document.getElementById('s-calib-to').value})">Save</button>
    </div>

    <!-- Accord Settings -->
    <div class="card">
      <div class="card-title">Accord Settings</div>
      <div class="form-group">
        <label class="form-label">Low Liquid Warning Threshold (%)</label>
        <input class="form-input" type="number" id="s-low" value="${s.low_liquid_warn_pct||20}" min="0" max="50" style="max-width:120px;"/>
        <div class="form-hint">Warning shown when remaining level falls below this percentage.</div>
      </div>
      <button class="btn btn-primary btn-sm" onclick="saveSetting('accords',{low_liquid_warn_pct:+document.getElementById('s-low').value})">Save</button>
    </div>

    <!-- Network Settings -->
    <div class="card">
      <div class="card-title">Network Settings</div>
      <p class="text-muted" style="font-size:0.85rem;margin-bottom:14px;">Change the Wi-Fi network this device connects to.</p>
      <div style="display:grid;gap:10px;max-width:360px;">
        <div class="form-group">
          <label class="form-label">SSID (Network Name)</label>
          <div style="display:flex;gap:8px;">
            <select class="form-select" id="s-ssid"><option value="">— Select or enter manually —</option></select>
            <button class="btn btn-ghost btn-sm" onclick="wifiScan()">⟳ Scan</button>
          </div>
          <input class="form-input" type="text" id="s-ssid-manual" placeholder="Or enter manually" style="margin-top:8px;"/>
        </div>
        <div class="form-group">
          <label class="form-label">Password</label>
          <input class="form-input" type="password" id="s-wifi-pass" placeholder="Wi-Fi password"/>
        </div>
        <button class="btn btn-primary btn-sm" onclick="changeWifi()">Apply Network Change</button>
      </div>
    </div>

    <!-- Security Settings -->
    <div class="card">
      <div class="card-title">Security</div>
      <div style="max-width:360px;">
        <div class="form-group">
          <label class="form-label">Current Password</label>
          <input class="form-input" type="password" id="s-cur-pass" placeholder="Current admin password"/>
        </div>
        <div class="form-group">
          <label class="form-label">New Password (min 6 characters)</label>
          <input class="form-input" type="password" id="s-new-pass" placeholder="New admin password"/>
        </div>
        <button class="btn btn-warn btn-sm" onclick="changePassword()">Change Password</button>
      </div>
      <hr class="divider" style="margin:16px 0;"/>
      <button class="btn btn-danger btn-sm" onclick="logout()">Logout</button>
    </div>`;
}

async function saveSetting(section, body) {
  if (!requireAdmin()) return;
  try {
    await api('POST', `/api/settings/${section}`, body);
    toast('Settings saved', 'success');
  } catch(e) { toast(e.message, 'error'); }
}

async function wifiScan() {
  const sel = document.getElementById('s-ssid');
  if (sel) { sel.innerHTML = '<option>Scanning…</option>'; }
  try {
    const d = await api('GET', '/api/wifi/scan');
    if (sel) {
      sel.innerHTML = '<option value="">— Select network —</option>' +
        d.networks.sort((a,b)=>b.rssi-a.rssi).map(n=>`<option value="${n.ssid}">${n.secure?'🔒 ':''} ${n.ssid} (${n.rssi}dBm)</option>`).join('');
    }
  } catch(e) { toast('Scan failed: ' + e.message, 'error'); }
}

async function changeWifi() {
  if (!requireAdmin()) return;
  const ssid = document.getElementById('s-ssid-manual')?.value?.trim() || document.getElementById('s-ssid')?.value;
  const pass = document.getElementById('s-wifi-pass')?.value || '';
  if (!ssid) { toast('SSID is required', 'error'); return; }
  const ok = await confirm('Change Wi-Fi Network?',
    `Connect to "${ssid}"? If the connection fails, the device will fall back to setup AP mode.`, true);
  if (!ok) return;
  try {
    await api('POST', '/api/wifi/connect', { ssid, password: pass });
    toast('Connecting to ' + ssid + '… check Network page for status', 'info', 8000);
  } catch(e) { toast(e.message, 'error'); }
}

async function changePassword() {
  if (!requireAdmin()) return;
  const cur = document.getElementById('s-cur-pass')?.value || '';
  const nw  = document.getElementById('s-new-pass')?.value || '';
  if (nw.length < 6) { toast('Password must be at least 6 characters', 'error'); return; }
  try {
    await api('POST', '/api/settings/security', { current_password: cur, new_password: nw });
    toast('Password changed. Please log in again.', 'success', 6000);
    State.role = 'guest';
    updateAuthUI();
  } catch(e) { toast(e.message, 'error'); }
}

// ==============================================================
// Helpers
// ==============================================================
function formatUptime(s) {
  if (!s && s !== 0) return '—';
  const h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), sc = s % 60;
  if (h > 0) return `${h}h ${m}m`;
  if (m > 0) return `${m}m ${sc}s`;
  return `${sc}s`;
}

function rssiLabel(r) {
  if (!r) return '';
  if (r >= -50) return 'Excellent';
  if (r >= -65) return 'Good';
  if (r >= -75) return 'Fair';
  return 'Weak';
}

function stateClass(state) {
  if (!state) return 'idle';
  if (state.includes('dispens')) return 'dispensing';
  if (state.includes('purg'))    return 'purging';
  if (state.includes('calib'))   return 'calibrating';
  if (state.includes('error'))   return 'error';
  if (state.includes('pending') || state.includes('confirm')) return 'pending';
  return 'idle';
}

function dot(state) {
  if (!state) return '●';
  if (state.includes('dispens')) return '●';
  if (state.includes('purg'))    return '●';
  if (state.includes('error'))   return '●';
  return '●';
}

// ==============================================================
// Startup
// ==============================================================
async function init() {
  await checkAuth();
  await pollStatus();
  navigate('dashboard');

  // Start polling every 1.5s
  State.pollingTimer = setInterval(pollStatus, 1500);
}

window.addEventListener('DOMContentLoaded', init);
