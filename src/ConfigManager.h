#pragma once

#include <Arduino.h>
#include <IPAddress.h>
#include <vector>

#include "PinConfig.h"

enum class CalibrationMode : uint8_t {
  Linear = 0,
  Quadratic = 1,
  Lut = 2,
};

struct LutPoint {
  float inputVoltage;
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
  IPAddress ip = IPAddress(192, 168, 1, 50);
  IPAddress gateway = IPAddress(192, 168, 1, 1);
  IPAddress subnet = IPAddress(255, 255, 255, 0);
  IPAddress dns = IPAddress(8, 8, 8, 8);
  char hostname[32] = "fm-snmp-gateway";
};

struct SnmpSettings {
  char community[32] = "public";
  IPAddress trapServer = IPAddress(192, 168, 1, 100);
  uint16_t trapPort = 162;
  uint32_t freqKHz = 0;
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
  void resetNetworkToDefaults();

  DeviceConfig& mutableConfig();
  const DeviceConfig& getConfig() const;

 private:
  void applyDefaults();
  void applyDefaultCalibration();

  DeviceConfig config_;
};
