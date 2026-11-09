#include "AnalogManager.h"
#include "PinConfig.h"

#include <Wire.h>
#include <math.h>

namespace {
constexpr float kAdsFullScaleV = 4.096f;
constexpr float kAdsMaxCode = 32767.0f;
constexpr float kDividerTopOhm = 4700.0f;
constexpr float kDividerBottomOhm = 10000.0f;
constexpr float kDividerInputOverAdc = (kDividerTopOhm + kDividerBottomOhm) / kDividerBottomOhm;
}

bool AnalogManager::begin(const DeviceConfig* cfg) {
  cfg_ = cfg;
  lastSampleMs_ = 0;
  lastDiagMs_ = 0;
  Wire.setSDA(PinConfig::kAdsSdaPin);
  Wire.setSCL(PinConfig::kAdsSclPin);
  Wire.begin();

  adsReady_ = ads_.begin();
  if (adsReady_) {
    ads_.setGain(GAIN_ONE);
    ads_.setDataRate(RATE_ADS1115_128SPS);
    Serial.println("ADS1115 detected, using hardware ADC");
  } else {
    Serial.println("WARN: ADS1115 not detected, using zero ADC fallback");
  }

  return cfg_ != nullptr;
}

void AnalogManager::update(uint32_t nowMs) {
  if (cfg_ == nullptr) {
    return;
  }

  const uint16_t period = max<uint16_t>(20, cfg_->samplePeriodMs);
  if (nowMs - lastSampleMs_ < period) {
    return;
  }
  lastSampleMs_ = nowMs;

  for (uint8_t i = 0; i < 4; ++i) {
    rawAdc_[i] = readAdcChannel(i);
    const float inputVoltage = adcToInputVoltage(rawAdc_[i]);
    calibrated_[i] = applyCalibration(cfg_->calibration[i], inputVoltage);
    if (calibrated_[i] < 0.0f) {
      calibrated_[i] = 0.0f;
    }
    const float alpha = 0.15f;
    filtered_[i] = filtered_[i] + alpha * (calibrated_[i] - filtered_[i]);
    if (filtered_[i] < 0.0f) {
      filtered_[i] = 0.0f;
    }
  }

  if (nowMs - lastDiagMs_ >= 10000) {
    lastDiagMs_ = nowMs;
    Serial.printf(
      "ADC ch0 raw=%d vin=%.3f cal=%.2f filt=%.2f | ch1 raw=%d vin=%.3f cal=%.2f filt=%.2f | ch2 raw=%d vin=%.3f cal=%.2f filt=%.2f | ch3 raw=%d vin=%.3f cal=%.2f filt=%.2f adsReady=%d\n",
      rawAdc_[0], adcToInputVoltage(rawAdc_[0]), calibrated_[0], filtered_[0],
      rawAdc_[1], adcToInputVoltage(rawAdc_[1]), calibrated_[1], filtered_[1],
      rawAdc_[2], adcToInputVoltage(rawAdc_[2]), calibrated_[2], filtered_[2],
      rawAdc_[3], adcToInputVoltage(rawAdc_[3]), calibrated_[3], filtered_[3],
      adsReady_ ? 1 : 0);
  }
}

AnalogValues AnalogManager::getValues() const {
  AnalogValues values;
  values.forward = filtered_[0] < 0.0f ? 0.0f : filtered_[0];
  values.reflect = filtered_[1] < 0.0f ? 0.0f : filtered_[1];
  values.temperature = filtered_[2] < 0.0f ? 0.0f : filtered_[2];
  values.deviation = filtered_[3] < 0.0f ? 0.0f : filtered_[3];
  return values;
}

int16_t AnalogManager::getRawAdc(uint8_t channel) const {
  if (channel > 3) {
    return 0;
  }
  return rawAdc_[channel];
}

int16_t AnalogManager::readAdcChannel(uint8_t channel) {
  if (adsReady_ && channel < 4) {
    return ads_.readADC_SingleEnded(channel);
  }

  return 0;
}

float AnalogManager::adcToInputVoltage(int16_t adc) const {
  if (adc <= 0) {
    return 0.0f;
  }

  const float adcVoltage = (static_cast<float>(adc) * kAdsFullScaleV) / kAdsMaxCode;
  return adcVoltage * kDividerInputOverAdc;
}

float AnalogManager::applyCalibration(const CalibrationProfile& profile, float inputVoltage) const {
  const float x = inputVoltage;

  switch (profile.mode) {
    case CalibrationMode::Linear:
      return profile.k * x + profile.offset;
    case CalibrationMode::Quadratic:
      return profile.a * x * x + profile.b * x + profile.c;
    case CalibrationMode::Lut:
      return applyLutInterpolation(profile, inputVoltage);
    default:
      return x;
  }
}

float AnalogManager::applyLutInterpolation(const CalibrationProfile& profile, float inputVoltage) const {
  if (profile.lut.empty()) {
    return inputVoltage;
  }

  if (inputVoltage <= profile.lut.front().inputVoltage) {
    return profile.lut.front().value;
  }

  if (inputVoltage >= profile.lut.back().inputVoltage) {
    return profile.lut.back().value;
  }

  for (size_t i = 1; i < profile.lut.size(); ++i) {
    const LutPoint& left = profile.lut[i - 1];
    const LutPoint& right = profile.lut[i];
    if (inputVoltage <= right.inputVoltage) {
      const float dx = right.inputVoltage - left.inputVoltage;
      if (dx <= 0.0f) {
        return right.value;
      }
      const float ratio = (inputVoltage - left.inputVoltage) / dx;
      return left.value + ratio * (right.value - left.value);
    }
  }

  return profile.lut.back().value;
}
