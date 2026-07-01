/*******************************************************************************
 * FILE:         mat3.h
 * MODULE:       Core/Math
 * DESCRIPTION:  3x3 rotation/scale matrix type for use with the PSX GTE.
 *
 *               The GTE (Geometry Transformation Engine) performs its core
 *               transform in hardware via a 3x3 rotation matrix (the "RT"
 *               matrix in GTE terminology). The GTE matrix format uses int16_t
 *               components in FP4_12 format (scale = 4096).
 *
 *               WHY int16_t FOR MATRIX COMPONENTS?
 *                 The GTE register file stores matrix coefficients as signed
 *                 16-bit integers (RTXX, RTXY, etc.). Storing our Mat3 as
 *                 int16_t[9] means we can load it directly into the GTE via
 *                 the CTC2 / MTV0 instructions without reformatting.
 *
 *               MEMORY LAYOUT (column-major to match GTE convention):
 *                 m[0]  m[3]  m[6]      r0c0  r0c1  r0c2
 *                 m[1]  m[4]  m[7]   =  r1c0  r1c1  r1c2
 *                 m[2]  m[5]  m[8]      r2c0  r2c1  r2c2
 *
 *               FP FORMAT: int16_t in FP4_12.
 *                 1.0 is stored as 4096 (FP4_12_ONE).
 *                 An identity matrix has m[0]=m[4]=m[8]=4096, rest=0.
 *
 * DEPENDENCIES: fixed.h, trig.h (for rotation constructors)
 *******************************************************************************/

#ifndef PSX_MAT3_H
#define PSX_MAT3_H

#include <stdint.h>
#include "fixed.h"
#include "trig.h"
#include "vec3.h"

/* ---------------------------------------------------------------------------
 * Mat3 — 3x3 matrix with int16_t components in FP4_12 format.
 * Stored as a flat array of 9 elements, column-major.
 * Total size: 9 * 2 = 18 bytes.
 * --------------------------------------------------------------------------- */

typedef struct {
    int16_t m[9]; /* Column-major: m[col*3 + row] */
} Mat3;

/* Accessor macros for clarity. (r=row, c=col), both 0-indexed. */
#define MAT3_AT(mat, r, c)   ((mat).m[(c) * 3 + (r)])

/* ---------------------------------------------------------------------------
 * Mat3_Identity — Return the identity matrix.
 * Diagonal elements = FP4_12_ONE (4096 = 1.0), off-diagonal = 0.
 * --------------------------------------------------------------------------- */

static inline Mat3 Mat3_Identity(void)
{
    Mat3 mat;
    int i;

    /* Zero all 9 elements first. */
    for (i = 0; i < 9; i++) {
        mat.m[i] = 0;
    }

    /* Set diagonal to 1.0 in FP4_12. */
    MAT3_AT(mat, 0, 0) = (int16_t)FP4_12_ONE;
    MAT3_AT(mat, 1, 1) = (int16_t)FP4_12_ONE;
    MAT3_AT(mat, 2, 2) = (int16_t)FP4_12_ONE;

    return mat;
}

/* ---------------------------------------------------------------------------
 * Mat3_Mul — Multiply two Mat3 matrices: result = a * b.
 *
 * Each result element is a dot product of a row from 'a' with a column of 'b'.
 * Components are int16_t in FP4_12, so each multiply produces a 32-bit
 * intermediate. We sum three such products and shift right by FP4_12_FRAC
 * (12) to bring the result back to FP4_12 scale.
 *
 * int32_t intermediates are used because 3 * (int16_t_max)^2 = 3 * 4096^2
 * = 50,331,648 which fits comfortably in int32_t. No int64_t needed here.
 * --------------------------------------------------------------------------- */

static inline Mat3 Mat3_Mul(Mat3 a, Mat3 b)
{
    Mat3 result;
    int r, c;

    for (c = 0; c < 3; c++) {
        for (r = 0; r < 3; r++) {
            /* Dot product of row r from 'a' with column c from 'b'.
             * Each term: a[r][k] * b[k][c], then sum and shift-back. */
            int32_t sum = 0;
            int k;
            for (k = 0; k < 3; k++) {
                sum += (int32_t)MAT3_AT(a, r, k) * (int32_t)MAT3_AT(b, k, c);
            }
            /* Shift right by 12 to restore FP4_12 scale after the multiply. */
            MAT3_AT(result, r, c) = (int16_t)(sum >> FP4_12_FRAC);
        }
    }

    return result;
}

/* ---------------------------------------------------------------------------
 * Mat3_MulVec3 — Transform a Vec3 by this matrix: result = mat * v.
 *
 * Input vector must be in FP4_12 for the units to cancel correctly when
 * multiplied against FP4_12 matrix components.
 * Result is also FP4_12.
 *
 * This is the software fallback path. In performance-critical code, use
 * the GTE intrinsics (MVMVA instruction) which performs this operation in
 * hardware in a handful of cycles.
 * --------------------------------------------------------------------------- */

static inline Vec3 Mat3_MulVec3(Mat3 mat, Vec3 v)
{
    Vec3 result;

    /* Each output component is a dot product of a row of the matrix
     * with the input vector. Since both are FP4_12, each multiply produces
     * double the fractional bits, requiring a shift-right by 12. */

    result.x = (FP12_4)(
        ((int32_t)MAT3_AT(mat, 0, 0) * (int32_t)FP4_12_TO_FP12_4(v.x)
       + (int32_t)MAT3_AT(mat, 0, 1) * (int32_t)FP4_12_TO_FP12_4(v.y)
       + (int32_t)MAT3_AT(mat, 0, 2) * (int32_t)FP4_12_TO_FP12_4(v.z))
        >> FP4_12_FRAC);

    result.y = (FP12_4)(
        ((int32_t)MAT3_AT(mat, 1, 0) * (int32_t)FP4_12_TO_FP12_4(v.x)
       + (int32_t)MAT3_AT(mat, 1, 1) * (int32_t)FP4_12_TO_FP12_4(v.y)
       + (int32_t)MAT3_AT(mat, 1, 2) * (int32_t)FP4_12_TO_FP12_4(v.z))
        >> FP4_12_FRAC);

    result.z = (FP12_4)(
        ((int32_t)MAT3_AT(mat, 2, 0) * (int32_t)FP4_12_TO_FP12_4(v.x)
       + (int32_t)MAT3_AT(mat, 2, 1) * (int32_t)FP4_12_TO_FP12_4(v.y)
       + (int32_t)MAT3_AT(mat, 2, 2) * (int32_t)FP4_12_TO_FP12_4(v.z))
        >> FP4_12_FRAC);

    return result;
}

/* ---------------------------------------------------------------------------
 * Mat3_RotX — Build a rotation matrix around the X axis.
 *
 * Standard Rx(theta):
 *   [ 1,       0,        0    ]
 *   [ 0,  cos(t),  -sin(t)   ]
 *   [ 0,  sin(t),   cos(t)   ]
 *
 * @param angle  BAM8 rotation angle
 * --------------------------------------------------------------------------- */

static inline Mat3 Mat3_RotX(BAM8 angle)
{
    FP4_12 s, c;
    Mat3 mat = Mat3_Identity();

    FP_SinCos(angle, &s, &c);

    /* Row 1: [0, cos, -sin] */
    MAT3_AT(mat, 1, 1) = (int16_t)c;
    MAT3_AT(mat, 1, 2) = (int16_t)(-s);

    /* Row 2: [0, sin,  cos] */
    MAT3_AT(mat, 2, 1) = (int16_t)s;
    MAT3_AT(mat, 2, 2) = (int16_t)c;

    return mat;
}

/* ---------------------------------------------------------------------------
 * Mat3_RotY — Build a rotation matrix around the Y axis.
 *
 * Standard Ry(theta):
 *   [  cos(t),  0,  sin(t) ]
 *   [    0,     1,    0    ]
 *   [ -sin(t),  0,  cos(t) ]
 * --------------------------------------------------------------------------- */

static inline Mat3 Mat3_RotY(BAM8 angle)
{
    FP4_12 s, c;
    Mat3 mat = Mat3_Identity();

    FP_SinCos(angle, &s, &c);

    MAT3_AT(mat, 0, 0) = (int16_t)c;
    MAT3_AT(mat, 0, 2) = (int16_t)s;
    MAT3_AT(mat, 2, 0) = (int16_t)(-s);
    MAT3_AT(mat, 2, 2) = (int16_t)c;

    return mat;
}

/* ---------------------------------------------------------------------------
 * Mat3_RotZ — Build a rotation matrix around the Z axis.
 *
 * Standard Rz(theta):
 *   [ cos(t), -sin(t),  0 ]
 *   [ sin(t),  cos(t),  0 ]
 *   [   0,       0,     1 ]
 * --------------------------------------------------------------------------- */

static inline Mat3 Mat3_RotZ(BAM8 angle)
{
    FP4_12 s, c;
    Mat3 mat = Mat3_Identity();

    FP_SinCos(angle, &s, &c);

    MAT3_AT(mat, 0, 0) = (int16_t)c;
    MAT3_AT(mat, 0, 1) = (int16_t)(-s);
    MAT3_AT(mat, 1, 0) = (int16_t)s;
    MAT3_AT(mat, 1, 1) = (int16_t)c;

    return mat;
}

/* ---------------------------------------------------------------------------
 * Mat3_RotXYZ — Compose Rx * Ry * Rz in one call.
 *
 * This applies rotation in Z-first, then Y, then X order (extrinsic).
 * This matches the convention used in most PSX games for euler angles.
 * The two Mat3_Mul calls expand to 9-element dot products each — if this
 * is in a hot path, consider caching the result rather than recomputing
 * every frame for static objects.
 * --------------------------------------------------------------------------- */

static inline Mat3 Mat3_RotXYZ(BAM8 rx, BAM8 ry, BAM8 rz)
{
    Mat3 mx = Mat3_RotX(rx);
    Mat3 my = Mat3_RotY(ry);
    Mat3 mz = Mat3_RotZ(rz);

    /* Compose: first rz, then ry, then rx */
    return Mat3_Mul(mx, Mat3_Mul(my, mz));
}

/* ---------------------------------------------------------------------------
 * Mat3_Transpose — Transpose the matrix (swap rows and columns).
 *
 * For pure rotation matrices, the transpose equals the inverse (R^T = R^-1).
 * This is much cheaper than a general matrix inverse. Use this to convert
 * a world-to-camera rotation into a camera-to-world rotation.
 * --------------------------------------------------------------------------- */

static inline Mat3 Mat3_Transpose(Mat3 mat)
{
    Mat3 result;
    int r, c;

    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) {
            MAT3_AT(result, r, c) = MAT3_AT(mat, c, r);
        }
    }

    return result;
}

#endif /* PSX_MAT3_H */
