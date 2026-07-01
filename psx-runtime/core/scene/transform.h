/*******************************************************************************
 * FILE:         transform.h
 * MODULE:       Core/Scene
 * DESCRIPTION:  3D spatial transform management.
 *
 *               A Transform defines the position, rotation, and scale of an
 *               object in 3D space. It bridges the gap between the game logic
 *               (which wants to set position as a Vec3 and rotation as Euler
 *               angles) and the GTE (which wants a 3x3 rotation matrix and a
 *               translation vector).
 *
 *               ROTATION CONVENTION:
 *               Rotations are stored as Euler angles (pitch, yaw, roll) in
 *               fixed-point format. We define rotation order as Y-X-Z:
 *                 1. Y (Yaw)   — turn left/right
 *                 2. X (Pitch) — look up/down
 *                 3. Z (Roll)  — tilt side-to-side
 *               This is standard for cameras and characters.
 *
 *               COORDINATE SYSTEM:
 *               The PSX GTE conventionally uses a left-handed coordinate system:
 *                 +X = Right
 *                 +Y = Down (like 2D screen coordinates)
 *                 +Z = Forward (into the screen)
 *
 * DEPENDENCIES: vec3.h, mat3.h, trig.h
 *******************************************************************************/

#ifndef PSX_TRANSFORM_H
#define PSX_TRANSFORM_H

#include <stdint.h>
#include "../math/vec3.h"
#include "../math/mat3.h"

/* ---------------------------------------------------------------------------
 * Transform
 *
 * Represents an object's state in 3D space.
 *   position: World space position in FP12_4.
 *   rotation: Euler angles (pitch, yaw, roll) in PSX angle units (0-4095).
 *             x = pitch, y = yaw, z = roll.
 *   scale:    Scaling factor in FP4_12 (4096 = 1.0).
 * --------------------------------------------------------------------------- */

typedef struct {
    Vec3 position;  /* FP12_4 world coordinates */
    Vec3 rotation;  /* 0-4095 angle units (X=pitch, Y=yaw, Z=roll) */
    Vec3 scale;     /* FP4_12 scale factors (4096 = 1.0) */
} Transform;

/* ---------------------------------------------------------------------------
 * Transform_Init — Initialize a transform to identity.
 *
 * Sets position and rotation to zero, and scale to 1.0 (4096).
 * --------------------------------------------------------------------------- */

void Transform_Init(Transform* t);

/* ---------------------------------------------------------------------------
 * Transform_ComputeMatrix — Generate a GTE rotation matrix.
 *
 * Converts the transform's Euler angles into a 3x3 rotation matrix.
 * Applies scale if the scale vector is not (1.0, 1.0, 1.0).
 *
 * NOTE: The resulting matrix is the local-to-world rotation matrix.
 * For drawing an object, you typically multiply this by the camera's
 * view matrix to get the model-view matrix that the GTE requires.
 *
 * @param t    The transform to evaluate.
 * @param out  The output Mat3 (FP4_12).
 * --------------------------------------------------------------------------- */

void Transform_ComputeMatrix(const Transform* t, Mat3* out);

#endif /* PSX_TRANSFORM_H */
