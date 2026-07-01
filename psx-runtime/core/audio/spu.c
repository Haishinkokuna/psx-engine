/*******************************************************************************
 * FILE:         spu.c
 * MODULE:       Core/Audio
 * DESCRIPTION:  SPU driver implementation.
 *
 *               Includes a host-test path that mocks hardware registers
 *               so the editor can simulate voice allocation and memory
 *               management without a real SPU.
 *******************************************************************************/

#include "spu.h"
#include <stddef.h>

/* ---------------------------------------------------------------------------
 * Internal State
 * --------------------------------------------------------------------------- */

/* We track which voices are currently "playing".
 * On hardware, this is read from the SPU status registers (0x1F801D88).
 * For the host test, we simulate it via a bitmask. */
static uint32_t g_voice_status = 0;

/* Basic memory allocator for SPU RAM (512KB).
 * SPU RAM address starts at 0x1000 (lower memory is reserved by hardware). */
#define SPU_RAM_START 0x1000
#define SPU_RAM_END   0x80000 /* 512KB */

static uint32_t g_spu_alloc_ptr = SPU_RAM_START;

/* ---------------------------------------------------------------------------
 * SPU_Init
 * --------------------------------------------------------------------------- */

void SPU_Init(void)
{
    g_voice_status = 0;
    g_spu_alloc_ptr = SPU_RAM_START;

#ifdef PSX_HOST_TEST
    /* Host environment requires no hardware init */
#else
    /* Future hardware init:
     * - Write 0 to master volume to prevent popping
     * - Clear all 512KB of SPU RAM via DMA
     * - Setup reverb work area (if used)
     * - Unmute master volume
     */
#endif
}

/* ---------------------------------------------------------------------------
 * SPU_LoadVAG
 * --------------------------------------------------------------------------- */

uint32_t SPU_LoadVAG(void* vag_data)
{
    if (!vag_data) return 0;
    
    VAG_Header* header = (VAG_Header*)vag_data;
    
    /* Endian swap for "VAGp" magic might be needed depending on compiler,
     * but VAG files are big-endian in header. Standard PSX toolchains provide
     * byteswap macros. For host test, we just do a simplistic check. */
    if (header->magic != VAG_MAGIC && header->magic != 0x70474156) {
        return 0; /* Invalid VAG file */
    }
    
    /* VAG data size is big-endian, we need to swap it for MIPS (little-endian)
     * or Host. A naive swap for the test: */
    uint32_t size_be = header->data_size;
    uint32_t data_size = ((size_be >> 24) & 0xff) |
                         ((size_be << 8) & 0xff0000) |
                         ((size_be >> 8) & 0xff00) |
                         ((size_be << 24) & 0xff000000);

    /* Allocate space in SPU RAM. Ensure 8-byte alignment. */
    uint32_t addr = g_spu_alloc_ptr;
    g_spu_alloc_ptr += data_size;
    
    if (g_spu_alloc_ptr % 8 != 0) {
        g_spu_alloc_ptr += 8 - (g_spu_alloc_ptr % 8);
    }
    
    if (g_spu_alloc_ptr > SPU_RAM_END) {
        /* Out of SPU RAM */
        return 0;
    }

#ifdef PSX_HOST_TEST
    /* In the host editor, we simulate the upload success. */
#else
    /* Future hardware upload:
     * - Set SPU transfer mode and start address
     * - Trigger DMA block transfer
     * - Wait for DMA to complete
     */
#endif

    return addr;
}

/* ---------------------------------------------------------------------------
 * SPU_PlayVoice
 * --------------------------------------------------------------------------- */

int SPU_PlayVoice(uint32_t spu_addr, uint16_t pitch, uint16_t left_vol, uint16_t right_vol)
{
    if (spu_addr == 0) return -1;
    
    int voice = -1;
    int i;
    
#ifndef PSX_HOST_TEST
    /* Read real hardware voice status registers */
    /* g_voice_status = ReadSPUStatus(); */
#endif

    /* Find lowest free voice */
    for (i = 0; i < SPU_NUM_VOICES; i++) {
        if ((g_voice_status & (1 << i)) == 0) {
            voice = i;
            break;
        }
    }
    
    if (voice == -1) {
        return -1; /* All voices busy */
    }
    
    /* Mark voice as playing */
    g_voice_status |= (1 << voice);

#ifdef PSX_HOST_TEST
    /* Host simulation */
#else
    /* Hardware registers for Voice 'i':
     * SPU_VOICE_VOL_L(voice) = left_vol;
     * SPU_VOICE_VOL_R(voice) = right_vol;
     * SPU_VOICE_PITCH(voice) = pitch;
     * SPU_VOICE_ADDR(voice)  = spu_addr >> 3; // Hardware expects divided by 8
     * SPU_VOICE_ADSR(voice)  = 0x80FF;        // Standard ADSR
     * 
     * // Key On trigger
     * SPU_KEY_ON = (1 << voice);
     */
#endif

    return voice;
}

/* ---------------------------------------------------------------------------
 * SPU_StopVoice
 * --------------------------------------------------------------------------- */

void SPU_StopVoice(int voice_index)
{
    if (voice_index < 0 || voice_index >= SPU_NUM_VOICES) return;
    
    /* Clear playing bit */
    g_voice_status &= ~(1 << voice_index);

#ifdef PSX_HOST_TEST
    /* Host simulation */
#else
    /* SPU_KEY_OFF = (1 << voice_index); */
#endif
}

/* ---------------------------------------------------------------------------
 * SPU_StopAll
 * --------------------------------------------------------------------------- */

void SPU_StopAll(void)
{
    g_voice_status = 0;

#ifdef PSX_HOST_TEST
    /* Host simulation */
#else
    /* SPU_KEY_OFF = 0xFFFFFF; */
#endif
}
