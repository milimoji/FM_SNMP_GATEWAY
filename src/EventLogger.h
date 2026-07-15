#pragma once

#include <Arduino.h>

#include <array>

enum class EventType : uint8_t {
  System = 0,
  Network = 1,
  Alarm = 2,
  Snmp = 3,
  Ota = 4,
};

struct EventRecord {
  uint32_t timeMs;
  EventType type;
  char text[64];
};

class EventLogger {
 public:
  void log(EventType type, const char* message);
  size_t count() const;
  bool getRecent(size_t indexFromNewest, EventRecord& out) const;

 private:
  static constexpr size_t kCapacity = 32;
  std::array<EventRecord, kCapacity> records_{};
  size_t head_ = 0;
  size_t count_ = 0;
};
