#include "NetworkManager.h"

#include "PinConfig.h"

#include <pico/unique_id.h>

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

  fillBoardMac(mac_);
  ethernetReady_ = false;
  w5500Responding_ = false;
  w5500ResetApplied_ = false;
  spiReady_ = false;

  Serial.println("NET: Ethernet-only start");
  ethernetReady_ = connectEthernet();
  if (ethernetIsOnline()) {
    status_.activeLink = LinkType::Ethernet;
    status_.connected = true;
    status_.ip = Ethernet.localIP();
    status_.gateway = Ethernet.gatewayIP();
    Serial.printf("NET: ETH online, IP=%s\n", status_.ip.toString().c_str());
  } else {
    status_.activeLink = LinkType::None;
    status_.connected = false;
    status_.ip = IPAddress(0, 0, 0, 0);
    status_.gateway = IPAddress(0, 0, 0, 0);
    Serial.println("NET: ETH unavailable");
  }
  return true;
}

void NetworkManager::update(uint32_t nowMs) {
  if (cfg_ == nullptr) {
    return;
  }

  if (ethernetIsOnline()) {
    status_.activeLink = LinkType::Ethernet;
    status_.connected = true;
    status_.ip = Ethernet.localIP();
    status_.gateway = Ethernet.gatewayIP();
    return;
  }

  if (nowMs - lastEthernetRetryMs_ >= kReconnectPeriodMs) {
    lastEthernetRetryMs_ = nowMs;
    ethernetReady_ = connectEthernet();
  }

  if (ethernetIsOnline()) {
    status_.activeLink = LinkType::Ethernet;
    status_.connected = true;
    status_.ip = Ethernet.localIP();
    status_.gateway = Ethernet.gatewayIP();
    return;
  }

  status_.activeLink = LinkType::None;
  status_.connected = false;
  status_.ip = IPAddress(0, 0, 0, 0);
  status_.gateway = IPAddress(0, 0, 0, 0);
}

NetworkStatus NetworkManager::getStatus() const {
  return status_;
}

void NetworkManager::fillBoardMac(uint8_t* mac) const {
  pico_unique_board_id_t uid;
  pico_get_unique_board_id(&uid);
  mac[0] = 0x02;
  mac[1] = uid.id[2];
  mac[2] = uid.id[3];
  mac[3] = uid.id[4];
  mac[4] = uid.id[5];
  mac[5] = uid.id[6];
}

bool NetworkManager::connectEthernet() {
  if (cfg_ == nullptr) {
    return false;
  }

  const uint8_t csPin = PinConfig::kW5500CsPin;
  const uint8_t rstPin = PinConfig::kW5500RstPin;
  Serial.printf("NET: ETH pins sck=%u miso=%u mosi=%u cs=%u rst=%u\n",
                static_cast<unsigned>(PinConfig::kW5500SckPin),
                static_cast<unsigned>(PinConfig::kW5500MisoPin),
                static_cast<unsigned>(PinConfig::kW5500MosiPin),
                static_cast<unsigned>(csPin),
                static_cast<unsigned>(rstPin));

  if (!w5500ResetApplied_) {
    Serial.println("NET: ETH resetting W5500");
    pinMode(rstPin, OUTPUT);
    digitalWrite(rstPin, LOW);
    delay(2);
    digitalWrite(rstPin, HIGH);
    delay(50);
    w5500ResetApplied_ = true;
  }

  pinMode(csPin, OUTPUT);
  digitalWrite(csPin, HIGH);

  if (!spiReady_) {
    SPI.setRX(PinConfig::kW5500MisoPin);
    SPI.setTX(PinConfig::kW5500MosiPin);
    SPI.setSCK(PinConfig::kW5500SckPin);
    SPI.setCS(csPin);
    SPI.begin();
    spiReady_ = true;
  }

  const uint8_t version = readW5500Version(csPin);
  Serial.printf("NET: W5500 version probe=0x%02X\n", static_cast<unsigned>(version));
  w5500Responding_ = (version == 0x04);

  Ethernet.init(csPin);

  const auto hw = Ethernet.hardwareStatus();
  const auto linkBeforeBegin = Ethernet.linkStatus();
  Serial.printf("NET: ETH probe hw=%d link=%d mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
                static_cast<int>(hw),
                static_cast<int>(linkBeforeBegin),
                mac_[0],
                mac_[1],
                mac_[2],
                mac_[3],
                mac_[4],
                mac_[5]);
  if (!w5500Responding_) {
    Serial.println("NET: W5500 version probe did not match expected chip, continuing with caution");
  }

  if (cfg_->network.dhcp) {
    Serial.println("NET: ETH DHCP start");
    if (Ethernet.begin(mac_, 4000, 1200) == 0) {
      Serial.println("NET: ETH DHCP failed");
      ethernetReady_ = false;
      return false;
    }
  } else {
    Serial.printf("NET: ETH static start ip=%s gw=%s sn=%s dns=%s\n",
                  cfg_->network.ip.toString().c_str(),
                  cfg_->network.gateway.toString().c_str(),
                  cfg_->network.subnet.toString().c_str(),
                  cfg_->network.dns.toString().c_str());
    Ethernet.begin(mac_, cfg_->network.ip, cfg_->network.dns, cfg_->network.gateway, cfg_->network.subnet);
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

  if (Ethernet.linkStatus() != LinkON) {
    return false;
  }

  return Ethernet.localIP() != IPAddress(0, 0, 0, 0);
}
