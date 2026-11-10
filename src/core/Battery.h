// Derived from WhatCable by Darryl Morley (https://github.com/darrylmorley/whatcable)
// Linux equivalent of BatteryChargeState.swift — reads /sys/class/power_supply/
#pragma once

#include <optional>
#include <string>

namespace WhatCable {

// State of the machine's own battery (or batteries), so charging wording
// can tell "charging" from "plugged in but full / held at a charge limit".
struct SystemBattery {
    enum class Status { Charging, Discharging, NotCharging, Full, Unknown };

    Status status = Status::Unknown;
    std::optional<int> capacityPercent;
    std::optional<int> chargeLimitPercent;   // charge_control_end_threshold

    // Stable machine-readable name for --json, e.g. "notCharging".
    static const char *statusCode(Status status);

    // nullopt when the machine has no system battery. Peripheral batteries
    // (scope "Device", e.g. a wireless mouse) are ignored. With several
    // batteries the status is aggregated (any charging > any discharging >
    // any not charging > all full); percentages come from the first one.
    static std::optional<SystemBattery> read();
};

} // namespace WhatCable
