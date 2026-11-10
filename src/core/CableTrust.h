// Derived from WhatCable by Darryl Morley (https://github.com/darrylmorley/whatcable)
// Port of CableTrustReport.swift — hedged notes about a cable's e-marker data
#pragma once

#include "PDDecoder.h"

#include <cstdint>
#include <string>
#include <vector>

namespace WhatCable {

// Something unusual in a cable's e-marker data. These are notes, never a
// verdict: upstream found spec-encoding flags firing on genuine cables
// (Apple's among them), so wording stays hedged ("looks unusual", never
// "fake"). Whether a cable delivers its claim needs live behaviour, which
// is not judged here.
struct CableTrustNote {
    std::string code;     // stable, for --json, e.g. "reservedCableLatencyEncoding"
    std::string title;
    std::string detail;
    // true: a calm, common fact (e.g. no vendor ID); false: the e-marker
    // data looks unusual against the spec.
    bool isInfo = false;
};

// Notes for a cable e-marker: its vendor ID and the decode warnings from
// its Cable VDO (nullptr when it had none). 0xFFFF is the spec's "no
// vendor ID" value and is not flagged.
std::vector<CableTrustNote> cableTrustNotes(uint16_t vendorId, const CableVDO *cableVdo);

} // namespace WhatCable
