#include "EventLogger.h"

#include <stdio.h>
#include <string.h>

void EventLogger::log(EventType type, const char* message) {
  EventRecord& slot = records_[head_];
  slot.timeMs = millis();
  slot.type = type;
  snprintf(slot.text, sizeof(slot.text), "%s", message == nullptr ? "" : message);

  head_ = (head_ + 1) % kCapacity;
  if (count_ < kCapacity) {
    ++count_;
  }
}

size_t EventLogger::count() const {
  return count_;
}

bool EventLogger::getRecent(size_t indexFromNewest, EventRecord& out) const {
  if (indexFromNewest >= count_) {
    return false;
  }

  size_t newest = (head_ + kCapacity - 1) % kCapacity;
  size_t pos = (newest + kCapacity - indexFromNewest) % kCapacity;
  out = records_[pos];
  return true;
}
