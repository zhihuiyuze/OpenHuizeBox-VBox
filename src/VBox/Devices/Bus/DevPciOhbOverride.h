/* $Id$ */
/** @file
 * OpenHuizeBox PCI identity override — header.
 *
 * Include from each device R3 ctor that wants per-VM PCI identity
 * override via CFGM extradata. See DevPciOhbOverride.cpp for the
 * full per-field specification.
 *
 * Copyright 2026 OpenHuizeBox Project.
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef VBOX_INCLUDED_SRC_Bus_DevPciOhbOverride_h
#define VBOX_INCLUDED_SRC_Bus_DevPciOhbOverride_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/vmm/pdmdev.h>

RT_C_DECLS_BEGIN

/**
 * Read OHB per-device PCI identity override keys from CFGM extradata
 * and patch the given PCI device's config space in place.
 *
 * Safe to call with no keys present — becomes a no-op.
 *
 * @returns VINF_SUCCESS (never fails hard).
 */
DECLHIDDEN(int) ohbPciOverrideFromExtraData(PPDMDEVINS pDevIns,
                                             PPDMPCIDEV pPciDev);

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_SRC_Bus_DevPciOhbOverride_h */
