// Charging verdict (ChargingDiagnostic), system battery, power-supply helpers.
#include "TestFramework.h"
#include "Fixtures.h"
#include "Battery.h"
#include "ChargingDiagnostic.h"
#include "TypeCPort.h"

using namespace WhatCable;
using namespace wct;
using Status = SystemBattery::Status;

namespace {

PowerDeliveryPort charger(int maxW)
{
    PowerDeliveryPort pd;
    PowerDataObject p;
    p.index = 1;
    p.type = PowerDataObject::FixedSupply;
    p.voltageMV = 20000;
    p.powerMW = maxW * 1000;
    p.currentMA = maxW * 50;
    pd.sourceCapabilities = {p};
    pd.maxSourcePowerMW = maxW * 1000;
    return pd;
}

std::optional<CableInfo> cableRated(int watts)
{
    CableInfo c;
    c.maxWatts = watts;
    return c;
}

std::optional<SystemBattery> battery(Status status, std::optional<int> capacity = {},
                                     std::optional<int> limit = {})
{
    SystemBattery b;
    b.status = status;
    b.capacityPercent = capacity;
    b.chargeLimitPercent = limit;
    return b;
}

ChargingDiagnostic eval(int chargerW, std::optional<CableInfo> cable,
                        std::optional<int> negotiatedMW,
                        std::optional<SystemBattery> bat = std::nullopt)
{
    return ChargingDiagnostic::evaluate(charger(chargerW), cable, negotiatedMW, bat).value();
}

} // namespace

TEST_CASE(no_source_capabilities_no_verdict)
{
    CHECK(!ChargingDiagnostic::evaluate(PowerDeliveryPort{}, std::nullopt, 45000));
}

TEST_CASE(verdict_branches)
{
    auto d = eval(100, cableRated(60), 60000);
    CHECK(d.bottleneck == ChargingDiagnostic::CableLimit);
    CHECK(d.isWarning);
    CHECK_EQ(d.cableMaxW, 60);

    d = eval(100, std::nullopt, std::nullopt);       // contract not reported
    CHECK(d.bottleneck == ChargingDiagnostic::NegotiationUnknown);
    CHECK_EQ(d.summary, "Charger offers up to 100W");
    CHECK(!d.isWarning);
    CHECK(!d.negotiatedW);

    d = eval(100, std::nullopt, 0);                  // zero counts as not reported
    CHECK(d.bottleneck == ChargingDiagnostic::NegotiationUnknown);

    d = eval(100, std::nullopt, 30000);
    CHECK(d.bottleneck == ChargingDiagnostic::DeviceLimit);
    CHECK_EQ(d.summary, "Charging at 30W");

    d = eval(100, std::nullopt, 95400);
    CHECK(d.bottleneck == ChargingDiagnostic::Fine);
    CHECK_EQ(d.summary, "Charging well at 95W");
    CHECK_EQ(d.negotiatedW, std::optional<int>(95));
    CHECK_EQ(d.chargerMaxW, 100);
}

TEST_CASE(usb_default_cable_is_no_longer_a_false_cable_limit)
{
    // 00b current used to rate the cable at 18 W.
    const auto d = eval(45, cableRated(decodeCableVDO(0, false).maxWatts), 45000);
    CHECK(d.bottleneck == ChargingDiagnostic::Fine);
}

TEST_CASE(battery_state_changes_the_wording)
{
    auto d = eval(65, std::nullopt, 65000, battery(Status::Charging));
    CHECK_EQ(d.summary, "Charging well at 65W");

    d = eval(65, std::nullopt, 65000, battery(Status::Full, 100, 100));
    CHECK_EQ(d.summary, "Plugged in at 65W, battery full");
    CHECK(!d.isWarning);

    d = eval(65, std::nullopt, 65000, battery(Status::NotCharging, 80, 80));
    CHECK_EQ(d.summary, "Plugged in at 65W, not charging");
    CHECK_EQ(d.detail, "Battery at 80%, held at the 80% charge limit");

    d = eval(65, std::nullopt, 65000, battery(Status::NotCharging, 79, 80));
    CHECK_EQ(d.detail, "Charging is paused by the system");

    d = eval(65, std::nullopt, 30000, battery(Status::NotCharging, 85, 80));
    CHECK(d.bottleneck == ChargingDiagnostic::DeviceLimit);
    CHECK_EQ(d.detail, "Battery at 85%, held at the 80% charge limit. "
                       "Laptop requested 30W; charger can deliver up to 65W");
}

TEST_CASE(discharging_while_plugged_in_warns)
{
    auto d = eval(65, std::nullopt, 64000, battery(Status::Discharging));
    CHECK(d.bottleneck == ChargingDiagnostic::ChargerLimit);
    CHECK(d.isWarning);
    CHECK_EQ(d.summary, "Charger can't keep up at 64W: battery discharging");

    d = eval(65, std::nullopt, 30000, battery(Status::Discharging));
    CHECK(d.bottleneck == ChargingDiagnostic::DeviceLimit);
    CHECK(d.isWarning);
    CHECK_EQ(d.summary, "Plugged in at 30W, but battery is discharging");

    // Below a charge limit, discharging is still a real warning.
    d = eval(65, std::nullopt, 64000, battery(Status::Discharging, 79, 80));
    CHECK(d.bottleneck == ChargingDiagnostic::ChargerLimit);
    CHECK(d.isWarning);
}

TEST_CASE(discharging_at_a_charge_limit_is_not_a_warning)
{
    // Firmware draining the battery back down to its limit reports
    // "Discharging"; that is the limit working, not a weak charger.
    auto d = eval(65, std::nullopt, 64000, battery(Status::Discharging, 85, 80));
    CHECK(d.bottleneck == ChargingDiagnostic::Fine);
    CHECK(!d.isWarning);
    CHECK_EQ(d.summary, "Plugged in at 64W, not charging");
    CHECK_EQ(d.detail, "Battery at 85%, held at the 80% charge limit");

    d = eval(65, std::nullopt, 64000, battery(Status::Discharging, 80, 80));   // exactly at the limit
    CHECK(!d.isWarning);

    d = eval(65, std::nullopt, 30000, battery(Status::Discharging, 85, 80));
    CHECK(d.bottleneck == ChargingDiagnostic::DeviceLimit);
    CHECK(!d.isWarning);

    // A 100% "limit" is no limit: discharging at 100% still warns.
    d = eval(65, std::nullopt, 64000, battery(Status::Discharging, 100, 100));
    CHECK(d.isWarning);
    // The JSON still reports the raw battery status.
    CHECK(d.battery->status == Status::Discharging);
}

TEST_CASE(bottleneck_codes_are_stable)
{
    CHECK_EQ(std::string(ChargingDiagnostic::bottleneckCode(ChargingDiagnostic::CableLimit)),
             "cableLimit");
    CHECK_EQ(std::string(ChargingDiagnostic::bottleneckCode(ChargingDiagnostic::ChargerLimit)),
             "chargerLimit");
    CHECK_EQ(std::string(ChargingDiagnostic::bottleneckCode(
                 ChargingDiagnostic::NegotiationUnknown)),
             "negotiationUnknown");
    CHECK_EQ(std::string(SystemBattery::statusCode(Status::NotCharging)), "notCharging");
}

TEST_CASE(system_battery_ignores_peripherals_and_aggregates)
{
    {
        FakeSysfs fs;
        addBattery(fs, "BAT0", "Not charging", 93, 80);
        addBattery(fs, "hidpp_battery_0", "Discharging", 40, -1, "Device");
        const auto b = SystemBattery::read();
        REQUIRE(b);
        CHECK(b->status == Status::NotCharging);
        CHECK_EQ(b->capacityPercent, std::optional<int>(93));
        CHECK_EQ(b->chargeLimitPercent, std::optional<int>(80));
    }
    {
        FakeSysfs fs;
        addBattery(fs, "hidpp_battery_0", "Discharging", 40, -1, "Device");
        CHECK(!SystemBattery::read());     // desktop with only a mouse battery
    }
    {
        FakeSysfs fs;
        addBattery(fs, "BAT0", "Full", 100);
        addBattery(fs, "BAT1", "Charging", 50);
        const auto b = SystemBattery::read();
        REQUIRE(b);
        CHECK(b->status == Status::Charging);
        CHECK(!b->chargeLimitPercent);
    }
}

TEST_CASE(power_supply_helpers)
{
    TypeCPowerSupply psy;
    psy.online = true;
    psy.voltageNowUV = 20000000;
    psy.currentNowUA = 2250000;
    CHECK_EQ(psy.negotiatedMilliwatts(), std::optional<int>(45000));
    psy.online = false;
    CHECK(!psy.negotiatedMilliwatts());
    psy.online = true;
    psy.currentNowUA = 0;
    CHECK(!psy.negotiatedMilliwatts());

    psy.usbType = "C PD [PD_PPS]";
    CHECK_EQ(psy.selectedUsbType(), "PD_PPS");
    psy.usbType = "C [PD] PD_PPS";
    CHECK_EQ(psy.selectedUsbType(), "PD");
    psy.usbType = "Unknown";
    CHECK_EQ(psy.selectedUsbType(), "");
}
