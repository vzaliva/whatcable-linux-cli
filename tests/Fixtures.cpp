#include "Fixtures.h"

#include <cstdio>

namespace wct {

namespace {

std::string hex(uint32_t v)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%08x", v);
    return buf;
}

std::string typecDevice(int n)
{
    return std::string("/sys/devices/platform/") + kUcsiController + "/typec/port" +
           std::to_string(n);
}

void writeIdentity(FakeSysfs &fs, const std::string &dev, const Identity &id)
{
    fs.write(dev + "/identity/id_header", hex(id.idHeader));
    fs.write(dev + "/identity/cert_stat", hex(id.certStat));
    fs.write(dev + "/identity/product", hex(id.product));
    fs.write(dev + "/identity/product_type_vdo1", hex(id.vdo1));
    fs.write(dev + "/identity/product_type_vdo2", hex(id.vdo2));
    fs.write(dev + "/identity/product_type_vdo3", hex(id.vdo3));
}

void writePdos(FakeSysfs &fs, const std::string &dir, const std::vector<Pdo> &pdos)
{
    fs.mkdir(dir + "/power");          // not a PDO; must be skipped
    fs.write(dir + "/uevent", "");
    for (size_t i = 0; i < pdos.size(); ++i) {
        const Pdo &p = pdos[i];
        const std::string d = dir + "/" + std::to_string(i + 1) + ":" + p.kind;
        if (p.kind == "fixed_supply") {
            fs.write(d + "/voltage", std::to_string(p.voltageMV) + "mV");
            fs.write(d + "/maximum_current", std::to_string(p.currentMA) + "mA");
        } else {
            fs.write(d + "/minimum_voltage", std::to_string(p.voltageMV) + "mV");
            fs.write(d + "/maximum_voltage", std::to_string(p.maxVoltageMV) + "mV");
            fs.write(d + "/maximum_current", std::to_string(p.currentMA) + "mA");
        }
    }
}

} // namespace

std::string addTypecPort(FakeSysfs &fs, int n, const std::string &powerRole,
                         const std::string &dataRole, const std::string &opMode)
{
    const std::string dev = typecDevice(n);
    fs.write(dev + "/power_role", powerRole);
    fs.write(dev + "/data_role", dataRole);
    fs.write(dev + "/power_operation_mode", opMode);
    fs.write(dev + "/usb_power_delivery_revision", "2.0");
    fs.write(dev + "/usb_typec_revision", "2.0");
    fs.write(dev + "/orientation", "normal");
    fs.link("/sys/class/typec/port" + std::to_string(n), dev);
    return dev;
}

std::string addPartner(FakeSysfs &fs, int n, const Identity *identity)
{
    const std::string name = "port" + std::to_string(n) + "-partner";
    const std::string dev = typecDevice(n) + "/" + name;
    fs.write(dev + "/supports_usb_power_delivery", "yes");
    if (identity)
        writeIdentity(fs, dev, *identity);
    fs.link("/sys/class/typec/" + name, dev);
    return dev;
}

std::string addCable(FakeSysfs &fs, int n, const std::string &type, const Identity &identity,
                     const std::string &pdRevision)
{
    const std::string name = "port" + std::to_string(n) + "-cable";
    const std::string dev = typecDevice(n) + "/" + name;
    fs.write(dev + "/type", type);
    fs.write(dev + "/plug_type", "type-c");
    fs.write(dev + "/usb_power_delivery_revision", pdRevision);
    writeIdentity(fs, dev, identity);
    fs.link("/sys/class/typec/" + name, dev);
    return dev;
}

void addPd(FakeSysfs &fs, const std::string &name, const std::string &ownerDevice,
           const std::string &ownerLink, const std::vector<Pdo> &sourceCaps,
           const std::vector<Pdo> &sinkCaps)
{
    const std::string dev = ownerDevice + "/" + name;
    fs.write(dev + "/revision", "2.0");
    writePdos(fs, dev + "/source-capabilities", sourceCaps);
    writePdos(fs, dev + "/sink-capabilities", sinkCaps);
    fs.link("/sys/class/usb_power_delivery/" + name, dev);
    fs.link(ownerLink + "/usb_power_delivery", dev);
}

void addUcsiPowerSupply(FakeSysfs &fs, int portNumber, bool online, int voltageUV,
                        int currentUA, const std::string &usbType)
{
    // The UCSI driver numbers its supplies from 1: port0 -> ...:001.
    const std::string name = std::string("ucsi-source-psy-") + kUcsiController +
                             std::to_string(portNumber + 1);
    const std::string dev = std::string("/sys/devices/platform/") + kUcsiController +
                            "/power_supply/" + name;
    fs.write(dev + "/online", online ? "1" : "0");
    fs.write(dev + "/voltage_now", std::to_string(voltageUV));
    fs.write(dev + "/current_now", std::to_string(currentUA));
    fs.write(dev + "/current_max", std::to_string(currentUA));
    fs.write(dev + "/usb_type", usbType);
    fs.link("/sys/class/power_supply/" + name, dev);
}

void addBattery(FakeSysfs &fs, const std::string &name, const std::string &status,
                int capacity, int chargeLimit, const std::string &scope)
{
    const std::string dev = "/sys/devices/battery/" + name;
    fs.write(dev + "/type", "Battery");
    fs.write(dev + "/status", status);
    fs.write(dev + "/capacity", std::to_string(capacity));
    if (chargeLimit >= 0)
        fs.write(dev + "/charge_control_end_threshold", std::to_string(chargeLimit));
    if (!scope.empty())
        fs.write(dev + "/scope", scope);
    fs.link("/sys/class/power_supply/" + name, dev);
}

void buildDevLaptop(FakeSysfs &fs)
{
    const std::string controller = std::string("/sys/devices/platform/") + kUcsiController;

    // port0: the laptop sinks from a 45 W charger.
    const std::string port0 = addTypecPort(fs, 0, "source [sink]", "host [device]",
                                           "usb_power_delivery");
    Identity charger;
    charger.idHeader = (3u << 23) | 0x291A;          // DFP Power Brick, Anker VID
    const std::string partner0 = addPartner(fs, 0, &charger);
    addPd(fs, "pd0", controller + "/usb_power_delivery", port0,
          {{"fixed_supply", 5000, 0, 3000}},           // the laptop itself can source 15 W
          {{"fixed_supply", 20000, 0, 3250}});
    addPd(fs, "pd2", partner0, partner0,
          {{"fixed_supply", 5000, 0, 3000},
           {"fixed_supply", 9000, 0, 3000},
           {"fixed_supply", 12000, 0, 3000},
           {"fixed_supply", 15000, 0, 3000},
           {"fixed_supply", 20000, 0, 2250}});
    addUcsiPowerSupply(fs, 0, true, 20000000, 2250000, "C [PD] PD_PPS");

    // port1: the laptop sources to a dual-role phone over an active cable.
    const std::string port1 = addTypecPort(fs, 1, "[source] sink", "[host] device",
                                           "usb_power_delivery");
    Identity phone;
    phone.idHeader = (2u << 27) | (2u << 23) | 0x18D1;  // UFP peripheral, DFP host, Google
    const std::string partner1 = addPartner(fs, 1, &phone);
    addPd(fs, "pd1", controller + "/usb_power_delivery", port1,
          {{"fixed_supply", 5000, 0, 3000}});
    addPd(fs, "pd3", partner1, partner1, {{"fixed_supply", 5000, 0, 3000}});
    addUcsiPowerSupply(fs, 1, false, 0, 0, "[C] PD PD_PPS");
    Identity cable;
    cable.idHeader = (4u << 27) | 0x05AC;   // active cable, Apple VID
    cable.certStat = 3u << 27;              // would decode as "passive cable"
    cable.product = 0x12340100;             // PID 0x1234, bcdDevice 0x0100
    cable.vdo1 = kActiveUsb4CableVdo1;
    cable.vdo2 = kActiveUsb4CableVdo2;
    addCable(fs, 1, "passive", cable);       // sysfs type disagrees with the header

    // A small usb.ids: Anker (0x291A) is deliberately missing, as in the
    // real file, so it comes from the built-in fallback table.
    fs.writeBytes("/usr/share/hwdata/usb.ids",
                  "# test usb.ids\n"
                  "046d  Logitech, Inc.\n"
                  "\tc52b  Unifying Receiver\n"
                  "05ac  Apple, Inc.\n"
                  "18d1  Google Inc.\n"
                  "C 00  (Defined at Interface level)\n"
                  "ffff  not a vendor (class section)\n");

    fs.write("/sys/devices/ac/AC/type", "Mains");
    fs.link("/sys/class/power_supply/AC", "/sys/devices/ac/AC");
    addBattery(fs, "BAT0", "Not charging", 93, 80);
    addBattery(fs, "hidpp_battery_0", "Discharging", 40, -1, "Device");

    // One USB device; bos_descriptors is binary, as on real hardware.
    const std::string usb = "/sys/devices/pci0000:00/usb1/1-1";
    fs.write(usb + "/idVendor", "046d");
    fs.write(usb + "/idProduct", "c52b");
    fs.write(usb + "/manufacturer", "Logitech");
    fs.write(usb + "/product", "USB Receiver");
    fs.write(usb + "/speed", "12");
    fs.write(usb + "/version", " 2.00");
    fs.write(usb + "/bMaxPower", "98mA");
    fs.write(usb + "/busnum", "1");
    fs.write(usb + "/devnum", "2");
    fs.writeBytes(usb + "/bos_descriptors",
                  std::string("\x05\x0f\x29\x00\x02\xdf\x60\xdd\xd8\x89\x45\xc7", 12));
    fs.link("/sys/bus/usb/devices/1-1", usb);
}

} // namespace wct
