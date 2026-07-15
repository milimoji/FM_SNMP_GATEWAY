#include "AlarmManager.h"

bool AlarmManager::begin(uint8_t alarmPin, uint8_t statusPin, bool activeLow) {
  alarmPin_ = alarmPin;
  statusPin_ = statusPin;
  activeLow_ = activeLow;

  pinMode(alarmPin_, INPUT_PULLUP);
  pinMode(statusPin_, INPUT_PULLUP);

  rawAlarmLast_ = readInput(alarmPin_, activeLow_);
  rawStatusLast_ = readInput(statusPin_, activeLow_);
  debouncedAlarm_ = rawAlarmLast_;
  debouncedStatus_ = rawStatusLast_;
  return true;
}

void AlarmManager::update(uint32_t nowMs) {
  changed_ = false;
  alarmChanged_ = false;

  const bool rawAlarm = readInput(alarmPin_, activeLow_);
  if (rawAlarm != rawAlarmLast_) {
    rawAlarmLast_ = rawAlarm;
    alarmLastChangeMs_ = nowMs;
  }
  if ((nowMs - alarmLastChangeMs_) >= kDebounceMs && debouncedAlarm_ != rawAlarmLast_) {
    debouncedAlarm_ = rawAlarmLast_;
    changed_ = true;
    alarmChanged_ = true;
  }

  const bool rawStatus = readInput(statusPin_, activeLow_);
  if (rawStatus != rawStatusLast_) {
    rawStatusLast_ = rawStatus;
    statusLastChangeMs_ = nowMs;
  }
  if ((nowMs - statusLastChangeMs_) >= kDebounceMs && debouncedStatus_ != rawStatusLast_) {
    debouncedStatus_ = rawStatusLast_;
    changed_ = true;
  }
}

AlarmState AlarmManager::getState() const {
  AlarmState state;
  state.alarmActive = debouncedAlarm_;
  state.statusInput = debouncedStatus_;
  state.changed = changed_;
  state.alarmChanged = alarmChanged_;
  return state;
}

bool AlarmManager::readInput(uint8_t pin, bool activeLow) const {
  const bool level = digitalRead(pin) == HIGH;
  return activeLow ? !level : level;
}
