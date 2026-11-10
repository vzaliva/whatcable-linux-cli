// Derived from WhatCable by Darryl Morley (https://github.com/darrylmorley/whatcable)
// Linux equivalent of PowerSource.swift — reads /sys/class/usb_power_delivery/
#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>

namespace WhatCable {

struct PowerDataObject {
    enum Type { FixedSupply, Battery, VariableSupply, PPS, Unknown };

    Type type = Unknown;
    int voltageMV = 0;
    int maxVoltageMV = 0;   // for PPS/variable
    int currentMA = 0;
    int powerMW = 0;
    bool isActive = false;
    int index = 0;

    std::string voltageLabel() const;
    std::string currentLabel() const;
    std::string powerLabel() const;
    std::string typeLabel() const;
};

// One /sys/class/usb_power_delivery/pdN entry. Which device it describes
// (a port's own capabilities or its partner's) is known only from the
// Type-C side: TypeCPort::pdName and TypeCPartner::pdName.
struct PowerDeliveryPort {
    std::string sysfsPath;
    std::string name;          // "pd0", "pd1", ...

    std::vector<PowerDataObject> sourceCapabilities;
    std::vector<PowerDataObject> sinkCapabilities;

    int maxSourcePowerMW = 0;
    std::optional<int> activeSourcePdoIndex;

    std::map<std::string, std::string> rawAttributes;

    // Mark the source PDO behind the current contract as active.
    // Fixed contract: the fixed PDO at negotiatedMV. PPS contract
    // (ppsContract, e.g. UCSI usb_type "[PD_PPS]"): the only PPS PDO, or
    // with several, the one whose range contains negotiatedMV; fixed PDOs
    // are never marked for a PPS contract. Nothing is marked if ambiguous.
    void markActiveSourcePdo(int negotiatedMV, bool ppsContract = false);

    static std::vector<PowerDeliveryPort> enumerate();

private:
    static std::optional<PowerDeliveryPort> fromSysfs(const std::string &path, const std::string &name);
    static std::vector<PowerDataObject> parsePDOs(const std::string &capsPath);
};

} // namespace WhatCable
