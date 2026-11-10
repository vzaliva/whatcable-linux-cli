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
    std::optional<int> negotiatedMW)
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

    return diag;
}

} // namespace WhatCable
