// Vendor names: system usb.ids first, built-in table as fallback (VendorDB).
#include "TestFramework.h"
#include "FakeSysfs.h"
#include "VendorDB.h"

using namespace WhatCable;
using namespace wct;

TEST_CASE(builtin_table_without_usb_ids)
{
    FakeSysfs fs;                                   // no usb.ids in the tree
    CHECK_EQ(VendorDB::usbIdsPath(), "");
    CHECK_EQ(VendorDB::lookup(0x05AC), "Apple");
    CHECK_EQ(VendorDB::lookup(0x291A), "Anker");
    CHECK_EQ(VendorDB::lookup(0x1234), "0x1234");
}

TEST_CASE(usb_ids_names_win_and_parse_only_vendor_lines)
{
    FakeSysfs fs;
    fs.writeBytes("/usr/share/hwdata/usb.ids",
                  "#\tList of USB ID's\n"
                  "# 05ac  commented out\n"
                  "05ac  Apple, Inc.\n"
                  "\t1234  Some device\n"
                  "\t\t00  interface\n"
                  "1d6b  Linux Foundation  \n"     // trailing spaces trimmed
                  "abcd  Vendor \xc3\x96 GmbH\n"    // UTF-8 kept
                  "C 00  (Defined at Interface level)\n"
                  "1234  after the class section: ignored\n");
    CHECK(VendorDB::usbIdsPath().find("/usr/share/hwdata/usb.ids") != std::string::npos);
    CHECK_EQ(VendorDB::lookup(0x05AC), "Apple, Inc.");
    CHECK_EQ(VendorDB::lookup(0x1D6B), "Linux Foundation");
    CHECK_EQ(VendorDB::lookup(0xABCD), "Vendor \xc3\x96 GmbH");
    CHECK_EQ(VendorDB::lookup(0x1234), "0x1234");
    CHECK_EQ(VendorDB::lookup(0x291A), "Anker");    // missing from usb.ids: fallback
}

TEST_CASE(usb_ids_alternate_location_and_cache_follows_root)
{
    {
        FakeSysfs fs;
        fs.writeBytes("/usr/share/misc/usb.ids", "05ac  Apple Computer (misc)\n");
        CHECK_EQ(VendorDB::lookup(0x05AC), "Apple Computer (misc)");
    }
    {
        FakeSysfs fs;                               // new root, no usb.ids: reloaded
        CHECK_EQ(VendorDB::lookup(0x05AC), "Apple");
    }
}

TEST_CASE(ffff_is_the_pd_no_vendor_sentinel_only_for_pd)
{
    FakeSysfs fs;
    CHECK_EQ(VendorDB::lookupPd(0xFFFF), "No vendor ID assigned (USB-PD spec sentinel)");
    CHECK_EQ(VendorDB::lookupPd(0x291A), "Anker");
    CHECK_EQ(VendorDB::lookup(0xFFFF), "0xffff");   // a plain USB device: no PD meaning
}
