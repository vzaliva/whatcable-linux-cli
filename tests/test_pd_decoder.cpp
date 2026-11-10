// USB PD VDO decoding (PDDecoder).
#include "TestFramework.h"
#include "PDDecoder.h"

using namespace WhatCable;

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
        {2, 1, 30, 150},
        {2, 2, 40, 200},
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
