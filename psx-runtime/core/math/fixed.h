/*******************************************************************************
 * FILE:         fixed.h
 * MODULE:       Core/Math
 * DESCRIPTION:  Core fixed-point arithmetic type definitions, constants, and
 *               operation macros for the PSX runtime.
 *
 *               The PSX MIPS R3000A has NO hardware floating-point unit.
 *               Every value that would normally be a float must instead live
 *               as a scaled integer. This file defines that scaling contract
 *               for the entire engine.
 *
 *               Two formats are defined:
 *
 *                 FP_4_12  (int32_t)  -  4 integer bits, 12 fractional bits.
 *                   Scale factor = 4096.  Range: -8.0 to ~7.9998.
 *                   Use for: normals, rotation components, sub-unit values
 *                   where high fractional precision matters most.
 *
 *                 FP_12_4  (int32_t)  - 12 integer bits, 4 fractional bits.
 *                   Scale factor = 16.    Range: -2048.0 to ~2047.9375.
 *                   Use for: world-space XYZ coordinates. This matches the
 *                   native layout of the GTE's SVECTOR (short) and VECTOR
 *                   (long) types — the GTE expects its input in this format.
 *
 *               MULTIPLY RULE:
 *                 When multiplying two FP values, the result has DOUBLE the
 *                 fractional bits. You must shift right by the fractional width
 *                 to restore the original scale. The macros below handle this.
 *                 Intermediates use int64_t to prevent 32-bit overflow — on
 *                 R3000A this compiles to MULT/MFHI/MFLO, which is a known,
 *                 acceptable cost for software math paths not on the GTE.
 *
 * DEPENDENCIES: <stdint.h>  (C99 integer types; no libc math headers)
 *******************************************************************************/

#ifndef PSX_FIXED_H
#define PSX_FIXED_H

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Type Aliases
 * These are plain int32_t under the hood. The typedef names serve as
 * documentation — they communicate intent to the reader at every call site.
 * --------------------------------------------------------------------------- */

/* High-precision fractional format: 4 integer bits, 12 fractional bits.
 * Scale factor (FP4_12_ONE) = 4096. Used for normals, angles, weights. */
typedef int32_t FP4_12;

/* World-space coordinate format: 12 integer bits, 4 fractional bits.
 * Scale factor (FP12_4_ONE) = 16. Used for XYZ positions. Matches GTE. */
typedef int32_t FP12_4;

/* ---------------------------------------------------------------------------
 * Scale Constants
 * "ONE" is the fixed-point representation of the real number 1.0.
 * All constants in a fixed-point system are expressed as multiples of ONE.
 * --------------------------------------------------------------------------- */

/* FP4_12: 1.0 = 1 << 12 = 4096 */
#define FP4_12_ONE      ((FP4_12)4096)
#define FP4_12_HALF     ((FP4_12)2048)
#define FP4_12_QUARTER  ((FP4_12)1024)
#define FP4_12_FRAC     12                /* Number of fractional bits */

/* FP12_4: 1.0 = 1 << 4 = 16 */
#define FP12_4_ONE      ((FP12_4)16)
#define FP12_4_HALF     ((FP12_4)8)
#define FP12_4_QUARTER  ((FP12_4)4)
#define FP12_4_FRAC     4                 /* Number of fractional bits */

/* ---------------------------------------------------------------------------
 * Conversion Macros — FP4_12
 *
 * INT_TO_FP4_12(x):  Convert an integer literal to FP4_12.
 *                    Example: INT_TO_FP4_12(3)  ->  3 * 4096 = 12288
 *
 * FP4_12_TO_INT(x):  Truncate an FP4_12 value back to an integer.
 *                    Equivalent to floor() for positive numbers.
 *
 * FP4_12_ROUND(x):   Round an FP4_12 to the nearest integer (as FP4_12).
 *                    Adds half before truncation — classic rounding trick.
 *
 * FP4_12_FLOOR(x):   Mask off fractional bits. Same as truncation for pos.
 *
 * FP4_12_FRAC_PART(x): Isolate the fractional component only.
 * --------------------------------------------------------------------------- */

#define INT_TO_FP4_12(x)       ((FP4_12)((x) << FP4_12_FRAC))
#define FP4_12_TO_INT(x)       ((int32_t)((x) >> FP4_12_FRAC))
#define FP4_12_ROUND(x)        (((x) + FP4_12_HALF) >> FP4_12_FRAC)
#define FP4_12_FLOOR(x)        ((x) & ~(FP4_12_ONE - 1))
#define FP4_12_FRAC_PART(x)    ((x) & (FP4_12_ONE - 1))

/* ---------------------------------------------------------------------------
 * Conversion Macros — FP12_4
 * --------------------------------------------------------------------------- */

#define INT_TO_FP12_4(x)       ((FP12_4)((x) << FP12_4_FRAC))
#define FP12_4_TO_INT(x)       ((int32_t)((x) >> FP12_4_FRAC))
#define FP12_4_ROUND(x)        (((x) + FP12_4_HALF) >> FP12_4_FRAC)
#define FP12_4_FLOOR(x)        ((x) & ~(FP12_4_ONE - 1))
#define FP12_4_FRAC_PART(x)    ((x) & (FP12_4_ONE - 1))

/* ---------------------------------------------------------------------------
 * Arithmetic Macros — FP4_12
 *
 * Addition and subtraction of same-format values is just integer add/sub —
 * the scale factors cancel out naturally. No macro needed for those.
 *
 * FP4_12_MUL(a, b):
 *   Multiply two FP4_12 values. The intermediate is int64_t to absorb the
 *   doubled bit width before shifting back by 12. Without int64_t, values
 *   near ±4.0 would overflow a 32-bit intermediate.
 *
 * FP4_12_DIV(a, b):
 *   Divide two FP4_12 values. Pre-shift 'a' left by 12 before dividing so
 *   the quotient lands at the correct scale. Raises by int64_t for safety.
 *   WARNING: Division is expensive on R3000A (no DIV hardware in the sense
 *   that it stalls the pipeline). Use sparingly — prefer multiply by reciprocal
 *   where the divisor is known at compile time.
 * --------------------------------------------------------------------------- */

#define FP4_12_MUL(a, b)   ((FP4_12)(((int64_t)(a) * (int64_t)(b)) >> FP4_12_FRAC))
#define FP4_12_DIV(a, b)   ((FP4_12)(((int64_t)(a) << FP4_12_FRAC) / (int64_t)(b)))

/* ---------------------------------------------------------------------------
 * Arithmetic Macros — FP12_4
 * --------------------------------------------------------------------------- */

#define FP12_4_MUL(a, b)   ((FP12_4)(((int64_t)(a) * (int64_t)(b)) >> FP12_4_FRAC))
#define FP12_4_DIV(a, b)   ((FP12_4)(((int64_t)(a) << FP12_4_FRAC) / (int64_t)(b)))

/* ---------------------------------------------------------------------------
 * Cross-Format Conversion
 *
 * Converting between formats means adjusting the scale factor.
 * FP4_12 has 12 fractional bits; FP12_4 has 4.
 * To go FP4_12 -> FP12_4: divide by (4096/16) = shift right by 8.
 * To go FP12_4 -> FP4_12: multiply by 256 = shift left by 8.
 *
 * Be careful of overflow when promoting FP12_4 to FP4_12 — the world
 * coordinate range (up to ±2048) times 256 would overflow int32_t.
 * Use int64_t intermediate or constrain input range accordingly.
 * --------------------------------------------------------------------------- */

/* FP4_12 to FP12_4: reduce precision (right shift 8 bits) */
#define FP4_12_TO_FP12_4(x)   ((FP12_4)((x) >> (FP4_12_FRAC - FP12_4_FRAC)))

/* FP12_4 to FP4_12: increase precision (left shift 8 bits).
 * Safe only for values in the FP4_12 integer range (-8 to 7). */
#define FP12_4_TO_FP4_12(x)   ((FP4_12)((x) << (FP4_12_FRAC - FP12_4_FRAC)))

/* ---------------------------------------------------------------------------
 * Absolute Value
 * Branchless absolute value using arithmetic right-shift sign extension.
 * On R3000A, SRA (arithmetic right shift) propagates the sign bit — this
 * compiles without a branch instruction.
 * --------------------------------------------------------------------------- */

#define FP_ABS(x)   (((x) ^ ((x) >> 31)) - ((x) >> 31))

/* ---------------------------------------------------------------------------
 * Clamping
 * --------------------------------------------------------------------------- */

#define FP_MIN(a, b)    ((a) < (b) ? (a) : (b))
#define FP_MAX(a, b)    ((a) > (b) ? (a) : (b))
#define FP_CLAMP(x, lo, hi)  FP_MAX((lo), FP_MIN((hi), (x)))

#endif /* PSX_FIXED_H */
