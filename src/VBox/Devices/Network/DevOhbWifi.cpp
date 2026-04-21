/* $Id$ */
/** @file
 * DevOhbWifi - OpenHuizeBox stealth Wi-Fi PCI stub device.
 *
 * This is a NO-FUNCTION PCI function whose sole purpose is to appear
 * in the guest's PCI bus enumeration as a wireless network adapter.
 * It has no BARs, no I/O ports, no MMIO, no DMA, no interrupts, and
 * no saved state. Guest drivers that attempt to attach will simply
 * fail probe - which is fine for our purpose: anti-detection scripts
 * (Pafish, Antisec sysprobe, Windows Defender telemetry heuristics,
 * and our own Check-PCIDevices / Check-NetworkInterfaceDetails) only
 * read PCI config space identity via Win32_PnPEntity / SetupAPI, so
 * VEN_8086 & DEV_A370 in a `laptop` VM profile is enough to pass as
 * "Intel Wireless-AC 9560 160MHz" without any real radio.
 *
 * Defaults:
 *   Vendor      0x8086  Intel Corporation
 *   Device      0xA370  Wireless-AC 9560
 *   Class       0x02    Network controller
 *   SubClass    0x80    Other network controller
 *
 * Overrides via per-VM CFGM extradata (handled in DevPciOhbOverride.cpp):
 *   PciVendorId / PciDeviceId / PciSubsysVendorId / PciSubsysDeviceId /
 *   PciRevisionId / PciClassCode / PciSubClassCode.
 *
 * Copyright 2026 OpenHuizeBox Project.
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_PCNET  /* reuse nearest log group; dedicated group not worth a header edit for a stub */
#include <VBox/vmm/pdmdev.h>
#include "../Bus/DevPciOhbOverride.h"
#include <VBox/version.h>
#include <iprt/assert.h>

#include "VBoxDD.h"

#ifdef VBOX_WITH_OHB_VMX_STEALTH


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * OHB stealth Wi-Fi shared state.
 *
 * Kept non-empty because PDM requires a positive cbInstanceShared for
 * device instantiation. No runtime data lives here - the device is
 * purely decorative on the PCI bus.
 */
typedef struct OHBWIFISTATE
{
    /** Instance number (diagnostics only). */
    uint32_t        iInstance;
    /** Effective vendor ID after any CFGM override (for LogRel). */
    uint16_t        uVendorId;
    /** Effective device ID after any CFGM override (for LogRel). */
    uint16_t        uDeviceId;
    uint32_t        uPadding;
} OHBWIFISTATE;
/** Pointer to shared state. */
typedef OHBWIFISTATE *POHBWIFISTATE;


/** Default Intel Wireless-AC 9560 vendor/device. */
#define OHB_WIFI_DEFAULT_VENDOR_ID      UINT16_C(0x8086)
#define OHB_WIFI_DEFAULT_DEVICE_ID      UINT16_C(0xA370)
/** Default subsystem Intel NUC-style SSID. */
#define OHB_WIFI_DEFAULT_SUBSYS_VENDOR  UINT16_C(0x8086)
#define OHB_WIFI_DEFAULT_SUBSYS_DEVICE  UINT16_C(0x0034)
/** PCI class: network controller / other network controller. */
#define OHB_WIFI_PCI_CLASS_BASE         UINT8_C(0x02)
#define OHB_WIFI_PCI_CLASS_SUB          UINT8_C(0x80)
/** Revision of a 9560 found on typical consumer laptops. */
#define OHB_WIFI_DEFAULT_REVISION       UINT8_C(0x10)


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 *
 * Nothing to release - no allocations, no locks, no timers.
 */
static DECLCALLBACK(int) ohbWifiR3Destruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);
    RT_NOREF(pDevIns);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) ohbWifiR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    POHBWIFISTATE pThis = PDMDEVINS_2_DATA(pDevIns, POHBWIFISTATE);
    RT_NOREF(pCfg);

    pThis->iInstance = (uint32_t)iInstance;
    pThis->uVendorId = OHB_WIFI_DEFAULT_VENDOR_ID;
    pThis->uDeviceId = OHB_WIFI_DEFAULT_DEVICE_ID;

    /*
     * No config keys are consumed locally. All identity-tuning flows
     * through ohbPciOverrideFromExtraData() which reads VBoxInternal
     * extradata (PciVendorId, PciDeviceId, ...). Still declare an
     * empty allowed-keys set so the user gets a clean error for typos.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "", "");

    /*
     * Populate PCI config space with a plausible Intel wireless adapter.
     */
    PPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    PDMPCIDEV_ASSERT_VALID(pDevIns, pPciDev);

    PDMPciDevSetVendorId(pPciDev,           OHB_WIFI_DEFAULT_VENDOR_ID);
    PDMPciDevSetDeviceId(pPciDev,           OHB_WIFI_DEFAULT_DEVICE_ID);
    PDMPciDevSetClassBase(pPciDev,          OHB_WIFI_PCI_CLASS_BASE);
    PDMPciDevSetClassSub(pPciDev,           OHB_WIFI_PCI_CLASS_SUB);
    PDMPciDevSetRevisionId(pPciDev,         OHB_WIFI_DEFAULT_REVISION);
    PDMPciDevSetSubSystemVendorId(pPciDev,  OHB_WIFI_DEFAULT_SUBSYS_VENDOR);
    PDMPciDevSetSubSystemId(pPciDev,        OHB_WIFI_DEFAULT_SUBSYS_DEVICE);

    /* Header type 0, no multifunction. */
    PDMPciDevSetHeaderType(pPciDev,         0);
    /* Interrupt pin 0 == none. No BARs, no IRQ line. */
    PDMPciDevSetByte(pPciDev, 0x3d,         0);
    PDMPciDevSetByte(pPciDev, 0x3e,         0);
    PDMPciDevSetByte(pPciDev, 0x3f,         0);

    int rc = PDMDevHlpPCIRegister(pDevIns, pPciDev);
    AssertRCReturn(rc, rc);

    /* OpenHuizeBox: honour per-VM PCI identity override from CFGM extradata. */
    ohbPciOverrideFromExtraData(pDevIns, pPciDev);

    /* Reflect post-override identity into shared state for diagnostics. */
    pThis->uVendorId = PDMPciDevGetVendorId(pPciDev);
    pThis->uDeviceId = PDMPciDevGetDeviceId(pPciDev);

    LogRel(("OhbWifi#%u: stub PCI Wi-Fi device registered - VEN_%04X&DEV_%04X (class %02X/%02X)\n",
            iInstance, pThis->uVendorId, pThis->uDeviceId,
            OHB_WIFI_PCI_CLASS_BASE, OHB_WIFI_PCI_CLASS_SUB));

    return VINF_SUCCESS;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceOhbWifi =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "ohbwifi",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS,
    /* .fClass = */                 PDM_DEVREG_CLASS_NETWORK,
    /* .cMaxInstances = */          1,
    /* .uSharedVersion = */         1,
    /* .cbInstanceShared = */       sizeof(OHBWIFISTATE),
    /* .cbInstanceCC = */           0,
    /* .cbInstanceRC = */           0,
    /* .cMaxPciDevices = */         1,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "OpenHuizeBox stealth Wi-Fi PCI stub (no I/O, identity only).\n",
#if defined(IN_RING3)
    /* .pszRCMod = */               "",
    /* .pszR0Mod = */               "",
    /* .pfnConstruct = */           ohbWifiR3Construct,
    /* .pfnDestruct = */            ohbWifiR3Destruct,
    /* .pfnRelocate = */            NULL,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               NULL,
    /* .pfnSuspend = */             NULL,
    /* .pfnResume = */              NULL,
    /* .pfnAttach = */              NULL,
    /* .pfnDetach = */              NULL,
    /* .pfnQueryInterface = */      NULL,
    /* .pfnInitComplete = */        NULL,
    /* .pfnPowerOff = */            NULL,
    /* .pfnSoftReset = */           NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#else
# error "Not in IN_RING3 - OHB Wi-Fi stub is R3-only"
#endif
    /* .u32VersionEnd = */          PDM_DEVREG_VERSION
};

#endif /* VBOX_WITH_OHB_VMX_STEALTH */
