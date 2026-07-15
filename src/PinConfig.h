#pragma once

#include <Arduino.h>

namespace PinConfig {
// Digital input used for an external alarm contact or fault line.
constexpr uint8_t kAlarmInputPin = 34;
// Second digital input for a device status contact, ready/ok line, or auxiliary state signal.
// AlarmManager reads it separately from the alarm input so both signals can be debounced and reported.
constexpr uint8_t kStatusInputPin = 35;
// I2C pins for the ADS1115 ADC.
constexpr uint8_t kAdsSdaPin = 21;
constexpr uint8_t kAdsSclPin = 22;
// W5500 Ethernet SPI pins.
constexpr uint8_t kW5500SckPin = 18;
constexpr uint8_t kW5500MisoPin = 19;
constexpr uint8_t kW5500MosiPin = 23;
constexpr uint8_t kW5500CsPin = 5;
constexpr int8_t kW5500RstPin = 32;
// Jumper/input that forces WiFi configuration mode on boot.
constexpr uint8_t kForceWifiPin = 27;
// Fan tachometer input and PWM output.
constexpr uint8_t kFanTachPin = 26;
constexpr uint8_t kFanPwmPin = 25;
}  // namespace PinConfig
