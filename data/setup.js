'use strict';

async function scanNetworks() {
  const btn = document.getElementById('scan-btn');
  const sel = document.getElementById('ssid-select');
  btn.textContent = '⟳';
  btn.disabled = true;
  sel.innerHTML = '<option value="">— Scanning… —</option>';

  try {
    const res = await fetch('/api/wifi/scan');
    const data = await res.json();
    sel.innerHTML = '';
    if (data.networks && data.networks.length > 0) {
      data.networks
        .sort((a, b) => b.rssi - a.rssi)
        .forEach(n => {
          const opt = document.createElement('option');
          opt.value = n.ssid;
          const lock = n.secure ? '🔒 ' : '  ';
          const bars = rssiToBars(n.rssi);
          opt.textContent = `${lock}${n.ssid}  ${bars} (${n.rssi} dBm)`;
          sel.appendChild(opt);
        });
    } else {
      sel.innerHTML = '<option value="">— No networks found —</option>';
    }
  } catch (e) {
    sel.innerHTML = '<option value="">— Scan failed —</option>';
    showStatus('Scan failed: ' + e.message, 'error');
  } finally {
    btn.disabled = false;
  }
}

function rssiToBars(rssi) {
  if (rssi >= -50) return '▂▄▆█';
  if (rssi >= -65) return '▂▄▆_';
  if (rssi >= -75) return '▂▄__';
  return '▂___';
}

function togglePass() {
  const p = document.getElementById('wifi-pass');
  p.type = p.type === 'password' ? 'text' : 'password';
}

async function connect() {
  const sel = document.getElementById('ssid-select');
  const manual = document.getElementById('ssid-manual').value.trim();
  const ssid = manual || sel.value;
  const password = document.getElementById('wifi-pass').value;

  if (!ssid) {
    showStatus('Please select or enter a network name.', 'error');
    return;
  }

  const btn = document.getElementById('connect-btn');
  btn.disabled = true;
  btn.textContent = 'Connecting…';

  try {
    const res = await fetch('/api/wifi/connect', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ ssid, password })
    });
    const data = await res.json();

    if (data.status === 'ok') {
      document.getElementById('setup-card').classList.add('hidden');
      document.getElementById('connecting-card').classList.remove('hidden');
      document.getElementById('connecting-ssid').textContent =
        `Connecting to: ${ssid}`;
    } else {
      showStatus(data.message || 'Connection failed.', 'error');
      btn.disabled = false;
      btn.textContent = 'Connect';
    }
  } catch (e) {
    showStatus('Request failed: ' + e.message, 'error');
    btn.disabled = false;
    btn.textContent = 'Connect';
  }
}

function showStatus(msg, type) {
  const el = document.getElementById('status-msg');
  el.textContent = msg;
  el.className = `status-msg ${type}`;
}

// Auto-scan on load
window.addEventListener('DOMContentLoaded', () => {
  scanNetworks();
});
