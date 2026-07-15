#include <Arduino.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "AlarmManager.h"
#include "AnalogManager.h"
#include "ConfigManager.h"
#include "EventLogger.h"
#include "FanManager.h"
#include "PinConfig.h"
#include "NetworkManager.h"
#include "OTAManager.h"
#include "SNMPManager.h"
#include "WebManager.h"

namespace {
constexpr uint32_t kWdtTimeoutSec = 10;
constexpr uint16_t MEASUREMENT_PERIOD_MS = 50;
constexpr uint16_t COMMUNICATION_PERIOD_MS = 50;

ConfigManager configManager;
AnalogManager analogManager;
AlarmManager alarmManager;
NetworkManager networkManager;
SNMPManager snmpManager;
WebManager webManager;
OTAManager otaManager;
EventLogger eventLogger;
FanManager fanManager;

SnmpTelemetry gTelemetry;
bool gAlarmEdgePending = false;
bool gAlarmEdgeState = false;
LinkType gLastLink = LinkType::None;
portMUX_TYPE gTelemetryMux = portMUX_INITIALIZER_UNLOCKED;

void measurementTask(void* parameter) {
  (void)parameter;
  esp_task_wdt_add(nullptr);

  TickType_t lastWake = xTaskGetTickCount();
  while (true) {
    const uint32_t nowMs = millis();
    analogManager.update(nowMs);
    const AnalogValues analog = analogManager.getValues();
    fanManager.update(nowMs, analog.temperature);
    alarmManager.update(nowMs);

    const AlarmState alarm = alarmManager.getState();

    portENTER_CRITICAL(&gTelemetryMux);
    gTelemetry.analog = analog;
    gTelemetry.alarm = alarm;
    gTelemetry.fanRpm = fanManager.getRpm();
    gTelemetry.fanDutyPercent = fanManager.getDutyPercent();
    gTelemetry.fanHealthStatus = fanManager.getHealthStatusCode();
    gTelemetry.uptimeSec = nowMs / 1000;
    if (alarm.alarmChanged) {
      gAlarmEdgePending = true;
      gAlarmEdgeState = alarm.alarmActive;
    }
    portEXIT_CRITICAL(&gTelemetryMux);

    esp_task_wdt_reset();
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(MEASUREMENT_PERIOD_MS));
  }
}

void communicationTask(void* parameter) {
  (void)parameter;
  esp_task_wdt_add(nullptr);

  TickType_t lastWake = xTaskGetTickCount();
  while (true) {
    const uint32_t nowMs = millis();
    networkManager.update(nowMs);

    SnmpTelemetry telemetry;
    bool alarmEdgePending = false;
    bool alarmEdgeState = false;
    portENTER_CRITICAL(&gTelemetryMux);
    telemetry = gTelemetry;
    alarmEdgePending = gAlarmEdgePending;
    alarmEdgeState = gAlarmEdgeState;
    gAlarmEdgePending = false;
    portEXIT_CRITICAL(&gTelemetryMux);

    telemetry.network = networkManager.getStatus();

    if (telemetry.network.activeLink != gLastLink) {
      if (telemetry.network.activeLink == LinkType::Ethernet) {
        eventLogger.log(EventType::Network, "ETH_CONNECTED");
      } else if (telemetry.network.activeLink == LinkType::WiFi) {
        eventLogger.log(EventType::Network, "WIFI_CONNECTED");
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
    otaManager.update(telemetry.network);

    esp_task_wdt_reset();
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(COMMUNICATION_PERIOD_MS));
  }
}
}

void setup() {
  Serial.begin(115200);
  Serial.println("FM_SNMP_GATEWAY boot");

  configManager.begin();
  const DeviceConfig& cfg = configManager.getConfig();

  esp_task_wdt_init(kWdtTimeoutSec, true);

  analogManager.begin(&cfg);
  fanManager.begin(
      cfg.fanTachPin,
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
  otaManager.begin(cfg.network.hostname);

  eventLogger.log(EventType::System, "SYSTEM_BOOT");

  xTaskCreatePinnedToCore(measurementTask, "MeasurementTask", 4096, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(communicationTask, "CommunicationTask", 6144, nullptr, 2, nullptr, 0);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
