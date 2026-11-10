#!/usr/bin/env python3
"""Run the CLI on the dev-laptop fixture and check its --json output.

usage: check_cli_json.py <whatcable-linux> <fixture-root>
"""
import json
import os
import subprocess
import sys

cli, root = sys.argv[1], sys.argv[2]
env = dict(os.environ, WHATCABLE_SYSFS_ROOT=root)
failures = []


def check(cond, what):
    if not cond:
        failures.append(what)


def run(*args):
    out = subprocess.run([cli, *args], env=env, check=True, capture_output=True).stdout
    try:
        # Strict UTF-8 and JSON: binary sysfs attributes must be escaped.
        return json.loads(out.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as e:
        failures.append(f"{' '.join(args)}: invalid JSON: {e}")
        return []


def port(devices, n):
    for d in devices:
        if d.get("category") == "typec" and d["typec"]["port"] == n:
            return d
    return None


devices = run("--json")
run("--json", "--raw")

check(sum(d["category"] == "typec" for d in devices) == 3, "three Type-C ports")

p0 = port(devices, 0)
check(p0 is not None, "port 0 present")
if p0:
    pd, ch = p0.get("powerDelivery", {}), p0.get("charging", {})
    check(pd.get("pd") == "pd2", "port 0 uses the partner's pd2")
    check(pd.get("isCharger") is True, "port 0 partner is a charger")
    check(pd.get("activeIndex") == 5, "port 0 active PDO is #5")
    check(pd.get("maxPowerMW") == 45000, "port 0 charger max 45 W")
    check(ch.get("bottleneck") == "fine", "port 0 bottleneck fine")
    check(ch.get("negotiatedW") == 45, "port 0 negotiated 45 W")
    check(ch.get("cableMaxW") is None, "port 0 no cable rating -> null")
    check((ch.get("battery") or {}).get("status") == "notCharging", "battery notCharging")
    check((ch.get("battery") or {}).get("chargeLimitPercent") == 80, "charge limit 80")
    check(p0["typec"]["powerSupply"].get("negotiatedPowerMW") == 45000, "negotiatedPowerMW")
    check(p0["typec"]["powerSupply"].get("selectedUsbType") == "PD", "selectedUsbType PD")
    ident = p0["typec"].get("partner", {}).get("identity", {})
    check(ident.get("productType") == "Power Brick", "charger identity is a Power Brick")

p1 = port(devices, 1)
check(p1 is not None, "port 1 present")
if p1:
    check("charging" not in p1, "port 1 (source) has no charging verdict")
    check(p1.get("powerDelivery", {}).get("isCharger") is False, "port 1 partner not a charger")
    check(p1.get("powerDelivery", {}).get("activeIndex") is None, "port 1 no active PDO")
    cable = p1.get("cable", {})
    check(cable.get("type") == "active", "cable resolved as active")
    check(cable.get("maxWatts") == 240 and cable.get("ratedVbusVolts") == 50, "cable 240 W / 50 V")
    check(cable.get("identity", {}).get("productType") == "Active Cable", "cable identity type")
    check(cable.get("identity", {}).get("productId") == "0x1234", "cable product ID")
    check(cable.get("vendorName") == "Apple, Inc.", "cable vendor from usb.ids")
    check(cable.get("certificationXid") == 402653184, "certification XID")
    vdo = cable.get("vdo", {})
    check(vdo.get("latency") == "up to 10 ns (about 1 m)", "cable latency")
    check(vdo.get("termination") == "both ends active", "cable termination")
    check(vdo.get("eprCapable") is True, "EPR capable")
    check(vdo.get("speedReserved") is False, "speed not reserved")
    check("vbusThroughCable" in vdo, "active layout fields present")
    act = cable.get("activeCable", {})
    check(act.get("activeElement") == "re-timer", "active element")
    check(act.get("usb4Supported") is True and act.get("twoLanes") is True, "VDO2 protocols")
    check(act.get("maxOperatingTempC") == 70, "VDO2 temperature")
    check(cable.get("trustNotes") == [], "no trust notes for a spec-valid cable")
    ident1 = p1["typec"].get("partner", {}).get("identity", {})
    check(ident1.get("productType") == "USB Peripheral", "phone identity")

p2 = port(devices, 2)
check(p2 is not None, "port 2 present")
if p2:
    cable = p2.get("cable", {})
    notes = cable.get("trustNotes", [])
    check([n.get("code") for n in notes] == ["zeroVendorID", "reservedCableLatencyEncoding"],
          "port 2 cable note codes")
    check([n.get("severity") for n in notes] == ["info", "unusual"], "port 2 note severities")
    check(all(n.get("title") and n.get("detail") for n in notes), "notes have title and detail")
    check(cable.get("pdRevision") == "2.0", "cable PD revision")
    check(cable.get("vdo", {}).get("pd2Layout") is True, "PD 2.0 layout")
    check(cable.get("maxWatts") == 100 and cable.get("ratedVbusVolts") == 20, "PD 2.0 cable 100 W")
    check(cable.get("certificationXid") is None, "no XID -> null")

for f in failures:
    print("FAIL:", f)
print(f"{len(failures)} failure(s)")
sys.exit(1 if failures else 0)
