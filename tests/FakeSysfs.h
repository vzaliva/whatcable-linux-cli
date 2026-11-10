// A throwaway sysfs tree for tests. Creating one points SysfsReader at it;
// destroying it removes the tree and restores the live /sys.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace wct {

class FakeSysfs {
public:
    // A fresh temporary tree, removed on destruction.
    FakeSysfs();
    // A tree at a fixed location that is kept (for --write-fixture).
    explicit FakeSysfs(const std::filesystem::path &root);
    ~FakeSysfs();

    FakeSysfs(const FakeSysfs &) = delete;
    FakeSysfs &operator=(const FakeSysfs &) = delete;

    const std::filesystem::path &root() const { return m_root; }

    // All paths are absolute sysfs paths ("/sys/class/typec/port0"),
    // created under root(). write() adds the trailing newline sysfs uses.
    void write(const std::string &path, const std::string &value);
    void writeBytes(const std::string &path, const std::string &bytes);
    void mkdir(const std::string &path);
    // Symlink at `path` pointing to the tree's `target`.
    void link(const std::string &path, const std::string &target);

    std::filesystem::path real(const std::string &path) const;

private:
    std::filesystem::path m_root;
    bool m_keep = false;
};

} // namespace wct
