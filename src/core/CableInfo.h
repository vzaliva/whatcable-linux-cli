// Derived from WhatCable by Darryl Morley (https://github.com/darrylmorley/whatcable)
// Reads cable e-marker data from /sys/class/typec/portN-cable/identity/
#pragma once

#include "TypeCPort.h"
#include "PDDecoder.h"
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace WhatCable {

struct CableInfo {
    bool isActive = false;
    bool isPassive = false;
    // A VCONN-powered device answers on the cable channel (SOP') but is not
    // a cable: its VDOs are not decoded as cable VDOs.
    bool isVpd = false;
    std::string cableType;           // resolved: "active", "passive", "vpd", or sysfs value
    std::string plugType;
    std::string pdRevision;          // the e-marker's PD revision ("" if unknown)

    std::optional<CableSpeed> speed; // nullopt if not read or a reserved encoding
    std::optional<CableCurrent> currentRating;
    int ratedVbusVolts = 0;          // 20 or 50 from the Cable VDO; 0 if not read
    int maxWatts = 0;                // deliverable (voltage capped at 48V)

    uint16_t vendorId = 0;
    std::string vendorName;
    std::optional<TypeCIdentity> identity;   // raw Discover Identity VDOs

    std::optional<CableVDO> vdo;              // decoded Cable VDO 1
    std::optional<ActiveCableVDO2> activeVdo2;   // active cables only
    std::optional<uint32_t> certificationXid; // USB-IF XID from cert_stat, if non-zero

    std::map<std::string, std::string> rawAttributes;

    static std::optional<CableInfo> fromTypeCCable(const TypeCCable &cable);
};

} // namespace WhatCable
