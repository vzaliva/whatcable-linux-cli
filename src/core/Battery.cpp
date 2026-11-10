// Derived from WhatCable by Darryl Morley (https://github.com/darrylmorley/whatcable)
// Linux equivalent of BatteryChargeState.swift — reads /sys/class/power_supply/
#include "Battery.h"
#include "SysfsReader.h"

namespace WhatCable {

static const char kPowerSupplyPath[] = "/sys/class/power_supply";

const char *SystemBattery::statusCode(Status status)
{
    switch (status) {
    case Status::Charging:    return "charging";
    case Status::Discharging: return "discharging";
    case Status::NotCharging: return "notCharging";
    case Status::Full:        return "full";
    case Status::Unknown:     return "unknown";
    }
    return "unknown";
}

static SystemBattery::Status parseStatus(const std::string &s)
{
    if (s == "Charging")     return SystemBattery::Status::Charging;
    if (s == "Discharging")  return SystemBattery::Status::Discharging;
    if (s == "Not charging") return SystemBattery::Status::NotCharging;
    if (s == "Full")         return SystemBattery::Status::Full;
    return SystemBattery::Status::Unknown;
}

std::optional<SystemBattery> SystemBattery::read()
{
    const std::string base = SysfsReader::sysPath(kPowerSupplyPath);
    std::optional<SystemBattery> result;
    bool anyCharging = false, anyDischarging = false, anyNotCharging = false;
    bool allFull = true;

    for (const auto &name : SysfsReader::listSubdirectories(base)) {
        const std::string path = base + "/" + name;
        if (SysfsReader::readAttribute(path + "/type") != "Battery")
            continue;
        if (SysfsReader::readAttribute(path + "/scope") == "Device")
            continue;

        const Status status = parseStatus(SysfsReader::readAttribute(path + "/status"));
        anyCharging |= status == Status::Charging;
        anyDischarging |= status == Status::Discharging;
        anyNotCharging |= status == Status::NotCharging;
        allFull &= status == Status::Full;

        if (!result) {
            result = SystemBattery{};
            result->capacityPercent = SysfsReader::readIntAttribute(path + "/capacity");
            result->chargeLimitPercent =
                SysfsReader::readIntAttribute(path + "/charge_control_end_threshold");
        }
    }

    if (!result)
        return std::nullopt;

    if (anyCharging)
        result->status = Status::Charging;
    else if (anyDischarging)
        result->status = Status::Discharging;
    else if (anyNotCharging)
        result->status = Status::NotCharging;
    else if (allFull)
        result->status = Status::Full;
    return result;
}

} // namespace WhatCable
