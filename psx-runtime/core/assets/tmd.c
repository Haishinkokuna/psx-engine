/*******************************************************************************
 * FILE:         tmd.c
 * MODULE:       Core/Assets
 * DESCRIPTION:  TMD parser and minimal renderer (POLY_F3 only).
 *******************************************************************************/

#include "tmd.h"
#include "../renderer/prim.h"
#include "../gte/gte_ops.h"
#include "../gte/gte_project.h"

/* ---------------------------------------------------------------------------
 * TMD_Load
 * --------------------------------------------------------------------------- */

uint32_t TMD_Load(void* file_data)
{
    if (!file_data) return 0;
    
    TMD_Header* header = (TMD_Header*)file_data;
    if (header->id != 0x00000041) {
        return 0; /* Invalid magic */
    }
    
    /* The Object Table immediately follows the header */
    uint32_t* obj_table = (uint32_t*)((uint8_t*)file_data + sizeof(TMD_Header));
    
    /* Convert offsets into absolute pointers */
    uint32_t i;
    for (i = 0; i < header->num_objs; i++) {
        /* In TMD files, the object table contains physical byte offsets
         * relative to the start of the file OR relative to the object table. 
         * Depending on the flag, we convert it to an absolute memory pointer.
         * For simplicity in this engine, we assume the TMD was compiled
         * such that offsets are relative to the top of the object table. */
         
        TMD_Object* obj = (TMD_Object*)((uint8_t*)obj_table + obj_table[i]);
        
        /* Convert the pointers inside the object. Note: These offsets
         * are relative to the top of the object table as well. */
        obj->vert_top = (uint32_t)(uintptr_t)((uint8_t*)obj_table + obj->vert_top);
        obj->norm_top = (uint32_t)(uintptr_t)((uint8_t*)obj_table + obj->norm_top);
        obj->prim_top = (uint32_t)(uintptr_t)((uint8_t*)obj_table + obj->prim_top);
    }
    
    return header->num_objs;
}

/* ---------------------------------------------------------------------------
 * TMD_GetObject
 * --------------------------------------------------------------------------- */

TMD_Object* TMD_GetObject(void* file_data, uint32_t obj_index)
{
    if (!file_data) return NULL;
    
    TMD_Header* header = (TMD_Header*)file_data;
    if (obj_index >= header->num_objs) return NULL;
    
    uint32_t* obj_table = (uint32_t*)((uint8_t*)file_data + sizeof(TMD_Header));
    
    return (TMD_Object*)((uint8_t*)obj_table + obj_table[obj_index]);
}

/* ---------------------------------------------------------------------------
 * TMD_Draw
 * --------------------------------------------------------------------------- */

void TMD_Draw(const TMD_Object* obj, const Transform* t, DisplayBuffer* buf)
{
    if (!obj || !t || !buf) return;
    
    /* Set up GTE Rotation and Translation matrices for this object */
    Mat3 rot;
    Transform_ComputeMatrix(t, &rot);
    
    GTE_SetRotMatrix(&rot);
    GTE_SetTransVector(&t->position);
    
    /* Get data arrays */
    SVECTOR* verts = (SVECTOR*)(uintptr_t)obj->vert_top;
    uint8_t* prims = (uint8_t*)(uintptr_t)obj->prim_top;
    
    uint32_t i;
    for (i = 0; i < obj->num_prims; i++) {
        TMD_Prim* prim_hdr = (TMD_Prim*)prims;
        
        /* Check if it's a POLY_F3 (Flat Triangle) */
        /* Mode 0x20 represents POLY_F3 in standard TMD spec. */
        if (prim_hdr->mode == 0x20) {
            TMD_P_F3* pf3 = (TMD_P_F3*)prims;
            
            /* Load vertices into GTE */
            GTE_SetVertex0(&verts[pf3->v0]);
            GTE_SetVertex1(&verts[pf3->v1]);
            GTE_SetVertex2(&verts[pf3->v2]);
            
            /* Transform and project */
            if (GTE_TransformTriangle()) {
                /* Create GPU packet */
                POLY_F3* poly = Prim_AllocPolyF3(buf);
                if (!poly) break; /* Out of packet memory */
                
                /* Get screen coords from GTE */
                poly->x0 = GTE_ReadSXY0_X(); poly->y0 = GTE_ReadSXY0_Y();
                poly->x1 = GTE_ReadSXY1_X(); poly->y1 = GTE_ReadSXY1_Y();
                poly->x2 = GTE_ReadSXY2_X(); poly->y2 = GTE_ReadSXY2_Y();
                
                /* Set color */
                Prim_SetColor0(poly, pf3->r0, pf3->g0, pf3->b0);
                
                /* Add to OT */
                OT_Add(buf->ot, poly, GTE_ReadOTZ());
            }
        }
        
        /* Advance to next primitive.
         * olen is the length of the primitive definition in 32-bit words,
         * excluding the header. But typically olen includes the header in
         * some TMD variations. Standard is (ilen + olen) words. 
         * By Sony specs: total length in bytes = (prim_hdr->olen + 1) * 4. */
        prims += (prim_hdr->olen + 1) * 4;
    }
}
