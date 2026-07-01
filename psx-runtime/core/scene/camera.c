/*******************************************************************************
 * FILE:         camera.c
 * MODULE:       Core/Scene
 * DESCRIPTION:  Camera view matrix implementation.
 *
 * DEPENDENCIES: camera.h
 *******************************************************************************/

#include "camera.h"

/* ---------------------------------------------------------------------------
 * Camera_Init
 * --------------------------------------------------------------------------- */

void Camera_Init(Camera* cam)
{
    Transform_Init(&cam->transform);
    cam->fov_h = 256; /* 90-degree horizontal FOV */
}

/* ---------------------------------------------------------------------------
 * Camera_ComputeView
 *
 * To convert world coordinates to camera coordinates, we need the inverse
 * of the camera's world matrix.
 *
 * For an orthogonal rotation matrix R, its inverse is its transpose:
 *   R_inv = R^T
 *
 * So we compute the forward rotation matrix from the camera's Euler angles,
 * and then transpose it.
 * --------------------------------------------------------------------------- */

void Camera_ComputeView(const Camera* cam, Mat3* out)
{
    Mat3 forward;

    /* Compute the forward matrix (where the camera is looking in world space) */
    Transform_ComputeMatrix(&cam->transform, &forward);

    /* Transpose the matrix to get the inverse (view matrix).
     *
     *   [ m0 m1 m2 ]^T   [ m0 m3 m6 ]
     *   [ m3 m4 m5 ]   = [ m1 m4 m7 ]
     *   [ m6 m7 m8 ]     [ m2 m5 m8 ]
     */
    out->m[0] = forward.m[0];
    out->m[1] = forward.m[3];
    out->m[2] = forward.m[6];

    out->m[3] = forward.m[1];
    out->m[4] = forward.m[4];
    out->m[5] = forward.m[7];

    out->m[6] = forward.m[2];
    out->m[7] = forward.m[5];
    out->m[8] = forward.m[8];
}
