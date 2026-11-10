// Derived from WhatCable by Darryl Morley (https://github.com/darrylmorley/whatcable)
// Port of PDVDO.swift — USB PD 3.x VDO bit-field decoding
#include "PDDecoder.h"
#include <cstdio>

namespace WhatCable {

std::string productTypeLabel(ProductType type)
{
    switch (type) {
    case ProductType::Hub:          return "USB Hub";
    case ProductType::Peripheral:   return "USB Peripheral";
    case ProductType::PassiveCable: return "Passive Cable";
    case ProductType::ActiveCable:  return "Active Cable";
    case ProductType::AMA:          return "Alternate Mode Adapter";
    case ProductType::VPD:          return "VCONN-Powered Device";
    case ProductType::Other:        return "Other";
    case ProductType::Host:         return "USB Host";
    case ProductType::PowerBrick:   return "Power Brick";
    case ProductType::PSD:          return "Power Sink Device";
    default:                        return "Unknown";
    }
}

std::string cableSpeedLabel(CableSpeed speed)
{
    switch (speed) {
    case CableSpeed::USB20:     return "USB 2.0";
    case CableSpeed::USB32Gen1: return "USB 3.2 Gen 1 (5 Gbps)";
    case CableSpeed::USB32Gen2: return "USB 3.2 Gen 2 (10 Gbps)";
    case CableSpeed::USB4Gen3:  return "USB4 Gen 3 (20/40 Gbps)";
    case CableSpeed::USB4Gen4:  return "USB4 Gen 4 (40/80 Gbps)";
    default:                    return "Unknown";
    }
}

int cableSpeedMaxGbps(CableSpeed speed)
{
    switch (speed) {
    case CableSpeed::USB20:     return 0;
    case CableSpeed::USB32Gen1: return 5;
    case CableSpeed::USB32Gen2: return 10;
    case CableSpeed::USB4Gen3:  return 40;
    case CableSpeed::USB4Gen4:  return 80;
    default:                    return 0;
    }
}

std::string cableCurrentLabel(CableCurrent current)
{
    switch (current) {
    case CableCurrent::USBDefault: return "USB Default";
    case CableCurrent::ThreeAmp:   return "3A";
    case CableCurrent::FiveAmp:    return "5A";
    default:                       return "Unknown";
    }
}

double cableCurrentMaxAmps(CableCurrent current)
{
    switch (current) {
    case CableCurrent::FiveAmp:  return 5.0;
    // Every Type-C cable must carry 3A. Real USB 2.0 e-markers report 00b
    // routinely, so treat "USB default" as 3A, as upstream does.
    default:                     return 3.0;
    }
}

IDHeaderVDO decodeIDHeader(uint32_t vdo)
{
    IDHeaderVDO hdr;
    hdr.usbCommCapableAsHost = (vdo >> 31) & 1;
    hdr.usbCommCapableAsDevice = (vdo >> 30) & 1;
    hdr.modalOperation = (vdo >> 26) & 1;
    hdr.vendorId = static_cast<uint16_t>(vdo & 0xFFFF);

    uint8_t ufpBits = (vdo >> 27) & 0x7;
    switch (ufpBits) {
    case 1: hdr.ufpProductType = ProductType::Hub; break;
    case 2: hdr.ufpProductType = ProductType::Peripheral; break;
    case 3: hdr.ufpProductType = ProductType::PassiveCable; break;
    case 4: hdr.ufpProductType = ProductType::ActiveCable; break;
    case 5: hdr.ufpProductType = ProductType::AMA; break;
    case 6: hdr.ufpProductType = ProductType::VPD; break;
    default: hdr.ufpProductType = ProductType::Undefined; break;
    }

    // PD 3.x ID Header, Product Type (DFP): 1 = PDUSB Hub, 2 = PDUSB Host,
    // 3 = Power Brick; other values are reserved.
    uint8_t dfpBits = (vdo >> 23) & 0x7;
    switch (dfpBits) {
    case 1: hdr.dfpProductType = ProductType::Hub; break;
    case 2: hdr.dfpProductType = ProductType::Host; break;
    case 3: hdr.dfpProductType = ProductType::PowerBrick; break;
    default: hdr.dfpProductType = ProductType::Undefined; break;
    }

    return hdr;
}

ProductType partnerProductType(const IDHeaderVDO &hdr)
{
    switch (hdr.ufpProductType) {
    case ProductType::Undefined:
        return hdr.dfpProductType;
    case ProductType::PassiveCable:     // same code (3) means PSD on SOP
        return ProductType::PSD;
    case ProductType::ActiveCable:      // code 4 is reserved on SOP
        return hdr.dfpProductType;
    default:
        return hdr.ufpProductType;
    }
}

CableVDO decodeCableVDO(uint32_t vdo, bool isActive, bool pd2Layout)
{
    CableVDO cable;
    cable.isActive = isActive;
    cable.pd2Layout = pd2Layout;
    auto warn = [&cable](CableDecodeWarning::Kind kind, int value) {
        cable.warnings.push_back({kind, value});
    };

    cable.speedEncoded = static_cast<int>(vdo & 0x7);
    switch (cable.speedEncoded) {
    case 0: cable.speed = CableSpeed::USB20; break;
    case 1: cable.speed = CableSpeed::USB32Gen1; break;
    case 2: cable.speed = CableSpeed::USB32Gen2; break;
    case 3: cable.speed = CableSpeed::USB4Gen3; break;
    case 4: cable.speed = CableSpeed::USB4Gen4; break;
    default:
        cable.speed = CableSpeed::USB20;
        cable.speedReserved = true;
        warn(CableDecodeWarning::ReservedSpeed, cable.speedEncoded);
        break;
    }

    // Bits 4:3 are reserved in the passive layout; in the active layout
    // bit 4 is "VBUS through cable" and bit 3 "SOP'' controller present".
    cable.vbusThroughCable = isActive && ((vdo >> 4) & 1);
    cable.sopDoublePrimePresent = (vdo >> 3) & 1;

    const int currentBits = static_cast<int>((vdo >> 5) & 0x3);
    switch (currentBits) {
    case 1: cable.currentRating = CableCurrent::ThreeAmp; break;
    case 2: cable.currentRating = CableCurrent::FiveAmp; break;
    default: cable.currentRating = CableCurrent::USBDefault; break;
    }
    // 00b is also Invalid in the spec, but real USB 2.0 cables emit it
    // routinely, so only the reserved 11b is reported (as upstream does).
    if (currentBits == 3)
        warn(CableDecodeWarning::ReservedCurrent, currentBits);

    // Max VBUS voltage: 11b = 50V. 01b (30V) and 10b (40V) are deprecated
    // and mean 20V (USB PD R3.2): only a 50V-rated cable may carry EPR.
    // PD 2.0 e-markers use bits 10:9 for SuperSpeed directionality and
    // predate EPR, so they are 20V cables whatever those bits hold.
    cable.maxVbusVoltageEncoded = static_cast<int>((vdo >> 9) & 0x3);
    cable.maxVbusVolts = (!pd2Layout && cable.maxVbusVoltageEncoded == 3) ? 50 : 20;

    cable.terminationEncoded = static_cast<int>((vdo >> 11) & 0x3);
    // Passive: 00 / 01 valid. Active: 10 / 11 valid.
    if (isActive ? cable.terminationEncoded < 2 : cable.terminationEncoded >= 2)
        warn(CableDecodeWarning::InvalidTermination, cable.terminationEncoded);

    // Latency: 0000 is Invalid; passive cables also reject 1001..1111,
    // active cables accept 1001 / 1010 (optical lengths) and reject 1011+.
    cable.latencyEncoded = static_cast<int>((vdo >> 13) & 0xF);
    if (cable.latencyEncoded == 0 || cable.latencyEncoded >= (isActive ? 0xB : 0x9))
        warn(CableDecodeWarning::ReservedLatency, cable.latencyEncoded);

    cable.plug = static_cast<CablePlug>((vdo >> 18) & 0x3);

    if (!pd2Layout) {
        // Bits 23:21 and 17 are reserved in PD 2.0.
        cable.eprCapable = (vdo >> 17) & 1;

        // VDO version: passive cables only 000 (v1.0); active cables 000,
        // 010 and 011 (v1.0 and v1.2 deprecated, v1.3).
        cable.vdoVersionEncoded = static_cast<int>((vdo >> 21) & 0x7);
        const int ver = cable.vdoVersionEncoded;
        if (isActive ? !(ver == 0 || ver == 2 || ver == 3) : ver != 0)
            warn(CableDecodeWarning::InvalidVdoVersion, ver);

        // EPR needs a 50V-rated cable. Active cables' EPR semantics need VDO 2.
        if (!isActive && cable.eprCapable && cable.maxVbusVoltageEncoded != 3)
            warn(CableDecodeWarning::EprWithLowMaxVoltage, cable.maxVbusVoltageEncoded);
    }

    // USB PD never delivers above 48V (EPR fixed and AVS both top out
    // there); a 50V rating is insulation headroom. 50V/5A cable -> 240W.
    const int deliverableVolts = cable.maxVbusVolts < 48 ? cable.maxVbusVolts : 48;
    const double amps = cableCurrentMaxAmps(cable.currentRating);
    cable.maxWatts = static_cast<int>(deliverableVolts * amps + 0.5);

    return cable;
}

ActiveCableVDO2 decodeActiveCableVDO2(uint32_t vdo)
{
    ActiveCableVDO2 v;
    v.maxOperatingTempC = static_cast<int>((vdo >> 24) & 0xFF);
    v.shutdownTempC = static_cast<int>((vdo >> 16) & 0xFF);
    v.u3CLdPower = static_cast<U3CLdPower>((vdo >> 12) & 0x7);
    v.u3ToU0ThroughU3S = (vdo >> 11) & 1;
    v.optical = (vdo >> 10) & 1;
    v.retimer = (vdo >> 9) & 1;
    v.usb4Supported = !((vdo >> 8) & 1);    // inverted in the spec
    v.usb2HubHopsConsumed = static_cast<int>((vdo >> 6) & 0x3);
    v.usb2Supported = !((vdo >> 5) & 1);    // inverted
    v.usb32Supported = !((vdo >> 4) & 1);   // inverted
    v.twoLanes = (vdo >> 3) & 1;
    v.opticallyIsolated = (vdo >> 2) & 1;
    v.usb4Asymmetric = (vdo >> 1) & 1;
    v.gen2OrHigher = vdo & 1;
    return v;
}

std::string cablePlugLabel(CablePlug plug)
{
    switch (plug) {
    case CablePlug::TypeA:   return "Type-A";
    case CablePlug::TypeB:   return "Type-B";
    case CablePlug::TypeC:   return "Type-C";
    case CablePlug::Captive: return "captive";
    }
    return "Type-C";
}

std::string cableLatencyLabel(const CableVDO &cable)
{
    const int e = cable.latencyEncoded;
    char buf[64];
    // Roughly 10 ns per metre of copper.
    if (e >= 1 && e <= 7) {
        std::snprintf(buf, sizeof(buf), "up to %d ns (about %d m)", e * 10, e);
        return buf;
    }
    if (e == 8)
        return "over 70 ns (over 7 m)";
    if (cable.isActive && e == 9)
        return "about 1000 ns (optical)";
    if (cable.isActive && e == 10)
        return "about 2000 ns (optical)";
    return {};
}

std::string cableTerminationLabel(const CableVDO &cable)
{
    switch (cable.terminationEncoded) {
    case 0: return cable.isActive ? std::string() : "VCONN not required";
    case 1: return cable.isActive ? std::string() : "VCONN required";
    case 2: return cable.isActive ? "one end active" : std::string();
    case 3: return cable.isActive ? "both ends active" : std::string();
    }
    return {};
}

std::string u3CLdPowerLabel(U3CLdPower power)
{
    switch (power) {
    case U3CLdPower::Over10mW:      return "> 10 mW";
    case U3CLdPower::From5To10mW:   return "5-10 mW";
    case U3CLdPower::From1To5mW:    return "1-5 mW";
    case U3CLdPower::From05To1mW:   return "0.5-1 mW";
    case U3CLdPower::From02To05mW:  return "0.2-0.5 mW";
    case U3CLdPower::From50To200uW: return "50-200 uW";
    case U3CLdPower::Under50uW:     return "< 50 uW";
    case U3CLdPower::Reserved:      return "reserved";
    }
    return "reserved";
}

} // namespace WhatCable
