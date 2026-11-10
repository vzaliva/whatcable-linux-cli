// Derived from WhatCable by Darryl Morley (https://github.com/darrylmorley/whatcable)
// Port of PDVDO.swift — USB Power Delivery VDO bit-field decoding
#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace WhatCable {

enum class ProductType : uint8_t {
    Undefined = 0,
    Hub,
    Peripheral,
    PassiveCable,
    ActiveCable,
    AMA,         // Alternate Mode Adapter
    VPD,         // VCONN-Powered Device
    Other,
    Host,        // DFP only
    PowerBrick,  // DFP only
    PSD          // SOP UFP code 3: Power Sink Device
};

enum class CableSpeed : uint8_t {
    USB20 = 0,
    USB32Gen1,   // 5 Gbps
    USB32Gen2,   // 10 Gbps
    USB4Gen3,    // 20 Gbps (single-lane) / 40 Gbps (dual-lane)
    USB4Gen4     // 40 Gbps (single-lane) / 80 Gbps (dual-lane)
};

enum class CableCurrent : uint8_t {
    USBDefault = 0,
    ThreeAmp,
    FiveAmp
};

struct IDHeaderVDO {
    bool usbCommCapableAsHost = false;
    bool usbCommCapableAsDevice = false;
    bool modalOperation = false;
    ProductType ufpProductType = ProductType::Undefined;
    ProductType dfpProductType = ProductType::Undefined;
    uint16_t vendorId = 0;
};

enum class CablePlug : uint8_t { TypeA = 0, TypeB, TypeC, Captive };

// A Cable VDO field holding a value the USB PD spec reserves or marks
// Invalid for this cable type. `value` is the raw field.
struct CableDecodeWarning {
    enum Kind {
        ReservedSpeed,
        ReservedCurrent,
        ReservedLatency,
        InvalidVdoVersion,
        InvalidTermination,
        EprWithLowMaxVoltage    // passive cable: EPR capable but not 50V-rated
    };
    Kind kind;
    int value = 0;
};

// Passive / Active Cable VDO 1 (USB PD R3.2 Tables 6.42 / 6.43).
struct CableVDO {
    CableSpeed speed = CableSpeed::USB20;
    bool speedReserved = false;  // bits 2:0 = 5..7; `speed` is then USB20
    int speedEncoded = 0;
    CableCurrent currentRating = CableCurrent::USBDefault;
    bool vbusThroughCable = false;   // bit 4, active cables only
    int maxVbusVoltageEncoded = 0;   // bits 10:9
    int maxVbusVolts = 20;     // 50 for 11b; 00b and the deprecated 01b/10b mean 20V
    bool isActive = false;
    int maxWatts = 0;          // deliverable: min(rated V, 48V) x current
    CablePlug plug = CablePlug::TypeC;   // bits 19:18
    int latencyEncoded = 0;          // bits 16:13
    int vdoVersionEncoded = 0;       // bits 23:21
    int terminationEncoded = 0;      // bits 12:11
    bool eprCapable = false;         // bit 17
    bool sopDoublePrimePresent = false;  // bit 3, active layout only
    // Decoded as a PD 2.0 e-marker: bits 10:9 (SuperSpeed directionality
    // there, not max voltage), the VDO version and EPR bits are not read.
    bool pd2Layout = false;
    std::vector<CableDecodeWarning> warnings;
};

enum class U3CLdPower : uint8_t {
    Over10mW = 0, From5To10mW, From1To5mW, From05To1mW, From02To05mW,
    From50To200uW, Under50uW, Reserved
};

// Active Cable VDO 2 (USB PD R3.2 Table 6.44). The USB4 / USB 3.2 /
// USB 2.0 bits are inverted in the spec (0 = supported); decoded here as
// true = supported.
struct ActiveCableVDO2 {
    int maxOperatingTempC = 0;       // 0 = not specified
    int shutdownTempC = 0;           // 0 = not specified
    U3CLdPower u3CLdPower = U3CLdPower::Reserved;
    bool u3ToU0ThroughU3S = false;
    bool optical = false;            // physical connection: copper / optical
    bool retimer = false;            // active element: re-driver / re-timer
    bool usb4Supported = false;
    int usb2HubHopsConsumed = 0;
    bool usb2Supported = false;
    bool usb32Supported = false;
    bool twoLanes = false;
    bool opticallyIsolated = false;
    bool usb4Asymmetric = false;
    bool gen2OrHigher = false;
};

std::string productTypeLabel(ProductType type);
std::string cableSpeedLabel(CableSpeed speed);
int cableSpeedMaxGbps(CableSpeed speed);
std::string cableCurrentLabel(CableCurrent current);
double cableCurrentMaxAmps(CableCurrent current);

// UFP type codes are decoded for a cable plug (SOP'): 3 = passive cable,
// 4 = active cable. For a port partner (SOP) use partnerProductType().
IDHeaderVDO decodeIDHeader(uint32_t vdo);

// What a port partner (SOP) is, from its ID header: the UFP product type
// read with SOP meanings (3 = PSD), falling back to the DFP type for pure
// sources such as chargers. Undefined if neither is set.
ProductType partnerProductType(const IDHeaderVDO &hdr);
// pd2Layout: the e-marker reports USB PD 2.0. PD 2.0 predates EPR, so such
// a cable is never 50V-rated whatever bits 10:9 hold.
CableVDO decodeCableVDO(uint32_t vdo, bool isActive, bool pd2Layout = false);
ActiveCableVDO2 decodeActiveCableVDO2(uint32_t vdo);

std::string cablePlugLabel(CablePlug plug);
// "up to 20 ns (about 2 m)"; empty for reserved latency encodings.
std::string cableLatencyLabel(const CableVDO &cable);
// e.g. "VCONN required" (passive) or "both ends active" (active); empty if invalid.
std::string cableTerminationLabel(const CableVDO &cable);
std::string u3CLdPowerLabel(U3CLdPower power);

} // namespace WhatCable
