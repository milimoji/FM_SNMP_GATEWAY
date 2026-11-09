#pragma once

#include <Arduino.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Udp.h>
#include <vector>

#include "AlarmManager.h"
#include "AnalogManager.h"
#include "ConfigManager.h"
#include "NetworkManager.h"

namespace SNMP {
class Agent;
class BER;
class Message;
}  // namespace SNMP

struct SnmpTelemetry {
  AnalogValues analog;
  AlarmState alarm;
  NetworkStatus network;
  uint16_t fanRpm = 0;
  uint8_t fanDutyPercent = 0;
  uint8_t fanHealthStatus = 0;
  uint32_t freeHeapBytes = 0;
  uint32_t uptimeSec = 0;
};

enum class OidAccess : uint8_t {
  ReadOnly = 0,
  ReadWrite = 1,
};

struct OidBinding {
  String oid;
  String name;
  OidAccess access = OidAccess::ReadOnly;
};

class SNMPManager {
 public:
  bool begin(const DeviceConfig* cfg, const char* deviceName);
  void update(uint32_t nowMs, const SnmpTelemetry& telemetry);
  void onAlarmEdge(bool active, const SnmpTelemetry& telemetry);
  void handleRequest(const SNMP::Message* request, const IPAddress remote, const uint16_t port);
  void setOidMap(const std::vector<OidBinding>& map);
  const std::vector<OidBinding>& getOidMap() const;

 private:
  void registerDefaultOidMap();
  void emitTrap(const char* eventName, const SnmpTelemetry& telemetry) const;
  void ensureAgentBound();
  void buildResponse(const SNMP::Message* request, SNMP::Message* response) const;
  const OidBinding* findOid(const char* oid) const;
  const OidBinding* findNextOid(const char* oid) const;
  static int compareOids(const char* lhs, const char* rhs);
  SNMP::BER* buildValueForOid(const OidBinding& binding) const;

  const DeviceConfig* cfg_ = nullptr;
  String deviceName_;
  std::vector<OidBinding> oidMap_;
  SnmpTelemetry telemetry_;
  bool snmpStackReady_ = false;
  uint32_t lastLogMs_ = 0;
  SNMP::Agent* snmp_ = nullptr;
  EthernetUDP* ethernetUdp_ = nullptr;
  UDP* activeUdp_ = nullptr;
  LinkType boundLink_ = LinkType::None;
};
