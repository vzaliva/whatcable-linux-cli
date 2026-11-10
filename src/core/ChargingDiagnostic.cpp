// Derived from WhatCable by Darryl Morley (https://github.com/darrylmorley/whatcable)
// Port of ChargingDiagnostic.swift
#include "ChargingDiagnostic.h"
#include <cstdio>

namespace WhatCable {

const char *ChargingDiagnostic::bottleneckCode(Bottleneck b)
{
    switch (b) {
    case NoCharger:          return "noCharger";
    case ChargerLimit:       return "chargerLimit";
    case CableLimit:         return "cableLimit";
    case DeviceLimit:        return "deviceLimit";
    case Fine:               return "fine";
    case NegotiationUnknown: return "negotiationUnknown";
    }
    return "unknown";
}

std::optional<ChargingDiagnostic> ChargingDiagnostic::evaluate(
    const PowerDeliveryPort &chargerCaps,
    const std::optional<CableInfo> &cable,
    std::optional<int> negotiatedMW,
    const std::optional<SystemBattery> &battery)
{
    if (chargerCaps.sourceCapabilities.empty())
        return std::nullopt;

    const int chargerMaxW = chargerCaps.maxSourcePowerMW / 1000;
    if (chargerMaxW <= 0)
        return std::nullopt;

    ChargingDiagnostic diag;

    const int cableMaxW = (cable && cable->maxWatts > 0) ? cable->maxWatts : 0;
    diag.chargerMaxW = chargerMaxW;
    diag.cableMaxW = cableMaxW;
    diag.battery = battery;
    if (negotiatedMW && *negotiatedMW > 0)
        diag.negotiatedW = (*negotiatedMW + 500) / 1000;

    char buf[256];
    if (cableMaxW > 0 && cableMaxW < chargerMaxW) {
        diag.bottleneck = CableLimit;
        diag.summary = "Cable is limiting charging speed";
        std::snprintf(buf, sizeof(buf),
                      "Cable rated for %dW, but charger can deliver %dW",
                      cableMaxW, chargerMaxW);
        diag.detail = buf;
        diag.isWarning = true;
        return diag;
    }

    // Never stand in the charger's maximum for the negotiated value: that
    // would report "charging well" for a link we know nothing about.
    if (!diag.negotiatedW) {
        diag.bottleneck = NegotiationUnknown;
        std::snprintf(buf, sizeof(buf), "Charger offers up to %dW", chargerMaxW);
        diag.summary = buf;
        diag.detail = "This system doesn't report the negotiated power";
        return diag;
    }

    const int negotiatedW = *diag.negotiatedW;
    if (negotiatedW < chargerMaxW * 0.8) {
        diag.bottleneck = DeviceLimit;
        std::snprintf(buf, sizeof(buf), "Charging at %dW", negotiatedW);
        diag.summary = buf;
        std::snprintf(buf, sizeof(buf),
                      "Laptop requested %dW; charger can deliver up to %dW",
                      negotiatedW, chargerMaxW);
        diag.detail = buf;
    } else {
        diag.bottleneck = Fine;
        std::snprintf(buf, sizeof(buf), "Charging well at %dW", negotiatedW);
        diag.summary = buf;
    }

    // Power is flowing, but say "charging" only if the battery agrees.
    using Status = SystemBattery::Status;
    Status status = battery ? battery->status : Status::Unknown;
    // At or above a charge limit, some firmware reports "Discharging" while
    // it lets the battery drain back down to the limit. That is the limit
    // working, not a charger that can't keep up: treat it as not charging.
    const bool atChargeLimit = battery && battery->capacityPercent &&
                               battery->chargeLimitPercent &&
                               *battery->chargeLimitPercent < 100 &&
                               *battery->capacityPercent >= *battery->chargeLimitPercent;
    if (status == Status::Discharging && atChargeLimit)
        status = Status::NotCharging;
    const std::string contractDetail = diag.detail;
    auto withContract = [&](const std::string &text) {
        return contractDetail.empty() ? text : text + ". " + contractDetail;
    };

    switch (status) {
    case Status::Full:
        std::snprintf(buf, sizeof(buf), "Plugged in at %dW, battery full", negotiatedW);
        diag.summary = buf;
        break;
    case Status::NotCharging:
        std::snprintf(buf, sizeof(buf), "Plugged in at %dW, not charging", negotiatedW);
        diag.summary = buf;
        if (atChargeLimit) {
            std::snprintf(buf, sizeof(buf), "Battery at %d%%, held at the %d%% charge limit",
                          *battery->capacityPercent, *battery->chargeLimitPercent);
            diag.detail = withContract(buf);
        } else {
            diag.detail = withContract("Charging is paused by the system");
        }
        break;
    case Status::Discharging:
        diag.isWarning = true;
        if (diag.bottleneck == Fine) {
            // Already drawing (nearly) all the charger offers: it is too weak.
            diag.bottleneck = ChargerLimit;
            std::snprintf(buf, sizeof(buf), "Charger can't keep up at %dW: battery discharging",
                          negotiatedW);
            diag.summary = buf;
            std::snprintf(buf, sizeof(buf),
                          "The laptop uses more than the %dW this charger can deliver",
                          chargerMaxW);
            diag.detail = buf;
        } else {
            std::snprintf(buf, sizeof(buf), "Plugged in at %dW, but battery is discharging",
                          negotiatedW);
            diag.summary = buf;
        }
        break;
    case Status::Charging:
    case Status::Unknown:
        break;
    }

    return diag;
}

} // namespace WhatCable
