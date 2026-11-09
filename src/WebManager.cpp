#include "WebManager.h"

#include <cstring>

namespace {
constexpr float kAdsFullScaleV = 4.096f;
constexpr float kAdsMaxCode = 32767.0f;
constexpr float kDividerTopOhm = 4700.0f;
constexpr float kDividerBottomOhm = 10000.0f;
constexpr float kDividerInputOverAdc = (kDividerTopOhm + kDividerBottomOhm) / kDividerBottomOhm;

void sendEthHeaders(EthernetClient& client,
                    const char* status,
                    const char* contentType,
                    size_t contentLength) {
  client.print("HTTP/1.1 ");
  client.print(status);
  client.print("\r\n");
  client.print("Content-Type: ");
  client.print(contentType);
  client.print("\r\n");
  client.print("Connection: close\r\n");
  client.print("Cache-Control: no-store\r\n");
  client.print("Content-Length: ");
  client.print(static_cast<unsigned long>(contentLength));
  client.print("\r\n\r\n");
}

void sendEthBody(EthernetClient& client, const uint8_t* data, size_t len) {
  size_t sent = 0;
  while (sent < len) {
    const size_t chunk = min(static_cast<size_t>(512), len - sent);
    const size_t wrote = client.write(data + sent, chunk);
    if (wrote == 0) {
      delay(1);
      continue;
    }
    sent += wrote;
  }
}

void sendEthStringResponse(EthernetClient& client,
                           const char* status,
                           const char* contentType,
                           const String& body) {
  sendEthHeaders(client, status, contentType, body.length());
  sendEthBody(client, reinterpret_cast<const uint8_t*>(body.c_str()), body.length());
}

void sendEthCStrResponse(EthernetClient& client,
                         const char* status,
                         const char* contentType,
                         const char* body) {
  const size_t len = strlen(body);
  sendEthHeaders(client, status, contentType, len);
  sendEthBody(client, reinterpret_cast<const uint8_t*>(body), len);
}

const char kIndexHtml[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>FM SNMP Gateway</title>
<style>
:root{--bg:#0f1419;--card:#1a2330;--line:#2a3648;--tx:#e8eef6;--mut:#8aa0b8;--on:#3dd68c;--off:#ef6b6b;--acc:#4c8dff;--btn:#243044}
*{box-sizing:border-box}
body{margin:0;font-family:Inter,Segoe UI,system-ui,sans-serif;background:var(--bg);color:var(--tx)}
header{display:flex;flex-wrap:wrap;gap:.8rem;align-items:center;justify-content:space-between;padding:1rem 1.2rem;border-bottom:1px solid var(--line);background:#121922}
h1{font-size:1.05rem;margin:0;font-weight:650}
.badge{font-size:.75rem;padding:.2rem .55rem;border-radius:999px;background:#223049;color:var(--mut)}
.badge.ok{background:#163528;color:var(--on)}
.badge.bad{background:#3a1c1c;color:var(--off)}
nav{display:flex;gap:.4rem;flex-wrap:wrap}
nav button{background:var(--btn);color:var(--tx);border:1px solid var(--line);border-radius:8px;padding:.45rem .8rem;cursor:pointer}
nav button.active{background:var(--acc);border-color:var(--acc);color:#fff}
main{padding:1rem;max-width:1100px;margin:0 auto}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(180px,1fr));gap:.8rem}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:.9rem;margin-bottom:.8rem}
.label{font-size:.78rem;color:var(--mut)}
.metric{font-size:1.35rem;font-weight:700;margin-top:.2rem}
.vu{height:16px;background:#0b1220;border-radius:9px;overflow:hidden;border:1px solid var(--line);margin-top:.4rem}
.vu-bar{height:100%;width:0;background:#1f8a55}
.pill{min-width:3.2rem;text-align:center;padding:.15rem .4rem;border-radius:6px;font-size:.75rem;font-weight:700;display:inline-block}
.pill.on{background:#163528;color:var(--on)}
.pill.off{background:#3a1c1c;color:var(--off)}
button.save{border:0;border-radius:8px;padding:.45rem .7rem;font-weight:650;cursor:pointer;color:#fff;background:var(--acc);margin-right:.4rem;margin-top:.4rem}
label{display:block;font-size:.8rem;color:var(--mut);margin:.55rem 0 .2rem}
input,select,textarea{width:100%;background:#0f1722;color:var(--tx);border:1px solid var(--line);border-radius:8px;padding:.5rem}
textarea{min-height:70px}
.form{display:grid;grid-template-columns:1fr 1fr;gap:.4rem 1rem}
.span2{grid-column:1/-1}
.msg{margin:.6rem 0;color:var(--on);min-height:1.1rem}
.hint{color:var(--mut);font-size:.82rem;line-height:1.4}
@media(max-width:720px){.form{grid-template-columns:1fr}header{padding:.8rem}}
</style>
</head>
<body>
<header>
  <div>
    <h1>FM SNMP Gateway</h1>
    <div class="hint" id="netline">Ethernet …</div>
  </div>
  <div>
    <span class="badge" id="ethBadge">ETH</span>
    <span class="badge" id="alarmBadge">ALARM</span>
    <span class="badge" id="statusBadge">STATUS</span>
  </div>
  <nav>
    <button class="active" data-tab="telemetry">Telemetry</button>
    <button data-tab="network">Network</button>
    <button data-tab="fan">Fan</button>
    <button data-tab="calibration">Calibration</button>
  </nav>
</header>
<main>
<section id="telemetry">
  <div class="grid">
    <div class="card"><div class="label">Forward</div><div class="metric" id="fwd">0</div></div>
    <div class="card"><div class="label">Reflect</div><div class="metric" id="ref">0</div></div>
    <div class="card"><div class="label">Temperature</div><div class="metric" id="temp">0</div></div>
    <div class="card"><div class="label">Fan RPM</div><div class="metric" id="fan">0</div></div>
    <div class="card"><div class="label">Fan PWM</div><div class="metric" id="fanDuty">0%</div></div>
    <div class="card"><div class="label">Fan Status</div><div class="metric" id="fanStatus">N/A</div></div>
    <div class="card"><div class="label">Uptime</div><div class="metric" id="uptime">0 s</div></div>
    <div class="card"><div class="label">Free Heap</div><div class="metric" id="heapKb">0 kB</div></div>
  </div>
  <div class="card">
    <div class="label">Deviation</div>
    <div class="metric" id="dev">0</div>
    <div class="vu"><div class="vu-bar" id="devBar"></div></div>
  </div>
</section>
<section id="network" hidden>
  <form class="card form" id="networkForm">
    <label class="span2">Addressing
      <select name="dhcp" id="dhcp">
        <option value="1">DHCP</option>
        <option value="0">Static IP</option>
      </select>
    </label>
    <label>IP address<input name="ip" id="ip" placeholder="192.168.1.50"></label>
    <label>Gateway<input name="gateway" id="gateway" placeholder="192.168.1.1"></label>
    <label>Subnet<input name="subnet" id="subnet" placeholder="255.255.255.0"></label>
    <label>DNS<input name="dns" id="dns" placeholder="8.8.8.8"></label>
    <label class="span2">Hostname<input name="hostname" id="hostname"></label>
    <label>SNMP trap IP<input name="trapIp" id="trapIp" placeholder="192.168.1.100"></label>
    <label>SNMP trap port<input name="trapPort" id="trapPort" type="number" min="1" max="65535"></label>
    <label class="span2">FM frequency (kHz)<input name="freqKHz" id="freqKHz" type="number" min="0" max="300000"></label>
    <div class="span2">
      <button class="save" type="submit">Save network</button>
      <button class="save" type="button" id="rebootBtn">Save and reboot</button>
    </div>
    <div class="msg span2" id="networkMsg"></div>
    <p class="hint span2">Hold the network-reset button for 5 s to restore DHCP and 192.168.1.50. Calibration, fan and SNMP trap settings are kept.</p>
  </form>
</section>
<section id="fan" hidden>
  <form class="card form" id="fanForm">
    <label>Fan PWM
      <select name="fanPwmEnabled" id="fanPwmEnabled">
        <option value="1">Enabled</option>
        <option value="0">Disabled</option>
      </select>
    </label>
    <label>Startup max (s)<input name="fanStartupMaxSec" id="fanStartupMaxSec" type="number" min="0" max="120"></label>
    <label>Temp start (°C)<input name="fanTempStartC" id="fanTempStartC" type="number" step="0.1"></label>
    <label>Temp max (°C)<input name="fanTempMaxC" id="fanTempMaxC" type="number" step="0.1"></label>
    <label>Min duty (%)<input name="fanMinDutyPercent" id="fanMinDutyPercent" type="number" min="0" max="100"></label>
    <label>Max duty (%)<input name="fanMaxDutyPercent" id="fanMaxDutyPercent" type="number" min="0" max="100"></label>
    <div class="span2"><button class="save" type="submit">Save fan</button></div>
    <div class="msg span2" id="fanMsg"></div>
  </form>
</section>
<section id="calibration" hidden>
  <p class="hint">Mode is Linear, Quadratic or LUT. LUT uses input volts: volt:value;volt:value (example 0.375:0;0.458:25). Range 0..6 V.</p>
  <form id="calibrationForm">
    <div id="calWrap"></div>
    <button class="save" type="submit">Save calibration</button>
    <div class="msg" id="calMsg"></div>
  </form>
</section>
</main>
<script>
const $ = (id) => document.getElementById(id);
const chNames = ['Forward','Reflect','Temperature','Deviation'];
const POLL_VISIBLE_MS = 400, POLL_HIDDEN_MS = 3000;
let pollTimer = 0, polling = false, configLoaded = false;

document.querySelectorAll('nav button').forEach((b) => b.onclick = () => {
  document.querySelectorAll('nav button').forEach((x) => x.classList.remove('active'));
  b.classList.add('active');
  ['telemetry','network','fan','calibration'].forEach((id) => $(id).hidden = id !== b.dataset.tab);
});

$('calWrap').innerHTML = chNames.map((name, i) => `
  <div class="card">
    <div class="ch" style="font-weight:650;margin-bottom:.4rem">Channel ${i} (${name})</div>
    <div class="form">
      <label>Mode
        <select name="c${i}Mode" id="c${i}Mode">
          <option value="0">Linear</option>
          <option value="1">Quadratic</option>
          <option value="2">LUT</option>
        </select>
      </label>
      <label>Linear K<input name="c${i}K" id="c${i}K" type="number" step="0.0001"></label>
      <label>Linear offset<input name="c${i}Offset" id="c${i}Offset" type="number" step="0.0001"></label>
      <label>Quadratic A<input name="c${i}A" id="c${i}A" type="number" step="0.000001"></label>
      <label>Quadratic B<input name="c${i}B" id="c${i}B" type="number" step="0.0001"></label>
      <label>Quadratic C<input name="c${i}C" id="c${i}C" type="number" step="0.0001"></label>
      <label class="span2">LUT<textarea name="c${i}Lut" id="c${i}Lut"></textarea></label>
      <div class="hint span2" id="c${i}LutErr" style="color:var(--off)"></div>
    </div>
  </div>`).join('');

function setBadge(el, ok, onText, offText) {
  el.textContent = ok ? onText : offText;
  el.className = 'badge ' + (ok ? 'ok' : 'bad');
}

function applyTelemetry(d) {
  if (!d) return;
  $('fwd').textContent = Number(d.forward ?? 0).toFixed(2);
  $('ref').textContent = Number(d.reflect ?? 0).toFixed(2);
  $('temp').textContent = Number(d.temperature ?? 0).toFixed(2);
  $('fan').textContent = String(d.fanRpm ?? 0);
  $('fanDuty').textContent = `${d.fanDutyPercent ?? 0}%`;
  $('fanStatus').textContent = ['FAN_CFG_OK','FAN_CFG_CLAMPED','FAN_TACH_MISSING'][d.fanHealthStatus] || 'FAN_STATUS_UNKNOWN';
  $('uptime').textContent = `${d.uptimeSec ?? 0} s`;
  $('heapKb').textContent = `${Math.round(Number(d.freeHeapBytes ?? 0) / 1024)} kB`;
  const dev = Number(d.deviation ?? 0);
  $('dev').textContent = dev.toFixed(2);
  const p = Math.max(0, Math.min(100, dev));
  const bar = $('devBar');
  bar.style.width = `${p}%`;
  bar.style.background = p <= 70 ? '#1f8a55' : (p <= 75 ? '#e4b600' : '#d91e18');
  const ethUp = (d.activeLink || '') === 'ETH';
  setBadge($('ethBadge'), ethUp, 'ETH UP', 'ETH DOWN');
  setBadge($('alarmBadge'), !d.alarmActive, 'ALARM OK', 'ALARM');
  setBadge($('statusBadge'), !d.statusInput, 'STATUS OK', 'STATUS');
  $('netline').textContent = `${ethUp ? 'DHCP/ETH' : 'NO LINK'}  ${d.ip || '0.0.0.0'}`;
}

function applyConfig(c) {
  if (!c) return;
  $('hostname').value = c.hostname || '';
  $('dhcp').value = c.dhcp ? '1' : '0';
  $('ip').value = c.ip || '';
  $('gateway').value = c.gateway || '';
  $('subnet').value = c.subnet || '';
  $('dns').value = c.dns || '';
  $('trapIp').value = c.trapIp || '';
  $('trapPort').value = String(c.trapPort ?? 162);
  $('freqKHz').value = String(c.freqKHz ?? 0);
  $('fanPwmEnabled').value = c.fanPwmEnabled ? '1' : '0';
  $('fanTempStartC').value = String(c.fanTempStartC ?? 30.0);
  $('fanTempMaxC').value = String(c.fanTempMaxC ?? 65.0);
  $('fanMinDutyPercent').value = String(c.fanMinDutyPercent ?? 30);
  $('fanMaxDutyPercent').value = String(c.fanMaxDutyPercent ?? 100);
  $('fanStartupMaxSec').value = String(c.fanStartupMaxSec ?? 15);
  for (let i = 0; i < 4; i++) {
    const ch = (c.calibration && c.calibration[i]) || {};
    $(`c${i}Mode`).value = String(ch.mode ?? 0);
    $(`c${i}K`).value = String(ch.k ?? 1.0);
    $(`c${i}Offset`).value = String(ch.offset ?? 0.0);
    $(`c${i}A`).value = String(ch.a ?? 0.0);
    $(`c${i}B`).value = String(ch.b ?? 1.0);
    $(`c${i}C`).value = String(ch.c ?? 0.0);
    $(`c${i}Lut`).value = ch.lut || '';
  }
}

function schedulePoll(ms) {
  clearTimeout(pollTimer);
  pollTimer = setTimeout(syncState, ms !== undefined ? ms : (document.hidden ? POLL_HIDDEN_MS : POLL_VISIBLE_MS));
}

async function syncState() {
  if (polling) return;
  polling = true;
  try {
    const r = await fetch('/api/state', { cache: 'no-store' });
    if (r.ok) {
      const payload = await r.json();
      applyTelemetry(payload.telemetry || {});
      if (!configLoaded && payload.config) {
        applyConfig(payload.config);
        configLoaded = true;
      }
    }
  } catch (e) {}
  polling = false;
  schedulePoll();
}
document.addEventListener('visibilitychange', () => {
  if (!document.hidden) schedulePoll(0);
  else schedulePoll();
});
syncState();

function validateLut(text) {
  const clean = (text || '').trim();
  if (!clean) return '';
  const parts = clean.split(';');
  for (let i = 0; i < parts.length; i++) {
    const row = parts[i].trim();
    if (!row) continue;
    const fields = row.split(':');
    if (fields.length !== 2) return `LUT row ${i + 1}: use volt:value`;
    const inputV = Number(fields[0]);
    const val = Number(fields[1]);
    if (!Number.isFinite(inputV) || inputV < 0 || inputV > 6.0) return `LUT row ${i + 1}: volt must be 0..6`;
    if (!Number.isFinite(val)) return `LUT row ${i + 1}: value must be a number`;
  }
  return '';
}

async function postForm(formId, messageId) {
  const form = $(formId);
  const params = new URLSearchParams(new FormData(form));
  const resp = await fetch('/api/config/save', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: params
  });
  $(messageId).textContent = await resp.text();
  configLoaded = false;
  await syncState();
}

$('networkForm').addEventListener('submit', async (e) => {
  e.preventDefault();
  await postForm('networkForm', 'networkMsg');
});
$('fanForm').addEventListener('submit', async (e) => {
  e.preventDefault();
  await postForm('fanForm', 'fanMsg');
});
$('calibrationForm').addEventListener('submit', async (e) => {
  e.preventDefault();
  for (let i = 0; i < 4; i++) $(`c${i}LutErr`).textContent = '';
  const params = new URLSearchParams(new FormData(e.target));
  for (let i = 0; i < 4; i++) {
    const lutErr = validateLut($(`c${i}Lut`).value);
    if (lutErr) {
      $(`c${i}LutErr`).textContent = lutErr;
      $('calMsg').textContent = 'Fix LUT errors before saving.';
      return;
    }
  }
  const resp = await fetch('/api/config/save', { method: 'POST', body: params });
  $('calMsg').textContent = resp.ok ? 'Calibration saved.' : (await resp.text() || 'Failed to save calibration.');
  configLoaded = false;
  await syncState();
});
$('rebootBtn').addEventListener('click', async () => {
  await postForm('networkForm', 'networkMsg');
  const resp = await fetch('/api/reboot', { method: 'POST' });
  $('networkMsg').textContent = await resp.text();
});
</script>
</body>
</html>
)HTML";
}  // namespace

bool WebManager::begin(EventLogger* eventLogger, ConfigManager* configManager) {
  eventLogger_ = eventLogger;
  configManager_ = configManager;
  return true;
}

void WebManager::update(uint32_t nowMs, const SnmpTelemetry& telemetry) {
  (void)nowMs;
  latestTelemetry_ = telemetry;
  ensureServerStarted(telemetry);

  if (telemetry.network.activeLink == LinkType::Ethernet) {
    handleEthernetHttp(telemetry);
  }

  if (rebootRequested_) {
    rebootRequested_ = false;
    delay(100);
    rp2040.reboot();
  }
}

void WebManager::ensureServerStarted(const SnmpTelemetry& telemetry) {
  if (telemetry.network.activeLink != LinkType::Ethernet ||
      telemetry.network.ip == IPAddress(0, 0, 0, 0)) {
    return;
  }

  if (!ethHttpStarted_) {
    ethHttpServer_.begin();
    ethHttpStarted_ = true;
    Serial.printf("WEB: ETH HTTP started on %s\n", telemetry.network.ip.toString().c_str());
  }
}

void WebManager::handleEthernetHttp(const SnmpTelemetry& telemetry) {
  if (!ethHttpStarted_) {
    return;
  }

  EthernetClient client = ethHttpServer_.available();
  if (!client) {
    return;
  }

  const uint32_t start = millis();
  while (!client.available() && (millis() - start) < 250) {
    delay(1);
  }

  if (!client.available()) {
    client.stop();
    return;
  }

  const String requestLine = client.readStringUntil('\n');
  int firstSpace = requestLine.indexOf(' ');
  int secondSpace = firstSpace >= 0 ? requestLine.indexOf(' ', firstSpace + 1) : -1;
  String method;
  String path;
  if (firstSpace > 0 && secondSpace > firstSpace) {
    method = requestLine.substring(0, firstSpace);
    path = requestLine.substring(firstSpace + 1, secondSpace);
  }

  int contentLength = 0;
  while (client.available()) {
    String line = client.readStringUntil('\n');
    if (line.startsWith("Content-Length:")) {
      const String lenText = line.substring(strlen("Content-Length:"));
      contentLength = lenText.toInt();
    }
    if (line == "\r" || line.length() == 0) {
      break;
    }
  }

  String body;
  if (contentLength > 0) {
    const uint32_t bodyStart = millis();
    while (static_cast<int>(body.length()) < contentLength && (millis() - bodyStart) < 1000) {
      while (client.available() && static_cast<int>(body.length()) < contentLength) {
        body += static_cast<char>(client.read());
      }
      if (static_cast<int>(body.length()) < contentLength) {
        delay(1);
      }
    }
  }

  if (method == "GET" && (path == "/" || path == "/index.html")) {
    sendEthCStrResponse(client, "200 OK", "text/html; charset=utf-8", kIndexHtml);
  } else if (method == "GET" && (path == "/api/state" || path == "/api/telemetry")) {
    const String payload = buildStateJson(telemetry);
    sendEthStringResponse(client, "200 OK", "application/json", payload);
  } else if (method == "POST" && path == "/api/config/save") {
    const bool ok = applyConfigFromFormBody(body);
    sendEthCStrResponse(client,
                        ok ? "200 OK" : "500 Internal Server Error",
                        "text/plain",
                        ok ? "Saved. Reboot to apply network changes." : "Failed to save configuration.");
  } else if (method == "POST" && path == "/api/reboot") {
    rebootRequested_ = true;
    sendEthCStrResponse(client, "200 OK", "text/plain", "Reboot scheduled.");
  } else {
    sendEthCStrResponse(client, "404 Not Found", "text/plain", "Not Found");
  }

  delay(5);
  client.stop();
}

String WebManager::buildConfigJson() const {
  if (configManager_ == nullptr) {
    return "{\"error\":\"config unavailable\"}";
  }

  const DeviceConfig& cfg = configManager_->getConfig();
  String payload;
  payload.reserve(1200);
  payload += "{\"hostname\":\"" + String(cfg.network.hostname) + "\",";
  payload += "\"dhcp\":" + String(cfg.network.dhcp ? "true" : "false") + ",";
  payload += "\"ip\":\"" + cfg.network.ip.toString() + "\",";
  payload += "\"gateway\":\"" + cfg.network.gateway.toString() + "\",";
  payload += "\"subnet\":\"" + cfg.network.subnet.toString() + "\",";
  payload += "\"dns\":\"" + cfg.network.dns.toString() + "\",";
  payload += "\"trapIp\":\"" + cfg.snmp.trapServer.toString() + "\",";
  payload += "\"trapPort\":" + String(cfg.snmp.trapPort) + ",";
  payload += "\"freqKHz\":" + String(cfg.snmp.freqKHz) + ",";
  payload += "\"fanPwmEnabled\":" + String(cfg.fanPwmEnabled ? "true" : "false") + ",";
  payload += "\"fanTempStartC\":" + String(cfg.fanTempStartC, 2) + ",";
  payload += "\"fanTempMaxC\":" + String(cfg.fanTempMaxC, 2) + ",";
  payload += "\"fanMinDutyPercent\":" + String(cfg.fanMinDutyPercent) + ",";
  payload += "\"fanMaxDutyPercent\":" + String(cfg.fanMaxDutyPercent) + ",";
  payload += "\"fanStartupMaxSec\":" + String(cfg.fanStartupMaxSec) + ",";
  payload += "\"calibration\":[";
  for (uint8_t i = 0; i < 4; ++i) {
    if (i > 0) {
      payload += ",";
    }
    const CalibrationProfile& c = cfg.calibration[i];
    String lut;
    for (size_t j = 0; j < c.lut.size(); ++j) {
      if (j > 0) {
        lut += ";";
      }
      lut += String(c.lut[j].inputVoltage, 4);
      lut += ":";
      lut += String(c.lut[j].value, 4);
    }

    payload += "{\"mode\":" + String(static_cast<int>(c.mode));
    payload += ",\"k\":" + String(c.k, 6);
    payload += ",\"offset\":" + String(c.offset, 6);
    payload += ",\"a\":" + String(c.a, 6);
    payload += ",\"b\":" + String(c.b, 6);
    payload += ",\"c\":" + String(c.c, 6);
    payload += ",\"lut\":\"" + lut + "\"}";
  }
  payload += "]}";
  return payload;
}

String WebManager::buildStateJson(const SnmpTelemetry& telemetry) const {
  const char* link = telemetry.network.activeLink == LinkType::Ethernet ? "ETH" : "NONE";

  char telemetryJson[640];
  snprintf(telemetryJson,
           sizeof(telemetryJson),
           "{\"forward\":%.2f,\"reflect\":%.2f,\"temperature\":%.2f,\"freeHeapBytes\":%lu,\"deviation\":%.2f,\"fanRpm\":%u,\"fanDutyPercent\":%u,\"fanHealthStatus\":%u,\"alarmActive\":%s,\"statusInput\":%s,\"uptimeSec\":%lu,\"activeLink\":\"%s\",\"ip\":\"%s\"}",
           telemetry.analog.forward,
           telemetry.analog.reflect,
           telemetry.analog.temperature,
           static_cast<unsigned long>(telemetry.freeHeapBytes),
           telemetry.analog.deviation,
           static_cast<unsigned>(telemetry.fanRpm),
           static_cast<unsigned>(telemetry.fanDutyPercent),
           static_cast<unsigned>(telemetry.fanHealthStatus),
           telemetry.alarm.alarmActive ? "true" : "false",
           telemetry.alarm.statusInput ? "true" : "false",
           static_cast<unsigned long>(telemetry.uptimeSec),
           link,
           telemetry.network.ip.toString().c_str());

  String payload;
  payload.reserve(1800);
  payload += "{\"telemetry\":";
  payload += telemetryJson;
  payload += ",\"config\":";
  payload += buildConfigJson();
  payload += "}";
  return payload;
}

bool WebManager::applyConfigFromFormBody(const String& body) {
  if (configManager_ == nullptr) {
    return false;
  }

  DeviceConfig& cfg = configManager_->mutableConfig();
  String value;

  if (getFormArg(body, "hostname", value)) {
    value.toCharArray(cfg.network.hostname, sizeof(cfg.network.hostname));
  }
  if (getFormArg(body, "dhcp", value)) {
    cfg.network.dhcp = parseBoolArg(value);
  }

  IPAddress ip;
  if (getFormArg(body, "ip", value) && parseIpValue(value, ip)) {
    cfg.network.ip = ip;
  }
  if (getFormArg(body, "gateway", value) && parseIpValue(value, ip)) {
    cfg.network.gateway = ip;
  }
  if (getFormArg(body, "subnet", value) && parseIpValue(value, ip)) {
    cfg.network.subnet = ip;
  }
  if (getFormArg(body, "dns", value) && parseIpValue(value, ip)) {
    cfg.network.dns = ip;
  }
  if (getFormArg(body, "trapIp", value) && parseIpValue(value, ip)) {
    cfg.snmp.trapServer = ip;
  }
  if (getFormArg(body, "trapPort", value)) {
    const int trapPort = value.toInt();
    if (trapPort >= 1 && trapPort <= 65535) {
      cfg.snmp.trapPort = static_cast<uint16_t>(trapPort);
    }
  }
  if (getFormArg(body, "freqKHz", value)) {
    const long freqKHz = value.toInt();
    if (freqKHz >= 0) {
      cfg.snmp.freqKHz = static_cast<uint32_t>(freqKHz);
    }
  }

  if (getFormArg(body, "fanPwmEnabled", value)) {
    cfg.fanPwmEnabled = parseBoolArg(value);
  }
  if (getFormArg(body, "fanTempStartC", value)) {
    cfg.fanTempStartC = value.toFloat();
  }
  if (getFormArg(body, "fanTempMaxC", value)) {
    cfg.fanTempMaxC = value.toFloat();
  }
  if (getFormArg(body, "fanMinDutyPercent", value)) {
    cfg.fanMinDutyPercent = static_cast<uint8_t>(value.toInt());
  }
  if (getFormArg(body, "fanMaxDutyPercent", value)) {
    cfg.fanMaxDutyPercent = static_cast<uint8_t>(value.toInt());
  }
  if (getFormArg(body, "fanStartupMaxSec", value)) {
    cfg.fanStartupMaxSec = static_cast<uint16_t>(value.toInt());
  }

  for (uint8_t i = 0; i < 4; ++i) {
    CalibrationProfile& c = cfg.calibration[i];
    const String prefix = "c" + String(i);

    if (getFormArg(body, prefix + "Mode", value)) {
      const int mode = value.toInt();
      if (mode >= 0 && mode <= 2) {
        c.mode = static_cast<CalibrationMode>(mode);
      }
    }
    if (getFormArg(body, prefix + "K", value)) {
      c.k = value.toFloat();
    }
    if (getFormArg(body, prefix + "Offset", value)) {
      c.offset = value.toFloat();
    }
    if (getFormArg(body, prefix + "A", value)) {
      c.a = value.toFloat();
    }
    if (getFormArg(body, prefix + "B", value)) {
      c.b = value.toFloat();
    }
    if (getFormArg(body, prefix + "C", value)) {
      c.c = value.toFloat();
    }
    if (getFormArg(body, prefix + "Lut", value)) {
      parseLutText(value, c);
    }
  }

  return configManager_->save();
}

bool WebManager::getFormArg(const String& body, const String& key, String& out) const {
  const String token = key + "=";
  int start = body.indexOf(token);
  if (start < 0) {
    return false;
  }
  if (start > 0 && body.charAt(start - 1) != '&') {
    start = body.indexOf("&" + token);
    if (start < 0) {
      return false;
    }
    start += 1;
  }

  start += token.length();
  int end = body.indexOf('&', start);
  if (end < 0) {
    end = body.length();
  }

  out = decodeUrlFormComponent(body.substring(start, end));
  return true;
}

String WebManager::decodeUrlFormComponent(const String& text) const {
  String decoded;
  decoded.reserve(text.length());

  for (size_t i = 0; i < text.length(); ++i) {
    const char c = text.charAt(i);
    if (c == '+') {
      decoded += ' ';
      continue;
    }

    if (c == '%' && (i + 2) < text.length()) {
      char hex[3] = {text.charAt(i + 1), text.charAt(i + 2), '\0'};
      decoded += static_cast<char>(strtol(hex, nullptr, 16));
      i += 2;
      continue;
    }

    decoded += c;
  }

  return decoded;
}

bool WebManager::parseIpValue(const String& value, IPAddress& out) const {
  return out.fromString(value);
}

bool WebManager::parseBoolArg(const String& value) const {
  String lower = value;
  lower.toLowerCase();
  return lower == "1" || lower == "true" || lower == "on" || lower == "yes";
}

void WebManager::parseLutText(const String& text, CalibrationProfile& profile) const {
  profile.lut.clear();

  int start = 0;
  while (start < text.length()) {
    int end = text.indexOf(';', start);
    if (end < 0) {
      end = text.length();
    }

    const String token = text.substring(start, end);
    const int sep = token.indexOf(':');
    if (sep > 0) {
      const float x = token.substring(0, sep).toFloat();
      const float value = token.substring(sep + 1).toFloat();
      float inputVoltage = x;
      if (x > 6.0f) {
        inputVoltage = x * (kAdsFullScaleV / kAdsMaxCode) * kDividerInputOverAdc;
      }
      profile.lut.push_back({inputVoltage, value});
    }

    start = end + 1;
  }
}
