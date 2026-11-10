// Derived from WhatCable by Darryl Morley (https://github.com/darrylmorley/whatcable)
#include "CableInfo.h"
#include "VendorDB.h"

namespace WhatCable {

std::optional<CableInfo> CableInfo::fromTypeCCable(const TypeCCable &cable)
{
    CableInfo info;
    info.cableType = cable.type;
    info.plugType = cable.plugType;
    info.isActive = (cable.type == "active");
    info.isPassive = (cable.type == "passive");
    info.rawAttributes = cable.rawAttributes;

    if (cable.identity) {
        const auto &id = *cable.identity;
        info.vendorId = id.vendorId;
        info.vendorName = VendorDB::lookup(id.vendorId);

        // SOP' ID header carries the cable type in the UFP product-type
        // field. Only override the sysfs "type" when it says so explicitly.
        const auto hdr = decodeIDHeader(id.idHeader);
        if (hdr.ufpProductType == ProductType::ActiveCable) {
            info.isActive = true;
            info.isPassive = false;
            info.cableType = "active";
        } else if (hdr.ufpProductType == ProductType::PassiveCable) {
            info.isActive = false;
            info.isPassive = true;
            info.cableType = "passive";
        }

        const uint32_t cableVdo1 = id.productTypeVdos[0];
        if (cableVdo1 != 0) {
            const auto cableVdo = decodeCableVDO(cableVdo1, info.isActive);
            info.speed = cableVdo.speed;
            info.currentRating = cableVdo.currentRating;
            info.ratedVbusVolts = cableVdo.maxVbusVolts;
            info.maxWatts = cableVdo.maxWatts;
        }
        info.identity = id;
    }

    return info;
}

} // namespace WhatCable
