#pragma once

#include <Arduino.h>

struct AlarmState {
  bool alarmActive = false;
  bool statusInput = false;
  bool changed = false;
  bool alarmChanged = false;
};

class AlarmManager {
 public:
  bool begin(uint8_t alarmPin, uint8_t statusPin, bool activeLow);
  void update(uint32_t nowMs);

  AlarmState getState() const;

 private:
  bool readInput(uint8_t pin, bool activeLow) const;

  uint8_t alarmPin_ = 255;
  uint8_t statusPin_ = 255;
  bool activeLow_ = true;

  bool debouncedAlarm_ = false;
  bool debouncedStatus_ = false;
  bool changed_ = false;
  bool alarmChanged_ = false;

  bool rawAlarmLast_ = false;
  bool rawStatusLast_ = false;
  uint32_t alarmLastChangeMs_ = 0;
  uint32_t statusLastChangeMs_ = 0;

  static constexpr uint32_t kDebounceMs = 30;
};
