#pragma once

#include <Arduino.h>
#include <Ethernet.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

#include "ConfigManager.h"
#include "EventLogger.h"
#include "SNMPManager.h"

class EthHttpServerCompat : public EthernetServer {
 public:
  explicit EthHttpServerCompat(uint16_t port) : EthernetServer(port) {}

  void begin(uint16_t port = 0) override {
    (void)port;
    EthernetServer::begin();
  }
};

class WebManager {
 public:
  bool begin(EventLogger* eventLogger, ConfigManager* configManager);
  void update(uint32_t nowMs, const SnmpTelemetry& telemetry);

 private:
  void ensureServersStarted(const SnmpTelemetry& telemetry);
  void handleEthernetHttp(const SnmpTelemetry& telemetry);
  String buildConfigJson() const;
    void handleStateGet();
    String buildStateJson(const SnmpTelemetry& telemetry) const;
  bool applyConfigFromFormBody(const String& body);
  bool getFormArg(const String& body, const String& key, String& out) const;
  String decodeUrlFormComponent(const String& text) const;
  bool parseIpValue(const String& value, IPAddress& out) const;
  void setupRoutes();
  void handleRoot();
  void handleTelemetryGet();
  void handleConfigGet();
  void handleConfigSave();
  void handleReboot();
  void handleWsEvent(uint8_t clientNum, WStype_t type, uint8_t* payload, size_t length);

  bool parseBoolArg(const String& value) const;
  bool parseIpArg(const String& argName, IPAddress& out);
  bool parseFloatArg(const String& argName, float& out);
  void parseLutText(const String& text, CalibrationProfile& profile) const;

  void publishStatusWebSocket(const SnmpTelemetry& telemetry);
  void publishEventsWebSocket();

  WebServer httpServer_{80};
  EthHttpServerCompat ethHttpServer_{80};
  WebSocketsServer wsServer_{81};
  EventLogger* eventLogger_ = nullptr;
  ConfigManager* configManager_ = nullptr;
  bool rebootRequested_ = false;
  bool wifiHttpStarted_ = false;
  bool ethHttpStarted_ = false;
  bool websocketEnabled_ = false;
  SnmpTelemetry latestTelemetry_;
  uint32_t lastPublishMs_ = 0;
  uint32_t lastEventPublishMs_ = 0;
};
