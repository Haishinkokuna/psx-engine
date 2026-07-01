/*******************************************************************************
 * FILE:         gte.h
 * MODULE:       Core/GTE
 * DESCRIPTION:  GTE (Geometry Transformation Engine) Coprocessor 2 register
 *               map and load/store interface.
 *
 *               WHAT IS THE GTE?
 *               The GTE is a fixed-function math accelerator wired directly
 *               into the MIPS R3000A CPU die as Coprocessor 2. It performs
 *               the following operations in hardware, far faster than the CPU
 *               could do them in software:
 *                 - 3x3 fixed-point matrix * vector multiply (rotation)
 *                 - Vector + translation add
 *                 - Perspective division (divide by projected Z)
 *                 - Average Z for OT depth insertion
 *                 - Backface (NCLIP) test
 *                 - Lighting calculations (NCDS, NCDT, etc.)
 *
 *               GTE REGISTER MODEL:
 *               The GTE has 32 data registers (accessed via MFC2/MTC2) and
 *               32 control registers (accessed via CFC2/CTC2). The CPU writes
 *               inputs (vertices, matrices) into these registers, issues a
 *               GTE opcode (via a special COP2 instruction encoding), and
 *               reads back the results.
 *
 *               THIS FILE:
 *               Provides inline functions to load our engine types (Vec3, Mat3)
 *               into the GTE's register file and to read results back out.
 *               On MIPS hardware, these use inline assembly (MTC2/CTC2/MFC2).
 *               On the host test build (PSX_HOST_TEST=1), they write to a
 *               software-emulated register file so the logic can be tested
 *               without MIPS hardware.
 *
 *               GTE REGISTER REFERENCE (data registers):
 *               r0  VXY0  V0.x (lo), V0.y (hi)  — input vertex 0 X and Y
 *               r1  VZ0   V0.z (lo)              — input vertex 0 Z
 *               r2  VXY1  V1.x, V1.y             — input vertex 1
 *               r3  VZ1   V1.z
 *               r4  VXY2  V2.x, V2.y             — input vertex 2
 *               r5  VZ2   V2.z
 *               r6  RGBC  color/code
 *               r7  OTZ   average Z output (from AVSZ3/AVSZ4)
 *               r8  IR0   interpolation factor
 *               r9  IR1   intermediate X result
 *               r10 IR2   intermediate Y result
 *               r11 IR3   intermediate Z result
 *               r12 SXY0  screen XY fifo[0]  — projected vertex output
 *               r13 SXY1  screen XY fifo[1]
 *               r14 SXY2  screen XY fifo[2]
 *               r15 SXYP  most recent SXY
 *               r16 SZ0   screen Z  fifo[0]
 *               r17 SZ1   screen Z  fifo[1]
 *               r18 SZ2   screen Z  fifo[2]
 *               r19 SZ3   screen Z  fifo[3]
 *               r24 MAC0  accumulator 0 (32-bit)
 *               r25 MAC1  accumulator 1 (32-bit)
 *               r26 MAC2  accumulator 2
 *               r27 MAC3  accumulator 3
 *
 *               GTE REGISTER REFERENCE (control registers):
 *               cr0  R11R12  rot[0][0] (lo), rot[0][1] (hi)
 *               cr1  R13R21  rot[0][2] (lo), rot[1][0] (hi)
 *               cr2  R22R23  rot[1][1] (lo), rot[1][2] (hi)
 *               cr3  R31R32  rot[2][0] (lo), rot[2][1] (hi)
 *               cr4  R33     rot[2][2] (lo)
 *               cr5  TRX     translation X (32-bit, same scale as FP12_4 vertices)
 *               cr6  TRY     translation Y
 *               cr7  TRZ     translation Z
 *               cr26 H       projection plane distance (used in perspective div)
 *               cr29 ZSF3    Z-scale factor for AVSZ3 (Q12 fixed-point)
 *               cr30 ZSF4    Z-scale factor for AVSZ4
 *               cr31 FLAG    error/overflow flags (read-only result)
 *
 * DEPENDENCIES: vec3.h, mat3.h, fixed.h, <stdint.h>
 *******************************************************************************/

#ifndef PSX_GTE_H
#define PSX_GTE_H

#include <stdint.h>
#include "../math/fixed.h"
#include "../math/vec3.h"
#include "../math/mat3.h"

/* ===========================================================================
 * Host-test software GTE register file.
 *
 * On real MIPS hardware, registers live in the GTE coprocessor and are
 * accessed via MTC2/MFC2 instructions. On the host, we emulate them as a
 * simple struct so the load/store functions have somewhere to write.
 * The GTE opcodes (gte_ops.h) then do software-equivalent math.
 * =========================================================================== */

#ifdef PSX_HOST_TEST

typedef struct {
    /* Data registers */
    int16_t  vxy0_x,  vxy0_y;   /* r0:  VXY0 */
    int16_t  vz0;                /* r1:  VZ0  */
    int16_t  vxy1_x,  vxy1_y;   /* r2:  VXY1 */
    int16_t  vz1;                /* r3:  VZ1  */
    int16_t  vxy2_x,  vxy2_y;   /* r4:  VXY2 */
    int16_t  vz2;                /* r5:  VZ2  */
    int32_t  otz;                /* r7:  OTZ  */
    int16_t  sxy0_x,  sxy0_y;   /* r12: SXY0 */
    int16_t  sxy1_x,  sxy1_y;   /* r13: SXY1 */
    int16_t  sxy2_x,  sxy2_y;   /* r14: SXY2 */
    int32_t  sz0, sz1, sz2, sz3;/* r16-r19 */
    int32_t  mac0, mac1, mac2, mac3; /* r24-r27 */
    /* Control registers */
    int16_t  rt[9];              /* cr0-cr4: rotation matrix (3x3 int16) */
    int32_t  trx, try_, trz;    /* cr5-cr7: translation vector           */
    int32_t  h;                  /* cr26:    projection distance          */
    int16_t  zsf3, zsf4;         /* cr29-30: Z-scale factors for AVSZ    */
    uint32_t flag;               /* cr31:    error flags                  */
} GTE_Regs;

/* Single global instance of the software register file. */
extern GTE_Regs g_gte_regs;

#endif /* PSX_HOST_TEST */

/* ===========================================================================
 * GTE_SetRotMatrix — Load a rotation matrix into GTE control registers.
 *
 * The GTE rotation matrix is stored as five packed 32-bit control registers:
 *   cr0 = R[0][0] | (R[0][1] << 16)
 *   cr1 = R[0][2] | (R[1][0] << 16)
 *   cr2 = R[1][1] | (R[1][2] << 16)
 *   cr3 = R[2][0] | (R[2][1] << 16)
 *   cr4 = R[2][2]
 *
 * Our Mat3 stores values as int16_t m[9] in row-major order:
 *   m[0]=R[0][0], m[1]=R[0][1], m[2]=R[0][2],
 *   m[3]=R[1][0], m[4]=R[1][1], m[5]=R[1][2],
 *   m[6]=R[2][0], m[7]=R[2][1], m[8]=R[2][2]
 * These are FP4_12 values (scaled by 4096).
 * =========================================================================== */

static inline void GTE_SetRotMatrix(const Mat3* m)
{
#ifdef __mips__
    /* Pack pairs of int16_t into 32-bit words and write to COP2 via CTC2.
     * CTC2 writes to control register file (NOT data register file). */
    __asm__ volatile (
        "ctc2 %0, $0\n\t"   /* cr0 = R11R12 */
        "ctc2 %1, $1\n\t"   /* cr1 = R13R21 */
        "ctc2 %2, $2\n\t"   /* cr2 = R22R23 */
        "ctc2 %3, $3\n\t"   /* cr3 = R31R32 */
        "ctc2 %4, $4\n\t"   /* cr4 = R33    */
        :
        : "r" ((uint32_t)(uint16_t)m->m[0] | ((uint32_t)(uint16_t)m->m[1] << 16)),
          "r" ((uint32_t)(uint16_t)m->m[2] | ((uint32_t)(uint16_t)m->m[3] << 16)),
          "r" ((uint32_t)(uint16_t)m->m[4] | ((uint32_t)(uint16_t)m->m[5] << 16)),
          "r" ((uint32_t)(uint16_t)m->m[6] | ((uint32_t)(uint16_t)m->m[7] << 16)),
          "r" ((uint32_t)(uint16_t)m->m[8])
    );
#else
    /* Host test: copy into software register file. */
    int i;
    for (i = 0; i < 9; i++) {
        g_gte_regs.rt[i] = m->m[i];
    }
#endif
}

/* ===========================================================================
 * GTE_SetTransVector — Load a translation vector into GTE control registers.
 *
 * The GTE translation registers (TRX, TRY, TRZ) are 32-bit signed integers
 * at the same fixed-point scale as the vertex coordinates. Our Vec3 uses
 * FP12_4, so the translation should also be in FP12_4.
 * =========================================================================== */

static inline void GTE_SetTransVector(const Vec3* t)
{
#ifdef __mips__
    __asm__ volatile (
        "ctc2 %0, $5\n\t"   /* cr5 = TRX */
        "ctc2 %1, $6\n\t"   /* cr6 = TRY */
        "ctc2 %2, $7\n\t"   /* cr7 = TRZ */
        :
        : "r" (t->x), "r" (t->y), "r" (t->z)
    );
#else
    g_gte_regs.trx  = t->x;
    g_gte_regs.try_ = t->y;
    g_gte_regs.trz  = t->z;
#endif
}

/* ===========================================================================
 * GTE_SetProjection — Set the perspective projection plane distance (H).
 *
 * H is the distance between the viewpoint and the projection plane in the
 * GTE's perspective division formula:
 *   screen_x = (world_x * H) / screen_z
 *
 * Larger H = more perspective (narrow FOV feel).
 * Smaller H = less perspective (telephoto / isometric feel).
 *
 * Typical PSX games use H in the range 100-512. 256 is a common default
 * for a 320x240 viewport (gives approximately 90-degree horizontal FOV).
 * =========================================================================== */

static inline void GTE_SetProjection(int32_t h)
{
#ifdef __mips__
    __asm__ volatile (
        "ctc2 %0, $26\n\t"  /* cr26 = H */
        :
        : "r" (h)
    );
#else
    g_gte_regs.h = h;
#endif
}

/* ===========================================================================
 * GTE_SetZScaleFactors — Set ZSF3 and ZSF4 for AVSZ3 / AVSZ4 operations.
 *
 * AVSZ3 computes: OTZ = (SZ1 + SZ2 + SZ3) * ZSF3 >> 12
 * AVSZ4 computes: OTZ = (SZ0 + SZ1 + SZ2 + SZ3) * ZSF4 >> 12
 *
 * To get a true average of 3 Z values, set ZSF3 = 4096 / 3 = 1365.
 * To get a true average of 4 Z values, set ZSF4 = 4096 / 4 = 1024.
 * =========================================================================== */

static inline void GTE_SetZScaleFactors(int16_t zsf3, int16_t zsf4)
{
#ifdef __mips__
    __asm__ volatile (
        "ctc2 %0, $29\n\t"  /* cr29 = ZSF3 */
        "ctc2 %1, $30\n\t"  /* cr30 = ZSF4 */
        :
        : "r" ((int32_t)zsf3), "r" ((int32_t)zsf4)
    );
#else
    g_gte_regs.zsf3 = zsf3;
    g_gte_regs.zsf4 = zsf4;
#endif
}

/* ===========================================================================
 * GTE_SetVertex0/1/2 — Load input vertices into GTE data registers.
 *
 * The GTE processes vertices in FP12_4 scale (same as our Vec3 type).
 * Vertices are loaded as three pairs of int16_t packed into 32-bit words:
 *   VXY: low  16 bits = X, high 16 bits = Y
 *   VZ:  low  16 bits = Z
 *
 * NOTE: Vec3 components are FP12_4 int32_t, but the GTE only accepts int16_t
 * vertex inputs. This means world coordinates must fit in [-32768, +32767]
 * in FP12_4 scale, i.e., world units from -2048 to +2047. For larger worlds,
 * translate into camera space (subtract camera position) before loading.
 * =========================================================================== */

static inline void GTE_SetVertex0(const Vec3* v)
{
#ifdef __mips__
    __asm__ volatile (
        "mtc2 %0, $0\n\t"   /* r0 = VXY0: X | (Y << 16) */
        "mtc2 %1, $1\n\t"   /* r1 = VZ0:  Z              */
        :
        : "r" ((uint32_t)(uint16_t)(int16_t)v->x | ((uint32_t)(uint16_t)(int16_t)v->y << 16)),
          "r" ((int32_t)(int16_t)v->z)
    );
#else
    g_gte_regs.vxy0_x = (int16_t)v->x;
    g_gte_regs.vxy0_y = (int16_t)v->y;
    g_gte_regs.vz0    = (int16_t)v->z;
#endif
}

static inline void GTE_SetVertex1(const Vec3* v)
{
#ifdef __mips__
    __asm__ volatile (
        "mtc2 %0, $2\n\t"
        "mtc2 %1, $3\n\t"
        :
        : "r" ((uint32_t)(uint16_t)(int16_t)v->x | ((uint32_t)(uint16_t)(int16_t)v->y << 16)),
          "r" ((int32_t)(int16_t)v->z)
    );
#else
    g_gte_regs.vxy1_x = (int16_t)v->x;
    g_gte_regs.vxy1_y = (int16_t)v->y;
    g_gte_regs.vz1    = (int16_t)v->z;
#endif
}

static inline void GTE_SetVertex2(const Vec3* v)
{
#ifdef __mips__
    __asm__ volatile (
        "mtc2 %0, $4\n\t"
        "mtc2 %1, $5\n\t"
        :
        : "r" ((uint32_t)(uint16_t)(int16_t)v->x | ((uint32_t)(uint16_t)(int16_t)v->y << 16)),
          "r" ((int32_t)(int16_t)v->z)
    );
#else
    g_gte_regs.vxy2_x = (int16_t)v->x;
    g_gte_regs.vxy2_y = (int16_t)v->y;
    g_gte_regs.vz2    = (int16_t)v->z;
#endif
}

/* ===========================================================================
 * GTE Read-back functions — retrieve results after executing a GTE opcode.
 * =========================================================================== */

/* GTE_ReadSXY0/1/2 — Read projected screen XY from the SXY FIFO.
 *
 * After RTPS or RTPT executes, the GTE writes screen-space (sx, sy)
 * coordinates for each input vertex into the SXY FIFO slots 0, 1, 2.
 * These values are already in screen pixels, centered at (OFX, OFY) which
 * defaults to (screen_width/2, screen_height/2) in the GTE setup. */

static inline void GTE_ReadSXY0(int16_t* sx, int16_t* sy)
{
#ifdef __mips__
    uint32_t val;
    __asm__ volatile (
        "mfc2 %0, $12\n\t"  /* r12 = SXY0 */
        : "=r" (val)
    );
    *sx = (int16_t)(val & 0xFFFFu);
    *sy = (int16_t)(val >> 16);
#else
    *sx = g_gte_regs.sxy0_x;
    *sy = g_gte_regs.sxy0_y;
#endif
}

static inline void GTE_ReadSXY1(int16_t* sx, int16_t* sy)
{
#ifdef __mips__
    uint32_t val;
    __asm__ volatile ("mfc2 %0, $13\n\t" : "=r" (val));
    *sx = (int16_t)(val & 0xFFFFu);
    *sy = (int16_t)(val >> 16);
#else
    *sx = g_gte_regs.sxy1_x;
    *sy = g_gte_regs.sxy1_y;
#endif
}

static inline void GTE_ReadSXY2(int16_t* sx, int16_t* sy)
{
#ifdef __mips__
    uint32_t val;
    __asm__ volatile ("mfc2 %0, $14\n\t" : "=r" (val));
    *sx = (int16_t)(val & 0xFFFFu);
    *sy = (int16_t)(val >> 16);
#else
    *sx = g_gte_regs.sxy2_x;
    *sy = g_gte_regs.sxy2_y;
#endif
}

/* GTE_ReadOTZ — Read the average Z value (for OT insertion depth).
 * Valid after AVSZ3 or AVSZ4. Returns the Z depth bucket index.
 * Caller clamps to [0, OT_ENTRY_COUNT-1] before passing to OT_Add. */

static inline int32_t GTE_ReadOTZ(void)
{
#ifdef __mips__
    int32_t val;
    __asm__ volatile ("mfc2 %0, $7\n\t" : "=r" (val));  /* r7 = OTZ */
    return val;
#else
    return g_gte_regs.otz;
#endif
}

/* GTE_ReadMAC0 — Read MAC0 (used after NCLIP to get the cross product sign).
 * Positive MAC0 = front-facing (counter-clockwise winding in screen space).
 * Negative or zero = back-facing or edge-on (cull this triangle). */

static inline int32_t GTE_ReadMAC0(void)
{
#ifdef __mips__
    int32_t val;
    __asm__ volatile ("mfc2 %0, $24\n\t" : "=r" (val)); /* r24 = MAC0 */
    return val;
#else
    return g_gte_regs.mac0;
#endif
}

/* GTE_ReadFLAG — Read the GTE flag register (error/overflow detection).
 * Non-zero value indicates a clamp or overflow occurred during the last op.
 * Bit 31 = 1 if any error bit is set (convenient single-check). */

static inline uint32_t GTE_ReadFLAG(void)
{
#ifdef __mips__
    uint32_t val;
    __asm__ volatile ("cfc2 %0, $31\n\t" : "=r" (val)); /* cr31 = FLAG */
    return val;
#else
    return g_gte_regs.flag;
#endif
}

/* ===========================================================================
 * GTE_SetScreenOffset — Set the screen-space output center point (OFX, OFY).
 *
 * The GTE adds this offset to all projected screen coordinates after
 * perspective division. For a 320x240 display, set to (160*16, 120*16) —
 * the values are in GTE sub-pixel units (Q4, shifted by 4).
 *
 * Control registers:
 *   cr24 = OFX: X offset (Q4 fixed-point, e.g., 160*16 = 2560 for 320-wide)
 *   cr25 = OFY: Y offset (Q4 fixed-point, e.g., 120*16 = 1920 for 240-tall)
 * =========================================================================== */

static inline void GTE_SetScreenOffset(int32_t ofx_q4, int32_t ofy_q4)
{
#ifdef __mips__
    __asm__ volatile (
        "ctc2 %0, $24\n\t"  /* cr24 = OFX */
        "ctc2 %1, $25\n\t"  /* cr25 = OFY */
        :
        : "r" (ofx_q4), "r" (ofy_q4)
    );
#else
    /* Store in spare mac fields for host-test access by gte_ops software impl. */
    g_gte_regs.mac1 = ofx_q4;
    g_gte_regs.mac2 = ofy_q4;
#endif
}

/* ===========================================================================
 * GTE_Init — Initialize all GTE control registers to sane defaults.
 *
 * Call once at startup, after Display_Init(). Sets:
 *   - Screen offset to center of a 320x240 display
 *   - Projection distance to 256 (90-degree approximate FOV)
 *   - ZSF3 = 1365 (1/3 scale, for true 3-vertex average)
 *   - ZSF4 = 1024 (1/4 scale, for true 4-vertex average)
 *   - Rotation matrix = identity
 *   - Translation = (0, 0, 0)
 * =========================================================================== */

static inline void GTE_Init(void)
{
    static const Mat3 identity = {{ 4096, 0, 0,  /* FP4_12 identity: 1.0 on diagonal */
                                    0, 4096, 0,
                                    0, 0, 4096 }};
    static const Vec3 zero     = { 0, 0, 0 };

    GTE_SetRotMatrix(&identity);
    GTE_SetTransVector(&zero);

    /* 320x240 center in Q4 units: 160*16=2560, 120*16=1920 */
    GTE_SetScreenOffset(2560, 1920);

    /* Standard perspective distance. Increase for narrower FOV, decrease for wider. */
    GTE_SetProjection(256);

    /* ZSF3 = 4096/3 ≈ 1365 gives true 1/3 average for AVSZ3.
     * ZSF4 = 4096/4 = 1024 gives true 1/4 average for AVSZ4. */
    GTE_SetZScaleFactors(1365, 1024);
}

#endif /* PSX_GTE_H */
