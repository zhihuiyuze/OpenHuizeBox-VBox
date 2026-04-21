/* $Id$ */
/** @file
 * OpenHuizeBox VMX stealth — public helper API.
 *
 * Copyright 2026 OpenHuizeBox Project.
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef VBOX_INCLUDED_SRC_VMMR0_target_x86_OhbVmxStealthR0_h
#define VBOX_INCLUDED_SRC_VMMR0_target_x86_OhbVmxStealthR0_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/vmm/vmcc.h>

RT_C_DECLS_BEGIN

/** Describe SGDT / SIDT / LGDT / LIDT based on exit qualification. */
DECLHIDDEN(const char *) ohbVmxDescribeGdtrIdtrExit(uint64_t uExitQual);

/** Describe SLDT / STR / LLDT / LTR based on exit qualification. */
DECLHIDDEN(const char *) ohbVmxDescribeLdtrTrExit(uint64_t uExitQual);

/** Runtime check: is desc-table exiting on for this VM? */
DECLHIDDEN(bool) ohbVmxDescTableExitEnabled(PVMCC pVM);

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_SRC_VMMR0_target_x86_OhbVmxStealthR0_h */
