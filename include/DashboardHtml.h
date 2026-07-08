#pragma once

// Self-contained Web UI page (#70) — polls GET /api/status every 2s and
// renders the same fields the CYD's Main screen shows, plus two small
// canvas sparklines from the same history arrays. No external assets (no
// filesystem to serve them from — SPIFFS was removed by the OTA partition
// table), so CSS/JS are inlined here. Kept out of main.cpp for readability;
// PROGMEM keeps this ~3KB string out of RAM.
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>AxePilot</title>
<style>
  body { font-family: system-ui, sans-serif; background: #111; color: #eee; margin: 0; padding: 16px; }
  h1 { font-size: 1.2rem; margin: 0 0 12px; }
  .row { display: flex; justify-content: space-between; padding: 6px 0; border-bottom: 1px solid #333; }
  .label { color: #999; }
  .value { font-weight: bold; }
  .hot { color: #f55; }
  .cold { color: #5af; }
  .ok { color: #5f5; }
  .warn { color: #fa5; }
  canvas { width: 100%; height: 60px; background: #000; margin-top: 8px; border-radius: 4px; }
  .charts { display: flex; gap: 12px; margin-top: 12px; }
  .chart-box { flex: 1; }
  .chart-title { font-size: 0.8rem; color: #999; }
  #err { color: #f55; display: none; margin-top: 12px; }
</style>
</head>
<body>
<h1>AxePilot</h1>
<div class="row"><span class="label">Temperature</span><span class="value" id="temperature">-</span></div>
<div class="row"><span class="label">Hashrate</span><span class="value" id="hashrate">-</span></div>
<div class="row"><span class="label">Voltage / Frequency</span><span class="value" id="voltFreq">-</span></div>
<div class="row"><span class="label">Power / Fan</span><span class="value" id="powerFan">-</span></div>
<div class="row"><span class="label">Mode</span><span class="value" id="mode">-</span></div>
<div class="row"><span class="label">WiFi</span><span class="value" id="wifi">-</span></div>
<div class="charts">
  <div class="chart-box"><div class="chart-title">Temperature</div><canvas id="tempChart"></canvas></div>
  <div class="chart-box"><div class="chart-title">Hashrate</div><canvas id="hashChart"></canvas></div>
</div>
<div id="err">Lost connection to device...</div>
<script>
function drawSparkline(canvas, values, color) {
  const ctx = canvas.getContext('2d');
  const w = canvas.width = canvas.clientWidth;
  const h = canvas.height = canvas.clientHeight;
  ctx.clearRect(0, 0, w, h);
  if (values.length < 2) return;
  let min = Math.min(...values), max = Math.max(...values);
  const range = max - min || 1;
  ctx.strokeStyle = color;
  ctx.lineWidth = 2;
  ctx.beginPath();
  values.forEach((v, i) => {
    const x = i * (w - 1) / (values.length - 1);
    const y = (h - 1) - ((v - min) / range) * (h - 1);
    i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
  });
  ctx.stroke();
}

async function refresh() {
  try {
    const res = await fetch('/api/status');
    if (!res.ok) throw new Error('bad status');
    const s = await res.json();
    document.getElementById('err').style.display = 'none';

    const tempEl = document.getElementById('temperature');
    tempEl.textContent = s.temperature.toFixed(1) + ' C';
    tempEl.className = 'value ' + (s.isOverheating ? 'hot' : 'ok');

    document.getElementById('hashrate').textContent =
      (s.hashrate > 9999 ? (s.hashrate / 1000).toFixed(1) + ' TH/s' : s.hashrate.toFixed(1) + ' GH/s');
    document.getElementById('voltFreq').textContent = s.coreVoltage + ' mV / ' + s.frequency + ' MHz';
    document.getElementById('powerFan').textContent = s.power.toFixed(1) + ' W / ' +
      (s.fanSpeedPercent > 0 ? s.fanSpeedPercent + '%' : s.fanRpm + ' rpm');

    const modeEl = document.getElementById('mode');
    modeEl.textContent = s.mode;
    modeEl.className = 'value ' + (s.mode === 'AUTO' ? 'ok' : 'warn');

    const wifiEl = document.getElementById('wifi');
    wifiEl.textContent = s.wifiOk ? 'connected' : 'reconnecting...';
    wifiEl.className = 'value ' + (s.wifiOk ? 'ok' : 'hot');

    drawSparkline(document.getElementById('tempChart'), s.tempHistory, '#5f5');
    drawSparkline(document.getElementById('hashChart'), s.hashHistory, '#5af');
  } catch (e) {
    document.getElementById('err').style.display = 'block';
  }
}
refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawliteral";
