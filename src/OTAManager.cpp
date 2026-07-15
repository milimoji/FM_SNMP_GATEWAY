#include "OTAManager.h"

#include <ArduinoOTA.h>

bool OTAManager::begin(const char* hostname) {
  hostname_ = hostname == nullptr ? "fm-snmp-gateway" : hostname;

  if (!hooksConfigured_) {
    ArduinoOTA.onStart([]() {
      Serial.println("OTA start");
    });

    ArduinoOTA.onEnd([]() {
      Serial.println("OTA end");
    });

    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("OTA error: %u\n", static_cast<unsigned>(error));
    });
    hooksConfigured_ = true;
  }

  ready_ = false;
  return true;
}

void OTAManager::update(const NetworkStatus& networkStatus) {
  if (!ready_) {
    if (networkStatus.activeLink == LinkType::None || networkStatus.ip == IPAddress(0, 0, 0, 0)) {
      return;
    }

    ArduinoOTA.setHostname(hostname_.c_str());
    ArduinoOTA.begin();
    ready_ = true;
    Serial.printf("OTA: started on %s via %s\n",
                  networkStatus.ip.toString().c_str(),
                  networkStatus.activeLink == LinkType::Ethernet ? "ETH" : "WiFi");
  }

  if (!ready_) {
    return;
  }

  ArduinoOTA.handle();
}
