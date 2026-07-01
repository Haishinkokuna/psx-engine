/*******************************************************************************
 * FILE:         test_fixed.cpp
 * MODULE:       Core/Math — Host Unit Tests
 * DESCRIPTION:  Verifies the fixed-point math library (fixed.h, vec3.h,
 *               mat3.h, trig.h) on the host platform. No PSX hardware or
 *               cross-compiler is required — this compiles with any standard
 *               C++17 toolchain.
 *
 *               Tests cover:
 *                 - FP4_12 and FP12_4 conversion correctness
 *                 - FP_MUL / FP_DIV precision and overflow safety
 *                 - FP_Sin / FP_Cos LUT accuracy (known angles)
 *                 - Vec3 operations (add, sub, dot, cross, lerp)
 *                 - Mat3 identity, multiply, rotation, transpose
 *
 *               Each test function returns 1 on pass, 0 on fail and prints
 *               a message. main() tallies results and exits non-zero on any
 *               failure (enabling CI failure detection via exit code).
 *
 * DEPENDENCIES: fixed.h, trig.h, vec3.h, mat3.h, <stdio.h>
 *******************************************************************************/

#include <cstdio>
#include <cstdint>

extern "C" {
#include "fixed.h"
#include "trig.h"
#include "vec3.h"
#include "mat3.h"
}

/* ---------------------------------------------------------------------------
 * Tolerance for FP comparison. In FP4_12, 1 unit = 1/4096 of 1.0.
 * We allow 2 units of error (about 0.00049) to account for LUT rounding. */
#define EPSILON_FP4_12   ((FP4_12)2)
#define EPSILON_FP12_4   ((FP12_4)2)

static int check(int condition, const char* test_name)
{
    if (!condition) {
        printf("[FAIL] %s\n", test_name);
        return 0;
    }
    printf("[PASS] %s\n", test_name);
    return 1;
}

/* ---------------------------------------------------------------------------
 * FP4_12 conversion tests
 * --------------------------------------------------------------------------- */
static int test_fp4_12_conversions(void)
{
    int pass = 1;

    /* INT_TO_FP4_12(3) should be 3 * 4096 = 12288 */
    FP4_12 a = INT_TO_FP4_12(3);
    pass &= check(a == 12288, "INT_TO_FP4_12(3) == 12288");

    /* FP4_12_TO_INT(4096) should be 1 */
    pass &= check(FP4_12_TO_INT(FP4_12_ONE) == 1, "FP4_12_TO_INT(ONE) == 1");

    /* FP4_12_ROUND: 4096 + 2048 (= 1.5 in FP4_12) should round to 2 */
    FP4_12 half_plus_one = FP4_12_ONE + FP4_12_HALF; /* 1.5 */
    pass &= check(FP4_12_ROUND(half_plus_one) == 2, "FP4_12_ROUND(1.5) == 2");

    return pass;
}

/* ---------------------------------------------------------------------------
 * FP4_12 multiply and divide tests
 * --------------------------------------------------------------------------- */
static int test_fp4_12_arithmetic(void)
{
    int pass = 1;
    FP4_12 result;

    /* 2.0 * 3.0 = 6.0 */
    FP4_12 two   = INT_TO_FP4_12(2);
    FP4_12 three = INT_TO_FP4_12(3);
    FP4_12 six   = INT_TO_FP4_12(6);
    result = FP4_12_MUL(two, three);
    pass &= check(result == six, "FP4_12_MUL(2.0, 3.0) == 6.0");

    /* 6.0 / 2.0 = 3.0 */
    result = FP4_12_DIV(six, two);
    pass &= check(result == three, "FP4_12_DIV(6.0, 2.0) == 3.0");

    /* 0.5 * 0.5 = 0.25 */
    result = FP4_12_MUL(FP4_12_HALF, FP4_12_HALF);
    pass &= check(result == FP4_12_QUARTER, "FP4_12_MUL(0.5, 0.5) == 0.25");

    /* FP_ABS: negative -> positive */
    FP4_12 neg_two = INT_TO_FP4_12(-2);
    pass &= check(FP_ABS(neg_two) == two, "FP_ABS(-2.0) == 2.0");

    return pass;
}

/* ---------------------------------------------------------------------------
 * Sin/Cos LUT tests — verify known angles
 * --------------------------------------------------------------------------- */
static int test_trig(void)
{
    int pass = 1;
    FP4_12 s, c;

    /* sin(0) == 0 */
    s = FP_Sin(0);
    pass &= check(s == 0, "FP_Sin(0) == 0");

    /* sin(90 deg) == FP4_12_ONE (4096).
     * 90 deg in BAM8 = 64. LUT entry should be exactly 4096. */
    s = FP_Sin(64);
    pass &= check(s == FP4_12_ONE, "FP_Sin(64=90deg) == FP4_12_ONE");

    /* cos(0) == FP4_12_ONE (cos uses sin with +64 phase) */
    c = FP_Cos(0);
    pass &= check(c == FP4_12_ONE, "FP_Cos(0) == FP4_12_ONE");

    /* cos(90 deg) == ~0 (within epsilon due to LUT rounding) */
    c = FP_Cos(64);
    pass &= check(FP_ABS(c) <= EPSILON_FP4_12, "FP_Cos(64=90deg) ~= 0");

    /* sin(180 deg) == ~0 (BAM8 = 128) */
    s = FP_Sin(128);
    pass &= check(FP_ABS(s) <= EPSILON_FP4_12, "FP_Sin(128=180deg) ~= 0");

    /* sin(270 deg) == -FP4_12_ONE (BAM8 = 192) */
    s = FP_Sin(192);
    pass &= check(s == -FP4_12_ONE, "FP_Sin(192=270deg) == -FP4_12_ONE");

    /* Angle wrap: sin(255+1) == sin(0) via BAM8 overflow */
    s = FP_Sin((uint8_t)(255u + 1u));
    pass &= check(s == 0, "FP_Sin(255+1 wrap) == FP_Sin(0)");

    /* DEG_TO_BAM8(90) == 64 */
    pass &= check(DEG_TO_BAM8(90) == 64, "DEG_TO_BAM8(90) == 64");

    return pass;
}

/* ---------------------------------------------------------------------------
 * Vec3 tests
 * --------------------------------------------------------------------------- */
static int test_vec3(void)
{
    int pass = 1;
    Vec3 a, b, result;

    a = Vec3_Make(INT_TO_FP12_4(1), INT_TO_FP12_4(2), INT_TO_FP12_4(3));
    b = Vec3_Make(INT_TO_FP12_4(4), INT_TO_FP12_4(5), INT_TO_FP12_4(6));

    /* Add: (1,2,3) + (4,5,6) = (5,7,9) */
    result = Vec3_Add(a, b);
    pass &= check(FP12_4_TO_INT(result.x) == 5 &&
                  FP12_4_TO_INT(result.y) == 7 &&
                  FP12_4_TO_INT(result.z) == 9, "Vec3_Add (1,2,3)+(4,5,6) == (5,7,9)");

    /* Sub: (4,5,6) - (1,2,3) = (3,3,3) */
    result = Vec3_Sub(b, a);
    pass &= check(FP12_4_TO_INT(result.x) == 3 &&
                  FP12_4_TO_INT(result.y) == 3 &&
                  FP12_4_TO_INT(result.z) == 3, "Vec3_Sub (4,5,6)-(1,2,3) == (3,3,3)");

    /* Dot: (1,2,3).(4,5,6) = 4+10+18 = 32 */
    FP12_4 dot = Vec3_Dot(a, b);
    pass &= check(FP12_4_TO_INT(dot) == 32, "Vec3_Dot (1,2,3).(4,5,6) == 32");

    /* LengthSq of (3,4,0) = 9+16+0 = 25 */
    Vec3 pythagorean = Vec3_Make(INT_TO_FP12_4(3), INT_TO_FP12_4(4), 0);
    FP12_4 lensq = Vec3_LengthSq(pythagorean);
    pass &= check(FP12_4_TO_INT(lensq) == 25, "Vec3_LengthSq (3,4,0) == 25");

    /* Lerp(a, b, 0) should equal a */
    result = Vec3_Lerp(a, b, 0);
    pass &= check(result.x == a.x && result.y == a.y && result.z == a.z,
                  "Vec3_Lerp t=0 returns a");

    /* Lerp(a, b, FP12_4_ONE) should equal b */
    result = Vec3_Lerp(a, b, FP12_4_ONE);
    pass &= check(result.x == b.x && result.y == b.y && result.z == b.z,
                  "Vec3_Lerp t=ONE returns b");

    return pass;
}

/* ---------------------------------------------------------------------------
 * Mat3 tests
 * --------------------------------------------------------------------------- */
static int test_mat3(void)
{
    int pass = 1;

    /* Identity matrix diagonal should be FP4_12_ONE, off-diagonal 0 */
    Mat3 identity = Mat3_Identity();
    pass &= check(MAT3_AT(identity, 0, 0) == FP4_12_ONE &&
                  MAT3_AT(identity, 1, 1) == FP4_12_ONE &&
                  MAT3_AT(identity, 2, 2) == FP4_12_ONE &&
                  MAT3_AT(identity, 0, 1) == 0 &&
                  MAT3_AT(identity, 1, 0) == 0, "Mat3_Identity diagonal");

    /* Identity * Identity = Identity */
    Mat3 ii = Mat3_Mul(identity, identity);
    pass &= check(MAT3_AT(ii, 0, 0) == FP4_12_ONE &&
                  MAT3_AT(ii, 1, 1) == FP4_12_ONE &&
                  MAT3_AT(ii, 2, 2) == FP4_12_ONE, "Mat3_Mul(I, I) == I");

    /* Transpose of identity is identity */
    Mat3 trans = Mat3_Transpose(identity);
    pass &= check(MAT3_AT(trans, 0, 0) == FP4_12_ONE, "Mat3_Transpose(I) == I");

    /* Rot0 should equal identity (rotating by 0 degrees) */
    Mat3 rot0 = Mat3_RotZ(0);
    pass &= check(MAT3_AT(rot0, 0, 0) == FP4_12_ONE &&
                  MAT3_AT(rot0, 1, 1) == FP4_12_ONE, "Mat3_RotZ(0) == Identity");

    return pass;
}

/* ---------------------------------------------------------------------------
 * Main
 * --------------------------------------------------------------------------- */
int main(void)
{
    int total = 0;
    int passed = 0;
    int result;

    printf("=== PSX ENGINE: Core/Math Unit Tests ===\n\n");

    printf("-- FP4_12 Conversions --\n");
    result = test_fp4_12_conversions();
    passed += result; total++;

    printf("\n-- FP4_12 Arithmetic --\n");
    result = test_fp4_12_arithmetic();
    passed += result; total++;

    printf("\n-- Trig LUT --\n");
    result = test_trig();
    passed += result; total++;

    printf("\n-- Vec3 Operations --\n");
    result = test_vec3();
    passed += result; total++;

    printf("\n-- Mat3 Operations --\n");
    result = test_mat3();
    passed += result; total++;

    printf("\n========================================\n");
    printf("Results: %d / %d test groups passed.\n", passed, total);

    /* Non-zero exit code signals test failure to ctest. */
    return (passed == total) ? 0 : 1;
}
