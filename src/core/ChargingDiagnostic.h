// Derived from WhatCable by Darryl Morley (https://github.com/darrylmorley/whatcable)
// Port of ChargingDiagnostic.swift — identifies charging bottlenecks
#pragma once

#include <string>
#include "PowerDelivery.h"
#include "CableInfo.h"
#include <optional>

namespace WhatCable {

struct ChargingDiagnostic {
    enum Bottleneck {
        NoCharger,
        ChargerLimit,
        CableLimit,
        DeviceLimit,
        Fine,
        NegotiationUnknown   // charger present, system doesn't report the contract
    };

    Bottleneck bottleneck = NoCharger;
    std::string summary;
    std::string detail;
    bool isWarning = false;

    // Inputs behind the verdict, for --json.
    int chargerMaxW = 0;
    int cableMaxW = 0;                 // 0: no e-marker rating
    std::optional<int> negotiatedW;    // nullopt: not reported

    // Stable machine-readable name for --json, e.g. "cableLimit".
    static const char *bottleneckCode(Bottleneck b);

    // chargerCaps: the partner's PD capabilities. Call only for a port that
    // is currently sinking power. negotiatedMW: the live contract (e.g. from
    // the UCSI power supply); nullopt when the system doesn't report one.
    static std::optional<ChargingDiagnostic> evaluate(
        const PowerDeliveryPort &chargerCaps,
        const std::optional<CableInfo> &cable,
        std::optional<int> negotiatedMW);
};

} // namespace WhatCable
