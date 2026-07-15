#pragma once

#include <Arduino.h>

#include "NetworkManager.h"

class OTAManager {
 public:
  bool begin(const char* hostname);
  void update(const NetworkStatus& networkStatus);

 private:
  bool ready_ = false;
  bool hooksConfigured_ = false;
  String hostname_;
};
