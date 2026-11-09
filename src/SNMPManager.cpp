#include "SNMPManager.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <SNMP.h>

namespace {
SNMPManager* gSnmpManager = nullptr;
constexpr char kEventTxAlarmOid[] = "1.3.6.1.4.1.35305.1.1.5.4";  // Notifications.4
constexpr char kTsTxAlarmOid[] = "1.3.6.1.4.1.35305.1.1.1.6.0";    // General.6.0

void onSnmpMessage(const SNMP::Message* request, const IPAddress remote, const uint16_t port) {
  if (gSnmpManager != nullptr) {
    gSnmpManager->handleRequest(request, remote, port);
  }
}
}  // namespace

bool SNMPManager::begin(const DeviceConfig* cfg, const char* deviceName) {
  cfg_ = cfg;
  deviceName_ = deviceName == nullptr ? "fm-snmp-gateway" : String(deviceName);
  registerDefaultOidMap();
  telemetry_ = SnmpTelemetry();

  if (snmp_ == nullptr) {
    snmp_ = new SNMP::Agent();
  }
  if (ethernetUdp_ == nullptr) {
    ethernetUdp_ = new EthernetUDP();
  }

  gSnmpManager = this;
  snmp_->onMessage(onSnmpMessage);
  boundLink_ = LinkType::None;
  activeUdp_ = nullptr;
  snmpStackReady_ = false;

  Serial.println("SNMP library enabled, waiting for active network link");

  return cfg_ != nullptr;
}

void SNMPManager::update(uint32_t nowMs, const SnmpTelemetry& telemetry) {
  telemetry_ = telemetry;
  ensureAgentBound();

  if (snmpStackReady_ && snmp_ != nullptr) {
    snmp_->loop();
  }

  if (nowMs - lastLogMs_ < 10000) {
    return;
  }
  lastLogMs_ = nowMs;

  Serial.printf(
      "SNMP snapshot stack=%d link=%d oidCount=%u FWD=%.2f REF=%.2f TEMP=%.2f DEV=%.2f ALARM=%d UPTIME=%lu\n",
      snmpStackReady_ ? 1 : 0,
      static_cast<int>(boundLink_),
      static_cast<unsigned>(oidMap_.size()),
      telemetry.analog.forward,
      telemetry.analog.reflect,
      telemetry.analog.temperature,
      telemetry.analog.deviation,
      telemetry.alarm.alarmActive ? 1 : 0,
      static_cast<unsigned long>(telemetry.uptimeSec));
}

void SNMPManager::onAlarmEdge(bool active, const SnmpTelemetry& telemetry) {
  emitTrap(active ? "DEVICE_ALARM_ON" : "DEVICE_ALARM_OFF", telemetry);

  if (!snmpStackReady_ || snmp_ == nullptr || cfg_ == nullptr) {
    return;
  }

    SNMP::Message trap(
      SNMP::Version::V2C,
      cfg_->snmp.community,
      SNMP::Type::SNMPv2Trap);
    trap.setSNMPTrapOID(kEventTxAlarmOid);
    trap.add(kTsTxAlarmOid, new SNMP::IntegerBER(active ? 1 : 0));

    snmp_->send(&trap, cfg_->snmp.trapServer, cfg_->snmp.trapPort);
}

void SNMPManager::emitTrap(const char* eventName, const SnmpTelemetry& telemetry) const {
  Serial.printf(
      "TRAP event=%s device=%s ip=%s fwd=%.2f ref=%.2f temp=%.2f dev=%.2f\n",
      eventName,
      deviceName_.c_str(),
      telemetry.network.ip.toString().c_str(),
      telemetry.analog.forward,
      telemetry.analog.reflect,
      telemetry.analog.temperature,
      telemetry.analog.deviation);
}

void SNMPManager::setOidMap(const std::vector<OidBinding>& map) {
  oidMap_ = map;
  std::sort(oidMap_.begin(), oidMap_.end(), [](const OidBinding& left, const OidBinding& right) {
    return compareOids(left.oid.c_str(), right.oid.c_str()) < 0;
  });
}

const std::vector<OidBinding>& SNMPManager::getOidMap() const {
  return oidMap_;
}

void SNMPManager::ensureAgentBound() {
  if (snmp_ == nullptr) {
    snmpStackReady_ = false;
    return;
  }

  const LinkType desiredLink = telemetry_.network.activeLink;
  if (desiredLink == boundLink_) {
    return;
  }

  activeUdp_ = nullptr;
  snmpStackReady_ = false;

  bool bound = false;
  switch (desiredLink) {
    case LinkType::Ethernet:
      if (ethernetUdp_ != nullptr) {
        bound = snmp_->begin(*ethernetUdp_);
        activeUdp_ = ethernetUdp_;
      }
      break;
    case LinkType::None:
    default:
      boundLink_ = LinkType::None;
      return;
  }

  if (bound) {
    boundLink_ = desiredLink;
    snmpStackReady_ = true;
    Serial.println("SNMP: bound to Ethernet UDP port 161");
  }
}

void SNMPManager::handleRequest(const SNMP::Message* request, const IPAddress remote, const uint16_t port) {
  if (request == nullptr || cfg_ == nullptr) {
    return;
  }

  SNMP::Message response(
      request->getVersion(),
      request->getCommunity(),
      SNMP::Type::GetResponse);
  response.setRequestID(request->getRequestID());

  buildResponse(request, &response);
  snmp_->send(&response, remote, port);
}

void SNMPManager::buildResponse(const SNMP::Message* request, SNMP::Message* response) const {
  const uint8_t type = request->getType();
  SNMP::VarBindList* varbindlist = request->getVarBindList();
  const uint8_t count = varbindlist->count();

  for (uint8_t index = 0; index < count; ++index) {
    SNMP::VarBind* varbind = (*varbindlist)[index];
    const char* name = varbind->getName();

    if (type == SNMP::Type::SetRequest) {
      response->setError(SNMP::Error::NotWritable, static_cast<uint8_t>(index + 1));
      response->add(name);
      continue;
    }

    const OidBinding* binding = nullptr;
    if (type == SNMP::Type::GetNextRequest || type == SNMP::Type::GetBulkRequest) {
      binding = findNextOid(name);
    } else {
      binding = findOid(name);
    }

    if (binding == nullptr) {
      if (request->getVersion() == SNMP::Version::V1) {
        response->setError(SNMP::Error::NoSuchName, static_cast<uint8_t>(index + 1));
        response->add(name);
      } else {
        if (type == SNMP::Type::GetNextRequest || type == SNMP::Type::GetBulkRequest) {
          response->add(name, new SNMP::EndOfMIBViewBER());
        } else {
          response->add(name, new SNMP::NoSuchObjectBER());
        }
      }
      continue;
    }

    response->add(binding->oid.c_str(), buildValueForOid(*binding));
  }
}

const OidBinding* SNMPManager::findOid(const char* oid) const {
  for (const OidBinding& binding : oidMap_) {
    if (binding.oid == oid) {
      return &binding;
    }
  }
  return nullptr;
}

const OidBinding* SNMPManager::findNextOid(const char* oid) const {
  const OidBinding* best = nullptr;
  for (const OidBinding& binding : oidMap_) {
    if (compareOids(binding.oid.c_str(), oid) > 0) {
      if (best == nullptr || compareOids(binding.oid.c_str(), best->oid.c_str()) < 0) {
        best = &binding;
      }
    }
  }
  return best;
}

int SNMPManager::compareOids(const char* lhs, const char* rhs) {
  const char* left = lhs;
  const char* right = rhs;

  while (*left != '\0' || *right != '\0') {
    while (*left == '.') {
      ++left;
    }
    while (*right == '.') {
      ++right;
    }

    unsigned long leftPart = 0;
    unsigned long rightPart = 0;

    while (*left != '\0' && *left != '.') {
      if (std::isdigit(static_cast<unsigned char>(*left))) {
        leftPart = (leftPart * 10UL) + static_cast<unsigned long>(*left - '0');
      }
      ++left;
    }

    while (*right != '\0' && *right != '.') {
      if (std::isdigit(static_cast<unsigned char>(*right))) {
        rightPart = (rightPart * 10UL) + static_cast<unsigned long>(*right - '0');
      }
      ++right;
    }

    if (leftPart < rightPart) {
      return -1;
    }
    if (leftPart > rightPart) {
      return 1;
    }
  }

  return 0;
}

SNMP::BER* SNMPManager::buildValueForOid(const OidBinding& binding) const {
  if (binding.name == "fwd") {
    const int32_t fwdX10 = static_cast<int32_t>(lroundf(telemetry_.analog.forward * 10.0f));
    return new SNMP::IntegerBER(fwdX10);
  }
  if (binding.name == "ref") {
    const int32_t refX10 = static_cast<int32_t>(lroundf(telemetry_.analog.reflect * 10.0f));
    return new SNMP::IntegerBER(refX10);
  }
  if (binding.name == "temp") {
    const int32_t tempX10 = static_cast<int32_t>(lroundf(telemetry_.analog.temperature * 10.0f));
    return new SNMP::IntegerBER(tempX10);
  }
  if (binding.name == "freq") {
    return new SNMP::IntegerBER(static_cast<int32_t>(cfg_ != nullptr ? cfg_->snmp.freqKHz : 0));
  }
  if (binding.name == "modu") {
    const int32_t devX10 = static_cast<int32_t>(lroundf(telemetry_.analog.deviation * 10.0f));
    return new SNMP::IntegerBER(devX10);
  }
  if (binding.name == "alarmTx") {
    return new SNMP::IntegerBER(telemetry_.alarm.alarmActive ? 1 : 0);
  }
  if (binding.name == "alarmFault") {
    return new SNMP::IntegerBER(telemetry_.alarm.statusInput ? 1 : 0);
  }

  return new SNMP::OctetStringBER("");
}

void SNMPManager::registerDefaultOidMap() {
  auto addOid = [this](const char* oid, const char* name, OidAccess access) {
    OidBinding binding;
    binding.oid = oid;
    binding.name = name;
    binding.access = access;
    oidMap_.push_back(binding);
  };

  oidMap_.clear();
  addOid("1.3.6.1.4.1.35305.1.1.2.5.0", "fwd", OidAccess::ReadOnly);
  addOid("1.3.6.1.4.1.35305.1.1.2.6.0", "ref", OidAccess::ReadOnly);
  addOid("1.3.6.1.4.1.35305.1.1.2.12.0", "temp", OidAccess::ReadOnly);
  addOid("1.3.6.1.4.1.35305.1.1.2.3.0", "freq", OidAccess::ReadOnly);
  addOid("1.3.6.1.4.1.35305.1.1.2.7.0", "modu", OidAccess::ReadOnly);
  addOid("1.3.6.1.4.1.35305.1.1.1.6.0", "alarmTx", OidAccess::ReadOnly);
  addOid("1.3.6.1.4.1.35305.1.1.1.7.0", "alarmFault", OidAccess::ReadOnly);

  std::sort(oidMap_.begin(), oidMap_.end(), [](const OidBinding& left, const OidBinding& right) {
    return compareOids(left.oid.c_str(), right.oid.c_str()) < 0;
  });
}
