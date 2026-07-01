/*******************************************************************************
 * FILE:         gte_regs.c
 * MODULE:       Core/GTE
 * DESCRIPTION:  Host-test software GTE register file definition.
 *
 *               On real MIPS hardware, the GTE register file is hardware that
 *               lives in the coprocessor. On the host test build (PSX_HOST_TEST),
 *               we emulate it as a C struct (GTE_Regs g_gte_regs), defined here.
 *
 *               This translation unit is only compiled when PSX_HOST_TEST is
 *               active. The cross-compiled PSX build does NOT include this file
 *               — GTE registers are physical hardware on the target.
 *
 * DEPENDENCIES: gte.h (for GTE_Regs and PSX_HOST_TEST guard)
 *******************************************************************************/

#include "gte.h"

#ifdef PSX_HOST_TEST

/* The single software register file instance used by all gte.h inline
 * functions when compiled for the host. Initialized to all-zero, which
 * represents a passthrough identity state (no rotation, no translation). */
GTE_Regs g_gte_regs = { 0 };

#endif /* PSX_HOST_TEST */
