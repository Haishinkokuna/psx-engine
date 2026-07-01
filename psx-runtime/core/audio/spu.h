/*******************************************************************************
 * FILE:         spu.h
 * MODULE:       Core/Audio
 * DESCRIPTION:  SPU (Sound Processing Unit) hardware driver.
 *
 *               The PSX SPU provides 24 hardware voices for ADPCM playback.
 *               This driver manages hardware initialization, voice allocation,
 *               and sample playback.
 *******************************************************************************/

#ifndef PSX_SPU_H
#define PSX_SPU_H

#include <stdint.h>

#define SPU_NUM_VOICES 24

/* Standard ADPCM VAG Header Magic (0x56414770 = "VAGp") */
#define VAG_MAGIC 0x56414770

typedef struct {
    uint32_t magic;      /* "VAGp" */
    uint32_t version;
    uint32_t reserved;
    uint32_t data_size;  /* Size of the compressed ADPCM data */
    uint32_t sample_rate;
    uint32_t reserved2[3];
    char     name[16];
    /* ADPCM data follows */
} VAG_Header;

/* ---------------------------------------------------------------------------
 * SPU API
 * --------------------------------------------------------------------------- */

/* Resets the SPU hardware, clears sound RAM, and resets all 24 voices. */
void SPU_Init(void);

/* Uploads a VAG file (header + data) into SPU RAM.
 * Returns the SPU RAM start address (0x1000 - 0x7FFFF) on success, or 0 on error. */
uint32_t SPU_LoadVAG(void* vag_data);

/* Plays a sound at a specific SPU RAM address.
 * Automatically finds a free voice channel.
 * @param spu_addr  The memory address returned by SPU_LoadVAG.
 * @param pitch     Playback rate (0x1000 = original pitch/44.1kHz standard).
 * @param left_vol  Left channel volume (0x0000 - 0x3FFF).
 * @param right_vol Right channel volume (0x0000 - 0x3FFF).
 * @return The allocated voice index (0-23), or -1 if all voices are busy.
 */
int SPU_PlayVoice(uint32_t spu_addr, uint16_t pitch, uint16_t left_vol, uint16_t right_vol);

/* Stops a specific hardware voice. */
void SPU_StopVoice(int voice_index);

/* Stops all currently playing voices. */
void SPU_StopAll(void);

#endif /* PSX_SPU_H */
