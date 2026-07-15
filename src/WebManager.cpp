#include "WebManager.h"

namespace {
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
<!doctype html>
<html>
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <title>FM SNMP Gateway</title>
  <style>
    body{font-family:Arial,sans-serif;background:#f4f7fb;color:#111;margin:0;padding:16px}
    .card{background:#fff;border:1px solid #dfe5ef;border-radius:12px;padding:14px;margin-bottom:14px}
    h1{margin:0 0 12px;font-size:22px}
    .row{display:grid;grid-template-columns:1fr 1fr;gap:8px}
    .metric{font-size:18px;font-weight:700}
    .label{font-size:12px;color:#666}
    .vu{height:18px;background:#e5ebf5;border-radius:9px;overflow:hidden}
    .vu-bar{height:100%;width:0;background:linear-gradient(90deg,#1d8f4b,#e4b600,#d91e18)}
    label{display:block;font-size:13px;margin-top:8px}
    input{width:100%;padding:8px;border:1px solid #c8d2e1;border-radius:8px}
    textarea{width:100%;padding:8px;border:1px solid #c8d2e1;border-radius:8px;min-height:70px}
    button{margin-top:10px;padding:10px 14px;border:0;border-radius:8px;background:#1847a8;color:#fff}
    .muted{font-size:12px;color:#666}
    .err{color:#b00020}
    .ch{border:1px dashed #cbd5e6;padding:10px;border-radius:10px;margin-top:10px}
  </style>
</head>
<body>
  <div class="card">
    <h1>FM SNMP Gateway</h1>
    <div class="row">
      <div><div class="label">Forward</div><div class="metric" id="fwd">0</div></div>
      <div><div class="label">Reflect</div><div class="metric" id="ref">0</div></div>
      <div><div class="label">Temperature</div><div class="metric" id="temp">0</div></div>
      <div><div class="label">Fan RPM</div><div class="metric" id="fan">0</div></div>
      <div><div class="label">Fan PWM</div><div class="metric" id="fanDuty">0%</div></div>
      <div><div class="label">Fan Status</div><div class="metric" id="fanStatus">N/A</div></div>
    </div>
    <div class="label" style="margin-top:12px">Deviation</div>
    <div class="metric" id="dev">0</div>
    <div class="vu"><div class="vu-bar" id="devBar"></div></div>
    <div class="muted" id="net">Link: N/A</div>
  </div>

  <div class="card">
    <h2>Network Settings</h2>
    <form id="networkForm">
      <label>WiFi SSID (fixed in firmware)</label><input name="wifiSsid" id="wifiSsid" readonly />
      <label>WiFi Password (fixed in firmware)</label><input name="wifiPassword" id="wifiPassword" type="password" readonly />
      <div class="muted">WiFi credentials are hardcoded and cannot be changed from Web UI.</div>
      <label>Hostname</label><input name="hostname" id="hostname" />
      <label>ETH Enabled (1/0)</label><input name="ethEnabled" id="ethEnabled" type="number" min="0" max="1" step="1" />
      <label>ETH DHCP (1/0)</label><input name="dhcp" id="dhcp" type="number" min="0" max="1" step="1" />
      <label>ETH IP</label><input name="ip" id="ip" placeholder="192.168.1.50" />
      <label>ETH Gateway</label><input name="gateway" id="gateway" placeholder="192.168.1.1" />
      <label>ETH Subnet</label><input name="subnet" id="subnet" placeholder="255.255.255.0" />
      <label>ETH DNS</label><input name="dns" id="dns" placeholder="8.8.8.8" />
      <label>SNMP Trap IP</label><input name="trapIp" id="trapIp" placeholder="192.168.1.100" />
      <label>SNMP Trap Port</label><input name="trapPort" id="trapPort" type="number" min="1" max="65535" step="1" />
      <button type="submit">Save Network</button>
    </form>
    <button id="rebootBtn">Save and Reboot</button>
    <div class="muted" id="networkMsg"></div>
  </div>

  <div class="card">
    <h2>Fan Settings</h2>
    <form id="fanForm">
      <label>Fan PWM Enabled (1/0)</label><input name="fanPwmEnabled" id="fanPwmEnabled" type="number" min="0" max="1" step="1" />
      <label>Fan Temp Start (C)</label><input name="fanTempStartC" id="fanTempStartC" type="number" step="0.1" />
      <label>Fan Temp Max (C)</label><input name="fanTempMaxC" id="fanTempMaxC" type="number" step="0.1" />
      <label>Fan Min Duty (%)</label><input name="fanMinDutyPercent" id="fanMinDutyPercent" type="number" min="0" max="100" step="1" />
      <label>Fan Max Duty (%)</label><input name="fanMaxDutyPercent" id="fanMaxDutyPercent" type="number" min="0" max="100" step="1" />
      <label>Fan Startup Max Seconds</label><input name="fanStartupMaxSec" id="fanStartupMaxSec" type="number" min="0" max="120" step="1" />
      <button type="submit">Save Fan</button>
    </form>
    <div class="muted" id="fanMsg"></div>
  </div>

  <div class="card">
    <h2>ADC Calibration</h2>
    <p class="muted">Mode: 0=Linear, 1=Quadratic, 2=LUT. LUT format: adc:value;adc:value</p>
    <form id="calibrationForm">
    <div class="ch">
      <div class="label">Channel 0 (Forward)</div>
      <label>Mode</label><input name="c0Mode" id="c0Mode" type="number" min="0" max="2" step="1" />
      <label>Linear K</label><input name="c0K" id="c0K" type="number" step="0.0001" />
      <label>Linear Offset</label><input name="c0Offset" id="c0Offset" type="number" step="0.0001" />
      <label>Quadratic A</label><input name="c0A" id="c0A" type="number" step="0.000001" />
      <label>Quadratic B</label><input name="c0B" id="c0B" type="number" step="0.0001" />
      <label>Quadratic C</label><input name="c0C" id="c0C" type="number" step="0.0001" />
      <label>LUT</label><textarea name="c0Lut" id="c0Lut"></textarea>
      <button type="button" id="c0PresetBtn">Load Forward Preset</button>
      <div class="muted err" id="c0LutErr"></div>
    </div>
    <div class="ch">
      <div class="label">Channel 1 (Reflect)</div>
      <label>Mode</label><input name="c1Mode" id="c1Mode" type="number" min="0" max="2" step="1" />
      <label>Linear K</label><input name="c1K" id="c1K" type="number" step="0.0001" />
      <label>Linear Offset</label><input name="c1Offset" id="c1Offset" type="number" step="0.0001" />
      <label>Quadratic A</label><input name="c1A" id="c1A" type="number" step="0.000001" />
      <label>Quadratic B</label><input name="c1B" id="c1B" type="number" step="0.0001" />
      <label>Quadratic C</label><input name="c1C" id="c1C" type="number" step="0.0001" />
      <label>LUT</label><textarea name="c1Lut" id="c1Lut"></textarea>
      <button type="button" id="c1PresetBtn">Load Reflect Preset</button>
      <div class="muted err" id="c1LutErr"></div>
    </div>
    <div class="ch">
      <div class="label">Channel 2 (Temperature)</div>
      <label>Mode</label><input name="c2Mode" id="c2Mode" type="number" min="0" max="2" step="1" />
      <label>Linear K</label><input name="c2K" id="c2K" type="number" step="0.0001" />
      <label>Linear Offset</label><input name="c2Offset" id="c2Offset" type="number" step="0.0001" />
      <label>Quadratic A</label><input name="c2A" id="c2A" type="number" step="0.000001" />
      <label>Quadratic B</label><input name="c2B" id="c2B" type="number" step="0.0001" />
      <label>Quadratic C</label><input name="c2C" id="c2C" type="number" step="0.0001" />
      <label>LUT</label><textarea name="c2Lut" id="c2Lut"></textarea>
      <button type="button" id="c2PresetBtn">Load Temperature Preset</button>
      <div class="muted err" id="c2LutErr"></div>
    </div>
    <div class="ch">
      <div class="label">Channel 3 (Deviation)</div>
      <label>Mode</label><input name="c3Mode" id="c3Mode" type="number" min="0" max="2" step="1" />
      <label>Linear K</label><input name="c3K" id="c3K" type="number" step="0.0001" />
      <label>Linear Offset</label><input name="c3Offset" id="c3Offset" type="number" step="0.0001" />
      <label>Quadratic A</label><input name="c3A" id="c3A" type="number" step="0.000001" />
      <label>Quadratic B</label><input name="c3B" id="c3B" type="number" step="0.0001" />
      <label>Quadratic C</label><input name="c3C" id="c3C" type="number" step="0.0001" />
      <label>LUT</label><textarea name="c3Lut" id="c3Lut"></textarea>
      <button type="button" id="c3PresetBtn">Load Deviation Preset</button>
      <div class="muted err" id="c3LutErr"></div>
    </div>
    <button type="submit">Save Calibration</button>
    <div class="muted" id="calMsg"></div>
    </form>
  </div>

  <script>
    const applyTelemetry = (d) => {
      if (!d) {
        return;
      }

      document.getElementById('fwd').textContent = Number(d.forward ?? 0).toFixed(2);
      document.getElementById('ref').textContent = Number(d.reflect ?? 0).toFixed(2);
      document.getElementById('temp').textContent = Number(d.temperature ?? 0).toFixed(2);
      document.getElementById('fan').textContent = String(d.fanRpm ?? 0);
      document.getElementById('fanDuty').textContent = `${d.fanDutyPercent ?? 0}%`;
      const fanStatusText = ['FAN_CFG_OK','FAN_CFG_CLAMPED','FAN_TACH_MISSING'][d.fanHealthStatus] || 'FAN_STATUS_UNKNOWN';
      document.getElementById('fanStatus').textContent = fanStatusText;
      const dev = Number(d.deviation ?? 0);
      document.getElementById('dev').textContent = dev.toFixed(2);
      const p = Math.max(0, Math.min(100, dev));
      document.getElementById('devBar').style.width = `${p}%`;
      document.getElementById('net').textContent = `Link: ${d.activeLink || 'UNKNOWN'} IP: ${d.ip || location.hostname}`;
    };

    const applyConfig = (c) => {
      if (!c) {
        return;
      }

      document.getElementById('wifiSsid').value = c.wifiSsid || '';
      document.getElementById('wifiPassword').value = c.wifiPassword || '';
      document.getElementById('hostname').value = c.hostname || '';
      document.getElementById('ethEnabled').value = c.ethEnabled ? '1' : '0';
      document.getElementById('dhcp').value = c.dhcp ? '1' : '0';
      document.getElementById('ip').value = c.ip || '';
      document.getElementById('gateway').value = c.gateway || '';
      document.getElementById('subnet').value = c.subnet || '';
      document.getElementById('dns').value = c.dns || '';
      document.getElementById('trapIp').value = c.trapIp || '';
      document.getElementById('trapPort').value = String(c.trapPort ?? 162);
      document.getElementById('fanPwmEnabled').value = c.fanPwmEnabled ? '1' : '0';
      document.getElementById('fanTempStartC').value = String(c.fanTempStartC ?? 30.0);
      document.getElementById('fanTempMaxC').value = String(c.fanTempMaxC ?? 65.0);
      document.getElementById('fanMinDutyPercent').value = String(c.fanMinDutyPercent ?? 30);
      document.getElementById('fanMaxDutyPercent').value = String(c.fanMaxDutyPercent ?? 100);
      document.getElementById('fanStartupMaxSec').value = String(c.fanStartupMaxSec ?? 15);

      for (let i = 0; i < 4; i++) {
        const ch = (c.calibration && c.calibration[i]) || {};
        document.getElementById(`c${i}Mode`).value = String(ch.mode ?? 0);
        document.getElementById(`c${i}K`).value = String(ch.k ?? 1.0);
        document.getElementById(`c${i}Offset`).value = String(ch.offset ?? 0.0);
        document.getElementById(`c${i}A`).value = String(ch.a ?? 0.0);
        document.getElementById(`c${i}B`).value = String(ch.b ?? 1.0);
        document.getElementById(`c${i}C`).value = String(ch.c ?? 0.0);
        document.getElementById(`c${i}Lut`).value = ch.lut || '';
      }
    };

    let configLoaded = false;
    const syncState = async () => {
      try {
        const r = await fetch('/api/state', { cache: 'no-store' });
        if (!r.ok) {
          return;
        }
        const payload = await r.json();
        applyTelemetry(payload.telemetry || {});
        if (!configLoaded && payload.config) {
          applyConfig(payload.config);
          configLoaded = true;
        }
      } catch (e) {
      }
    };

    setInterval(syncState, 1000);
    syncState();

    const DEFAULT_LUT_PRESET = '1800:0;2200:25;2600:100;3200:300;3500:500';

    const validateLut = (text) => {
      const clean = (text || '').trim();
      if (!clean) {
        return '';
      }

      const parts = clean.split(';');
      for (let i = 0; i < parts.length; i++) {
        const row = parts[i].trim();
        if (!row) {
          continue;
        }

        const fields = row.split(':');
        if (fields.length !== 2) {
          return `LUT red ${i + 1}: format mora biti adc:value`;
        }

        const adc = Number(fields[0]);
        const val = Number(fields[1]);
        if (!Number.isFinite(adc) || !Number.isInteger(adc)) {
          return `LUT red ${i + 1}: adc mora biti ceo broj`;
        }
        if (!Number.isFinite(val)) {
          return `LUT red ${i + 1}: value mora biti broj`;
        }
      }

      return '';
    };

    const clearLutErrors = () => {
      for (let i = 0; i < 4; i++) {
        document.getElementById(`c${i}LutErr`).textContent = '';
      }
    };

    const applyPreset = (channel) => {
      document.getElementById(`c${channel}Mode`).value = '2';
      document.getElementById(`c${channel}Lut`).value = DEFAULT_LUT_PRESET;
    };

    document.getElementById('c0PresetBtn').addEventListener('click', () => applyPreset(0));
    document.getElementById('c1PresetBtn').addEventListener('click', () => applyPreset(1));
    document.getElementById('c2PresetBtn').addEventListener('click', () => applyPreset(2));
    document.getElementById('c3PresetBtn').addEventListener('click', () => applyPreset(3));

    const postForm = async (formId, messageId) => {
      const form = document.getElementById(formId);
      const fd = new FormData(form);
      const params = new URLSearchParams(fd);
      const resp = await fetch('/api/config/save', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: params,
      });
      document.getElementById(messageId).textContent = await resp.text();
      configLoaded = false;
      await syncState();
    };

    document.getElementById('networkForm').addEventListener('submit', async (e) => {
      e.preventDefault();
      await postForm('networkForm', 'networkMsg');
    });

    document.getElementById('fanForm').addEventListener('submit', async (e) => {
      e.preventDefault();
      await postForm('fanForm', 'fanMsg');
    });

    document.getElementById('calibrationForm').addEventListener('submit', async (e) => {
      e.preventDefault();
      clearLutErrors();
      const form = e.target;
      const fd = new FormData(form);
      const params = new URLSearchParams(fd);

      for (let i = 0; i < 4; i++) {
        const mode = document.getElementById(`c${i}Mode`).value;
        const k = document.getElementById(`c${i}K`).value;
        const offset = document.getElementById(`c${i}Offset`).value;
        const a = document.getElementById(`c${i}A`).value;
        const b = document.getElementById(`c${i}B`).value;
        const c = document.getElementById(`c${i}C`).value;
        const lut = document.getElementById(`c${i}Lut`).value;

        const lutErr = validateLut(lut);
        if (lutErr) {
          document.getElementById(`c${i}LutErr`).textContent = lutErr;
          document.getElementById('calMsg').textContent = 'Ispravi LUT greske pre snimanja.';
          return;
        }

        params.set(`c${i}Mode`, mode);
        params.set(`c${i}K`, k);
        params.set(`c${i}Offset`, offset);
        params.set(`c${i}A`, a);
        params.set(`c${i}B`, b);
        params.set(`c${i}C`, c);
        params.set(`c${i}Lut`, lut);
      }

      const resp = await fetch('/api/config/save', { method: 'POST', body: params });
      document.getElementById('calMsg').textContent = await resp.text();
    });

    document.getElementById('rebootBtn').addEventListener('click', async () => {
      const resp = await fetch('/api/reboot', { method: 'POST' });
      document.getElementById('networkMsg').textContent = await resp.text();
    });

  </script>
</body>
</html>
)HTML";
}

bool WebManager::begin(EventLogger* eventLogger, ConfigManager* configManager) {
  eventLogger_ = eventLogger;
  configManager_ = configManager;

  setupRoutes();
  wsServer_.onEvent(
      [this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
        this->handleWsEvent(num, type, payload, length);
      });

  return true;
}

void WebManager::update(uint32_t nowMs, const SnmpTelemetry& telemetry) {
  latestTelemetry_ = telemetry;
  ensureServersStarted(telemetry);

  if (telemetry.network.activeLink == LinkType::Ethernet) {
    handleEthernetHttp(telemetry);
  }

  if (wifiHttpStarted_) {
    httpServer_.handleClient();
  }
  if (websocketEnabled_) {
    wsServer_.loop();
  }

  if (nowMs - lastPublishMs_ < 5000) {
    // Keep event stream responsive even if telemetry cadence is lower.
    if (nowMs - lastEventPublishMs_ >= 1000) {
      lastEventPublishMs_ = nowMs;
      publishEventsWebSocket();
    }
    return;
  }
  lastPublishMs_ = nowMs;
  publishStatusWebSocket(telemetry);

  if (nowMs - lastEventPublishMs_ >= 1000) {
    lastEventPublishMs_ = nowMs;
    publishEventsWebSocket();
  }

  if (rebootRequested_) {
    rebootRequested_ = false;
    delay(100);
    ESP.restart();
  }
}

void WebManager::ensureServersStarted(const SnmpTelemetry& telemetry) {
  if (telemetry.network.activeLink == LinkType::None || telemetry.network.ip == IPAddress(0, 0, 0, 0)) {
    return;
  }

  if (telemetry.network.activeLink == LinkType::Ethernet) {
    if (!ethHttpStarted_) {
      ethHttpServer_.begin();
      ethHttpStarted_ = true;
      Serial.printf("WEB: ETH HTTP started on %s\n",
                    telemetry.network.ip.toString().c_str());
    }
    return;
  }

  if (telemetry.network.activeLink == LinkType::WiFi && !wifiHttpStarted_) {
    httpServer_.begin();
    wifiHttpStarted_ = true;
    websocketEnabled_ = false;
    Serial.printf("WEB: WiFi HTTP started on %s\n",
                  telemetry.network.ip.toString().c_str());
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
  payload += "{\"wifiSsid\":\"" + String(cfg.network.wifiSsid) + "\",";
  payload += "\"wifiPassword\":\"" + String(cfg.network.wifiPassword) + "\",";
  payload += "\"hostname\":\"" + String(cfg.network.hostname) + "\",";
  payload += "\"dhcp\":" + String(cfg.network.dhcp ? "true" : "false") + ",";
  payload += "\"ethEnabled\":" + String(cfg.network.ethEnabled ? "true" : "false") + ",";
  payload += "\"ip\":\"" + cfg.network.ip.toString() + "\",";
  payload += "\"gateway\":\"" + cfg.network.gateway.toString() + "\",";
  payload += "\"subnet\":\"" + cfg.network.subnet.toString() + "\",";
  payload += "\"dns\":\"" + cfg.network.dns.toString() + "\",";
  payload += "\"trapIp\":\"" + cfg.snmp.trapServer.toString() + "\",";
  payload += "\"trapPort\":" + String(cfg.snmp.trapPort) + ",";
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
      lut += String(c.lut[j].adc);
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
  const char* link = "NONE";
  if (telemetry.network.activeLink == LinkType::Ethernet) {
    link = "ETH";
  } else if (telemetry.network.activeLink == LinkType::WiFi) {
    link = "WIFI";
  }

  char telemetryJson[360];
  snprintf(telemetryJson,
           sizeof(telemetryJson),
           "{\"forward\":%.2f,\"reflect\":%.2f,\"temperature\":%.2f,\"deviation\":%.2f,\"fanRpm\":%u,\"fanDutyPercent\":%u,\"fanHealthStatus\":%u,\"activeLink\":\"%s\",\"ip\":\"%s\"}",
           telemetry.analog.forward,
           telemetry.analog.reflect,
           telemetry.analog.temperature,
           telemetry.analog.deviation,
           static_cast<unsigned>(telemetry.fanRpm),
           static_cast<unsigned>(telemetry.fanDutyPercent),
           static_cast<unsigned>(telemetry.fanHealthStatus),
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
  if (getFormArg(body, "ethEnabled", value)) {
    cfg.network.ethEnabled = parseBoolArg(value);
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

void WebManager::publishStatusWebSocket(const SnmpTelemetry& telemetry) {
  if (!websocketEnabled_) {
    return;
  }

  char payload[420];
  const char* link = "NONE";
  if (telemetry.network.activeLink == LinkType::Ethernet) {
    link = "ETH";
  } else if (telemetry.network.activeLink == LinkType::WiFi) {
    link = "WIFI";
  }

  snprintf(payload,
           sizeof(payload),
           "{\"type\":\"telemetry\",\"forward\":%.2f,\"reflect\":%.2f,\"temperature\":%.2f,\"deviation\":%.2f,\"fanRpm\":%u,\"fanDutyPercent\":%u,\"fanHealthStatus\":%u,\"activeLink\":\"%s\",\"ip\":\"%s\"}",
           telemetry.analog.forward,
           telemetry.analog.reflect,
           telemetry.analog.temperature,
           telemetry.analog.deviation,
           static_cast<unsigned>(telemetry.fanRpm),
           static_cast<unsigned>(telemetry.fanDutyPercent),
           static_cast<unsigned>(telemetry.fanHealthStatus),
           link,
           telemetry.network.ip.toString().c_str());
  wsServer_.broadcastTXT(payload);
}

void WebManager::publishEventsWebSocket() {
  if (!websocketEnabled_) {
    return;
  }

  if (eventLogger_ == nullptr) {
    return;
  }

  EventRecord rec;
  if (!eventLogger_->getRecent(0, rec)) {
    return;
  }

  char payload[180];
  snprintf(payload,
           sizeof(payload),
           "{\"type\":\"event\",\"timeMs\":%lu,\"eventType\":%u,\"message\":\"%s\"}",
           static_cast<unsigned long>(rec.timeMs),
           static_cast<unsigned>(rec.type),
           rec.text);
  wsServer_.broadcastTXT(payload);
}

void WebManager::setupRoutes() {
  httpServer_.on("/", HTTP_GET, [this]() { handleRoot(); });
  httpServer_.on("/api/state", HTTP_GET, [this]() { handleStateGet(); });
  httpServer_.on("/api/telemetry", HTTP_GET, [this]() { handleStateGet(); });
  httpServer_.on("/api/config/save", HTTP_POST, [this]() { handleConfigSave(); });
  httpServer_.on("/api/reboot", HTTP_POST, [this]() { handleReboot(); });
}

void WebManager::handleRoot() {
  httpServer_.send_P(200, "text/html", kIndexHtml);
}

void WebManager::handleStateGet() {
  httpServer_.send(200, "application/json", buildStateJson(latestTelemetry_));
}

void WebManager::handleTelemetryGet() {
  const char* link = "NONE";
  if (latestTelemetry_.network.activeLink == LinkType::Ethernet) {
    link = "ETH";
  } else if (latestTelemetry_.network.activeLink == LinkType::WiFi) {
    link = "WIFI";
  }

  char payload[360];
  snprintf(payload,
           sizeof(payload),
           "{\"forward\":%.2f,\"reflect\":%.2f,\"temperature\":%.2f,\"deviation\":%.2f,\"fanRpm\":%u,\"fanDutyPercent\":%u,\"fanHealthStatus\":%u,\"activeLink\":\"%s\",\"ip\":\"%s\"}",
           latestTelemetry_.analog.forward,
           latestTelemetry_.analog.reflect,
           latestTelemetry_.analog.temperature,
           latestTelemetry_.analog.deviation,
           static_cast<unsigned>(latestTelemetry_.fanRpm),
           static_cast<unsigned>(latestTelemetry_.fanDutyPercent),
           static_cast<unsigned>(latestTelemetry_.fanHealthStatus),
           link,
           latestTelemetry_.network.ip.toString().c_str());

  httpServer_.send(200, "application/json", payload);
}

void WebManager::handleConfigGet() {
  if (configManager_ == nullptr) {
    httpServer_.send(500, "application/json", "{\"error\":\"config unavailable\"}");
    return;
  }

  httpServer_.send(200, "application/json", buildConfigJson());
}

void WebManager::handleConfigSave() {
  if (configManager_ == nullptr) {
    httpServer_.send(500, "text/plain", "Config manager unavailable");
    return;
  }

  DeviceConfig& cfg = configManager_->mutableConfig();

  if (httpServer_.hasArg("hostname")) {
    httpServer_.arg("hostname").toCharArray(cfg.network.hostname, sizeof(cfg.network.hostname));
  }
  if (httpServer_.hasArg("ethEnabled")) {
    cfg.network.ethEnabled = parseBoolArg(httpServer_.arg("ethEnabled"));
  }
  if (httpServer_.hasArg("dhcp")) {
    cfg.network.dhcp = parseBoolArg(httpServer_.arg("dhcp"));
  }
  IPAddress ip;
  if (parseIpArg("ip", ip)) {
    cfg.network.ip = ip;
  }
  if (parseIpArg("gateway", ip)) {
    cfg.network.gateway = ip;
  }
  if (parseIpArg("subnet", ip)) {
    cfg.network.subnet = ip;
  }
  if (parseIpArg("dns", ip)) {
    cfg.network.dns = ip;
  }
  if (parseIpArg("trapIp", ip)) {
    cfg.snmp.trapServer = ip;
  }
  if (httpServer_.hasArg("trapPort")) {
    const int trapPort = httpServer_.arg("trapPort").toInt();
    if (trapPort >= 1 && trapPort <= 65535) {
      cfg.snmp.trapPort = static_cast<uint16_t>(trapPort);
    }
  }

  if (httpServer_.hasArg("fanPwmEnabled")) {
    cfg.fanPwmEnabled = parseBoolArg(httpServer_.arg("fanPwmEnabled"));
  }
  parseFloatArg("fanTempStartC", cfg.fanTempStartC);
  parseFloatArg("fanTempMaxC", cfg.fanTempMaxC);
  if (httpServer_.hasArg("fanMinDutyPercent")) {
    cfg.fanMinDutyPercent = static_cast<uint8_t>(httpServer_.arg("fanMinDutyPercent").toInt());
  }
  if (httpServer_.hasArg("fanMaxDutyPercent")) {
    cfg.fanMaxDutyPercent = static_cast<uint8_t>(httpServer_.arg("fanMaxDutyPercent").toInt());
  }
  if (httpServer_.hasArg("fanStartupMaxSec")) {
    cfg.fanStartupMaxSec = static_cast<uint16_t>(httpServer_.arg("fanStartupMaxSec").toInt());
  }

  for (uint8_t i = 0; i < 4; ++i) {
    CalibrationProfile& c = cfg.calibration[i];
    const String prefix = "c" + String(i);

    const String modeArg = prefix + "Mode";
    if (httpServer_.hasArg(modeArg)) {
      const int mode = httpServer_.arg(modeArg).toInt();
      if (mode >= 0 && mode <= 2) {
        c.mode = static_cast<CalibrationMode>(mode);
      }
    }

    parseFloatArg(prefix + "K", c.k);
    parseFloatArg(prefix + "Offset", c.offset);
    parseFloatArg(prefix + "A", c.a);
    parseFloatArg(prefix + "B", c.b);
    parseFloatArg(prefix + "C", c.c);

    const String lutArg = prefix + "Lut";
    if (httpServer_.hasArg(lutArg)) {
      parseLutText(httpServer_.arg(lutArg), c);
    }
  }

  if (configManager_->save()) {
    httpServer_.send(200, "text/plain", "Saved. Reboot to apply network changes.");
  } else {
    httpServer_.send(500, "text/plain", "Failed to save configuration.");
  }
}

void WebManager::handleReboot() {
  rebootRequested_ = true;
  httpServer_.send(200, "text/plain", "Reboot scheduled.");
}

void WebManager::handleWsEvent(uint8_t clientNum, WStype_t type, uint8_t* payload, size_t length) {
  (void)clientNum;
  (void)payload;
  (void)length;
  (void)type;
}

bool WebManager::parseBoolArg(const String& value) const {
  String lower = value;
  lower.toLowerCase();
  return lower == "1" || lower == "true" || lower == "on" || lower == "yes";
}

bool WebManager::parseIpArg(const String& argName, IPAddress& out) {
  if (!httpServer_.hasArg(argName)) {
    return false;
  }
  return out.fromString(httpServer_.arg(argName));
}

bool WebManager::parseFloatArg(const String& argName, float& out) {
  if (!httpServer_.hasArg(argName)) {
    return false;
  }
  out = httpServer_.arg(argName).toFloat();
  return true;
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
      const int16_t adc = static_cast<int16_t>(token.substring(0, sep).toInt());
      const float value = token.substring(sep + 1).toFloat();
      profile.lut.push_back({adc, value});
    }

    start = end + 1;
  }
}
