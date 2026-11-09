#pragma once

#include <Arduino.h>

namespace PinConfig {
// Raspberry Pi Pico 2 (RP2350A) pin map.
// SPI0 defaults: GP16 MISO, GP17 CS, GP18 SCK, GP19 MOSI.
// I2C0 defaults: GP4 SDA, GP5 SCL.
// Leave GP0/GP1 for Serial, and GP23/GP24/GP25/GP29 for Pico onboard functions.

constexpr uint8_t kAlarmInputPin = 6;
constexpr uint8_t kStatusInputPin = 7;

constexpr uint8_t kAdsSdaPin = 4;
constexpr uint8_t kAdsSclPin = 5;

constexpr uint8_t kW5500MisoPin = 16;
constexpr uint8_t kW5500CsPin = 17;
constexpr uint8_t kW5500SckPin = 18;
constexpr uint8_t kW5500MosiPin = 19;
constexpr uint8_t kW5500RstPin = 20;
constexpr uint8_t kW5500IntPin = 21;

constexpr uint8_t kNetworkResetPin = 22;

constexpr uint8_t kFanTachPin = 26;
constexpr uint8_t kFanPwmPin = 27;
}  // namespace PinConfig
