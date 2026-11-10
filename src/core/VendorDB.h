// Derived from WhatCable by Darryl Morley (https://github.com/darrylmorley/whatcable)
// Port of VendorDB.swift — USB VID to vendor name lookup
#pragma once

#include <string>
#include <cstdint>

namespace WhatCable {

class VendorDB {
public:
    // Vendor name from the system usb.ids (hwdata / usbutils), else a small
    // built-in table, else "0xVVVV".
    static std::string lookup(uint16_t vendorId);

    // lookup() for a USB PD identity (cable or port partner), where 0xFFFF
    // is the spec's "no vendor ID assigned" value rather than a vendor.
    static std::string lookupPd(uint16_t vendorId);

    // The usb.ids file in use, or empty if none was found.
    static std::string usbIdsPath();
};

} // namespace WhatCable
