/*******************************************************************************
 * FILE:         transform.c
 * MODULE:       Core/Scene
 * DESCRIPTION:  Transform matrix computation implementation.
 *
 * DEPENDENCIES: transform.h, mat3.h, trig.h
 *******************************************************************************/

#include "transform.h"
#include "../math/trig.h"

/* ---------------------------------------------------------------------------
 * Transform_Init
 * --------------------------------------------------------------------------- */

void Transform_Init(Transform* t)
{
    t->position.x = 0;
    t->position.y = 0;
    t->position.z = 0;

    t->rotation.x = 0;
    t->rotation.y = 0;
    t->rotation.z = 0;

    t->scale.x = 4096; /* 1.0 in FP4_12 */
    t->scale.y = 4096;
    t->scale.z = 4096;
}

/* ---------------------------------------------------------------------------
 * Transform_ComputeMatrix
 *
 * Generates a rotation matrix from Euler angles using Y-X-Z order.
 *
 *   Rx = [ 1   0    0 ]   Ry = [ cy  0   sy ]   Rz = [ cz -sz  0 ]
 *        [ 0  cx  -sx ]        [  0  1    0 ]        [ sz  cz  0 ]
 *        [ 0  sx   cx ]        [-sy  0   cy ]        [  0   0  1 ]
 *
 * Combined R = Ry * Rx * Rz (applies Z roll, then X pitch, then Y yaw).
 * --------------------------------------------------------------------------- */

void Transform_ComputeMatrix(const Transform* t, Mat3* out)
{
    /* Get sine/cosine for each axis (angles wrap automatically in Trig_Sin/Cos) */
    int32_t sx = Trig_Sin(t->rotation.x);
    int32_t cx = Trig_Cos(t->rotation.x);
    int32_t sy = Trig_Sin(t->rotation.y);
    int32_t cy = Trig_Cos(t->rotation.y);
    int32_t sz = Trig_Sin(t->rotation.z);
    int32_t cz = Trig_Cos(t->rotation.z);

    /* Compute combined rotation matrix (Ry * Rx * Rz).
     * All sine/cosine values are FP4_12.
     * When multiplying two FP4_12 numbers, we shift right by 12 to maintain scale. */

    int32_t sx_sy = (sx * sy) >> 12;
    int32_t sx_cy = (sx * cy) >> 12;

    out->m[0] = (int16_t)(((cy * cz) >> 12) - ((sx_sy * sz) >> 12));
    out->m[1] = (int16_t)(-((cy * sz) >> 12) - ((sx_sy * cz) >> 12));
    out->m[2] = (int16_t)((cx * sy) >> 12);

    out->m[3] = (int16_t)((cx * sz) >> 12);
    out->m[4] = (int16_t)((cx * cz) >> 12);
    out->m[5] = (int16_t)(-sx);

    out->m[6] = (int16_t)(((sy * cz) >> 12) + ((sx_cy * sz) >> 12));
    out->m[7] = (int16_t)(-((sy * sz) >> 12) + ((sx_cy * cz) >> 12));
    out->m[8] = (int16_t)((cx * cy) >> 12);

    /* Apply scale if not identity */
    if (t->scale.x != 4096 || t->scale.y != 4096 || t->scale.z != 4096) {
        out->m[0] = (int16_t)(((int32_t)out->m[0] * t->scale.x) >> 12);
        out->m[3] = (int16_t)(((int32_t)out->m[3] * t->scale.x) >> 12);
        out->m[6] = (int16_t)(((int32_t)out->m[6] * t->scale.x) >> 12);

        out->m[1] = (int16_t)(((int32_t)out->m[1] * t->scale.y) >> 12);
        out->m[4] = (int16_t)(((int32_t)out->m[4] * t->scale.y) >> 12);
        out->m[7] = (int16_t)(((int32_t)out->m[7] * t->scale.y) >> 12);

        out->m[2] = (int16_t)(((int32_t)out->m[2] * t->scale.z) >> 12);
        out->m[5] = (int16_t)(((int32_t)out->m[5] * t->scale.z) >> 12);
        out->m[8] = (int16_t)(((int32_t)out->m[8] * t->scale.z) >> 12);
    }
}
