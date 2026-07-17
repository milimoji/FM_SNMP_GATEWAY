#include "FanManager.h"

namespace {
constexpr uint32_t kFanPwmFreqHz = 25000;
// Fan specification: below 30% PWM duty the fan is not expected to rotate.
constexpr uint8_t kTachCheckDutyPercent = 30;
constexpr uint32_t kNoTachTimeoutMs = 10000;
constexpr uint16_t kFanMaxRpm = 10000;
constexpr uint8_t kRpmHeadroomPercent = 10;
// Keep a floor to avoid accepting unrealistically dense glitches even if config changes.
constexpr uint32_t kMinPulseIntervalFloorUs = 800;
constexpr float kRpmEmaAlpha = 0.25f;
}

FanManager* FanManager::instance_ = nullptr;

bool FanManager::begin(
    uint8_t tachPin,
    uint8_t pulsesPerRev,
    bool pwmEnabled,
    uint8_t pwmPin,
    float tempStartC,
    float tempMaxC,
    uint8_t minDutyPercent,
    uint8_t maxDutyPercent,
    uint16_t startupMaxSec) {
  tachPin_ = tachPin;
  pulsesPerRev_ = pulsesPerRev == 0 ? 1 : pulsesPerRev;
  maxValidRpm_ = static_cast<uint16_t>(
      kFanMaxRpm + ((static_cast<uint32_t>(kFanMaxRpm) * kRpmHeadroomPercent) / 100U));
  const uint32_t maxPulseRatePerMin = static_cast<uint32_t>(maxValidRpm_) * pulsesPerRev_;
  if (maxPulseRatePerMin > 0) {
    minPulseIntervalUs_ = 60000000UL / maxPulseRatePerMin;
  }
  if (minPulseIntervalUs_ < kMinPulseIntervalFloorUs) {
    minPulseIntervalUs_ = kMinPulseIntervalFloorUs;
  }
  pwmEnabled_ = pwmEnabled;
  pwmPin_ = pwmPin;
  tempStartC_ = tempStartC;
  tempMaxC_ = tempMaxC;
  minDutyPercent_ = minDutyPercent;
  maxDutyPercent_ = maxDutyPercent;
  startupMaxSec_ = startupMaxSec;
  baseHealthStatus_ = FanHealthStatus::Ok;
  healthStatus_ = FanHealthStatus::Ok;

  bool clamped = false;
  if (minDutyPercent_ > 100) {
    minDutyPercent_ = 100;
    clamped = true;
  }
  if (maxDutyPercent_ > 100) {
    maxDutyPercent_ = 100;
    clamped = true;
  }
  if (maxDutyPercent_ < minDutyPercent_) {
    maxDutyPercent_ = minDutyPercent_;
    clamped = true;
  }
  if (tempMaxC_ <= tempStartC_) {
    tempMaxC_ = tempStartC_;
    clamped = true;
  }

  if (startupMaxSec_ > 120) {
    startupMaxSec_ = 120;
    clamped = true;
  }

  if (clamped) {
    baseHealthStatus_ = FanHealthStatus::Clamped;
    healthStatus_ = baseHealthStatus_;
  }

  pinMode(tachPin_, INPUT_PULLUP);
  instance_ = this;
  attachInterrupt(digitalPinToInterrupt(tachPin_), isrThunk, FALLING);

  bootMs_ = millis();
  noTachSinceMs_ = 0;

  if (pwmEnabled_) {
    ledcSetup(pwmChannel_, kFanPwmFreqHz, pwmResolutionBits_);
    ledcAttachPin(pwmPin_, pwmChannel_);
    applyDutyPercent(100);
  }

  lastUpdateMs_ = bootMs_;
  return true;
}

void FanManager::update(uint32_t nowMs, float temperatureC) {
  if (pwmEnabled_) {
    applyDutyPercent(computeDutyPercent(nowMs, temperatureC));
  }

  const uint32_t elapsed = nowMs - lastUpdateMs_;
  if (elapsed < 1000) {
    return;
  }

  const uint32_t totalPulses = pulseCounter_;
  const uint32_t deltaPulses = totalPulses - lastPulseCounter_;
  lastPulseCounter_ = totalPulses;
  lastUpdateMs_ = nowMs;

  const uint32_t pulsesPerMinute = (deltaPulses * 60000UL) / elapsed;
  uint16_t instantRpm = static_cast<uint16_t>(pulsesPerMinute / pulsesPerRev_);
  if (instantRpm > maxValidRpm_) {
    instantRpm = maxValidRpm_;
  }

  if (!rpmFilterInitialized_) {
    filteredRpm_ = static_cast<float>(instantRpm);
    rpmFilterInitialized_ = true;
  } else {
    filteredRpm_ += kRpmEmaAlpha * (static_cast<float>(instantRpm) - filteredRpm_);
  }

  if (instantRpm == 0 && filteredRpm_ < 1.0f) {
    filteredRpm_ = 0.0f;
  }

  rpm_ = static_cast<uint16_t>(filteredRpm_ + 0.5f);

  const uint32_t startupMs = static_cast<uint32_t>(startupMaxSec_) * 1000UL;
  if (nowMs - bootMs_ < startupMs) {
    healthStatus_ = baseHealthStatus_;
    noTachSinceMs_ = 0;
    return;
  }

  if (dutyPercent_ >= kTachCheckDutyPercent && rpm_ == 0) {
    if (noTachSinceMs_ == 0) {
      noTachSinceMs_ = nowMs;
    }
    if (nowMs - noTachSinceMs_ >= kNoTachTimeoutMs) {
      healthStatus_ = FanHealthStatus::TachMissing;
      return;
    }
  } else {
    noTachSinceMs_ = 0;
  }

  healthStatus_ = baseHealthStatus_;
}

uint16_t FanManager::getRpm() const {
  return rpm_;
}

uint8_t FanManager::getDutyPercent() const {
  return dutyPercent_;
}

uint8_t FanManager::getHealthStatusCode() const {
  return static_cast<uint8_t>(healthStatus_);
}

const char* FanManager::getHealthStatusText() const {
  switch (healthStatus_) {
    case FanHealthStatus::Ok:
      return "FAN_CFG_OK";
    case FanHealthStatus::Clamped:
      return "FAN_CFG_CLAMPED";
    case FanHealthStatus::TachMissing:
      return "FAN_TACH_MISSING";
    default:
      return "FAN_STATUS_UNKNOWN";
  }
}

void IRAM_ATTR FanManager::isrThunk() {
  if (instance_ != nullptr) {
    instance_->onPulse();
  }
}

void FanManager::onPulse() {
  const uint32_t nowUs = micros();
  if ((nowUs - lastPulseUs_) < minPulseIntervalUs_) {
    return;
  }
  lastPulseUs_ = nowUs;
  ++pulseCounter_;
}

void FanManager::applyDutyPercent(uint8_t dutyPercent) {
  dutyPercent_ = dutyPercent > 100 ? 100 : dutyPercent;

  const uint32_t maxDuty = (1U << pwmResolutionBits_) - 1U;
  const uint32_t duty = (static_cast<uint32_t>(dutyPercent_) * maxDuty) / 100U;
  ledcWrite(pwmChannel_, duty);
}

uint8_t FanManager::computeDutyPercent(uint32_t nowMs, float temperatureC) const {
  if (!pwmEnabled_) {
    return 0;
  }

  const uint32_t startupMs = static_cast<uint32_t>(startupMaxSec_) * 1000UL;
  if (nowMs - bootMs_ < startupMs) {
    return 100;
  }

  if (temperatureC <= tempStartC_) {
    return minDutyPercent_;
  }
  if (temperatureC >= tempMaxC_) {
    return maxDutyPercent_;
  }

  const float ratio = (temperatureC - tempStartC_) / (tempMaxC_ - tempStartC_);
  const float mapped = minDutyPercent_ + ratio * (maxDutyPercent_ - minDutyPercent_);
  if (mapped <= 0.0f) {
    return 0;
  }
  if (mapped >= 100.0f) {
    return 100;
  }
  return static_cast<uint8_t>(mapped + 0.5f);
}

