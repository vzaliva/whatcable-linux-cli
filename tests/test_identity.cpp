// Type-C identity reading (TypeCPort) and cable decoding (CableInfo).
#include "TestFramework.h"
#include "Fixtures.h"
#include "CableInfo.h"
#include "TypeCPort.h"

using namespace WhatCable;
using namespace wct;

namespace {

std::optional<CableInfo> cableOnPort0(const std::string &sysfsType, const Identity &id,
                                      const std::string &pdRevision = "3.0")
{
    FakeSysfs fs;
    addTypecPort(fs, 0, "[source] sink", "[host] device", "usb_power_delivery");
    addCable(fs, 0, sysfsType, id, pdRevision);
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

TEST_CASE(active_cable_full_decode)
{
    Identity id;
    id.idHeader = (4u << 27) | 0x05AC;
    id.certStat = 12345;
    id.vdo1 = kActiveUsb4CableVdo1;
    id.vdo2 = kActiveUsb4CableVdo2;
    const auto cable = cableOnPort0("active", id);
    REQUIRE(cable);
    REQUIRE(cable->vdo);
    REQUIRE(cable->activeVdo2);
    CHECK(cable->activeVdo2->retimer);
    CHECK(cable->speed == CableSpeed::USB4Gen3);
    CHECK_EQ(cable->certificationXid, std::optional<uint32_t>(12345));
    CHECK(cable->trustNotes.empty());
}

TEST_CASE(pd2_active_cable_has_no_vdo2)
{
    Identity id;
    id.idHeader = (4u << 27) | 0x05AC;
    id.vdo1 = (1u << 13) | (2u << 11) | (2u << 5) | (3u << 9);
    id.vdo2 = kActiveUsb4CableVdo2;        // VDO 2 doesn't exist in PD 2.0
    const auto cable = cableOnPort0("active", id, "2.0");
    REQUIRE(cable);
    CHECK_EQ(cable->pdRevision, "2.0");
    CHECK(!cable->activeVdo2);
    CHECK_EQ(cable->maxWatts, 100);
}

TEST_CASE(passive_cable_ignores_vdo2)
{
    Identity id;
    id.idHeader = (3u << 27) | 0x05AC;
    id.vdo1 = (1u << 13) | (1u << 5);
    id.vdo2 = kActiveUsb4CableVdo2;        // must not be read for a passive cable
    const auto cable = cableOnPort0("passive", id);
    REQUIRE(cable);
    CHECK(!cable->activeVdo2);
    CHECK(!cable->certificationXid);       // cert_stat 0 = no XID
}

TEST_CASE(vconn_powered_device_is_not_decoded_as_a_cable)
{
    Identity id;
    id.idHeader = (6u << 27) | 0x05AC;     // VPD
    id.vdo1 = 0x642;                       // a VPD VDO; as a cable it would read 240 W
    const auto cable = cableOnPort0("passive", id);
    REQUIRE(cable);
    CHECK(cable->isVpd);
    CHECK(!cable->isPassive && !cable->isActive);
    CHECK_EQ(cable->cableType, "vpd");
    CHECK(!cable->vdo);
    CHECK_EQ(cable->maxWatts, 0);
}

TEST_CASE(reserved_speed_is_not_reported_as_usb2)
{
    Identity id;
    id.idHeader = (3u << 27) | 0x05AC;
    id.vdo1 = (1u << 13) | 6u;             // speed 6 is reserved
    const auto cable = cableOnPort0("", id);
    REQUIRE(cable);
    CHECK(!cable->speed);
    REQUIRE(cable->vdo);
    CHECK(cable->vdo->speedReserved);
    REQUIRE(cable->trustNotes.size() == 1);
    CHECK_EQ(cable->trustNotes[0].code, "reservedSpeedEncoding");
}

TEST_CASE(trust_notes_are_hedged_and_skip_the_ffff_sentinel)
{
    const auto clean = decodeCableVDO((1u << 13) | (1u << 5), false);
    CHECK(cableTrustNotes(0x05AC, &clean).empty());
    CHECK(cableTrustNotes(0xFFFF, &clean).empty());      // spec's "no vendor ID"

    const auto zero = cableTrustNotes(0x0000, &clean);
    REQUIRE(zero.size() == 1);
    CHECK_EQ(zero[0].code, "zeroVendorID");
    CHECK(zero[0].detail.find("common on genuine cables") != std::string::npos);

    const auto bad = decodeCableVDO((1u << 17) | (2u << 11), false);   // EPR@20V, term 10, latency 0
    std::vector<std::string> codes;
    for (const auto &n : cableTrustNotes(0x05AC, &bad))
        codes.push_back(n.code);
    CHECK(codes == (std::vector<std::string>{"invalidCableTermination",
                                             "reservedCableLatencyEncoding",
                                             "eprClaimedWithLowMaxVoltage"}));
    for (const auto &n : cableTrustNotes(0, &bad)) {
        CHECK(n.title.find("fake") == std::string::npos);
        CHECK(n.detail.find("counterfeit") == std::string::npos);
    }
}

TEST_CASE(partners_and_cables_are_not_listed_as_ports)
{
    FakeSysfs fs;
    buildDevLaptop(fs);
    const auto ports = TypeCPort::enumerate();
    REQUIRE(ports.size() == 3);
    CHECK_EQ(ports[0].portNumber, 0);
    CHECK_EQ(ports[1].portNumber, 1);
    CHECK_EQ(ports[0].pdName, "pd0");
    REQUIRE(ports[0].partner);
    CHECK_EQ(ports[0].partner->pdName, "pd2");
    CHECK(ports[0].isSink());
    CHECK(!ports[1].isSink());
    CHECK(ports[1].cable.has_value());
}
