#include <Arduino.h>
#include <pico/mutex.h>

#include "AlarmManager.h"
#include "AnalogManager.h"
#include "ConfigManager.h"
#include "EventLogger.h"
#include "FanManager.h"
#include "NetworkManager.h"
#include "PinConfig.h"
#include "SNMPManager.h"
#include "WebManager.h"

namespace {
constexpr uint32_t kWdtTimeoutMs = 10000;
constexpr uint16_t kMeasurementPeriodMs = 50;
constexpr uint16_t kCommunicationPeriodMs = 50;
constexpr uint32_t kNetworkResetHoldMs = 5000;
constexpr uint32_t kButtonDebounceMs = 50;

ConfigManager configManager;
AnalogManager analogManager;
AlarmManager alarmManager;
NetworkManager networkManager;
SNMPManager snmpManager;
WebManager webManager;
EventLogger eventLogger;
FanManager fanManager;

SnmpTelemetry gTelemetry;
bool gAlarmEdgePending = false;
bool gAlarmEdgeState = false;
LinkType gLastLink = LinkType::None;
mutex_t gTelemetryMutex;
volatile bool gHardwareReady = false;
volatile uint32_t gMeasurementHeartbeatMs = 0;

void pollNetworkResetButton(uint32_t nowMs) {
  static int lastRaw = HIGH;
  static uint32_t lastChangeMs = 0;
  static bool armed = false;
  static bool pressed = false;
  static uint32_t pressStartMs = 0;
  static bool fired = false;

  const int raw = digitalRead(PinConfig::kNetworkResetPin);
  if (raw != lastRaw) {
    lastRaw = raw;
    lastChangeMs = nowMs;
  }
  if ((nowMs - lastChangeMs) < kButtonDebounceMs) {
    return;
  }

  const bool isPressed = (raw == LOW);
  if (!isPressed) {
    armed = true;
    pressed = false;
    fired = false;
    return;
  }

  if (!armed || fired) {
    return;
  }

  if (!pressed) {
    pressed = true;
    pressStartMs = nowMs;
    return;
  }

  if ((nowMs - pressStartMs) < kNetworkResetHoldMs) {
    return;
  }

  fired = true;
  Serial.println("NET: long-press reset to default network settings");
  configManager.resetNetworkToDefaults();
  configManager.save();
  eventLogger.log(EventType::Network, "NET_RESET_DEFAULTS");
  delay(100);
  rp2040.reboot();
}

void runMeasurement(uint32_t nowMs) {
  analogManager.update(nowMs);
  const AnalogValues analog = analogManager.getValues();
  fanManager.update(nowMs, analog.temperature);
  alarmManager.update(nowMs);
  const AlarmState alarm = alarmManager.getState();

  mutex_enter_blocking(&gTelemetryMutex);
  gTelemetry.analog = analog;
  gTelemetry.alarm = alarm;
  gTelemetry.fanRpm = fanManager.getRpm();
  gTelemetry.fanDutyPercent = fanManager.getDutyPercent();
  gTelemetry.fanHealthStatus = fanManager.getHealthStatusCode();
  gTelemetry.freeHeapBytes = rp2040.getFreeHeap();
  gTelemetry.uptimeSec = nowMs / 1000;
  if (alarm.alarmChanged) {
    gAlarmEdgePending = true;
    gAlarmEdgeState = alarm.alarmActive || alarm.statusInput;
  }
  mutex_exit(&gTelemetryMutex);

  gMeasurementHeartbeatMs = nowMs;
}

void runCommunication(uint32_t nowMs) {
  networkManager.update(nowMs);
  pollNetworkResetButton(nowMs);

  SnmpTelemetry telemetry;
  bool alarmEdgePending = false;
  bool alarmEdgeState = false;
  mutex_enter_blocking(&gTelemetryMutex);
  telemetry = gTelemetry;
  alarmEdgePending = gAlarmEdgePending;
  alarmEdgeState = gAlarmEdgeState;
  gAlarmEdgePending = false;
  mutex_exit(&gTelemetryMutex);

  telemetry.network = networkManager.getStatus();

  if (telemetry.network.activeLink != gLastLink) {
    if (telemetry.network.activeLink == LinkType::Ethernet) {
      eventLogger.log(EventType::Network, "ETH_CONNECTED");
    } else {
      eventLogger.log(EventType::Network, "NET_DISCONNECTED");
    }
    gLastLink = telemetry.network.activeLink;
  }

  if (alarmEdgePending) {
    eventLogger.log(alarmEdgeState ? EventType::Alarm : EventType::Alarm,
                    alarmEdgeState ? "ALARM_ACTIVE" : "ALARM_CLEAR");
    snmpManager.onAlarmEdge(alarmEdgeState, telemetry);
    eventLogger.log(EventType::Snmp, "SNMP_TRAP_SENT");
  }

  snmpManager.update(nowMs, telemetry);
  webManager.update(nowMs, telemetry);

  const bool measurementOk =
      (gMeasurementHeartbeatMs != 0) && ((nowMs - gMeasurementHeartbeatMs) < 2000);
  const bool bootGrace = nowMs < 5000;
  if (measurementOk || bootGrace) {
    rp2040.wdt_reset();
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("FM_SNMP_GATEWAY boot (RP2350A)");

  mutex_init(&gTelemetryMutex);
  pinMode(PinConfig::kNetworkResetPin, INPUT_PULLUP);

  configManager.begin();
  const DeviceConfig& cfg = configManager.getConfig();

  analogManager.begin(&cfg);
  fanManager.begin(
      PinConfig::kFanTachPin,
      cfg.fanPulsesPerRev,
      cfg.fanPwmEnabled,
      PinConfig::kFanPwmPin,
      cfg.fanTempStartC,
      cfg.fanTempMaxC,
      cfg.fanMinDutyPercent,
      cfg.fanMaxDutyPercent,
      cfg.fanStartupMaxSec);
  alarmManager.begin(PinConfig::kAlarmInputPin, PinConfig::kStatusInputPin, cfg.alarmActiveLow);
  networkManager.begin(&cfg);
  snmpManager.begin(&cfg, cfg.network.hostname);
  webManager.begin(&eventLogger, &configManager);

  eventLogger.log(EventType::System, "SYSTEM_BOOT");
  rp2040.wdt_begin(kWdtTimeoutMs);
  gHardwareReady = true;
}

void setup1() {
  while (!gHardwareReady) {
    delay(1);
  }
}

void loop1() {
  static uint32_t lastWakeMs = 0;
  const uint32_t nowMs = millis();
  if ((nowMs - lastWakeMs) < kMeasurementPeriodMs) {
    delay(1);
    return;
  }
  lastWakeMs = nowMs;
  runMeasurement(nowMs);
}

void loop() {
  static uint32_t lastWakeMs = 0;
  const uint32_t nowMs = millis();
  if ((nowMs - lastWakeMs) < kCommunicationPeriodMs) {
    delay(1);
    return;
  }
  lastWakeMs = nowMs;
  runCommunication(nowMs);
}
