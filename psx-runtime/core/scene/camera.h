/*******************************************************************************
 * FILE:         camera.h
 * MODULE:       Core/Scene
 * DESCRIPTION:  Camera and view matrix management.
 *
 *               The Camera acts as the viewpoint for the scene. The GTE
 *               requires a "model-view" matrix for rendering — this is
 *               constructed by taking the object's transform and multiplying
 *               it by the inverse of the camera's transform.
 *
 *               For translations, we simply subtract the camera position from
 *               the object position.
 *               For rotations, we transpose the camera's rotation matrix
 *               (which is identical to the inverse for orthogonal rotation
 *               matrices) and multiply.
 *
 * DEPENDENCIES: transform.h, mat3.h
 *******************************************************************************/

#ifndef PSX_CAMERA_H
#define PSX_CAMERA_H

#include <stdint.h>
#include "transform.h"
#include "../math/mat3.h"

/* ---------------------------------------------------------------------------
 * Camera
 *
 * Represents the player's or viewer's viewpoint.
 *   transform: The position and rotation of the camera in the world.
 *              (Scale is ignored).
 *   fov_h:     The projection plane distance (H in the GTE).
 *              256 = standard 90-degree horizontal FOV.
 * --------------------------------------------------------------------------- */

typedef struct {
    Transform transform;
    int32_t   fov_h;
} Camera;

/* ---------------------------------------------------------------------------
 * Camera_Init
 *
 * Initializes the camera to origin (0,0,0), looking forward (+Z), with
 * a standard 256 projection distance.
 * --------------------------------------------------------------------------- */

void Camera_Init(Camera* cam);

/* ---------------------------------------------------------------------------
 * Camera_ComputeView
 *
 * Computes the view matrix (the inverse of the camera's rotation matrix).
 * This matrix is used to transform world-space objects into camera-space.
 *
 * @param cam   The camera.
 * @param out   The output view matrix (FP4_12).
 * --------------------------------------------------------------------------- */

void Camera_ComputeView(const Camera* cam, Mat3* out);

#endif /* PSX_CAMERA_H */
