// End to end: DeviceManager::refresh() over the dev-laptop fixture.
#include "TestFramework.h"
#include "Fixtures.h"
#include "DeviceManager.h"

#include <algorithm>

using namespace WhatCable;
using namespace wct;

namespace {

const DeviceSummary *find(const std::vector<DeviceSummary> &all, const std::string &headline)
{
    for (const auto &d : all)
        if (d.headline == headline)
            return &d;
    return nullptr;
}

bool hasBullet(const DeviceSummary &d, const std::string &bullet)
{
    return std::find(d.bullets.begin(), d.bullets.end(), bullet) != d.bullets.end();
}

bool hasBulletPrefix(const DeviceSummary &d, const std::string &prefix)
{
    return std::any_of(d.bullets.begin(), d.bullets.end(),
                       [&](const std::string &b) { return b.rfind(prefix, 0) == 0; });
}

} // namespace

TEST_CASE(dev_laptop_lists_ports_and_devices)
{
    FakeSysfs fs;
    buildDevLaptop(fs);
    DeviceManager mgr;
    mgr.refresh();

    int typec = 0;
    for (const auto &d : mgr.devices()) {
        if (d.category == DeviceSummary::TypeCPortCategory) {
            ++typec;
            CHECK(d.headline != "USB-C Port -1");
        }
    }
    CHECK_EQ(typec, 3);
    CHECK(find(mgr.devices(), "USB Receiver") != nullptr);
}

TEST_CASE(sinking_port_reports_the_charger)
{
    FakeSysfs fs;
    buildDevLaptop(fs);
    DeviceManager mgr;
    mgr.refresh();

    const DeviceSummary *port0 = find(mgr.devices(), "USB-C Port 0");
    REQUIRE(port0);
    CHECK_EQ(port0->subtitle, "Anker — Power Brick");
    CHECK(port0->status == DeviceSummary::Charging);
    CHECK(hasBullet(*port0, "Charger max: 45W"));          // not pd0's own 15 W
    CHECK(hasBullet(*port0, "Negotiated power: 20.0V @ 2.25A — 45W"));
    REQUIRE(port0->powerDelivery);
    CHECK_EQ(port0->powerDelivery->name, "pd2");
    CHECK_EQ(port0->powerDelivery->activeSourcePdoIndex, std::optional<int>(5));
    REQUIRE(port0->chargingDiag);
    CHECK_EQ(port0->chargingDiag->summary, "Plugged in at 45W, not charging");
    CHECK_EQ(port0->chargingDiag->detail, "Battery at 93%, held at the 80% charge limit");
    CHECK(!port0->chargingDiag->isWarning);
}

TEST_CASE(sourcing_port_is_not_a_charger)
{
    FakeSysfs fs;
    buildDevLaptop(fs);
    DeviceManager mgr;
    mgr.refresh();

    const DeviceSummary *port1 = find(mgr.devices(), "USB-C Port 1");
    REQUIRE(port1);
    CHECK_EQ(port1->subtitle, "Google Inc. — USB Peripheral");   // from the fixture's usb.ids
    CHECK(port1->status != DeviceSummary::Charging);
    CHECK(!port1->chargingDiag);
    CHECK(!hasBulletPrefix(*port1, "Charger max"));
    REQUIRE(port1->powerDelivery);
    CHECK_EQ(port1->powerDelivery->name, "pd3");          // the phone's caps, not pd1
    CHECK(!port1->powerDelivery->activeSourcePdoIndex);

    REQUIRE(port1->cable);
    CHECK(port1->cable->isActive);
    CHECK_EQ(port1->cable->maxWatts, 240);
    CHECK(hasBullet(*port1, "Active cable"));
    CHECK(hasBullet(*port1, "Cable max power: 240W"));
    CHECK(hasBullet(*port1, "Cable speed: USB4 Gen 3 (20/40 Gbps)"));
    CHECK(hasBullet(*port1, "Cable latency: up to 10 ns (about 1 m)"));
    CHECK(hasBullet(*port1, "EPR capable (up to 48V)"));
    CHECK(hasBullet(*port1, "Active electronics: copper, re-timer"));
    CHECK(hasBullet(*port1, "Cable data support: USB4, USB 3.2 Gen 2+ x2, USB 2.0"));
    CHECK(hasBullet(*port1, "Cable vendor: Apple, Inc."));
    CHECK(hasBullet(*port1, "USB-IF certification ID (XID): 402653184"));
    CHECK(port1->cable->trustNotes.empty());
}

TEST_CASE(old_cable_gets_notes_and_pd2_decode)
{
    FakeSysfs fs;
    buildDevLaptop(fs);
    DeviceManager mgr;
    mgr.refresh();
    const DeviceSummary *port2 = find(mgr.devices(), "USB-C Port 2");
    REQUIRE(port2);
    REQUIRE(port2->cable);
    CHECK_EQ(port2->cable->maxWatts, 100);           // not 240: PD 2.0 bits 10:9
    CHECK(hasBullet(*port2, "Cable max power: 100W"));
    CHECK(!hasBulletPrefix(*port2, "EPR"));
    REQUIRE(port2->cable->trustNotes.size() == 2);
    CHECK_EQ(port2->cable->trustNotes[0].code, "zeroVendorID");
    CHECK(port2->cable->trustNotes[0].isInfo);
    CHECK_EQ(port2->cable->trustNotes[1].code, "reservedCableLatencyEncoding");
    CHECK(!port2->cable->trustNotes[1].isInfo);
}

TEST_CASE(usb_device_vendor_from_usb_ids)
{
    FakeSysfs fs;
    buildDevLaptop(fs);
    DeviceManager mgr;
    mgr.refresh();
    const DeviceSummary *receiver = find(mgr.devices(), "USB Receiver");
    REQUIRE(receiver);
    CHECK(receiver->subtitle.rfind("Logitech, Inc.", 0) == 0);
}
