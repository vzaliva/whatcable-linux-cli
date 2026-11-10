// Type-C identity reading (TypeCPort) and cable decoding (CableInfo).
#include "TestFramework.h"
#include "Fixtures.h"
#include "CableInfo.h"
#include "TypeCPort.h"

using namespace WhatCable;
using namespace wct;

namespace {

std::optional<CableInfo> cableOnPort0(const std::string &sysfsType, const Identity &id)
{
    FakeSysfs fs;
    addTypecPort(fs, 0, "[source] sink", "[host] device", "usb_power_delivery");
    addCable(fs, 0, sysfsType, id);
    const auto ports = TypeCPort::enumerate();
    if (ports.size() != 1 || !ports[0].cable)
        return std::nullopt;
    return CableInfo::fromTypeCCable(*ports[0].cable);
}

} // namespace

TEST_CASE(identity_is_read_by_name_not_sorted_position)
{
    // cert_stat sorts first; decoded as a header it would say "active cable".
    Identity id;
    id.idHeader = (3u << 27) | 0x05AC;     // passive cable
    id.certStat = 4u << 27;
    id.product = 0x12340100;
    id.vdo1 = 0x22;                        // USB 3.2 Gen 2, 3 A, 20 V
    const auto cable = cableOnPort0("", id);
    REQUIRE(cable);
    CHECK(cable->isPassive);
    CHECK(!cable->isActive);
    CHECK_EQ(cable->cableType, "passive");
    CHECK_EQ(cable->vendorId, 0x05AC);
    CHECK(cable->speed == CableSpeed::USB32Gen2);
    CHECK_EQ(cable->maxWatts, 60);
    REQUIRE(cable->identity);
    CHECK_EQ(cable->identity->certStat, 4u << 27);
    CHECK_EQ(cable->identity->productId, 0x1234);   // bits 31:16, not bcdDevice
}

TEST_CASE(cable_header_overrides_sysfs_type)
{
    Identity id;
    id.idHeader = (4u << 27) | 0x05AC;     // active cable
    id.certStat = 3u << 27;
    id.vdo1 = 0x642;                       // 5 A, 50 V
    const auto cable = cableOnPort0("passive", id);
    REQUIRE(cable);
    CHECK(cable->isActive);
    CHECK_EQ(cable->cableType, "active");
    CHECK_EQ(cable->ratedVbusVolts, 50);
    CHECK_EQ(cable->maxWatts, 240);
}

TEST_CASE(all_zero_identity_is_no_identity)
{
    const auto cable = cableOnPort0("active", Identity{});
    REQUIRE(cable);
    CHECK(!cable->identity);
    CHECK(cable->isActive);                // the sysfs type still counts
    CHECK(!cable->speed);
    CHECK_EQ(cable->maxWatts, 0);          // nothing fabricated from zeros
    CHECK_EQ(cable->vendorId, 0);
}

TEST_CASE(partners_and_cables_are_not_listed_as_ports)
{
    FakeSysfs fs;
    buildDevLaptop(fs);
    const auto ports = TypeCPort::enumerate();
    REQUIRE(ports.size() == 2);
    CHECK_EQ(ports[0].portNumber, 0);
    CHECK_EQ(ports[1].portNumber, 1);
    CHECK_EQ(ports[0].pdName, "pd0");
    REQUIRE(ports[0].partner);
    CHECK_EQ(ports[0].partner->pdName, "pd2");
    CHECK(ports[0].isSink());
    CHECK(!ports[1].isSink());
    CHECK(ports[1].cable.has_value());
}
