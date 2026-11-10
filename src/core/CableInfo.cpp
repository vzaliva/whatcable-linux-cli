// Derived from WhatCable by Darryl Morley (https://github.com/darrylmorley/whatcable)
#include "CableInfo.h"
#include "VendorDB.h"

namespace WhatCable {

std::optional<CableInfo> CableInfo::fromTypeCCable(const TypeCCable &cable)
{
    CableInfo info;
    info.cableType = cable.type;
    info.plugType = cable.plugType;
    info.pdRevision = cable.pdRevision;
    info.isActive = (cable.type == "active");
    info.isPassive = (cable.type == "passive");
    info.rawAttributes = cable.rawAttributes;

    if (cable.identity) {
        const auto &id = *cable.identity;
        info.vendorId = id.vendorId;
        info.vendorName = VendorDB::lookupPd(id.vendorId);

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
        } else if (hdr.ufpProductType == ProductType::VPD) {
            info.isActive = false;
            info.isPassive = false;
            info.isVpd = true;
            info.cableType = "vpd";
        }

        if (id.certStat != 0)
            info.certificationXid = id.certStat;

        // A VPD's VDO 1 has a different layout; never decode it as a cable.
        const uint32_t cableVdo1 = id.productTypeVdos[0];
        // A PD 2.0 e-marker uses the older Cable VDO layout and has no VDO 2.
        const bool pd2 = cable.pdRevision.starts_with("2.");
        if (cableVdo1 != 0 && !info.isVpd) {
            info.vdo = decodeCableVDO(cableVdo1, info.isActive, pd2);
            if (!info.vdo->speedReserved)
                info.speed = info.vdo->speed;
            info.currentRating = info.vdo->currentRating;
            info.ratedVbusVolts = info.vdo->maxVbusVolts;
            info.maxWatts = info.vdo->maxWatts;

            const uint32_t activeVdo2 = id.productTypeVdos[1];
            if (info.isActive && !pd2 && activeVdo2 != 0)
                info.activeVdo2 = decodeActiveCableVDO2(activeVdo2);
        }
        info.identity = id;
    }

    return info;
}

} // namespace WhatCable
