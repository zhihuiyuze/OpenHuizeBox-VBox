/* $Id$ */
/** @file
 * OpenHuizeBox VMX stealth — R0 logging / decoding helpers.
 *
 * Small, self-contained helpers used by the VMX exit handlers in
 * VMXAllTemplate.cpp.h (the inline handlers for
 * VMX_EXIT_GDTR_IDTR_ACCESS and VMX_EXIT_LDTR_TR_ACCESS). Kept here
 * so the large template file stays readable.
 *
 * Feature is gated on the compile-time flag VBOX_WITH_OHB_VMX_STEALTH
 * and at runtime via the per-VM flag pVM->hm.s.fOhbHideDescTables
 * (seeded by HMR3Init from CFGM key "HM/OhbHideDescTables").
 *
 * Copyright 2026 OpenHuizeBox Project.
 * SPDX-License-Identifier: GPL-3.0-only
 */
#define LOG_GROUP LOG_GROUP_HM
#include "HMInternal.h"          /* MUST be before vmcc.h — sets the
                                  * VMM_INCLUDED_SRC_include_HMInternal_h
                                  * guard that exposes VM::hm.s. */
#include <VBox/vmm/vmcc.h>
#include <VBox/log.h>
#include "OhbVmxStealthR0.h"


/**
 * Decode VMX exit-qualification bits 29:28 for EXIT_GDTR_IDTR_ACCESS
 * into an instruction-name string.
 *
 * Per Intel SDM Vol 3, Table 27-8:
 *   0 = SGDT, 1 = SIDT, 2 = LGDT, 3 = LIDT.
 */
DECLHIDDEN(const char *) ohbVmxDescribeGdtrIdtrExit(uint64_t uExitQual)
{
    switch ((uExitQual >> 28) & 0x3)
    {
        case 0: return "SGDT";
        case 1: return "SIDT";
        case 2: return "LGDT";
        case 3: return "LIDT";
    }
    return "?";
}


/**
 * Decode VMX exit-qualification bits 29:28 for EXIT_LDTR_TR_ACCESS.
 *
 * Per Intel SDM Vol 3, Table 27-8:
 *   0 = SLDT, 1 = STR, 2 = LLDT, 3 = LTR.
 */
DECLHIDDEN(const char *) ohbVmxDescribeLdtrTrExit(uint64_t uExitQual)
{
    switch ((uExitQual >> 28) & 0x3)
    {
        case 0: return "SLDT";
        case 1: return "STR";
        case 2: return "LLDT";
        case 3: return "LTR";
    }
    return "?";
}


/**
 * Is descriptor-table exiting requested for this VM?
 *
 * Read once at VM start by the VMCS setup path; the flag is seeded
 * from CFGM "HM/OhbHideDescTables" at HMR3Init.
 */
DECLHIDDEN(bool) ohbVmxDescTableExitEnabled(PVMCC pVM)
{
    AssertPtrReturn(pVM, false);
#ifdef VBOX_WITH_OHB_VMX_STEALTH
    return pVM->hm.s.fOhbHideDescTables != 0;
#else
    return false;
#endif
}
