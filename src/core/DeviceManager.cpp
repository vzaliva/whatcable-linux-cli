// Derived from WhatCable by Darryl Morley (https://github.com/darrylmorley/whatcable)
#include "DeviceManager.h"
#include "CableInfo.h"

namespace WhatCable {

void DeviceManager::startMonitoring()
{
    m_monitor.start();
    refresh();
}

void DeviceManager::refresh()
{
    m_devices.clear();

    m_usbDevices = UsbDevice::enumerate();
    m_typecPorts = TypeCPort::enumerate();
    m_pdPorts = PowerDeliveryPort::enumerate();
    const auto battery = SystemBattery::read();

    for (const auto &tcPort : m_typecPorts) {
        // The partner's PD entry holds what the far end offers (a charger's
        // source PDOs). The port's own entry (tcPort.pdName) describes this
        // machine, so it is never used as the charger.
        std::optional<PowerDeliveryPort> pd;
        if (tcPort.partner && !tcPort.partner->pdName.empty()) {
            for (const auto &pdPort : m_pdPorts) {
                if (pdPort.name == tcPort.partner->pdName) {
                    pd = pdPort;
                    break;
                }
            }
        }

        const auto &psy = tcPort.powerSupply;
        if (pd && tcPort.isSink() && psy && psy->online &&
            psy->voltageNowUV && *psy->voltageNowUV > 0)
            pd->markActiveSourcePdo(*psy->voltageNowUV / 1000,
                                    psy->selectedUsbType() == "PD_PPS");

        std::optional<CableInfo> cable;
        if (tcPort.cable)
            cable = CableInfo::fromTypeCCable(*tcPort.cable);

        m_devices.push_back(DeviceSummary::fromTypeCPort(tcPort, pd, cable, battery));
    }

    for (const auto &dev : m_usbDevices) {
        if (dev.isRootHub)
            continue;
        m_devices.push_back(DeviceSummary::fromUsbDevice(dev));
    }

    if (m_devicesChanged)
        m_devicesChanged();
}

} // namespace WhatCable
