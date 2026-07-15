#include "NetworkManager.h"

#include "PinConfig.h"

namespace {
constexpr uint16_t kW5500VersionReg = 0x0039;
constexpr uint32_t kEthernetLinkWaitMs = 4000;
constexpr uint32_t kEthernetLinkPollMs = 100;

uint8_t readW5500Version(uint8_t csPin) {
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(csPin, LOW);
  SPI.transfer(static_cast<uint8_t>(kW5500VersionReg >> 8));
  SPI.transfer(static_cast<uint8_t>(kW5500VersionReg & 0xFF));
  SPI.transfer(0x00);
  const uint8_t version = SPI.transfer(0x00);
  digitalWrite(csPin, HIGH);
  SPI.endTransaction();
  return version;
}
}  // namespace

bool NetworkManager::begin(const DeviceConfig* cfg) {
  cfg_ = cfg;
  if (cfg_ == nullptr) {
    return false;
  }

  bootMs_ = millis();

  if (cfg_->network.forceWifiPinEnabled) {
    pinMode(cfg_->network.forceWifiPin,
            cfg_->network.forceWifiPinActiveLow ? INPUT_PULLUP : INPUT_PULLDOWN);
  }

  ethernetReady_ = false;
  w5500Responding_ = false;
  wifiApReady_ = false;
  w5500ResetApplied_ = false;

  // Startup policy: try Ethernet first when enabled.
  if (cfg_->network.ethEnabled) {
    Serial.println("NET: Ethernet enabled, trying ETH first");
    ethernetReady_ = connectEthernet();
    if (ethernetIsOnline()) {
      Serial.printf("NET: ETH online, IP=%s\n", Ethernet.localIP().toString().c_str());
      if (isForceWifiRequested()) {
        if (startWiFiAp()) {
          Serial.println("NET: WiFi AP enabled because FORCE WIFI is active");
        }
      } else {
        stopWiFi();
        Serial.println("NET: ETH priority active, WiFi AP disabled");
      }
      return true;
    }
    Serial.println("NET: ETH unavailable, falling back to WiFi AP");
  }

  startWiFiAp();
  return true;
}

void NetworkManager::update(uint32_t nowMs) {
  if (cfg_ == nullptr) {
    return;
  }

  const bool ethEnabled = cfg_->network.ethEnabled;

  if (ethEnabled && ethernetIsOnline()) {
    status_.activeLink = LinkType::Ethernet;
    status_.connected = true;
    status_.ip = Ethernet.localIP();

    if (isForceWifiRequested()) {
      startWiFiAp();
    } else {
      stopWiFi();
    }
    return;
  }

  if (ethEnabled && nowMs - lastEthernetRetryMs_ >= kReconnectPeriodMs) {
    lastEthernetRetryMs_ = nowMs;
    ethernetReady_ = connectEthernet();
  } else if (!ethEnabled) {
    ethernetReady_ = false;
  }

  if (startWiFiAp()) {
    status_.activeLink = LinkType::WiFi;
    status_.connected = true;
    status_.ip = WiFi.softAPIP();
    return;
  }

  status_.activeLink = LinkType::None;
  status_.connected = false;
  status_.ip = IPAddress(0, 0, 0, 0);
}

NetworkStatus NetworkManager::getStatus() const {
  return status_;
}

bool NetworkManager::startWiFiAp() {
  if (wifiApReady_) {
    return true;
  }

  if (cfg_ == nullptr) {
    return false;
  }

  if (strlen(cfg_->network.wifiSsid) == 0) {
    Serial.println("NET: WiFi SSID is empty, AP start aborted");
    return false;
  }

  WiFi.mode(WIFI_AP);
  wifiApReady_ = WiFi.softAP(cfg_->network.wifiSsid, cfg_->network.wifiPassword);
  if (wifiApReady_) {
    Serial.printf("NET: WiFi AP started, SSID=%s IP=%s\n",
                  cfg_->network.wifiSsid,
                  WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("NET: WiFi AP start failed");
  }
  return wifiApReady_;
}

void NetworkManager::stopWiFi() {
  const wifi_mode_t mode = WiFi.getMode();
  if (mode == WIFI_AP || mode == WIFI_AP_STA) {
    WiFi.softAPdisconnect(true);
  }

  // Keep WiFi/lwIP socket stack initialized so HTTP server can run on ETH.
  if (mode == WIFI_OFF || mode == WIFI_AP || mode == WIFI_AP_STA) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
  }

  wifiApReady_ = false;
}

bool NetworkManager::connectEthernet() {
  if (cfg_ == nullptr) {
    return false;
  }

  const uint8_t csPin = PinConfig::kW5500CsPin;
  const int8_t rstPin = PinConfig::kW5500RstPin;
  Serial.printf("NET: ETH pins sck=%u miso=%u mosi=%u cs=%u\n",
                static_cast<unsigned>(PinConfig::kW5500SckPin),
                static_cast<unsigned>(PinConfig::kW5500MisoPin),
                static_cast<unsigned>(PinConfig::kW5500MosiPin),
                static_cast<unsigned>(csPin));

  if (!w5500ResetApplied_ && rstPin >= 0) {
    Serial.printf("NET: ETH resetting W5500 via rst=%d\n", static_cast<int>(rstPin));
    pinMode(static_cast<uint8_t>(rstPin), OUTPUT);
    digitalWrite(static_cast<uint8_t>(rstPin), LOW);
    delay(10);
    digitalWrite(static_cast<uint8_t>(rstPin), HIGH);
    delay(120);
    w5500ResetApplied_ = true;
  }

  pinMode(csPin, OUTPUT);
  digitalWrite(csPin, HIGH);

  SPI.begin(PinConfig::kW5500SckPin, PinConfig::kW5500MisoPin, PinConfig::kW5500MosiPin, csPin);

  const uint8_t version = readW5500Version(csPin);
  Serial.printf("NET: W5500 version probe=0x%02X\n", static_cast<unsigned>(version));
  w5500Responding_ = (version == 0x04);

  Ethernet.init(csPin);

  // Pre-begin probe is diagnostic only; some modules report no-hardware before begin().
  const auto hw = Ethernet.hardwareStatus();
  const auto linkBeforeBegin = Ethernet.linkStatus();
  Serial.printf("NET: ETH probe hw=%d link=%d\n",
                static_cast<int>(hw),
                static_cast<int>(linkBeforeBegin));
  if (!w5500Responding_) {
    Serial.println("NET: W5500 version probe did not match expected chip, continuing with caution");
  }

  const uint8_t* mac = cfg_->network.ethMac;

  if (cfg_->network.dhcp) {
    Serial.println("NET: ETH DHCP start");
    // Keep DHCP bounded so periodic retries do not starve the task watchdog.
    if (Ethernet.begin(const_cast<uint8_t*>(mac), 4000, 1200) == 0) {
      Serial.println("NET: ETH DHCP failed");
      return false;
    }
  } else {
    Serial.printf("NET: ETH static start ip=%s gw=%s sn=%s dns=%s\n",
                  cfg_->network.ip.toString().c_str(),
                  cfg_->network.gateway.toString().c_str(),
                  cfg_->network.subnet.toString().c_str(),
                  cfg_->network.dns.toString().c_str());
    Ethernet.begin(
        const_cast<uint8_t*>(mac),
        cfg_->network.ip,
        cfg_->network.dns,
        cfg_->network.gateway,
        cfg_->network.subnet);
  }

  Serial.printf("NET: ETH post-begin ip=%s hw=%d link=%d\n",
                Ethernet.localIP().toString().c_str(),
                static_cast<int>(Ethernet.hardwareStatus()),
                static_cast<int>(Ethernet.linkStatus()));

  const uint32_t waitStartMs = millis();
  while (Ethernet.linkStatus() != LinkON && (millis() - waitStartMs) < kEthernetLinkWaitMs) {
    delay(kEthernetLinkPollMs);
  }

  if (Ethernet.linkStatus() == LinkON) {
    Serial.printf("NET: ETH link stabilized after %lu ms\n",
                  static_cast<unsigned long>(millis() - waitStartMs));
  } else {
    Serial.println("NET: ETH link still not ON after wait window");
  }

  ethernetReady_ = true;
  return ethernetIsOnline();
}

bool NetworkManager::ethernetIsOnline() const {
  if (!ethernetReady_) {
    return false;
  }

  if (!w5500Responding_) {
    return false;
  }

  const auto link = Ethernet.linkStatus();
  if (link != LinkON) {
    return false;
  }

  return Ethernet.localIP() != IPAddress(0, 0, 0, 0);
}

bool NetworkManager::shouldKeepWifiForConfig(uint32_t nowMs) const {
  if (cfg_ == nullptr || !cfg_->network.wifiConfigAccessEnabled) {
    return false;
  }

  const uint32_t windowMs = static_cast<uint32_t>(cfg_->network.wifiConfigWindowSec) * 1000UL;
  if (windowMs == 0) {
    return true;
  }

  return (nowMs - bootMs_) <= windowMs;
}

bool NetworkManager::isForceWifiRequested() const {
  if (cfg_ == nullptr || !cfg_->network.forceWifiPinEnabled) {
    return false;
  }

  const bool high = digitalRead(cfg_->network.forceWifiPin) == HIGH;
  return cfg_->network.forceWifiPinActiveLow ? !high : high;
}
