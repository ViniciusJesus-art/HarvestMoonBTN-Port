#ifndef HMBTN_PSX_AUDIO_H
#define HMBTN_PSX_AUDIO_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { HMBTN_AUDIO_RATE = 44100, HMBTN_AUDIO_VOICES = 24 };
typedef struct HmbtnSound HmbtnSound;

/* Native VAB sampler/SEQ sequencer. Owns decoded data, no PS1 executable/SDK. */
HmbtnSound *hmbtn_sound_create(void);
void hmbtn_sound_destroy(HmbtnSound *sound);
bool hmbtn_sound_bank(HmbtnSound *sound, unsigned slot, const uint8_t *vh, size_t vh_size,
    const uint8_t *vb, size_t vb_size, char *error, size_t error_size);
bool hmbtn_sound_sequence(HmbtnSound *sound, const uint8_t *seq, size_t size,
    char *error, size_t error_size);
void hmbtn_sound_start(HmbtnSound *sound);
void hmbtn_sound_music_volume(HmbtnSound *sound, unsigned volume); /* 0..127 */
void hmbtn_sound_effect(HmbtnSound *sound, unsigned tone); /* bank 0, prog 0, note 60, vol 60 */
void hmbtn_sound_render(HmbtnSound *sound, int16_t *stereo, size_t frames);
uint32_t hmbtn_sound_loop_count(const HmbtnSound *sound);
#endif
