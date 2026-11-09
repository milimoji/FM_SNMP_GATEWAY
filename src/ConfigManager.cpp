#include "ConfigManager.h"

#include <LittleFS.h>
#include <cstring>

namespace {
constexpr uint32_t kConfigMagic = 0x464D4757u;  // FMGW
constexpr uint16_t kConfigVersion = 1;
constexpr uint8_t kMaxLutPoints = 16;
constexpr const char* kConfigPath = "/fm_config.bin";

uint32_t ipToU32(const IPAddress& ip) {
  return (static_cast<uint32_t>(ip[0]) << 24) | (static_cast<uint32_t>(ip[1]) << 16) |
         (static_cast<uint32_t>(ip[2]) << 8) | static_cast<uint32_t>(ip[3]);
}

IPAddress u32ToIp(uint32_t v) {
  return IPAddress((v >> 24) & 0xFF, (v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
}

#pragma pack(push, 1)
struct StoredLutPoint {
  float inputVoltage;
  float value;
};

struct StoredCalibration {
  uint8_t mode;
  float k;
  float offset;
  float a;
  float b;
  float c;
  uint8_t lutCount;
  StoredLutPoint lut[kMaxLutPoints];
};

struct StoredConfig {
  uint32_t magic;
  uint16_t version;
  uint8_t dhcp;
  uint8_t alarmActiveLow;
  uint8_t fanPwmEnabled;
  uint8_t fanTachPin;
  uint8_t fanPwmPin;
  uint8_t fanPulsesPerRev;
  uint8_t fanMinDutyPercent;
  uint8_t fanMaxDutyPercent;
  uint16_t fanStartupMaxSec;
  uint16_t samplePeriodMs;
  uint16_t trapPort;
  uint32_t ip;
  uint32_t gateway;
  uint32_t subnet;
  uint32_t dns;
  uint32_t trapServer;
  uint32_t freqKHz;
  float fanTempStartC;
  float fanTempMaxC;
  char hostname[32];
  char community[32];
  StoredCalibration calibration[4];
};
#pragma pack(pop)

void copyCString(char* dst, size_t dstSize, const char* src) {
  if (dstSize == 0) {
    return;
  }
  strncpy(dst, src == nullptr ? "" : src, dstSize - 1);
  dst[dstSize - 1] = '\0';
}

void toStored(const DeviceConfig& cfg, StoredConfig& stored) {
  memset(&stored, 0, sizeof(stored));
  stored.magic = kConfigMagic;
  stored.version = kConfigVersion;
  stored.dhcp = cfg.network.dhcp ? 1 : 0;
  stored.alarmActiveLow = cfg.alarmActiveLow ? 1 : 0;
  stored.fanPwmEnabled = cfg.fanPwmEnabled ? 1 : 0;
  stored.fanTachPin = cfg.fanTachPin;
  stored.fanPwmPin = cfg.fanPwmPin;
  stored.fanPulsesPerRev = cfg.fanPulsesPerRev;
  stored.fanMinDutyPercent = cfg.fanMinDutyPercent;
  stored.fanMaxDutyPercent = cfg.fanMaxDutyPercent;
  stored.fanStartupMaxSec = cfg.fanStartupMaxSec;
  stored.samplePeriodMs = cfg.samplePeriodMs;
  stored.trapPort = cfg.snmp.trapPort;
  stored.ip = ipToU32(cfg.network.ip);
  stored.gateway = ipToU32(cfg.network.gateway);
  stored.subnet = ipToU32(cfg.network.subnet);
  stored.dns = ipToU32(cfg.network.dns);
  stored.trapServer = ipToU32(cfg.snmp.trapServer);
  stored.freqKHz = cfg.snmp.freqKHz;
  stored.fanTempStartC = cfg.fanTempStartC;
  stored.fanTempMaxC = cfg.fanTempMaxC;
  copyCString(stored.hostname, sizeof(stored.hostname), cfg.network.hostname);
  copyCString(stored.community, sizeof(stored.community), cfg.snmp.community);

  for (uint8_t ch = 0; ch < 4; ++ch) {
    const CalibrationProfile& src = cfg.calibration[ch];
    StoredCalibration& dst = stored.calibration[ch];
    dst.mode = static_cast<uint8_t>(src.mode);
    dst.k = src.k;
    dst.offset = src.offset;
    dst.a = src.a;
    dst.b = src.b;
    dst.c = src.c;
    const size_t count = src.lut.size() > kMaxLutPoints ? kMaxLutPoints : src.lut.size();
    dst.lutCount = static_cast<uint8_t>(count);
    for (size_t i = 0; i < count; ++i) {
      dst.lut[i].inputVoltage = src.lut[i].inputVoltage;
      dst.lut[i].value = src.lut[i].value;
    }
  }
}

void fromStored(const StoredConfig& stored, DeviceConfig& cfg) {
  cfg.network.dhcp = stored.dhcp != 0;
  cfg.alarmActiveLow = stored.alarmActiveLow != 0;
  cfg.fanPwmEnabled = stored.fanPwmEnabled != 0;
  cfg.fanTachPin = stored.fanTachPin;
  cfg.fanPwmPin = stored.fanPwmPin;
  cfg.fanPulsesPerRev = stored.fanPulsesPerRev == 0 ? 1 : stored.fanPulsesPerRev;
  cfg.fanMinDutyPercent = stored.fanMinDutyPercent;
  cfg.fanMaxDutyPercent = stored.fanMaxDutyPercent;
  cfg.fanStartupMaxSec = stored.fanStartupMaxSec;
  cfg.samplePeriodMs = stored.samplePeriodMs == 0 ? 50 : stored.samplePeriodMs;
  cfg.snmp.trapPort = stored.trapPort == 0 ? 162 : stored.trapPort;
  cfg.network.ip = u32ToIp(stored.ip);
  cfg.network.gateway = u32ToIp(stored.gateway);
  cfg.network.subnet = u32ToIp(stored.subnet);
  cfg.network.dns = u32ToIp(stored.dns);
  cfg.snmp.trapServer = u32ToIp(stored.trapServer);
  cfg.snmp.freqKHz = stored.freqKHz;
  cfg.fanTempStartC = stored.fanTempStartC;
  cfg.fanTempMaxC = stored.fanTempMaxC;
  copyCString(cfg.network.hostname, sizeof(cfg.network.hostname), stored.hostname);
  copyCString(cfg.snmp.community, sizeof(cfg.snmp.community), stored.community);

  for (uint8_t ch = 0; ch < 4; ++ch) {
    const StoredCalibration& src = stored.calibration[ch];
    CalibrationProfile& dst = cfg.calibration[ch];
    const uint8_t mode = src.mode > 2 ? 0 : src.mode;
    dst.mode = static_cast<CalibrationMode>(mode);
    dst.k = src.k;
    dst.offset = src.offset;
    dst.a = src.a;
    dst.b = src.b;
    dst.c = src.c;
    dst.lut.clear();
    const uint8_t count = src.lutCount > kMaxLutPoints ? kMaxLutPoints : src.lutCount;
    dst.lut.reserve(count);
    for (uint8_t i = 0; i < count; ++i) {
      dst.lut.push_back({src.lut[i].inputVoltage, src.lut[i].value});
    }
  }
}
}  // namespace

bool ConfigManager::begin() {
  applyDefaults();

  if (!LittleFS.begin()) {
    Serial.println("FS: mount failed, formatting");
    LittleFS.format();
    if (!LittleFS.begin()) {
      Serial.println("FS: unavailable, using RAM defaults");
      return true;
    }
  }

  if (!load()) {
    Serial.println("FS: no valid config, writing defaults");
    save();
  }
  return true;
}

bool ConfigManager::load() {
  File file = LittleFS.open(kConfigPath, "r");
  if (!file) {
    return false;
  }

  StoredConfig stored{};
  const size_t read = file.read(reinterpret_cast<uint8_t*>(&stored), sizeof(stored));
  file.close();
  if (read != sizeof(stored) || stored.magic != kConfigMagic || stored.version != kConfigVersion) {
    Serial.println("FS: config magic/version mismatch");
    return false;
  }

  fromStored(stored, config_);
  if (config_.network.hostname[0] == '\0') {
    copyCString(config_.network.hostname, sizeof(config_.network.hostname), "fm-snmp-gateway");
  }
  return true;
}

bool ConfigManager::save() {
  StoredConfig stored{};
  toStored(config_, stored);

  File file = LittleFS.open(kConfigPath, "w");
  if (!file) {
    Serial.println("FS: open for write failed");
    return false;
  }

  const size_t wrote = file.write(reinterpret_cast<const uint8_t*>(&stored), sizeof(stored));
  file.close();
  if (wrote != sizeof(stored)) {
    Serial.println("FS: write incomplete");
    return false;
  }
  return true;
}

void ConfigManager::resetNetworkToDefaults() {
  config_.network.dhcp = true;
  config_.network.ip = IPAddress(192, 168, 1, 50);
  config_.network.gateway = IPAddress(192, 168, 1, 1);
  config_.network.subnet = IPAddress(255, 255, 255, 0);
  config_.network.dns = IPAddress(8, 8, 8, 8);
  copyCString(config_.network.hostname, sizeof(config_.network.hostname), "fm-snmp-gateway");
}

DeviceConfig& ConfigManager::mutableConfig() {
  return config_;
}

const DeviceConfig& ConfigManager::getConfig() const {
  return config_;
}

void ConfigManager::applyDefaults() {
  config_ = DeviceConfig();
  applyDefaultCalibration();
}

void ConfigManager::applyDefaultCalibration() {
  config_.calibration[0].mode = CalibrationMode::Lut;
  config_.calibration[0].lut = {
      {0.3750f, 0.0f},
      {0.4584f, 25.0f},
      {0.5417f, 100.0f},
      {0.6666f, 300.0f},
      {0.7291f, 500.0f},
  };

  for (uint8_t i = 1; i < 4; ++i) {
    config_.calibration[i].mode = CalibrationMode::Linear;
    config_.calibration[i].k = 1.0f;
    config_.calibration[i].offset = 0.0f;
  }

  config_.calibration[2].k = 20.0f;
  config_.calibration[2].offset = 0.0f;
}
