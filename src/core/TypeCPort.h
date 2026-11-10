// Derived from WhatCable by Darryl Morley (https://github.com/darrylmorley/whatcable)
// Linux equivalent of USBCPort.swift — reads /sys/class/typec/
#pragma once

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>

namespace WhatCable {

// Discover Identity response, read by file name from <dev>/identity/.
// Only constructed when id_header is non-zero: the kernel shows 0x0 in
// every file until Discover Identity has completed.
struct TypeCIdentity {
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    uint32_t idHeader = 0;
    uint32_t certStat = 0;
    uint32_t product = 0;
    // product_type_vdo1..3; 0 means "not provided". For cables, [0] is the
    // Cable VDO and [1] the Active Cable VDO 2.
    std::array<uint32_t, 3> productTypeVdos{};
};

struct TypeCPartner {
    std::string type;
    std::optional<TypeCIdentity> identity;
    std::string pdName;        // usb_power_delivery entry, e.g. "pd2"
    std::map<std::string, std::string> rawAttributes;
};

struct TypeCCable {
    std::string type;          // "active", "passive"
    std::string plugType;
    std::string pdRevision;    // the e-marker's PD revision, e.g. "2.0", "3.0"
    std::optional<TypeCIdentity> identity;
    std::map<std::string, std::string> rawAttributes;
};

struct TypeCPowerSupply {
    std::string sysfsPath;
    std::string name;
    bool online = false;
    std::optional<int> voltageNowUV;
    std::optional<int> currentNowUA;
    std::optional<int> currentMaxUA;
    std::optional<int> voltageMinUV;
    std::optional<int> voltageMaxUV;
    std::string chargeType;
    std::string usbType;
    std::map<std::string, std::string> rawAttributes;

    // Requested contract (voltage_now x current_now), not measured draw.
    // nullopt unless online with positive values. UCSI reports "online"
    // only while the port is sinking power.
    std::optional<int> negotiatedMilliwatts() const;

    // The bracketed (active) entry of usb_type, e.g. "PD_PPS" from
    // "C PD [PD_PPS]"; empty if none is marked.
    std::string selectedUsbType() const;
};

struct TypeCPort {
    std::string sysfsPath;
    std::string portName;      // "port0", "port1", ...
    int portNumber = -1;

    std::string dataRole;      // "host", "device", "[host]", "[device]"
    std::string powerRole;     // "source", "sink", "[source]", "[sink]"
    std::string portType;      // "dual", "source", "sink"
    std::string powerOpMode;   // "default", "1.5A", "3.0A", "usb_power_delivery"
    std::string orientation;   // "normal", "reverse", "unknown"
    std::string pdRevision;
    std::string usbTypeCRev;
    std::optional<TypeCPowerSupply> powerSupply;
    std::string pdName;        // the port's own usb_power_delivery entry

    bool hasPartner = false;
    std::optional<TypeCPartner> partner;
    bool hasCable = false;
    std::optional<TypeCCable> cable;

    std::map<std::string, std::string> rawAttributes;

    bool isConnected() const;
    std::string currentDataRole() const;
    std::string currentPowerRole() const;
    bool isSink() const { return currentPowerRole() == "sink"; }

    static std::vector<TypeCPort> enumerate();

private:
    static std::optional<TypeCPort> fromSysfs(const std::string &path, const std::string &name);
    static std::optional<TypeCIdentity> readIdentity(const std::string &path);
};

} // namespace WhatCable
