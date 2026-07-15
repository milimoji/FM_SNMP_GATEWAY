#include "ConfigManager.h"

namespace {
const char* kPrefsNamespace = "fm_snmp_cfg";
constexpr char kFixedWifiSsid[] = "FM_GATEWAY_AP";
constexpr char kFixedWifiPassword[] = "fm_gateway_2026";

uint32_t ipToU32(const IPAddress& ip) {
  return (static_cast<uint32_t>(ip[0]) << 24) | (static_cast<uint32_t>(ip[1]) << 16) |
         (static_cast<uint32_t>(ip[2]) << 8) | static_cast<uint32_t>(ip[3]);
}

IPAddress u32ToIp(uint32_t v) {
  return IPAddress((v >> 24) & 0xFF, (v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
}

String calibrationPrefix(uint8_t ch) {
  return String("c") + String(ch) + "_";
}

void applyFixedWifi(NetworkSettings& network) {
  strncpy(network.wifiSsid, kFixedWifiSsid, sizeof(network.wifiSsid) - 1);
  network.wifiSsid[sizeof(network.wifiSsid) - 1] = '\0';
  strncpy(network.wifiPassword, kFixedWifiPassword, sizeof(network.wifiPassword) - 1);
  network.wifiPassword[sizeof(network.wifiPassword) - 1] = '\0';
}

String serializeLut(const std::vector<LutPoint>& lut) {
  String out;
  for (size_t i = 0; i < lut.size(); ++i) {
    if (i > 0) {
      out += ";";
    }
    out += String(lut[i].adc);
    out += ":";
    out += String(lut[i].value, 4);
  }
  return out;
}

void deserializeLut(const String& text, std::vector<LutPoint>& lut) {
  lut.clear();
  int start = 0;

  while (start < text.length()) {
    int end = text.indexOf(';', start);
    if (end < 0) {
      end = text.length();
    }

    const String token = text.substring(start, end);
    const int sep = token.indexOf(':');
    if (sep > 0) {
      const int16_t adc = static_cast<int16_t>(token.substring(0, sep).toInt());
      const float value = token.substring(sep + 1).toFloat();
      lut.push_back({adc, value});
    }

    start = end + 1;
  }
}
}

bool ConfigManager::begin() {
  applyDefaults();
  return load();
}

bool ConfigManager::load() {
  if (!preferences_.begin(kPrefsNamespace, true)) {
    return false;
  }

  config_.network.dhcp = preferences_.getBool("dhcp", config_.network.dhcp);
  config_.network.ethEnabled = preferences_.getBool("eth_en", config_.network.ethEnabled);
  config_.network.wifiConfigAccessEnabled =
      preferences_.getBool("wifi_cfg_en", config_.network.wifiConfigAccessEnabled);
  config_.network.wifiConfigWindowSec =
      preferences_.getUShort("wifi_cfg_win", config_.network.wifiConfigWindowSec);
  config_.network.forceWifiPinEnabled =
      preferences_.getBool("wifi_force_en", config_.network.forceWifiPinEnabled);
  config_.network.forceWifiPinActiveLow =
      preferences_.getBool("wifi_force_low", config_.network.forceWifiPinActiveLow);
  config_.network.forceWifiPin =
      preferences_.getUChar("wifi_force_pin", config_.network.forceWifiPin);
  config_.fanTachPin = preferences_.getUChar("fan_pin", config_.fanTachPin);
  config_.fanPulsesPerRev = preferences_.getUChar("fan_ppr", config_.fanPulsesPerRev);
  config_.fanPwmEnabled = preferences_.getBool("fan_pwm_en", config_.fanPwmEnabled);
  config_.fanPwmPin = preferences_.getUChar("fan_pwm_pin", config_.fanPwmPin);
  config_.fanTempStartC = preferences_.getFloat("fan_t_start", config_.fanTempStartC);
  config_.fanTempMaxC = preferences_.getFloat("fan_t_max", config_.fanTempMaxC);
  config_.fanMinDutyPercent = preferences_.getUChar("fan_duty_min", config_.fanMinDutyPercent);
  config_.fanMaxDutyPercent = preferences_.getUChar("fan_duty_max", config_.fanMaxDutyPercent);
  config_.fanStartupMaxSec =
      preferences_.getUShort("fan_start_max_s", config_.fanStartupMaxSec);
  config_.network.ip = u32ToIp(preferences_.getULong("ip", ipToU32(config_.network.ip)));
  config_.network.gateway =
      u32ToIp(preferences_.getULong("gw", ipToU32(config_.network.gateway)));
  config_.network.subnet =
      u32ToIp(preferences_.getULong("subnet", ipToU32(config_.network.subnet)));
  config_.network.dns = u32ToIp(preferences_.getULong("dns", ipToU32(config_.network.dns)));
  config_.snmp.trapServer =
      u32ToIp(preferences_.getULong("trap_ip", ipToU32(config_.snmp.trapServer)));
  config_.snmp.trapPort = preferences_.getUShort("trap_port", config_.snmp.trapPort);
  config_.samplePeriodMs = preferences_.getUShort("samp_ms", config_.samplePeriodMs);
  config_.alarmActiveLow = preferences_.getBool("alm_low", config_.alarmActiveLow);

  String comm = preferences_.getString("community", config_.snmp.community);
  comm.toCharArray(config_.snmp.community, sizeof(config_.snmp.community));

  String host = preferences_.getString("hostname", config_.network.hostname);
  host.toCharArray(config_.network.hostname, sizeof(config_.network.hostname));

  if (preferences_.isKey("eth_mac")) {
    preferences_.getBytes("eth_mac", config_.network.ethMac, sizeof(config_.network.ethMac));
  }

  for (uint8_t ch = 0; ch < 4; ++ch) {
    CalibrationProfile& p = config_.calibration[ch];
    const String prefix = calibrationPrefix(ch);

    p.mode = static_cast<CalibrationMode>(
        preferences_.getUChar((prefix + "mode").c_str(), static_cast<uint8_t>(p.mode)));
    p.k = preferences_.getFloat((prefix + "k").c_str(), p.k);
    p.offset = preferences_.getFloat((prefix + "off").c_str(), p.offset);
    p.a = preferences_.getFloat((prefix + "a").c_str(), p.a);
    p.b = preferences_.getFloat((prefix + "b").c_str(), p.b);
    p.c = preferences_.getFloat((prefix + "c").c_str(), p.c);

    const String lutKey = prefix + "lut";
    if (preferences_.isKey(lutKey.c_str())) {
      deserializeLut(preferences_.getString(lutKey.c_str(), ""), p.lut);
    }
  }

  preferences_.end();
  applyFixedWifi(config_.network);
  return true;
}

bool ConfigManager::save() {
  if (!preferences_.begin(kPrefsNamespace, false)) {
    return false;
  }

  preferences_.putBool("dhcp", config_.network.dhcp);
  preferences_.putBool("eth_en", config_.network.ethEnabled);
  preferences_.putBool("wifi_cfg_en", config_.network.wifiConfigAccessEnabled);
  preferences_.putUShort("wifi_cfg_win", config_.network.wifiConfigWindowSec);
  preferences_.putBool("wifi_force_en", config_.network.forceWifiPinEnabled);
  preferences_.putBool("wifi_force_low", config_.network.forceWifiPinActiveLow);
  preferences_.putUChar("wifi_force_pin", config_.network.forceWifiPin);
  preferences_.putUChar("fan_pin", config_.fanTachPin);
  preferences_.putUChar("fan_ppr", config_.fanPulsesPerRev);
  preferences_.putBool("fan_pwm_en", config_.fanPwmEnabled);
  preferences_.putUChar("fan_pwm_pin", config_.fanPwmPin);
  preferences_.putFloat("fan_t_start", config_.fanTempStartC);
  preferences_.putFloat("fan_t_max", config_.fanTempMaxC);
  preferences_.putUChar("fan_duty_min", config_.fanMinDutyPercent);
  preferences_.putUChar("fan_duty_max", config_.fanMaxDutyPercent);
  preferences_.putUShort("fan_start_max_s", config_.fanStartupMaxSec);
  preferences_.putULong("ip", ipToU32(config_.network.ip));
  preferences_.putULong("gw", ipToU32(config_.network.gateway));
  preferences_.putULong("subnet", ipToU32(config_.network.subnet));
  preferences_.putULong("dns", ipToU32(config_.network.dns));
  preferences_.putULong("trap_ip", ipToU32(config_.snmp.trapServer));
  preferences_.putUShort("trap_port", config_.snmp.trapPort);
  preferences_.putUShort("samp_ms", config_.samplePeriodMs);
  preferences_.putBool("alm_low", config_.alarmActiveLow);
  preferences_.putString("community", config_.snmp.community);
  preferences_.putString("hostname", config_.network.hostname);
  preferences_.putBytes("eth_mac", config_.network.ethMac, sizeof(config_.network.ethMac));

  for (uint8_t ch = 0; ch < 4; ++ch) {
    const CalibrationProfile& p = config_.calibration[ch];
    const String prefix = calibrationPrefix(ch);
    preferences_.putUChar((prefix + "mode").c_str(), static_cast<uint8_t>(p.mode));
    preferences_.putFloat((prefix + "k").c_str(), p.k);
    preferences_.putFloat((prefix + "off").c_str(), p.offset);
    preferences_.putFloat((prefix + "a").c_str(), p.a);
    preferences_.putFloat((prefix + "b").c_str(), p.b);
    preferences_.putFloat((prefix + "c").c_str(), p.c);
    preferences_.putString((prefix + "lut").c_str(), serializeLut(p.lut));
  }

  preferences_.end();
  return true;
}

DeviceConfig& ConfigManager::mutableConfig() {
  return config_;
}

const DeviceConfig& ConfigManager::getConfig() const {
  return config_;
}

void ConfigManager::applyDefaults() {
  config_ = DeviceConfig();
  applyFixedWifi(config_.network);
  applyDefaultCalibration();
}

void ConfigManager::applyDefaultCalibration() {
  // Example LUT for RF forward channel based on specification values.
  config_.calibration[0].mode = CalibrationMode::Lut;
  config_.calibration[0].lut = {
      {1800, 0.0f},
      {2200, 25.0f},
      {2600, 100.0f},
      {3200, 300.0f},
      {3500, 500.0f},
  };

  for (uint8_t i = 1; i < 4; ++i) {
    config_.calibration[i].mode = CalibrationMode::Linear;
    config_.calibration[i].k = 1.0f;
    config_.calibration[i].offset = 0.0f;
  }

  // AIN2 temperature calibration: 0.0V -> 0C, 3.3V -> 100C.
  // ADS1115 at GAIN_ONE has full-scale 4.096V, so k = 100 / (32767 * 3.3 / 4.096).
  config_.calibration[2].k = 100.0f / (32767.0f * 3.3f / 4.096f);
  config_.calibration[2].offset = 0.0f;
}
