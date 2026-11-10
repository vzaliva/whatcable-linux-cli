// Derived from WhatCable by Darryl Morley (https://github.com/darrylmorley/whatcable)
// Linux sysfs equivalent of the IOKit property reading in the original.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace WhatCable {

class SysfsReader {
public:
    // Prefix for every top-level sysfs path (default "" = the live system).
    // Tests and bug-report replays point it at a copied or fake tree, e.g.
    // "/tmp/snap" makes "/sys/class/typec" read "/tmp/snap/sys/class/typec".
    static void setRoot(std::string root);
    static const std::string &root();
    // root() + absolutePath. Use for the entry points ("/sys/class/...");
    // paths derived from enumerated entries already carry the root.
    static std::string sysPath(std::string_view absolutePath);

    static std::string readAttribute(const std::string &path);
    static std::optional<int> readIntAttribute(const std::string &path);
    static std::optional<uint32_t> readHexAttribute(const std::string &path);
    static std::vector<std::string> listSubdirectories(const std::string &path);
    static std::map<std::string, std::string> readAllAttributes(const std::string &dirPath);
    // Final path component of a symlink's target ("pd2" for
    // portN-partner/usb_power_delivery -> pd2), or empty if not a symlink.
    static std::string readLinkBasename(const std::string &path);
    static bool pathExists(const std::string &path);
};

} // namespace WhatCable
