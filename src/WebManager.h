#pragma once

#include <Arduino.h>
#include <Ethernet.h>

#include "ConfigManager.h"
#include "EventLogger.h"
#include "SNMPManager.h"

class WebManager {
 public:
  bool begin(EventLogger* eventLogger, ConfigManager* configManager);
  void update(uint32_t nowMs, const SnmpTelemetry& telemetry);

 private:
  void ensureServerStarted(const SnmpTelemetry& telemetry);
  void handleEthernetHttp(const SnmpTelemetry& telemetry);
  String buildConfigJson() const;
  String buildStateJson(const SnmpTelemetry& telemetry) const;
  bool applyConfigFromFormBody(const String& body);
  bool getFormArg(const String& body, const String& key, String& out) const;
  String decodeUrlFormComponent(const String& text) const;
  bool parseIpValue(const String& value, IPAddress& out) const;
  bool parseBoolArg(const String& value) const;
  void parseLutText(const String& text, CalibrationProfile& profile) const;

  EthernetServer ethHttpServer_{80};
  EventLogger* eventLogger_ = nullptr;
  ConfigManager* configManager_ = nullptr;
  bool rebootRequested_ = false;
  bool ethHttpStarted_ = false;
  SnmpTelemetry latestTelemetry_;
};
