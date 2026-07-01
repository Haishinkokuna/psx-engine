/*******************************************************************************
 * FILE:         vec3.h
 * MODULE:       Core/Math
 * DESCRIPTION:  Fixed-point 3-component vector type and operations for the
 *               PSX runtime. All components are FP12_4 (world-space format)
 *               to match the GTE's VECTOR type layout.
 *
 *               The GTE (Geometry Transformation Engine, Coprocessor 2) expects
 *               its input vectors as 32-bit integers in a 12.4 fixed-point
 *               format. By storing Vec3 in the same format, we can pass these
 *               structs directly to GTE load macros without reformatting.
 *
 *               DESIGN CHOICE — Struct vs. SVECTOR:
 *                 PSn00bSDK defines SVECTOR (short, 16-bit per component) and
 *                 VECTOR (long, 32-bit per component). Vec3 here uses int32_t
 *                 per component to match VECTOR. For GTE input that expects
 *                 SVECTOR, use Vec3_ToSVector() to downcast (with saturation).
 *
 *               INLINE POLICY:
 *                 All functions are static inline. On R3000A, function call
 *                 overhead (JAL + two pipeline stalls) matters. Inlining these
 *                 small operations eliminates that overhead at the cost of
 *                 slightly larger code — acceptable for a math hot path.
 *
 * DEPENDENCIES: fixed.h, <stdint.h>
 *******************************************************************************/

#ifndef PSX_VEC3_H
#define PSX_VEC3_H

#include <stdint.h>
#include "fixed.h"

/* ---------------------------------------------------------------------------
 * Vec3 — 3-component fixed-point vector (FP12_4 world-space format)
 * Memory layout matches the PSX GTE VECTOR struct: three int32_t.
 * --------------------------------------------------------------------------- */

typedef struct {
    FP12_4 x;
    FP12_4 y;
    FP12_4 z;
} Vec3;

/* ---------------------------------------------------------------------------
 * Vec3_Zero — Construct the zero vector (origin / no-displacement).
 * --------------------------------------------------------------------------- */

static inline Vec3 Vec3_Zero(void)
{
    Vec3 v;
    v.x = 0;
    v.y = 0;
    v.z = 0;
    return v;
}

/* ---------------------------------------------------------------------------
 * Vec3_Make — Construct a Vec3 from three FP12_4 component values.
 * Use INT_TO_FP12_4() to convert from integer literals.
 * Example: Vec3_Make(INT_TO_FP12_4(5), INT_TO_FP12_4(-2), INT_TO_FP12_4(0))
 * --------------------------------------------------------------------------- */

static inline Vec3 Vec3_Make(FP12_4 x, FP12_4 y, FP12_4 z)
{
    Vec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

/* ---------------------------------------------------------------------------
 * Vec3_Add — Component-wise addition.
 * Adding two FP values of the same format is always safe — the scale cancels.
 * --------------------------------------------------------------------------- */

static inline Vec3 Vec3_Add(Vec3 a, Vec3 b)
{
    Vec3 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    return result;
}

/* ---------------------------------------------------------------------------
 * Vec3_Sub — Component-wise subtraction.
 * --------------------------------------------------------------------------- */

static inline Vec3 Vec3_Sub(Vec3 a, Vec3 b)
{
    Vec3 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    return result;
}

/* ---------------------------------------------------------------------------
 * Vec3_Scale — Multiply every component by a FP12_4 scalar.
 *
 * FP12_4 * FP12_4 doubles the fractional bits, so we shift right by
 * FP12_4_FRAC (4) after each multiply to restore the correct scale.
 * The int64_t intermediate prevents overflow for large world coordinates.
 * --------------------------------------------------------------------------- */

static inline Vec3 Vec3_Scale(Vec3 v, FP12_4 s)
{
    Vec3 result;
    result.x = FP12_4_MUL(v.x, s);
    result.y = FP12_4_MUL(v.y, s);
    result.z = FP12_4_MUL(v.z, s);
    return result;
}

/* ---------------------------------------------------------------------------
 * Vec3_Negate — Flip all component signs.
 * --------------------------------------------------------------------------- */

static inline Vec3 Vec3_Negate(Vec3 v)
{
    Vec3 result;
    result.x = -v.x;
    result.y = -v.y;
    result.z = -v.z;
    return result;
}

/* ---------------------------------------------------------------------------
 * Vec3_Dot — Dot product of two vectors.
 *
 * Dot product = a.x*b.x + a.y*b.y + a.z*b.z
 * Each component multiply doubles the fractional bits; we shift each back
 * by FP12_4_FRAC before summing to keep the result in FP12_4 scale.
 * Summing three FP12_4 values is safe — no scale adjustment needed for add.
 *
 * Returns FP12_4 scalar.
 * --------------------------------------------------------------------------- */

static inline FP12_4 Vec3_Dot(Vec3 a, Vec3 b)
{
    return FP12_4_MUL(a.x, b.x)
         + FP12_4_MUL(a.y, b.y)
         + FP12_4_MUL(a.z, b.z);
}

/* ---------------------------------------------------------------------------
 * Vec3_Cross — Cross product: result is perpendicular to both inputs.
 *
 * Cross product formulas:
 *   result.x = a.y*b.z - a.z*b.y
 *   result.y = a.z*b.x - a.x*b.z
 *   result.z = a.x*b.y - a.y*b.x
 *
 * Each multiply needs the shift-back via FP12_4_MUL. Subtraction of two
 * same-scale values requires no adjustment.
 * --------------------------------------------------------------------------- */

static inline Vec3 Vec3_Cross(Vec3 a, Vec3 b)
{
    Vec3 result;
    result.x = FP12_4_MUL(a.y, b.z) - FP12_4_MUL(a.z, b.y);
    result.y = FP12_4_MUL(a.z, b.x) - FP12_4_MUL(a.x, b.z);
    result.z = FP12_4_MUL(a.x, b.y) - FP12_4_MUL(a.y, b.x);
    return result;
}

/* ---------------------------------------------------------------------------
 * Vec3_LengthSq — Squared magnitude (avoids sqrt).
 *
 * Use this wherever you need to COMPARE lengths (e.g., LOD distance checks,
 * finding the nearest entity). Comparing squared lengths avoids the expensive
 * sqrt entirely — if A_sq < B_sq then len(A) < len(B).
 *
 * Returns FP12_4.
 * --------------------------------------------------------------------------- */

static inline FP12_4 Vec3_LengthSq(Vec3 v)
{
    return Vec3_Dot(v, v);
}

/* ---------------------------------------------------------------------------
 * Vec3_Lerp — Linear interpolation between two vectors.
 *
 * result = a + (b - a) * t,  where t is FP12_4 in [0, FP12_4_ONE].
 * t = 0         -> returns a exactly
 * t = FP12_4_ONE -> returns b exactly
 *
 * Used for smooth LOD transitions, billboard fade-in, and animation blending.
 * --------------------------------------------------------------------------- */

static inline Vec3 Vec3_Lerp(Vec3 a, Vec3 b, FP12_4 t)
{
    /* Compute (b - a) * t component-wise, then add a. */
    Vec3 delta = Vec3_Sub(b, a);
    Vec3 scaled = Vec3_Scale(delta, t);
    return Vec3_Add(a, scaled);
}

/* ---------------------------------------------------------------------------
 * Vec3_EqualEpsilon — Approximate equality within a given tolerance.
 *
 * Returns 1 (true) if all components differ by less than epsilon.
 * Use instead of == to handle fixed-point rounding accumulation.
 * --------------------------------------------------------------------------- */

static inline int Vec3_EqualEpsilon(Vec3 a, Vec3 b, FP12_4 epsilon)
{
    FP12_4 dx = FP_ABS(a.x - b.x);
    FP12_4 dy = FP_ABS(a.y - b.y);
    FP12_4 dz = FP_ABS(a.z - b.z);
    return (dx < epsilon) && (dy < epsilon) && (dz < epsilon);
}

#endif /* PSX_VEC3_H */
