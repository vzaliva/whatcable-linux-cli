// Derived from WhatCable by Darryl Morley (https://github.com/darrylmorley/whatcable)
// Linux equivalent of PowerSource.swift + PowerSourceWatcher.swift
#include "PowerDelivery.h"
#include "SysfsReader.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

namespace WhatCable {

static const char kPdPath[] = "/sys/class/usb_power_delivery";

std::string PowerDataObject::voltageLabel() const
{
    char buf[64];
    if (type == PPS && maxVoltageMV > 0)
        std::snprintf(buf, sizeof(buf), "%.1f-%.1fV",
                      voltageMV / 1000.0, maxVoltageMV / 1000.0);
    else
        std::snprintf(buf, sizeof(buf), "%.1fV", voltageMV / 1000.0);
    return buf;
}

std::string PowerDataObject::currentLabel() const
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2fA", currentMA / 1000.0);
    return buf;
}

std::string PowerDataObject::powerLabel() const
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.0fW", powerMW / 1000.0);
    return buf;
}

std::string PowerDataObject::typeLabel() const
{
    switch (type) {
    case FixedSupply: return "Fixed";
    case Battery: return "Battery";
    case VariableSupply: return "Variable";
    case PPS: return "PPS";
    default: return "Unknown";
    }
}

std::vector<PowerDataObject> PowerDeliveryPort::parsePDOs(const std::string &capsPath)
{
    std::vector<PowerDataObject> pdos;
    if (!SysfsReader::pathExists(capsPath))
        return pdos;

    // Each PDO is a directory named "<position>:<kind>", e.g. "1:fixed_supply"
    // or "4:programmable_supply". Other entries ("power", "uevent") are not PDOs.
    struct Entry { int index; std::string name; std::string kind; };
    std::vector<Entry> entries;
    for (const auto &name : SysfsReader::listSubdirectories(capsPath)) {
        const size_t colon = name.find(':');
        if (colon == std::string::npos || colon == 0)
            continue;
        char *end = nullptr;
        const long index = std::strtol(name.c_str(), &end, 10);
        if (end != name.c_str() + colon || index <= 0)
            continue;
        entries.push_back({static_cast<int>(index), name, name.substr(colon + 1)});
    }
    std::sort(entries.begin(), entries.end(),
              [](const Entry &a, const Entry &b) { return a.index < b.index; });

    for (const auto &entry : entries) {
        const std::string pdoPath = capsPath + "/" + entry.name;
        PowerDataObject pdo;
        pdo.index = entry.index;

        if (entry.kind == "fixed_supply")
            pdo.type = PowerDataObject::FixedSupply;
        else if (entry.kind == "battery")
            pdo.type = PowerDataObject::Battery;
        else if (entry.kind == "variable_supply")
            pdo.type = PowerDataObject::VariableSupply;
        else if (entry.kind.find("programmable") != std::string::npos ||
                 entry.kind.find("pps") != std::string::npos)
            pdo.type = PowerDataObject::PPS;

        // Values carry units ("5000mV"); readIntAttribute stops at the suffix.
        auto voltage = SysfsReader::readIntAttribute(pdoPath + "/voltage");
        if (!voltage)
            voltage = SysfsReader::readIntAttribute(pdoPath + "/minimum_voltage");
        if (voltage)
            pdo.voltageMV = *voltage;

        auto maxVoltage = SysfsReader::readIntAttribute(pdoPath + "/maximum_voltage");
        if (maxVoltage)
            pdo.maxVoltageMV = *maxVoltage;

        auto current = SysfsReader::readIntAttribute(pdoPath + "/maximum_current");
        if (!current)
            current = SysfsReader::readIntAttribute(pdoPath + "/current");
        if (current)
            pdo.currentMA = *current;

        auto power = SysfsReader::readIntAttribute(pdoPath + "/maximum_power");
        const int topVoltageMV = pdo.maxVoltageMV > 0 ? pdo.maxVoltageMV : pdo.voltageMV;
        if (power)
            pdo.powerMW = *power;
        else if (topVoltageMV > 0 && pdo.currentMA > 0)
            pdo.powerMW = static_cast<int>(static_cast<int64_t>(topVoltageMV) * pdo.currentMA / 1000);

        pdos.push_back(pdo);
    }

    return pdos;
}

void PowerDeliveryPort::markActiveSourcePdo(int negotiatedMV, bool ppsContract)
{
    // Fixed PDOs are in 50 mV units; allow a little slack for rounding.
    constexpr int kToleranceMV = 100;

    PowerDataObject *match = nullptr;
    if (ppsContract) {
        std::vector<PowerDataObject *> pps, containing;
        for (auto &pdo : sourceCapabilities) {
            if (pdo.type != PowerDataObject::PPS)
                continue;
            pps.push_back(&pdo);
            if (negotiatedMV >= pdo.voltageMV - kToleranceMV &&
                negotiatedMV <= pdo.maxVoltageMV + kToleranceMV)
                containing.push_back(&pdo);
        }
        // The reported voltage of an APDO contract is not reliable across
        // drivers, so a single PPS PDO is matched without it.
        if (pps.size() == 1)
            match = pps.front();
        else if (containing.size() == 1)
            match = containing.front();
    } else {
        for (auto &pdo : sourceCapabilities) {
            if (pdo.type == PowerDataObject::FixedSupply &&
                std::abs(pdo.voltageMV - negotiatedMV) <= kToleranceMV) {
                match = &pdo;
                break;
            }
        }
    }

    if (match) {
        match->isActive = true;
        activeSourcePdoIndex = match->index;
    }
}

std::optional<PowerDeliveryPort> PowerDeliveryPort::fromSysfs(const std::string &path, const std::string &name)
{
    PowerDeliveryPort port;
    port.sysfsPath = path;
    port.name = name;

    port.sourceCapabilities = parsePDOs(path + "/source-capabilities");
    port.sinkCapabilities = parsePDOs(path + "/sink-capabilities");

    // A PPS/AVS range's max V x max I can exceed the charger's rating (hence
    // the "PPS power limited" flag), so rate the charger by its fixed PDOs
    // whenever it has any.
    const bool hasFixed = std::any_of(
        port.sourceCapabilities.begin(), port.sourceCapabilities.end(),
        [](const PowerDataObject &p) { return p.type == PowerDataObject::FixedSupply; });
    for (const auto &pdo : port.sourceCapabilities) {
        if (hasFixed && pdo.type != PowerDataObject::FixedSupply)
            continue;
        if (pdo.powerMW > port.maxSourcePowerMW)
            port.maxSourcePowerMW = pdo.powerMW;
    }

    port.rawAttributes = SysfsReader::readAllAttributes(path);

    if (port.sourceCapabilities.empty() && port.sinkCapabilities.empty())
        return std::nullopt;

    return port;
}

std::vector<PowerDeliveryPort> PowerDeliveryPort::enumerate()
{
    std::vector<PowerDeliveryPort> ports;
    if (!SysfsReader::pathExists(kPdPath))
        return ports;

    std::error_code ec;
    const fs::path base(kPdPath);
    std::vector<std::string> entries;
    for (const auto &e : fs::directory_iterator(base, fs::directory_options::skip_permission_denied, ec)) {
        if (!ec && e.is_directory(ec))
            entries.push_back(e.path().filename().string());
    }
    std::sort(entries.begin(), entries.end());

    for (const auto &entry : entries) {
        auto port = fromSysfs((base / entry).string(), entry);
        if (port)
            ports.push_back(std::move(*port));
    }

    return ports;
}

} // namespace WhatCable
