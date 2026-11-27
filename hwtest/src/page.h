#pragma once
// Web UI for the hardware test firmware (served as-is from flash).

static const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Fanboy hardware test</title>
<style>
  :root { color-scheme: light dark; --bg:#f4f4f2; --card:#fff; --fg:#1d1d1b; --muted:#77756f; --line:#e2e0da; --accent:#2a6fdb; --on:#1f9d55; --bad:#c0392b; }
  @media (prefers-color-scheme: dark) { :root { --bg:#151514; --card:#1f1f1d; --fg:#ecebe7; --muted:#9a988f; --line:#33322f; --accent:#6a9cf0; --on:#3cc47a; --bad:#e8675a; } }
  * { box-sizing: border-box; }
  body { margin:0; font:15px/1.4 system-ui, -apple-system, sans-serif; background:var(--bg); color:var(--fg); }
  main { max-width:560px; margin:0 auto; padding:20px 16px 40px; }
  h1 { font-size:18px; margin:0 0 16px; display:flex; justify-content:space-between; align-items:baseline; }
  h1 small { font-weight:400; font-size:13px; color:var(--muted); }
  .card { background:var(--card); border:1px solid var(--line); border-radius:10px; padding:16px; margin-bottom:12px; }
  .label { font-size:12px; text-transform:uppercase; letter-spacing:.06em; color:var(--muted); }
  .big { font-size:40px; font-weight:600; font-variant-numeric:tabular-nums; line-height:1.1; }
  .unit { font-size:18px; color:var(--muted); font-weight:400; }
  .sub { font-size:13px; color:var(--muted); font-variant-numeric:tabular-nums; margin-top:4px; }
  .row { display:flex; align-items:center; gap:12px; }
  .grow { flex:1; }
  .split { display:grid; grid-template-columns:1fr 1fr; gap:12px; margin-top:14px; }
  button { font:inherit; font-weight:600; border:0; border-radius:8px; padding:10px 18px; min-width:90px; cursor:pointer; background:var(--line); color:var(--fg); }
  button.on { background:var(--on); color:#fff; }
  input[type=range] { width:100%; accent-color:var(--accent); }
  input[type=number] { font:inherit; width:72px; -moz-appearance:textfield; padding:6px 8px; border:1px solid var(--line); border-radius:6px; background:var(--bg); color:var(--fg); }
  .bad { color:var(--bad); }
  .id { font:15px ui-monospace, SFMono-Regular, Menlo, monospace; letter-spacing:.04em; margin-top:6px; user-select:all; }
  .duty { display:flex; align-items:baseline; gap:6px; }
  .duty input { width:110px; font-size:28px; font-weight:600; padding:4px 10px; text-align:right; font-variant-numeric:tabular-nums; }
  .duty span { font-size:20px; color:var(--muted); }
  label.chk { display:flex; align-items:center; gap:8px; font-size:14px; cursor:pointer; }
  label.chk input { width:18px; height:18px; accent-color:var(--accent); }
  #err { display:none; color:var(--bad); font-size:13px; margin-bottom:12px; }
  table { width:100%; font-size:13px; border-collapse:collapse; }
  td { padding:3px 0; } td:last-child { text-align:right; font-variant-numeric:tabular-nums; }
</style>
</head>
<body>
<main>
  <h1>Fanboy hardware test <small id="uptime"></small></h1>
  <div id="err">Lost connection to device</div>

  <div class="card">
    <div class="label">Temperature</div>
    <div class="big" id="temp">–</div>
    <div class="sub" id="sensor"></div>
    <div class="id" id="sensorId"></div>
  </div>

  <div class="card">
    <div class="row">
      <div class="grow"><div class="label">Fan power</div><div class="sub">High-side switch</div></div>
      <button id="sw">–</button>
    </div>
    <div class="row" style="margin-top:16px">
      <div class="grow"><div class="label">Fan PWM duty</div><div class="sub" id="pinDuty"></div></div>
      <div class="duty"><input type="number" id="dutyNum" min="0" max="100" step="1"><span>%</span></div>
    </div>
    <input type="range" id="duty" min="0" max="100" step="1">
    <label class="chk" style="margin-top:8px"><input type="checkbox" id="inv"> Invert PWM output (inverting driver stage)</label>
    <div class="split">
      <div><div class="label">Speed</div><div class="big" id="rpm">–</div><div class="sub">RPM</div></div>
      <div><div class="label">Tach</div><div class="big" id="hz" style="font-size:28px">–</div><div class="sub" id="pulses"></div></div>
    </div>
  </div>

  <div class="card">
    <div class="row">
      <div class="label grow">Pulses per revolution</div>
      <input type="number" id="ppr" min="1" max="8" step="1">
    </div>
  </div>

  <div class="card">
    <table>
      <tr><td>Network</td><td id="net"></td></tr>
      <tr><td>Pins</td><td id="pins"></td></tr>
    </table>
  </div>
</main>
<script>
const $ = id => document.getElementById(id);
let st = null, dragging = false, sendTimer = null, pendingDuty = null;

async function post(params) {
  const r = await fetch('/api/set', { method: 'POST', body: new URLSearchParams(params) });
  if (!r.ok) throw new Error(await r.text());
  render(await r.json());
}

function render(s) {
  st = s;
  $('err').style.display = 'none';
  if (s.temp === null) {
    $('temp').innerHTML = '<span class="bad">no reading</span>';
  } else {
    $('temp').innerHTML = s.temp.toFixed(2) + '<span class="unit"> °C</span>';
  }
  $('sensor').textContent = !s.sensors ? 'No DS18B20 found'
    : s.sensors > 1 ? `${s.sensors} sensors found, using the lowest ID` : 'Sensor ID';
  $('sensorId').textContent = s.sensorId;
  $('sw').textContent = s.on ? 'ON' : 'OFF';
  $('sw').className = s.on ? 'on' : '';
  if (!dragging && document.activeElement !== $('dutyNum') && pendingDuty === null) {
    $('duty').value = s.duty;
    $('dutyNum').value = Math.round(s.duty);
  }
  $('pinDuty').textContent = `GPIO duty ${s.pinDuty.toFixed(0)} %` + (s.inverted ? ' (inverted)' : '');
  $('inv').checked = s.inverted;
  $('rpm').textContent = Math.round(s.rpm);
  $('hz').textContent = s.tachHz.toFixed(1) + ' Hz';
  $('pulses').textContent = `${s.pulses} pulses total`;
  if (document.activeElement !== $('ppr')) $('ppr').value = s.ppr;
  $('net').textContent = `${s.ssid} · ${s.ip} (${s.staticIp ? 'static' : 'DHCP'}) · ${s.rssi} dBm`;
  $('pins').textContent = `1-wire ${s.pins.ds18b20} · PWM ${s.pins.pwm} · tach ${s.pins.tach} · switch ${s.pins.switch}`;
  const u = s.uptime;
  $('uptime').textContent = `up ${Math.floor(u / 3600)}h ${Math.floor(u / 60) % 60}m ${u % 60}s`;
}

// Send duty at most every 150 ms while dragging; always send the final value.
function setDuty(v) {
  pendingDuty = v;
  if (sendTimer) return;
  sendTimer = setTimeout(async function flush() {
    const d = pendingDuty;
    try { await post({ duty: d }); } catch (e) {}
    if (pendingDuty !== d) { sendTimer = setTimeout(flush, 150); return; }
    pendingDuty = null; sendTimer = null;
  }, 150);
}

$('duty').addEventListener('pointerdown', () => dragging = true);
window.addEventListener('pointerup', () => dragging = false);
$('duty').addEventListener('input', e => { $('dutyNum').value = e.target.value; setDuty(e.target.value); });
// Text box: applies 400 ms after you stop typing, or at once on Enter / leaving the box.
let typeTimer = null;
function dutyFromBox(clamp) {
  clearTimeout(typeTimer);
  const raw = $('dutyNum').value;
  if (raw === '' || isNaN(Number(raw))) return;
  const v = Math.max(0, Math.min(100, Number(raw)));
  if (clamp) $('dutyNum').value = v;
  $('duty').value = v; setDuty(v);
}
$('dutyNum').addEventListener('input', () => { clearTimeout(typeTimer); typeTimer = setTimeout(() => dutyFromBox(false), 400); });
$('dutyNum').addEventListener('change', () => dutyFromBox(true));
$('dutyNum').addEventListener('keydown', e => { if (e.key === 'Enter') e.target.blur(); });
$('inv').addEventListener('change', e => post({ inv: e.target.checked ? 1 : 0 }).catch(() => {}));
$('sw').addEventListener('click', () => st && post({ on: st.on ? 0 : 1 }).catch(() => {}));
$('ppr').addEventListener('change', e => post({ ppr: e.target.value }).catch(err => alert(err.message)));

async function poll() {
  try {
    const r = await fetch('/api/status');
    render(await r.json());
  } catch (e) {
    $('err').style.display = 'block';
  }
  setTimeout(poll, 1000);
}
poll();
</script>
</body>
</html>
)HTML";
