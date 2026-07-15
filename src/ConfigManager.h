#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <vector>

#include "PinConfig.h"

enum class CalibrationMode : uint8_t {
  Linear = 0,
  Quadratic = 1,
  Lut = 2,
};

struct LutPoint {
  int16_t adc;
  float value;
};

struct CalibrationProfile {
  CalibrationMode mode = CalibrationMode::Linear;
  float k = 1.0f;
  float offset = 0.0f;
  float a = 0.0f;
  float b = 1.0f;
  float c = 0.0f;
  std::vector<LutPoint> lut;
};

struct NetworkSettings {
  bool dhcp = true;
  bool ethEnabled = false;
  bool wifiConfigAccessEnabled = true;
  uint16_t wifiConfigWindowSec = 300;
  bool forceWifiPinEnabled = true;
  bool forceWifiPinActiveLow = true;
  uint8_t forceWifiPin = PinConfig::kForceWifiPin;
  IPAddress ip = IPAddress(192, 168, 1, 50);
  IPAddress gateway = IPAddress(192, 168, 1, 1);
  IPAddress subnet = IPAddress(255, 255, 255, 0);
  IPAddress dns = IPAddress(8, 8, 8, 8);
  uint8_t ethMac[6] = {0x02, 0x46, 0x4D, 0x53, 0x4E, 0x4D};
  char hostname[32] = "fm-snmp-gateway";
  char wifiSsid[32] = "";
  char wifiPassword[64] = "";
};

struct SnmpSettings {
  char community[32] = "public";
  IPAddress trapServer = IPAddress(192, 168, 1, 100);
  uint16_t trapPort = 162;
};

struct DeviceConfig {
  NetworkSettings network;
  SnmpSettings snmp;
  CalibrationProfile calibration[4];
  bool alarmActiveLow = true;
  uint8_t fanTachPin = PinConfig::kFanTachPin;
  uint8_t fanPulsesPerRev = 2;
  bool fanPwmEnabled = true;
  uint8_t fanPwmPin = PinConfig::kFanPwmPin;
  float fanTempStartC = 30.0f;
  float fanTempMaxC = 65.0f;
  uint8_t fanMinDutyPercent = 30;
  uint8_t fanMaxDutyPercent = 100;
  uint16_t fanStartupMaxSec = 15;
  uint16_t samplePeriodMs = 50;
};

class ConfigManager {
 public:
  bool begin();
  bool load();
  bool save();

  DeviceConfig& mutableConfig();
  const DeviceConfig& getConfig() const;

 private:
  void applyDefaults();
  void applyDefaultCalibration();

  Preferences preferences_;
  DeviceConfig config_;
};
