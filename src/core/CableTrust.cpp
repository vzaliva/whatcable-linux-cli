// Derived from WhatCable by Darryl Morley (https://github.com/darrylmorley/whatcable)
// Port of CableTrustReport.swift — hedged notes about a cable's e-marker data
#include "CableTrust.h"

namespace WhatCable {

namespace {

CableTrustNote noteFor(const CableDecodeWarning &w)
{
    const std::string v = std::to_string(w.value);
    switch (w.kind) {
    case CableDecodeWarning::ReservedSpeed:
        return {"reservedSpeedEncoding", "E-marker uses a reserved data-speed value",
                "The cable's e-marker reports speed value " + v +
                    ", which is reserved by the USB-PD spec. Real e-marker chips should not "
                    "emit reserved values."};
    case CableDecodeWarning::ReservedCurrent:
        return {"reservedCurrentEncoding", "E-marker uses a reserved current-rating value",
                "The cable's e-marker reports current value " + v +
                    ", which is reserved by the USB-PD spec. Real e-marker chips should not "
                    "emit reserved values."};
    case CableDecodeWarning::ReservedLatency:
        return {"reservedCableLatencyEncoding", "E-marker uses a reserved cable-latency value",
                "The cable's e-marker reports cable-latency value " + v +
                    ", which is reserved by the USB-PD spec for this cable type. Real e-marker "
                    "chips should not emit reserved values."};
    case CableDecodeWarning::InvalidVdoVersion:
        return {"invalidVDOVersion", "E-marker uses an invalid VDO version",
                "The cable's e-marker reports VDO version " + v +
                    ", which the USB-PD spec marks Invalid for this cable type. Real e-marker "
                    "silicon should not emit it."};
    case CableDecodeWarning::InvalidTermination:
        return {"invalidCableTermination", "E-marker uses an invalid cable-termination value",
                "The cable's e-marker reports cable termination " + v +
                    ", which the USB-PD spec marks Invalid for this cable type. Mis-flashed "
                    "e-markers commonly disagree with themselves like this."};
    case CableDecodeWarning::EprWithLowMaxVoltage:
        return {"eprClaimedWithLowMaxVoltage",
                "E-marker claims EPR support but reports only 20V max VBUS",
                "The cable's e-marker advertises EPR Capable, but reports its Max VBUS "
                "Voltage as 20V. EPR operation needs a 50V-rated cable, so the two fields "
                "contradict each other."};
    }
    return {"unknown", "Unusual e-marker data", ""};
}

} // namespace

std::vector<CableTrustNote> cableTrustNotes(uint16_t vendorId, const CableVDO *cableVdo)
{
    std::vector<CableTrustNote> notes;
    if (vendorId == 0) {
        notes.push_back({"zeroVendorID", "E-marker reports no vendor identity",
                         "This cable's e-marker doesn't report a vendor ID, which is common on "
                         "genuine cables. It's only worth a closer look if the cable's other "
                         "capability data is also unusual.",
                         /*isInfo=*/true});
    }
    if (cableVdo) {
        for (const auto &w : cableVdo->warnings)
            notes.push_back(noteFor(w));
    }
    return notes;
}

} // namespace WhatCable
