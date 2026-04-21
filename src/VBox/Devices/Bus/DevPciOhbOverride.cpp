/* $Id$ */
/** @file
 * OpenHuizeBox PCI identity override helper.
 *
 * This is the reader half of the per-device PCI identity override
 * feature. Upstream VBox hard-codes each virtual device's PCI config
 * space (vendor ID, device ID, subsystem IDs, revision) at device
 * construct time via PDMDevHlpPCIRegister(). Guest OSes then enumerate
 * the VM's PCI bus and see `VEN_80EE` / `VEN_8086` etc — which is a
 * trivial tell that the environment is VirtualBox.
 *
 * This helper is called *immediately after* PDMDevHlpPCIRegister() by
 * each participating device's R3 constructor. It reads the per-device
 * extradata node:
 *
 *      VBoxInternal/Devices/<devname>/<inst>/Config/
 *                                          PciVendorId        (uint32)
 *                                          PciDeviceId        (uint32)
 *                                          PciSubsysVendorId  (uint32)
 *                                          PciSubsysDeviceId  (uint32)
 *                                          PciRevisionId      (uint8)
 *                                          PciClassCode       (uint8, optional)
 *                                          PciSubClassCode    (uint8, optional)
 *
 * ...and patches the already-registered PCI config space. Device
 * behaviour (BARs, IRQs, DMA channels, memory windows, register
 * offsets) is NOT touched — only identity bytes at offsets 0x00..0x0F
 * and 0x2C..0x2F.
 *
 * This is the OpenHuizeBox shallow-fork principle: we do not rewrite
 * how devices work; we make the device identity configurable so an
 * audit-research VM can claim to be a specific real-world PCI device.
 *
 * Copyright 2026 OpenHuizeBox Project.
 * SPDX-License-Identifier: GPL-3.0-only
 */

#define LOG_GROUP LOG_GROUP_DEV_PCI
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/cfgm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/string.h>

#include "DevPciOhbOverride.h"


/**
 * Patch a PCI device's identity from per-instance CFGM extradata.
 *
 * @param pDevIns    The device instance (already registered).
 * @param pPciDev    The PCI function whose config space to override.
 *
 * @returns VINF_SUCCESS on both "key found and applied" and "key
 *          absent, nothing to do". Never fails hard — a missing
 *          extradata key is the normal case (stock behaviour).
 *
 * @note Must be called after PDMDevHlpPCIRegister[Ex] otherwise the
 *       PCI config space doesn't yet exist to patch.
 */
DECLHIDDEN(int) ohbPciOverrideFromExtraData(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev)
{
    AssertPtrReturn(pDevIns, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pPciDev, VERR_INVALID_PARAMETER);

    PCFGMNODE pCfg = pDevIns->pCfg;
    AssertPtrReturn(pCfg, VERR_INVALID_PARAMETER);

    /* Query + apply each field. QueryU32 returns VERR_CFGM_VALUE_NOT_FOUND
     * when absent; we treat that as "user did not override, leave stock". */
    uint32_t u32;
    int rc;

    rc = CFGMR3QueryU32(pCfg, "PciVendorId", &u32);
    if (RT_SUCCESS(rc) && u32 <= UINT16_MAX)
    {
        PDMPciDevSetVendorId(pPciDev, (uint16_t)u32);
        LogRel(("OHB/PCI: %s/%u VendorId overridden to 0x%04x\n",
                pDevIns->pReg->szName, pDevIns->iInstance, (uint16_t)u32));
    }

    rc = CFGMR3QueryU32(pCfg, "PciDeviceId", &u32);
    if (RT_SUCCESS(rc) && u32 <= UINT16_MAX)
    {
        PDMPciDevSetDeviceId(pPciDev, (uint16_t)u32);
        LogRel(("OHB/PCI: %s/%u DeviceId overridden to 0x%04x\n",
                pDevIns->pReg->szName, pDevIns->iInstance, (uint16_t)u32));
    }

    rc = CFGMR3QueryU32(pCfg, "PciSubsysVendorId", &u32);
    if (RT_SUCCESS(rc) && u32 <= UINT16_MAX)
        PDMPciDevSetSubSystemVendorId(pPciDev, (uint16_t)u32);

    rc = CFGMR3QueryU32(pCfg, "PciSubsysDeviceId", &u32);
    if (RT_SUCCESS(rc) && u32 <= UINT16_MAX)
        PDMPciDevSetSubSystemId(pPciDev, (uint16_t)u32);

    rc = CFGMR3QueryU32(pCfg, "PciRevisionId", &u32);
    if (RT_SUCCESS(rc) && u32 <= UINT8_MAX)
        PDMPciDevSetRevisionId(pPciDev, (uint8_t)u32);

    rc = CFGMR3QueryU32(pCfg, "PciClassCode", &u32);
    if (RT_SUCCESS(rc) && u32 <= UINT8_MAX)
        PDMPciDevSetClassBase(pPciDev, (uint8_t)u32);

    rc = CFGMR3QueryU32(pCfg, "PciSubClassCode", &u32);
    if (RT_SUCCESS(rc) && u32 <= UINT8_MAX)
        PDMPciDevSetClassSub(pPciDev, (uint8_t)u32);

    return VINF_SUCCESS;
}
