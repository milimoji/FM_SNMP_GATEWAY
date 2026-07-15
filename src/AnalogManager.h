#pragma once

#include <Adafruit_ADS1X15.h>
#include <Arduino.h>

#include "ConfigManager.h"

struct AnalogValues {
  float forward = 0.0f;
  float reflect = 0.0f;
  float temperature = 0.0f;
  float deviation = 0.0f;
};

class AnalogManager {
 public:
  bool begin(const DeviceConfig* cfg);
  void update(uint32_t nowMs);

  AnalogValues getValues() const;
  int16_t getRawAdc(uint8_t channel) const;

 private:
    int16_t readAdcChannel(uint8_t channel);
  float applyCalibration(const CalibrationProfile& profile, int16_t adc) const;
  float applyLutInterpolation(const CalibrationProfile& profile, int16_t adc) const;

  const DeviceConfig* cfg_ = nullptr;
    Adafruit_ADS1115 ads_;
    bool adsReady_ = false;
  int16_t rawAdc_[4] = {0, 0, 0, 0};
  float calibrated_[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float filtered_[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  uint32_t lastSampleMs_ = 0;
  uint32_t lastDiagMs_ = 0;
};
