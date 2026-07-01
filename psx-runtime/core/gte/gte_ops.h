/*******************************************************************************
 * FILE:         gte_ops.h
 * MODULE:       Core/GTE
 * DESCRIPTION:  GTE opcode execution wrappers.
 *
 *               Each function in this file triggers one GTE operation. On MIPS
 *               hardware, GTE operations are encoded as special COP2 instructions
 *               (opcode 0x12) with a 25-bit function code. The CPU decodes them,
 *               waits for the GTE to complete (1-21 cycles depending on the op),
 *               and makes the results available in the GTE data register file.
 *
 *               LATENCY AND HAZARDS:
 *               The GTE is pipelined. After issuing a GTE opcode, the CPU must
 *               not read GTE output registers before the operation completes.
 *               The number of cycles to wait varies by operation. The MIPS
 *               assembler handles insertion of NOP delays when necessary, but
 *               to be safe, results should only be read in a separate function
 *               call (i.e., do not issue an opcode AND read results in the same
 *               __asm__ block without nops between them).
 *
 *               On the host test build (PSX_HOST_TEST=1), these functions
 *               perform software-equivalent computations using our fixed-point
 *               math library, writing results into g_gte_regs so the read-back
 *               functions in gte.h return correct values.
 *
 *               GTE OPCODE REFERENCE:
 *               Encoded as: cop2 <function_code>
 *               The function code is a 25-bit value embedded in the instruction.
 *
 *               RTPS  0x0180001  Rotate/translate/perspective, single vertex
 *               RTPT  0x0280030  Rotate/translate/perspective, three vertices
 *               NCLIP 0x1400006  Normal clip (backface test via 2D cross product)
 *               AVSZ3 0x158002D  Average Z of SZ1+SZ2+SZ3 -> OTZ
 *               AVSZ4 0x168002E  Average Z of SZ0+SZ1+SZ2+SZ3 -> OTZ
 *               NCDS  0x0E80413  Normal color single (lighting for one vertex)
 *               NCDT  0x0F80416  Normal color triple (lighting for three vertices)
 *               OP    0x178000C  Cross product of IR with RT matrix column
 *               GPF   0x190003D  General purpose interpolation forward
 *               GPL   0x1A0003E  General purpose interpolation backward
 *               MVMVA 0x0400012  Matrix-vector multiply and add
 *               DPCS  0x0780010  Depth cue single
 *               DPCT  0x0F80018  Depth cue triple
 *               INTPL 0x0800012  Interpolation
 *               SQR   0x0A00428  Square of IR vector
 *               NCS   0x0C80413  Normal color single (no depth cue)
 *               NCT   0x0D80416  Normal color triple (no depth cue)
 *
 * DEPENDENCIES: gte.h, fixed.h, vec3.h, mat3.h
 *******************************************************************************/

#ifndef PSX_GTE_OPS_H
#define PSX_GTE_OPS_H

#include <stdint.h>
#include "gte.h"
#include "../math/fixed.h"
#include "../math/vec3.h"
#include "../math/mat3.h"

/* ---------------------------------------------------------------------------
 * GTE_RTPS — Rotate, Translate, Perspective — Single vertex.
 *
 * Transforms vertex V0 (loaded via GTE_SetVertex0) by the current rotation
 * matrix and translation vector, then applies perspective division using H.
 *
 * Result: Screen XY written to SXY2 (and shifted through FIFO: SXY0<-SXY1,
 *         SXY1<-SXY2, SXY2<-new). Screen Z written to SZ FIFO.
 *
 * Latency: ~15 cycles. Do not read SXY2 in the instruction immediately after.
 * --------------------------------------------------------------------------- */

static inline void GTE_RTPS(void)
{
#ifdef __mips__
    __asm__ volatile ("cop2 0x0180001\n\tnop\n\tnop" : : : "memory");
#else
    /* Software equivalent:
     * transformed = RT * V0 + TR (3x3 matrix multiply + translation)
     * Coordinates in FP12_4 * FP4_12 = FP16_16, then shift back to FP12_4. */
    int32_t tx = ((int32_t)g_gte_regs.rt[0] * g_gte_regs.vxy0_x
                + (int32_t)g_gte_regs.rt[1] * g_gte_regs.vxy0_y
                + (int32_t)g_gte_regs.rt[2] * g_gte_regs.vz0) >> 12;
    int32_t ty = ((int32_t)g_gte_regs.rt[3] * g_gte_regs.vxy0_x
                + (int32_t)g_gte_regs.rt[4] * g_gte_regs.vxy0_y
                + (int32_t)g_gte_regs.rt[5] * g_gte_regs.vz0) >> 12;
    int32_t tz = ((int32_t)g_gte_regs.rt[6] * g_gte_regs.vxy0_x
                + (int32_t)g_gte_regs.rt[7] * g_gte_regs.vxy0_y
                + (int32_t)g_gte_regs.rt[8] * g_gte_regs.vz0) >> 12;

    tx += g_gte_regs.trx;
    ty += g_gte_regs.try_;
    tz += g_gte_regs.trz;

    /* Perspective division: sx = ofx + (tx * H) / tz (in Q4 units).
     * Guard against divide-by-zero with a minimum tz. */
    if (tz < 1) tz = 1;
    int32_t ofx = g_gte_regs.mac1; /* stored in mac1/mac2 by GTE_SetScreenOffset */
    int32_t ofy = g_gte_regs.mac2;

    /* Shift SXY fifo: 0<-1, 1<-2, 2<-new */
    g_gte_regs.sxy0_x = g_gte_regs.sxy1_x;
    g_gte_regs.sxy0_y = g_gte_regs.sxy1_y;
    g_gte_regs.sxy1_x = g_gte_regs.sxy2_x;
    g_gte_regs.sxy1_y = g_gte_regs.sxy2_y;

    /* The result is in Q4 (sub-pixel), then divide by 16 to get actual pixel. */
    g_gte_regs.sxy2_x = (int16_t)((ofx + (tx * g_gte_regs.h) / tz) >> 4);
    g_gte_regs.sxy2_y = (int16_t)((ofy + (ty * g_gte_regs.h) / tz) >> 4);

    /* Shift SZ fifo: 0<-1, 1<-2, 2<-3, 3<-new */
    g_gte_regs.sz0 = g_gte_regs.sz1;
    g_gte_regs.sz1 = g_gte_regs.sz2;
    g_gte_regs.sz2 = g_gte_regs.sz3;
    g_gte_regs.sz3 = tz >> 4; /* Screen Z is in Q4 units */
#endif
}

/* ---------------------------------------------------------------------------
 * GTE_RTPT — Rotate, Translate, Perspective — Three vertices (V0, V1, V2).
 *
 * Functionally equivalent to three consecutive RTPS operations but executes
 * in fewer total cycles (~23 vs ~45 for three separate RTPS calls).
 *
 * Result: SXY0, SXY1, SXY2 = projected screen coordinates for V0, V1, V2.
 *         SZ1, SZ2, SZ3    = screen Z values for V0, V1, V2.
 *
 * This is the standard call for triangle rendering. Load V0/V1/V2 via
 * GTE_SetVertex0/1/2, call RTPT, then read SXY0/1/2 for screen coordinates.
 * --------------------------------------------------------------------------- */

static inline void GTE_RTPT(void)
{
#ifdef __mips__
    __asm__ volatile ("cop2 0x0280030\n\tnop\n\tnop" : : : "memory");
#else
    /* Software equivalent: run three RTPS passes.
     * We temporarily swap V1/V2 into V0 position for the compute. */
    int16_t saved_x, saved_y, saved_z;

    /* Transform V0 (already in vxy0/vz0): */
    GTE_RTPS();

    /* Transform V1: copy V1 into V0 slot, run RTPS, restore. */
    saved_x = g_gte_regs.vxy0_x;
    saved_y = g_gte_regs.vxy0_y;
    saved_z = g_gte_regs.vz0;

    g_gte_regs.vxy0_x = g_gte_regs.vxy1_x;
    g_gte_regs.vxy0_y = g_gte_regs.vxy1_y;
    g_gte_regs.vz0    = g_gte_regs.vz1;
    GTE_RTPS();

    g_gte_regs.vxy0_x = g_gte_regs.vxy2_x;
    g_gte_regs.vxy0_y = g_gte_regs.vxy2_y;
    g_gte_regs.vz0    = g_gte_regs.vz2;
    GTE_RTPS();

    /* Restore V0 slot. */
    g_gte_regs.vxy0_x = saved_x;
    g_gte_regs.vxy0_y = saved_y;
    g_gte_regs.vz0    = saved_z;
#endif
}

/* ---------------------------------------------------------------------------
 * GTE_NCLIP — Normal Clip (backface culling via 2D cross product).
 *
 * Computes the signed area of the triangle formed by SXY0, SXY1, SXY2.
 * Result is written to MAC0:
 *   MAC0 > 0 : front-facing (CCW winding in screen space) -> DRAW
 *   MAC0 <= 0: back-facing or edge-on                     -> CULL
 *
 * Formula: MAC0 = (SX1-SX0)*(SY2-SY0) - (SX2-SX0)*(SY1-SY0)
 *
 * Call GTE_ReadMAC0() after this to get the result.
 * Latency: ~8 cycles.
 * --------------------------------------------------------------------------- */

static inline void GTE_NCLIP(void)
{
#ifdef __mips__
    __asm__ volatile ("cop2 0x1400006\n\tnop" : : : "memory");
#else
    /* 2D cross product of the screen-space triangle. */
    int32_t ax = g_gte_regs.sxy1_x - g_gte_regs.sxy0_x;
    int32_t ay = g_gte_regs.sxy1_y - g_gte_regs.sxy0_y;
    int32_t bx = g_gte_regs.sxy2_x - g_gte_regs.sxy0_x;
    int32_t by = g_gte_regs.sxy2_y - g_gte_regs.sxy0_y;
    g_gte_regs.mac0 = ax * by - bx * ay;
#endif
}

/* ---------------------------------------------------------------------------
 * GTE_AVSZ3 — Average Z of SZ1, SZ2, SZ3 -> OTZ.
 *
 * Computes: OTZ = (SZ1 + SZ2 + SZ3) * ZSF3 >> 12
 * Use after RTPT to get the OT insertion depth for the rendered triangle.
 *
 * Call GTE_ReadOTZ() after this to get the bucket index.
 * Latency: ~5 cycles.
 * --------------------------------------------------------------------------- */

static inline void GTE_AVSZ3(void)
{
#ifdef __mips__
    __asm__ volatile ("cop2 0x158002D\n\tnop" : : : "memory");
#else
    /* (SZ1 + SZ2 + SZ3) * ZSF3 >> 12 */
    int32_t sum = g_gte_regs.sz1 + g_gte_regs.sz2 + g_gte_regs.sz3;
    g_gte_regs.otz = (int32_t)(((int64_t)sum * g_gte_regs.zsf3) >> 12);
#endif
}

/* ---------------------------------------------------------------------------
 * GTE_AVSZ4 — Average Z of SZ0, SZ1, SZ2, SZ3 -> OTZ.
 *
 * Use for quads (four vertices). Computes the average of all four screen Z
 * values for a more accurate depth bucket placement.
 * --------------------------------------------------------------------------- */

static inline void GTE_AVSZ4(void)
{
#ifdef __mips__
    __asm__ volatile ("cop2 0x168002E\n\tnop" : : : "memory");
#else
    int32_t sum = g_gte_regs.sz0 + g_gte_regs.sz1
                + g_gte_regs.sz2 + g_gte_regs.sz3;
    g_gte_regs.otz = (int32_t)(((int64_t)sum * g_gte_regs.zsf4) >> 12);
#endif
}

/* ---------------------------------------------------------------------------
 * GTE_OP — Cross product of the IR vector with the rotation matrix columns.
 * Used for object-space normal transformation (lighting).
 * --------------------------------------------------------------------------- */

static inline void GTE_OP(void)
{
#ifdef __mips__
    __asm__ volatile ("cop2 0x178000C\n\tnop" : : : "memory");
#else
    /* Simplified stub for host test — full implementation not needed yet. */
    (void)0;
#endif
}

#endif /* PSX_GTE_OPS_H */
