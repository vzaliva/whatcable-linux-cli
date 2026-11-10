// Builders for fake sysfs trees laid out like the kernel's: devices live
// under /sys/devices/platform/<UCSI controller>/..., and /sys/class/*
// entries are symlinks to them.
#pragma once

#include "FakeSysfs.h"

#include <cstdint>
#include <string>
#include <vector>

namespace wct {

inline constexpr const char *kUcsiController = "USBC000:00";

struct Pdo {
    std::string kind;          // "fixed_supply", "programmable_supply", ...
    int voltageMV = 0;         // fixed: voltage; PPS: minimum voltage
    int maxVoltageMV = 0;      // PPS only
    int currentMA = 0;
};

struct Identity {
    uint32_t idHeader = 0, certStat = 0, product = 0;
    uint32_t vdo1 = 0, vdo2 = 0, vdo3 = 0;
};

// /sys/class/typec/portN with the given role strings, e.g. "source [sink]".
std::string addTypecPort(FakeSysfs &fs, int n, const std::string &powerRole,
                         const std::string &dataRole, const std::string &opMode);
std::string addPartner(FakeSysfs &fs, int n, const Identity *identity = nullptr);
std::string addCable(FakeSysfs &fs, int n, const std::string &type, const Identity &identity);
// A usb_power_delivery entry inside `ownerDevice` (a /sys/devices path),
// linked from /sys/class/usb_power_delivery/<name> and <ownerLink>.
void addPd(FakeSysfs &fs, const std::string &name, const std::string &ownerDevice,
           const std::string &ownerLink, const std::vector<Pdo> &sourceCaps,
           const std::vector<Pdo> &sinkCaps = {});
void addUcsiPowerSupply(FakeSysfs &fs, int portNumber, bool online, int voltageUV,
                        int currentUA, const std::string &usbType);
void addBattery(FakeSysfs &fs, const std::string &name, const std::string &status,
                int capacity, int chargeLimit = -1, const std::string &scope = "");

// The development laptop the fixes were checked on, plus extra coverage:
//   port0: sinking from a 45 W charger (partner pd2, 5 fixed PDOs, UCSI
//          contract 20 V x 2.25 A); the port's own pd0 offers 15 W.
//   port1: sourcing to a dual-role phone (partner pd3 advertises 15 W) over
//          an active 5 A / 50 V e-marked cable whose cert_stat would read
//          as a passive cable if mistaken for the ID header.
//   /usr/share/hwdata/usb.ids: a few vendors (not Anker).
//   BAT0 "Not charging" at 93% with an 80% limit; a mouse battery
//   (scope Device) that is discharging; one USB device with binary
//   bos_descriptors.
void buildDevLaptop(FakeSysfs &fs);

} // namespace wct
