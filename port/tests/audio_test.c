#include "psx_audio.h"
#include "psx_data.h"
#include "resource_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "Audio failed at line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

/* Optional local probe uses imported data only. Not registered as a proprietary
 * fixture in CTest. WAV output is streamed in small chunks, not held in RAM. */
static int probe(const char *root, const char *wav_path)
{
    uint8_t *fx = malloc(24576), *music = malloc(106496), *seq = malloc(10240);
    char error[256] = {0};
    HmbtnSound *s = hmbtn_sound_create();
    CHECK(fx && music && seq && s);
    CHECK(hmbtn_resource_read(root, "A_FILE/09_sound.stm", 0, fx, 24576, error, sizeof(error)));
    CHECK(hmbtn_resource_read(root, "A_FILE/09_sound.stm", 4411392, music, 106496, error, sizeof(error)));
    CHECK(hmbtn_resource_read(root, "A_FILE/09_sound.stm", 4517888, seq, 10240, error, sizeof(error)));
    if (!hmbtn_sound_bank(s, 0, fx, 4096, fx+4096, 20480, error, sizeof(error))
            || !hmbtn_sound_bank(s, 1, music, 6144, music+6144, 100352, error, sizeof(error))
            || !hmbtn_sound_sequence(s, seq, 10240, error, sizeof(error))) {
        fprintf(stderr, "%s\n", error); return 1;
    }
    free(fx); free(music); free(seq);
    const unsigned frames = HMBTN_AUDIO_RATE * 55;
    FILE *wav = wav_path ? fopen(wav_path, "wb") : NULL;
    if (wav_path) CHECK(wav);
    if (wav) {
        uint8_t header[44] = {0};
        memcpy(header, "RIFF", 4); hmbtn_write_le32(header+4, 36+frames*4);
        memcpy(header+8, "WAVEfmt ", 8); hmbtn_write_le32(header+16, 16);
        hmbtn_write_le16(header+20, 1); hmbtn_write_le16(header+22, 2);
        hmbtn_write_le32(header+24, HMBTN_AUDIO_RATE); hmbtn_write_le32(header+28, HMBTN_AUDIO_RATE*4);
        hmbtn_write_le16(header+32, 4); hmbtn_write_le16(header+34, 16);
        memcpy(header+36, "data", 4); hmbtn_write_le32(header+40, frames*4);
        CHECK(fwrite(header, 1, sizeof(header), wav) == sizeof(header));
    }
    hmbtn_sound_start(s);
    uint64_t energy = 0;
    unsigned peak = 0, clipped = 0;
    for (unsigned rendered = 0; rendered < frames;) {
        int16_t pcm[1024];
        uint8_t bytes[2048];
        const unsigned count = frames-rendered < 512 ? frames-rendered : 512;
        hmbtn_sound_render(s, pcm, count);
        for (unsigned i = 0; i < count*2; ++i) {
            const unsigned magnitude = (unsigned)(pcm[i] < 0 ? -(int)pcm[i] : pcm[i]);
            if (magnitude > peak) peak = magnitude;
            if (magnitude >= 32767) ++clipped;
            energy += (uint64_t)magnitude * magnitude;
            hmbtn_write_le16(bytes + i*2, (uint16_t)pcm[i]);
        }
        if (wav) CHECK(fwrite(bytes, 4, count, wav) == count);
        rendered += count;
    }
    if (wav) CHECK(fclose(wav) == 0);
    printf("Original title: peak=%u, clipped=%u, loops=%u, energy=%llu\n",
        peak, clipped, hmbtn_sound_loop_count(s), (unsigned long long)energy);
    CHECK(energy > 0 && peak > 100 && clipped == 0 && hmbtn_sound_loop_count(s) == 1);
    hmbtn_sound_destroy(s);
    return 0;
}
int main(int argc, char **argv)
{
    if (argc > 1) return probe(argv[1], argc > 2 ? argv[2] : NULL);
    uint8_t vh[4096] = {0}, vb[32] = {0};
    char error[256] = {0};
    HmbtnSound *s = hmbtn_sound_create();
    CHECK(s);
    memcpy(vh, "pBAV", 4); hmbtn_write_le32(vh+4, 7);
    hmbtn_write_le16(vh+18, 1); hmbtn_write_le16(vh+20, 1); hmbtn_write_le16(vh+22, 1);
    vh[24] = vh[33] = 127; vh[25] = vh[36] = 64; vh[32] = 1;
    vh[0x822] = 127; vh[0x823] = 64; vh[0x824] = 60; vh[0x827] = 127;
    hmbtn_write_le16(vh+0x830, 0x000f); hmbtn_write_le16(vh+0x832, 0x5fc5);
    hmbtn_write_le16(vh+0x836, 1); hmbtn_write_le16(vh+0xa22, 4);
    vb[0] = vb[16] = 8; vb[1] = 4; vb[17] = 3;
    for (unsigned i = 2; i < 16; ++i) vb[i] = vb[i+16] = 0x97;
    CHECK(hmbtn_sound_bank(s, 0, vh, sizeof(vh), vb, sizeof(vb), error, sizeof(error)));
    CHECK(hmbtn_sound_bank(s, 1, vh, sizeof(vh), vb, sizeof(vb), error, sizeof(error)));
    /* 96 PPQN, 500000us/quarter, 96-tick loop with program/volume/pan commands.
     * Releasing the note must silence the looped sample before sequence loops. */
    const uint8_t seq[] = {'p','Q','E','S',0,0,0,1,0,96,7,161,32,4,2,
        0,0xc0,0, 0,0xb0,99,20, 0,0xb0,6,127, 0,0xb0,7,100,
        0,0xb0,10,0, 0,0x90,60,100, 16,60,0, 80,0xb0,99,30, 0,255,47,0};
    CHECK(hmbtn_sound_sequence(s, seq, sizeof(seq), error, sizeof(error)));
    hmbtn_sound_start(s);
    int16_t pcm[1024];
    hmbtn_sound_render(s, pcm, 512);
    bool audible = false;
    for (unsigned i = 0; i < 512; ++i) { if (pcm[i*2]) audible = true; CHECK(pcm[i*2+1] == 0); }
    CHECK(audible); /* Actual decoded sample, with original pan semantics. */
    for (unsigned i = 0; i < 12; ++i) hmbtn_sound_render(s, pcm, 512);
    for (unsigned i = 0; i < 1024; ++i) CHECK(pcm[i] == 0); /* Note-off + ADSR. */
    for (unsigned i = 0; i < 32; ++i) hmbtn_sound_render(s, pcm, 512);
    CHECK(hmbtn_sound_loop_count(s) == 1);
    hmbtn_sound_music_volume(s, 0);
    hmbtn_sound_render(s, pcm, 512);
    for (unsigned i = 0; i < 1024; ++i) CHECK(pcm[i] == 0);
    hmbtn_sound_effect(s, 0); hmbtn_sound_render(s, pcm, 512);
    audible = false;
    for (unsigned i = 0; i < 512; ++i) {
        if (pcm[i*2]) audible = true;
        CHECK(pcm[i*2] == pcm[i*2+1]); /* Effect remains audible while music fades. */
    }
    CHECK(audible);
    /* Use a fresh mixer so the preceding looping effect cannot mask silence. */
    hmbtn_sound_destroy(s); s=hmbtn_sound_create(); CHECK(s);
    CHECK(hmbtn_sound_bank(s,0,vh,sizeof(vh),vb,sizeof(vb),error,sizeof(error)));
    CHECK(hmbtn_sound_bank(s,1,vh,sizeof(vh),vb,sizeof(vb),error,sizeof(error)));
    CHECK(hmbtn_sound_sequence(s, seq, sizeof(seq), error, sizeof(error)));
    hmbtn_sound_music_volume(s,127); hmbtn_sound_start(s);
    hmbtn_sound_render(s,pcm,512);
    hmbtn_sound_stop_music(s);
    for (unsigned i=0;i<50;++i) hmbtn_sound_render(s,pcm,512);
    CHECK(hmbtn_sound_loop_count(s)==0); /* Stopped sequencer must not restart at its loop. */
    for (unsigned i=0;i<1024;++i) CHECK(pcm[i]==0);
    hmbtn_sound_effect(s,0); hmbtn_sound_render(s,pcm,512);
    audible=false;
    for (unsigned i=0;i<1024;++i) if (pcm[i]) audible=true;
    CHECK(audible); /* Dialogue effects survive stopping the song. */
    CHECK(!hmbtn_sound_sequence(s, seq, 17, error, sizeof(error)));
    CHECK(!hmbtn_sound_bank(s, 0, vh, 40, vb, sizeof(vb), error, sizeof(error)));
    CHECK(!hmbtn_sound_bank(s, 0, vh, sizeof(vh), vb, 16, error, sizeof(error)));
    hmbtn_write_le16(vh+0xa22, 0xffff);
    CHECK(!hmbtn_sound_bank(s, 0, vh, sizeof(vh), vb, sizeof(vb), error, sizeof(error)));
    /* Malformed zero-time loops terminate without hanging the mixer. */
    const uint8_t zero[] = {'p','Q','E','S',0,0,0,1,0,96,7,161,32,4,2,0,255,47,0};
    CHECK(hmbtn_sound_sequence(s, zero, sizeof(zero), error, sizeof(error)));
    hmbtn_sound_start(s); hmbtn_sound_render(s, pcm, 512);
    CHECK(hmbtn_sound_loop_count(s) == 0);
    hmbtn_sound_destroy(s);
    return 0;
}
