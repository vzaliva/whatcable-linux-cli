// USB PD VDO decoding (PDDecoder).
#include "TestFramework.h"
#include "Fixtures.h"
#include "PDDecoder.h"

using namespace WhatCable;
using wct::kActiveUsb4CableVdo1;
using wct::kActiveUsb4CableVdo2;

namespace {

// Passive Cable VDO fields: speed bits 2:0, VBUS current bits 6:5,
// max VBUS voltage bits 10:9.
constexpr uint32_t cableVdo(uint32_t speed, uint32_t current, uint32_t voltage)
{
    return speed | (current << 5) | (voltage << 9);
}

} // namespace

TEST_CASE(cable_watts_use_3A_default_and_48V_cap)
{
    struct Case { uint32_t current, voltage; int ratedV, watts; };
    const Case cases[] = {
        {0, 0, 20, 60},    // 00b "USB default": every Type-C cable carries 3 A
        {1, 0, 20, 60},    // 3 A
        {2, 0, 20, 100},   // 5 A
        {2, 1, 20, 100},   // 01b (was 30 V) is deprecated: 20 V (PD R3.2)
        {2, 2, 20, 100},   // 10b (was 40 V) is deprecated: 20 V
        {2, 3, 50, 240},   // 50 V rating, but PD never delivers above 48 V
        {3, 0, 20, 60},    // reserved current encoding -> default (3 A)
    };
    for (const auto &c : cases) {
        const auto vdo = decodeCableVDO(cableVdo(2, c.current, c.voltage), false);
        CHECK_EQ(vdo.maxVbusVolts, c.ratedV);
        CHECK_EQ(vdo.maxWatts, c.watts);
    }
}

TEST_CASE(cable_speed_and_current_fields)
{
    CHECK(decodeCableVDO(cableVdo(0, 1, 0), false).speed == CableSpeed::USB20);
    CHECK(decodeCableVDO(cableVdo(1, 1, 0), false).speed == CableSpeed::USB32Gen1);
    CHECK(decodeCableVDO(cableVdo(2, 1, 0), false).speed == CableSpeed::USB32Gen2);
    CHECK(decodeCableVDO(cableVdo(3, 1, 0), false).speed == CableSpeed::USB4Gen3);
    CHECK(decodeCableVDO(cableVdo(4, 1, 0), false).speed == CableSpeed::USB4Gen4);
    CHECK(decodeCableVDO(cableVdo(2, 2, 0), false).currentRating == CableCurrent::FiveAmp);
    CHECK(decodeCableVDO(cableVdo(2, 0, 0), true).isActive);
}

TEST_CASE(active_cable_vdo1_all_fields)
{
    const auto v = decodeCableVDO(kActiveUsb4CableVdo1, true);
    CHECK(v.speed == CableSpeed::USB4Gen3);
    CHECK(!v.speedReserved);
    CHECK(v.currentRating == CableCurrent::FiveAmp);
    CHECK_EQ(v.maxVbusVolts, 50);
    CHECK_EQ(v.maxWatts, 240);
    CHECK_EQ(v.terminationEncoded, 3);
    CHECK_EQ(cableTerminationLabel(v), "both ends active");
    CHECK_EQ(v.latencyEncoded, 1);
    CHECK_EQ(cableLatencyLabel(v), "up to 10 ns (about 1 m)");
    CHECK(v.eprCapable);
    CHECK(v.plug == CablePlug::TypeC);
    CHECK_EQ(v.vdoVersionEncoded, 3);
    CHECK(v.warnings.empty());
}

TEST_CASE(cable_vdo_spec_validity_warnings)
{
    // Field helpers. A valid passive base: latency 0001, termination 00,
    // version 000; a valid active base: latency 0001, termination 10.
    const uint32_t lat1 = 1u << 13;
    auto kinds = [](uint32_t vdo, bool active) {
        std::vector<int> k;
        for (const auto &w : decodeCableVDO(vdo, active).warnings)
            k.push_back(static_cast<int>(w.kind));
        return k;
    };
    using W = CableDecodeWarning;
    const std::vector<int> none;

    CHECK(kinds(lat1, false) == none);
    CHECK(kinds(lat1 | (2u << 11), true) == none);
    CHECK(kinds(lat1 | 5u, false) == std::vector<int>{W::ReservedSpeed});
    CHECK(decodeCableVDO(lat1 | 5u, false).speedReserved);
    CHECK(kinds(lat1 | (3u << 5), false) == std::vector<int>{W::ReservedCurrent});
    CHECK(kinds(lat1 | (0u << 5), false) == none);              // 00b deliberately not flagged
    CHECK(kinds(0, false) == std::vector<int>{W::ReservedLatency});
    CHECK(kinds(9u << 13, false) == std::vector<int>{W::ReservedLatency});
    CHECK(kinds((9u << 13) | (2u << 11), true) == none);        // optical length, active only
    CHECK(kinds((0xBu << 13) | (2u << 11), true) == std::vector<int>{W::ReservedLatency});
    CHECK(kinds(lat1 | (2u << 11), false) == std::vector<int>{W::InvalidTermination});
    CHECK(kinds(lat1 | (1u << 11), true) == std::vector<int>{W::InvalidTermination});
    CHECK(kinds(lat1 | (1u << 21), false) == std::vector<int>{W::InvalidVdoVersion});
    CHECK(kinds(lat1 | (2u << 11) | (2u << 21), true) == none); // v1.2 (deprecated) ok
    CHECK(kinds(lat1 | (2u << 11) | (1u << 21), true) == std::vector<int>{W::InvalidVdoVersion});
    CHECK(kinds(lat1 | (1u << 17), false) == std::vector<int>{W::EprWithLowMaxVoltage});
    CHECK(kinds(lat1 | (1u << 17) | (3u << 9), false) == none);
}

TEST_CASE(pd2_cable_layout_is_never_50V)
{
    // Bits 10:9 set, EPR bit and a version that would be invalid in PD 3.x.
    const uint32_t vdo = (1u << 13) | (2u << 5) | (3u << 9) | (1u << 17) | (5u << 21);
    const auto pd3 = decodeCableVDO(vdo, false);
    CHECK_EQ(pd3.maxWatts, 240);
    const auto pd2 = decodeCableVDO(vdo, false, true);
    CHECK(pd2.pd2Layout);
    CHECK_EQ(pd2.maxVbusVolts, 20);
    CHECK_EQ(pd2.maxWatts, 100);
    CHECK(!pd2.eprCapable);
    CHECK(pd2.warnings.empty());           // version / EPR fields are reserved in PD 2.0
}

TEST_CASE(passive_layout_has_no_vbus_through_cable)
{
    CHECK(!decodeCableVDO(1u << 4, false).vbusThroughCable);
    CHECK(decodeCableVDO(1u << 4, true).vbusThroughCable);
}

TEST_CASE(cable_latency_labels)
{
    auto label = [](int encoded, bool active) {
        CableVDO v;
        v.latencyEncoded = encoded;
        v.isActive = active;
        return cableLatencyLabel(v);
    };
    CHECK_EQ(label(2, false), "up to 20 ns (about 2 m)");
    CHECK_EQ(label(8, false), "over 70 ns (over 7 m)");
    CHECK_EQ(label(9, false), "");
    CHECK_EQ(label(9, true), "about 1000 ns (optical)");
    CHECK_EQ(label(0, true), "");
}

TEST_CASE(active_cable_vdo2_inverted_protocol_bits)
{
    const auto v = decodeActiveCableVDO2(kActiveUsb4CableVdo2);
    CHECK_EQ(v.maxOperatingTempC, 70);
    CHECK_EQ(v.shutdownTempC, 85);
    CHECK(v.u3CLdPower == U3CLdPower::From05To1mW);
    CHECK(!v.optical);
    CHECK(v.retimer);
    CHECK(v.usb4Supported);
    CHECK(v.usb32Supported);
    CHECK(v.usb2Supported);
    CHECK(v.twoLanes);
    CHECK(v.gen2OrHigher);

    // Setting the "not supported" bits (8, 5, 4) clears support.
    const auto none = decodeActiveCableVDO2((1u << 8) | (1u << 5) | (1u << 4) | (1u << 10) | (1u << 2));
    CHECK(!none.usb4Supported);
    CHECK(!none.usb2Supported);
    CHECK(!none.usb32Supported);
    CHECK(none.optical);
    CHECK(none.opticallyIsolated);
    CHECK(!none.retimer);
}

TEST_CASE(id_header_fields)
{
    const auto hdr = decodeIDHeader((1u << 31) | (1u << 26) | (4u << 27) | 0x05AC);
    CHECK(hdr.usbCommCapableAsHost);
    CHECK(!hdr.usbCommCapableAsDevice);
    CHECK(hdr.modalOperation);
    CHECK(hdr.ufpProductType == ProductType::ActiveCable);
    CHECK_EQ(hdr.vendorId, 0x05AC);
}

TEST_CASE(dfp_product_types)
{
    CHECK(decodeIDHeader(1u << 23).dfpProductType == ProductType::Hub);
    CHECK(decodeIDHeader(2u << 23).dfpProductType == ProductType::Host);
    CHECK(decodeIDHeader(3u << 23).dfpProductType == ProductType::PowerBrick);
    CHECK(decodeIDHeader(4u << 23).dfpProductType == ProductType::Undefined);
}

TEST_CASE(partner_product_type_uses_sop_meanings)
{
    auto partner = [](uint32_t header) {
        return productTypeLabel(partnerProductType(decodeIDHeader(header)));
    };
    CHECK_EQ(partner(3u << 23), "Power Brick");                  // charger: no UFP type
    CHECK_EQ(partner(2u << 23), "USB Host");
    CHECK_EQ(partner((2u << 27) | (2u << 23)), "USB Peripheral"); // UFP wins
    CHECK_EQ(partner(3u << 27), "Power Sink Device");            // code 3 on SOP
    CHECK_EQ(partner(0), "Unknown");
    // The same code 3 is a passive cable when decoded for a cable plug.
    CHECK(decodeIDHeader(3u << 27).ufpProductType == ProductType::PassiveCable);
}
