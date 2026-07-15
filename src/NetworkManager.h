#pragma once

#include <Arduino.h>
#include <Ethernet.h>
#include <SPI.h>
#include <WiFi.h>

#include "ConfigManager.h"

enum class LinkType : uint8_t {
  None = 0,
  Ethernet = 1,
  WiFi = 2,
};

struct NetworkStatus {
  LinkType activeLink = LinkType::None;
  bool connected = false;
  IPAddress ip = IPAddress(0, 0, 0, 0);
};

class NetworkManager {
 public:
  bool begin(const DeviceConfig* cfg);
  void update(uint32_t nowMs);

  NetworkStatus getStatus() const;

 private:
    bool startWiFiAp();
    void stopWiFi();
  bool connectEthernet();
  bool ethernetIsOnline() const;
  bool shouldKeepWifiForConfig(uint32_t nowMs) const;
  bool isForceWifiRequested() const;

  const DeviceConfig* cfg_ = nullptr;
  NetworkStatus status_;
  bool ethernetReady_ = false;
  bool w5500Responding_ = false;
  bool wifiApReady_ = false;
  bool w5500ResetApplied_ = false;
  uint32_t bootMs_ = 0;
  uint32_t lastEthernetRetryMs_ = 0;
  static constexpr uint32_t kReconnectPeriodMs = 5000;
};
