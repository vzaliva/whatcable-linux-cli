// Derived from WhatCable by Darryl Morley (https://github.com/darrylmorley/whatcable)
// Port of VendorDB.swift + extended with common Linux USB vendors
#include "VendorDB.h"
#include "SysfsReader.h"
#include <cctype>
#include <cstdio>
#include <fstream>
#include <string>
#include <unordered_map>

namespace WhatCable {

static const std::unordered_map<uint16_t, std::string> kVendors = {
    {0x05AC, "Apple"},
    {0x0B95, "ASIX Electronics"},
    {0x2109, "VIA Labs"},
    {0x0BDA, "Realtek"},
    {0x8087, "Intel"},
    {0x04B4, "Cypress"},
    {0x1D6B, "Linux Foundation"},
    {0x1A40, "Terminus Technology"},
    {0x2188, "CalDigit"},
    {0x17EF, "Lenovo"},
    {0x04E8, "Samsung"},
    {0x0409, "NEC"},
    {0x0835, "Action Star Enterprise"},
    {0x291A, "Anker"},
    {0x1BCF, "Sunplus"},
    {0x2D01, "Google"},
    {0x18D1, "Google"},
    {0x046D, "Logitech"},
    {0x1532, "Razer"},
    {0x1B1C, "Corsair"},
    {0x258A, "Hailuck"},
    {0x0951, "Kingston"},
    {0x090C, "Silicon Motion"},
    {0x0781, "SanDisk"},
    {0x0930, "Toshiba"},
    {0x13FE, "Phison"},
    {0x058F, "Alcor Micro"},
    {0x05E3, "Genesys Logic"},
    {0x0451, "Texas Instruments"},
    {0x1058, "Western Digital"},
    {0x0BC2, "Seagate"},
    {0x174C, "ASMedia"},
    {0x0438, "AMD"},
    {0x1022, "AMD"},
    {0x1B21, "ASMedia"},
    {0x0955, "NVIDIA"},
    {0x056A, "Wacom"},
    {0x1050, "Yubico"},
    {0x045E, "Microsoft"},
    {0x03F0, "HP"},
    {0x413C, "Dell"},
    {0x04F2, "Chicony"},
    {0x0C45, "Microdia"},
    {0x0BDB, "Ericsson"},
    {0x2C7C, "Quectel"},
    {0x1199, "Sierra Wireless"},
    {0x12D1, "Huawei"},
    {0x22B8, "Motorola"},
    {0x0E8D, "MediaTek"},
    {0x0763, "M-Audio"},
    {0x1235, "Focusrite"},
    {0x1397, "BEHRINGER"},
    {0x0582, "Roland"},
    {0x1043, "iCreate Technologies"},
    {0x262A, "Savitech"},
    {0x37A8, "Ultimate Gadget Laboratories"},
};

namespace {

// Common usb.ids locations (hwdata, usbutils, Debian's misc/), read under
// SysfsReader::root() so tests and snapshot replays see their own copy.
const char *const kUsbIdsPaths[] = {
    "/usr/share/hwdata/usb.ids",
    "/usr/share/misc/usb.ids",
    "/usr/share/usb.ids",
    "/var/lib/usbutils/usb.ids",
};

bool isHex(char c)
{
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

struct UsbIds {
    std::string root;   // SysfsReader::root() this was loaded for
    std::string path;   // file used, or empty if none was found
    std::unordered_map<uint16_t, std::string> vendors;
};

// Vendor lines are "<4 hex digits>  <name>". Device lines are indented
// with a tab; the class and other tables start at the first "C " line.
void parseUsbIds(std::istream &in, std::unordered_map<uint16_t, std::string> &vendors)
{
    std::string line;
    while (std::getline(in, line)) {
        if (line.starts_with("C "))
            break;
        if (line.size() < 7 || !isHex(line[0]) || !isHex(line[1]) || !isHex(line[2]) ||
            !isHex(line[3]) || line[4] != ' ' || line[5] != ' ')
            continue;
        const auto id = static_cast<uint16_t>(std::stoul(line.substr(0, 4), nullptr, 16));
        std::string name = line.substr(6);
        while (!name.empty() && (name.back() == ' ' || name.back() == '\r'))
            name.pop_back();
        if (!name.empty())
            vendors.emplace(id, std::move(name));
    }
}

const UsbIds &usbIds()
{
    static UsbIds cache{"\x01", {}, {}};   // never equal to a real root
    if (cache.root != SysfsReader::root()) {
        cache = UsbIds{SysfsReader::root(), {}, {}};
        for (const char *candidate : kUsbIdsPaths) {
            const std::string path = SysfsReader::sysPath(candidate);
            std::ifstream in(path);
            if (!in)
                continue;
            parseUsbIds(in, cache.vendors);
            cache.path = path;
            break;
        }
    }
    return cache;
}

} // namespace

std::string VendorDB::lookupPd(uint16_t vendorId)
{
    // USB PD's "vendor not registered with USB-IF" value (PID must be 0).
    if (vendorId == 0xFFFF)
        return "No vendor ID assigned (USB-PD spec sentinel)";
    return lookup(vendorId);
}

std::string VendorDB::lookup(uint16_t vendorId)
{
    // usb.ids first: maintained and verbatim ("Apple, Inc."). A small
    // hand-curated table drifts into wrong names (upstream found several),
    // so the built-in list is only a fallback for systems without usb.ids
    // and for vendors it lacks (e.g. 0x291A Anker).
    const auto &ids = usbIds().vendors;
    if (auto it = ids.find(vendorId); it != ids.end())
        return it->second;
    if (auto it = kVendors.find(vendorId); it != kVendors.end())
        return it->second;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%04x", vendorId);
    return buf;
}

std::string VendorDB::usbIdsPath()
{
    return usbIds().path;
}

} // namespace WhatCable
