#pragma once

#include <Arduino.h>

enum class FanHealthStatus : uint8_t {
  Ok = 0,
  Clamped = 1,
  TachMissing = 2,
};

class FanManager {
 public:
  bool begin(
      uint8_t tachPin,
      uint8_t pulsesPerRev,
      bool pwmEnabled,
      uint8_t pwmPin,
      float tempStartC,
      float tempMaxC,
      uint8_t minDutyPercent,
      uint8_t maxDutyPercent,
      uint16_t startupMaxSec);
  void update(uint32_t nowMs, float temperatureC);

  uint16_t getRpm() const;
  uint8_t getDutyPercent() const;
  uint8_t getHealthStatusCode() const;
  const char* getHealthStatusText() const;

 private:
  static void IRAM_ATTR isrThunk();
  void onPulse();
  void applyDutyPercent(uint8_t dutyPercent);
  uint8_t computeDutyPercent(uint32_t nowMs, float temperatureC) const;

  static FanManager* instance_;

  volatile uint32_t pulseCounter_ = 0;
  volatile uint32_t lastPulseUs_ = 0;
  uint32_t lastPulseCounter_ = 0;
  uint32_t lastUpdateMs_ = 0;
  uint8_t tachPin_ = 255;
  uint8_t pulsesPerRev_ = 2;
  uint16_t rpm_ = 0;
  uint16_t maxValidRpm_ = 11000;
  uint32_t minPulseIntervalUs_ = 1500;
  float filteredRpm_ = 0.0f;
  bool rpmFilterInitialized_ = false;

  bool pwmEnabled_ = false;
  uint8_t pwmPin_ = 255;
  uint8_t pwmChannel_ = 0;
  uint8_t pwmResolutionBits_ = 8;
  uint8_t dutyPercent_ = 0;
  uint32_t bootMs_ = 0;
  uint32_t noTachSinceMs_ = 0;

  float tempStartC_ = 30.0f;
  float tempMaxC_ = 65.0f;
  uint8_t minDutyPercent_ = 30;
  uint8_t maxDutyPercent_ = 100;
  uint16_t startupMaxSec_ = 15;

  FanHealthStatus baseHealthStatus_ = FanHealthStatus::Ok;
  FanHealthStatus healthStatus_ = FanHealthStatus::Ok;
};
