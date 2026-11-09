#pragma once

#include <Arduino.h>
#include <Ethernet.h>
#include <SPI.h>

#include "ConfigManager.h"

enum class LinkType : uint8_t {
  None = 0,
  Ethernet = 1,
};

struct NetworkStatus {
  LinkType activeLink = LinkType::None;
  bool connected = false;
  IPAddress ip = IPAddress(0, 0, 0, 0);
  IPAddress gateway = IPAddress(0, 0, 0, 0);
};

class NetworkManager {
 public:
  bool begin(const DeviceConfig* cfg);
  void update(uint32_t nowMs);

  NetworkStatus getStatus() const;

 private:
  bool connectEthernet();
  bool ethernetIsOnline() const;
  void fillBoardMac(uint8_t* mac) const;

  const DeviceConfig* cfg_ = nullptr;
  NetworkStatus status_;
  uint8_t mac_[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00};
  bool ethernetReady_ = false;
  bool w5500Responding_ = false;
  bool w5500ResetApplied_ = false;
  bool spiReady_ = false;
  uint32_t lastEthernetRetryMs_ = 0;
  static constexpr uint32_t kReconnectPeriodMs = 5000;
};
