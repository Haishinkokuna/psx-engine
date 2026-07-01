/*******************************************************************************
 * FILE:         test_scene.cpp
 * MODULE:       Tests
 * DESCRIPTION:  Host-compiled unit tests for the Scene Management module.
 *******************************************************************************/

#include <iostream>
#include <cassert>
#include <cmath>

/* Include the C headers from the runtime */
extern "C" {
    #include "scene.h"
}

/* ---------------------------------------------------------------------------
 * Test Functions
 * --------------------------------------------------------------------------- */

void test_transform_matrix()
{
    std::cout << "  [TEST] Transform Matrix Computation...\n";

    Transform t;
    Transform_Init(&t);
    
    /* 90 degrees Yaw (Y-axis).
     * 4096 = 360 deg -> 1024 = 90 deg */
    t.rotation.y = 1024;
    
    Mat3 m;
    Transform_ComputeMatrix(&t, &m);
    
    /* Ry(90) = [ 0  0  1 ]
     *          [ 0  1  0 ]
     *          [-1  0  0 ] 
     * In FP4_12, 1 = 4096, -1 = -4096. 
     * There may be slight precision loss due to Trig_Sin/Cos. */
    
    /* Allow small tolerance */
    assert(m.m[0] >= -5 && m.m[0] <= 5);
    assert(m.m[2] >= 4090);
    assert(m.m[4] >= 4090);
    assert(m.m[6] <= -4090);
    assert(m.m[8] >= -5 && m.m[8] <= 5);

    std::cout << "    Passed.\n";
}

void test_camera_view()
{
    std::cout << "  [TEST] Camera View Matrix...\n";

    Camera cam;
    Camera_Init(&cam);
    
    /* 90 degrees Yaw */
    cam.transform.rotation.y = 1024;
    
    Mat3 view;
    Camera_ComputeView(&cam, &view);
    
    /* View matrix should be transpose of the transform matrix.
     * Ry(90)^T = [ 0  0 -1 ]
     *            [ 0  1  0 ]
     *            [ 1  0  0 ] */
     
    assert(view.m[0] >= -5 && view.m[0] <= 5);
    assert(view.m[2] <= -4090);
    assert(view.m[4] >= 4090);
    assert(view.m[6] >= 4090);
    assert(view.m[8] >= -5 && view.m[8] <= 5);

    std::cout << "    Passed.\n";
}

void test_scene_entities()
{
    std::cout << "  [TEST] Scene Entity Spawning and Lookup...\n";

    Scene_Init();
    
    assert(g_scene.active_count == 0);
    
    Entity* e1 = Scene_SpawnEntity();
    assert(e1 != NULL);
    assert(e1->id == 1);
    assert(g_scene.active_count == 1);
    
    Entity* e2 = Scene_SpawnEntity();
    assert(e2 != NULL);
    assert(e2->id == 2);
    assert(g_scene.active_count == 2);
    
    /* Lookup */
    Entity* lookup = Scene_GetEntity(1);
    assert(lookup == e1);
    
    /* Destroy */
    Scene_DestroyEntity(1);
    assert(g_scene.active_count == 1);
    
    lookup = Scene_GetEntity(1);
    assert(lookup == NULL); /* Should not be found */
    
    /* Max entities */
    Scene_Init();
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity* e = Scene_SpawnEntity();
        assert(e != NULL);
    }
    assert(g_scene.active_count == MAX_ENTITIES);
    
    Entity* over = Scene_SpawnEntity();
    assert(over == NULL); /* Should fail */

    std::cout << "    Passed.\n";
}

/* ---------------------------------------------------------------------------
 * Main
 * --------------------------------------------------------------------------- */

int main()
{
    std::cout << "=== Running Scene Unit Tests ===\n";

    test_transform_matrix();
    test_camera_view();
    test_scene_entities();

    std::cout << "=== All Scene Tests Passed ===\n";
    return 0;
}
