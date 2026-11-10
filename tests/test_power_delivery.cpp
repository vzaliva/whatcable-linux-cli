// USB PD capability parsing and active-PDO matching (PowerDelivery).
#include "TestFramework.h"
#include "Fixtures.h"
#include "PowerDelivery.h"

using namespace WhatCable;
using namespace wct;

namespace {

std::optional<PowerDeliveryPort> findPd(const std::vector<PowerDeliveryPort> &all,
                                        const std::string &name)
{
    for (const auto &pd : all)
        if (pd.name == name)
            return pd;
    return std::nullopt;
}

PowerDataObject fixed(int index, int mv)
{
    PowerDataObject p;
    p.index = index;
    p.type = PowerDataObject::FixedSupply;
    p.voltageMV = mv;
    return p;
}

PowerDataObject pps(int index, int minMV, int maxMV)
{
    PowerDataObject p;
    p.index = index;
    p.type = PowerDataObject::PPS;
    p.voltageMV = minMV;
    p.maxVoltageMV = maxMV;
    return p;
}

int activeAfter(PowerDeliveryPort port, int mv, bool ppsContract)
{
    port.markActiveSourcePdo(mv, ppsContract);
    return port.activeSourcePdoIndex.value_or(-1);
}

} // namespace

TEST_CASE(pdo_directories_are_parsed_by_name)
{
    FakeSysfs fs;
    std::vector<Pdo> caps;
    for (int i = 0; i < 9; ++i)
        caps.push_back({"fixed_supply", 5000 + i * 1000, 0, 3000});
    caps.push_back({"fixed_supply", 20000, 0, 5000});              // "10:fixed_supply"
    caps.push_back({"programmable_supply", 3300, 21000, 3000});    // "11:programmable_supply"
    addPd(fs, "pd7", "/sys/devices/platform/x", "/sys/devices/platform/x", caps);

    const auto pd = findPd(PowerDeliveryPort::enumerate(), "pd7");
    REQUIRE(pd);
    const auto &src = pd->sourceCapabilities;
    REQUIRE(src.size() == 11);                    // "power/" and "uevent" skipped
    CHECK_EQ(src[0].index, 1);
    CHECK_EQ(src[9].index, 10);                   // numeric, not lexical, order
    CHECK(src[0].type == PowerDataObject::FixedSupply);
    CHECK_EQ(src[0].voltageMV, 5000);             // "5000mV"
    CHECK_EQ(src[0].currentMA, 3000);
    CHECK_EQ(src[0].powerMW, 15000);
    CHECK(src[10].type == PowerDataObject::PPS);
    CHECK_EQ(src[10].voltageMV, 3300);
    CHECK_EQ(src[10].maxVoltageMV, 21000);
    CHECK_EQ(src[10].voltageLabel(), "3.3-21.0V");
    CHECK_EQ(pd->maxSourcePowerMW, 100000);
}

TEST_CASE(pps_range_does_not_inflate_charger_rating)
{
    FakeSysfs fs;
    addPd(fs, "pd4", "/sys/devices/platform/x", "/sys/devices/platform/x",
          {{"fixed_supply", 5000, 0, 3000},
           {"fixed_supply", 20000, 0, 2250},
           {"programmable_supply", 3300, 21000, 3000}});   // 63 W "max"
    const auto pd = findPd(PowerDeliveryPort::enumerate(), "pd4");
    REQUIRE(pd);
    CHECK_EQ(pd->maxSourcePowerMW, 45000);
}

TEST_CASE(pps_only_charger_is_rated_by_pps)
{
    FakeSysfs fs;
    addPd(fs, "pd5", "/sys/devices/platform/x", "/sys/devices/platform/x",
          {{"programmable_supply", 3300, 11000, 3000}});
    const auto pd = findPd(PowerDeliveryPort::enumerate(), "pd5");
    REQUIRE(pd);
    CHECK_EQ(pd->maxSourcePowerMW, 33000);
}

TEST_CASE(active_pdo_matching)
{
    PowerDeliveryPort onePps;
    onePps.sourceCapabilities = {fixed(1, 5000), fixed(2, 9000), pps(3, 3300, 11000)};
    PowerDeliveryPort twoPps;
    twoPps.sourceCapabilities = {fixed(1, 5000), fixed(2, 9000), pps(3, 3300, 11000),
                                 pps(4, 3300, 21000)};

    CHECK_EQ(activeAfter(onePps, 9000, false), 2);   // fixed contract
    CHECK_EQ(activeAfter(onePps, 9050, false), 2);   // within tolerance
    CHECK_EQ(activeAfter(onePps, 12000, false), -1); // no such fixed PDO
    CHECK_EQ(activeAfter(onePps, 9000, true), 3);    // PPS at 9 V is not the 9 V fixed
    CHECK_EQ(activeAfter(onePps, 2500, true), 3);    // single PPS: voltage not needed
    CHECK_EQ(activeAfter(twoPps, 15000, true), 4);   // only #4 reaches 15 V
    CHECK_EQ(activeAfter(twoPps, 9000, true), -1);   // ambiguous
}

TEST_CASE(marked_pdo_sets_flag_and_index)
{
    PowerDeliveryPort port;
    port.sourceCapabilities = {fixed(1, 5000), fixed(2, 20000)};
    port.markActiveSourcePdo(20000);
    CHECK(!port.sourceCapabilities[0].isActive);
    CHECK(port.sourceCapabilities[1].isActive);
    CHECK_EQ(port.activeSourcePdoIndex, std::optional<int>(2));
}
