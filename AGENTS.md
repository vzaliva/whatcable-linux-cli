# WhatCable-Linux — Agent Guidelines

## Project Overview

WhatCable-Linux is a Linux port of [WhatCable](https://github.com/darrylmorley/whatcable) (macOS) by Darryl Morley. It is a CLI tool that shows USB device and USB-C cable information by reading Linux sysfs.

## Architecture

Two components share a single core library:

- **`src/core/`** — `libwhatcablecore`, a static C++ library (C++20, STL only). Reads sysfs over `std::filesystem` / streams, decodes USB PD data, and produces human-readable summaries. Links **`libudev`** for hotplug FD monitoring only — no Qt or other GUI/toolkit dependency.
- **`src/cli/`** — `whatcable-linux` CLI binary. POSIX `getopt_long`, manual JSON serialization for `--json`, `poll()` + 500 ms debounce for `--watch`.

## Key Data Flow

```
/sys/bus/usb/devices/         → UsbDevice.cpp
/sys/class/typec/             → TypeCPort.cpp
/sys/class/usb_power_delivery/ → PowerDelivery.cpp
                                    ↓
                              DeviceManager.cpp  ← UDevMonitor.cpp (hotplug)
                                    ↓
                              DeviceSummary.cpp (plain-English output)
                                    ↓
                              CLI (main.cpp)
```

## Code Conventions

- C++20, standard library. Use ordinary string literals or `std::string` / `std::string_view` as appropriate.
- All core classes are in the `WhatCable` namespace.
- sysfs reads go through `SysfsReader` — never read `/sys/` directly with raw file I/O from call sites (implementation may use filesystem APIs inside `SysfsReader`).
- Source files derived from the original Swift code must keep the attribution header: `// Derived from WhatCable by Darryl Morley (https://github.com/darrylmorley/whatcable)`
- Handle missing sysfs paths gracefully — return empty/nullopt, never crash. Many systems lack `/sys/class/typec/` or `/sys/class/usb_power_delivery/`.
- Keep `--json` in step with the text output. Any new or changed fact, verdict, or field must also be emitted by `deviceToJson()` in `src/cli/main.cpp`:
  - Use camelCase keys.
  - Add keys rather than renaming or removing them; scripts depend on them.
  - Emit `null` for "not reported", not `0`.
  - Pair each verdict with a stable machine code (as `charging.bottleneck` does) and the numbers behind it.
  - Verify that both `--json` and `--json --raw` parse, e.g. `| python3 -m json.tool`.

## Build

```bash
cmake -B build
cmake --build build
```

## Testing

- Run the test suite: `ctest --test-dir build --output-on-failure`
  - `core` runs `build/tests/whatcable-tests`, a STL-only runner, over
    fake sysfs trees. Pass test names to run a subset; `--list` lists
    them.
  - `cli_text` and `cli_json` run the CLI on the dev-laptop fixture
    (`tests/Fixtures.cpp`). `cli_json` validates `--json` and
    `--json --raw` with `tests/check_cli_json.py`, and is skipped if
    Python 3 is missing.
- Add or extend a test with every behaviour change:
  - Build sysfs inputs with `wct::FakeSysfs` and the helpers in
    `tests/Fixtures.h`. Lay them out as the kernel does: devices under
    `/sys/devices`, `/sys/class` entries as symlinks.
  - New output must be checked in `check_cli_json.py` too.
- Every top-level sysfs path must go through `SysfsReader::sysPath()`,
  so tests (and `WHATCABLE_SYSFS_ROOT`) can redirect it.
- Run the CLI: `./build/src/cli/whatcable-linux`
- JSON output: `./build/src/cli/whatcable-linux --json`
- Watch mode: `./build/src/cli/whatcable-linux --watch`
- Replay a captured or fake tree:
  `WHATCABLE_SYSFS_ROOT=/path/to/snapshot ./build/src/cli/whatcable-linux`
  reads `<dir>/sys/...` instead of `/sys/...`.

## Key Files to Know

| File | Purpose |
|---|---|
| `src/core/UsbDevice.h/cpp` | Enumerates all USB devices from `/sys/bus/usb/devices/` |
| `src/core/TypeCPort.h/cpp` | Reads USB-C port state from `/sys/class/typec/` |
| `src/core/PDDecoder.h/cpp` | USB PD VDO bit-field decoding (ported from PDVDO.swift) |
| `src/core/PowerDelivery.h/cpp` | Parses PDO lists from `/sys/class/usb_power_delivery/` |
| `src/core/DeviceSummary.h/cpp` | Generates headlines, subtitles, bullets per device |
| `src/core/ChargingDiagnostic.h/cpp` | Identifies USB-C charging bottlenecks |
| `src/core/Battery.h/cpp` | System battery status from `/sys/class/power_supply/` (charging wording) |
| `src/core/DeviceManager.h/cpp` | Aggregates all sources, correlates data, owns refresh logic |
| `src/core/UDevMonitor.h/cpp` | libudev monitor + fd for `poll()` |
| `src/core/VendorDB.h/cpp` | USB VID → vendor name lookup (`usb.ids`, then a built-in fallback) |
| `src/core/UsbClassDB.h/cpp` | USB class code → human name |

## Adding New Vendors

Vendor names come from the system `usb.ids` (hwdata or usbutils), read
under `SysfsReader::root()`. Missing vendors belong upstream in `usb.ids`.

The `kVendors` map in `src/core/VendorDB.cpp` is only a fallback, for
systems without `usb.ids` and for IDs it lacks (e.g. `0x291A` Anker). Add
an entry (`{0xVID, "Vendor Name"}`) only in that case, and check the ID
against `usb.ids` or the USB-IF list first: hand-curated names drift into
wrong ones.

## Adding New USB Class Codes

Add cases to `UsbClassDB::className()` or `interfaceClassName()` in `src/core/UsbClassDB.cpp`.
