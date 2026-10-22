#include "psx_audio.h"
#include "psx_data.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FAIL(message) do { snprintf(error, error_size, "%s", message); return false; } while (0)
enum { MAX_EVENTS = 65536, MAX_PCM_BYTES = 4 * 1024 * 1024 };
typedef struct Sample { int16_t *pcm; uint32_t count, loop; bool repeats; } Sample;
typedef struct Tone { uint8_t volume, pan, center, shift, min, max; uint16_t adsr1, adsr2, sample; } Tone;
typedef struct Program { uint8_t count, volume, pan; Tone tones[16]; } Program;
typedef struct Bank { uint8_t volume, pan; Program programs[128]; Sample samples[256]; } Bank;
typedef struct Event { uint64_t frame; uint8_t status, a, b; } Event;
typedef struct Channel { unsigned program, volume, pan; } Channel;
typedef struct Voice {
    const Sample *sample;
    const Tone *tone;
    double position, pitch, left, right;
    int envelope, phase;
    uint32_t counter;
    unsigned channel, note, velocity, program;
    uint64_t serial;
    bool effect, active;
} Voice;
struct HmbtnSound {
    Bank banks[2];
    Event *events;
    size_t event_count, next_event, loop_event;
    uint64_t frame, loop_frame, serial;
    uint32_t loops;
    Channel channels[16];
    Voice voices[HMBTN_AUDIO_VOICES];
    unsigned music_volume;
    bool playing;
};

static void bank_free(Bank *b)
{
    for (unsigned i = 0; i < 256; ++i) free(b->samples[i].pcm);
    memset(b, 0, sizeof(*b));
}

HmbtnSound *hmbtn_sound_create(void)
{
    HmbtnSound *s = calloc(1, sizeof(*s));
    if (s) s->music_volume = 127;
    return s;
}
void hmbtn_sound_destroy(HmbtnSound *s)
{
    if (!s) return;
    bank_free(&s->banks[0]);
    bank_free(&s->banks[1]);
    free(s->events);
    free(s);
}

static int clamp16(int v) { return v < -32768 ? -32768 : v > 32767 ? 32767 : v; }
/* Defined arithmetic shift, including negative inputs, on every host compiler. */
static int sar(int v, unsigned bits)
{
    const int d = 1 << bits;
    return v >= 0 ? v / d : -((-v + d - 1) / d);
}

static bool sample_decode(Sample *s, const uint8_t *data, size_t size,
    char *error, size_t error_size)
{
    static const int coefficients[5][2] = {{0,0},{60,0},{115,-52},{98,-55},{122,-60}};
    if (!size || size % 16) FAIL("Invalid VAB sample size");
    s->pcm = malloc(size / 16 * 28 * sizeof(*s->pcm));
    if (!s->pcm) FAIL("Cannot allocate VAB sample");
    int history1 = 0, history2 = 0;
    for (size_t offset = 0; offset < size; offset += 16) {
        const uint8_t *block = data + offset;
        const unsigned filter = block[0] >> 4;
        unsigned shift = block[0] & 15;
        if (filter > 4) FAIL("Unsupported SPU ADPCM filter");
        if (shift > 12) shift = 9; /* Hardware behaviour for reserved shifts. */
        if (block[1] & 4) s->loop = s->count;
        for (unsigned i = 0; i < 28; ++i) {
            int nibble = (block[2 + i / 2] >> ((i % 2) * 4)) & 15;
            if (nibble >= 8) nibble -= 16;
            const int value = clamp16(sar(nibble * 4096, shift)
                + sar(history1 * coefficients[filter][0] + history2 * coefficients[filter][1] + 32, 6));
            s->pcm[s->count++] = (int16_t)value;
            history2 = history1;
            history1 = value;
        }
        if (block[1] & 1) {
            s->repeats = (block[1] & 2) != 0;
            break;
        }
    }
    return true;
}

bool hmbtn_sound_bank(HmbtnSound *s, unsigned slot, const uint8_t *vh, size_t vh_size,
    const uint8_t *vb, size_t vb_size, char *error, size_t error_size)
{
    if (slot > 1 || vh_size < 0x820 || memcmp(vh, "pBAV", 4)
            || hmbtn_read_le32(vh + 4) != 7) FAIL("Invalid VAB header");
    const unsigned programs = hmbtn_read_le16(vh + 18), samples = hmbtn_read_le16(vh + 22);
    const size_t sizes_offset = 0x820 + (size_t)programs * 512;
    if (!programs || programs > 128 || samples > 255
            || sizes_offset + 512 > vh_size) FAIL("VAB tables outside header");
    /* All voices borrow decoded samples. Do not replace a live bank. */
    for (unsigned i = 0; i < HMBTN_AUDIO_VOICES; ++i) s->voices[i].active = false;
    Bank *b = &s->banks[slot];
    bank_free(b);
    b->volume = vh[24]; b->pan = vh[25];
    size_t tone_program = 0, total_pcm = 0, offset = (size_t)hmbtn_read_le16(vh + sizes_offset) * 8;
    if (b->volume > 127 || b->pan > 127) FAIL("Invalid VAB master volume/pan");
    for (unsigned i = 0; i < 128; ++i) {
        const uint8_t *p = vh + 32 + i * 16;
        Program *program = &b->programs[i];
        program->count = p[0]; program->volume = p[1]; program->pan = p[4];
        if (!program->count) continue;
        if (program->count > 16 || tone_program >= programs || p[1] > 127 || p[4] > 127)
            FAIL("Invalid VAB program table");
        for (unsigned j = 0; j < program->count; ++j) {
            const uint8_t *t = vh + 0x820 + tone_program * 512 + j * 32;
            Tone *tone = &program->tones[j];
            tone->volume = t[2]; tone->pan = t[3]; tone->center = t[4]; tone->shift = t[5];
            tone->min = t[6]; tone->max = t[7];
            tone->adsr1 = hmbtn_read_le16(t + 16); tone->adsr2 = hmbtn_read_le16(t + 18);
            tone->sample = hmbtn_read_le16(t + 22);
            if (!tone->sample || tone->sample > samples || t[2] > 127 || t[3] > 127
                    || t[4] > 127 || t[5] > 127 || t[6] > t[7]) FAIL("Invalid VAB tone");
        }
        ++tone_program;
    }
    if (tone_program != programs) FAIL("VAB program count mismatch");
    for (unsigned i = 1; i <= samples; ++i) {
        const size_t bytes = (size_t)hmbtn_read_le16(vh + sizes_offset + i * 2) * 8;
        if (offset > vb_size || bytes > vb_size - offset) FAIL("VAB sample outside body");
        total_pcm += bytes / 16 * 28 * 2;
        if (total_pcm > MAX_PCM_BYTES) FAIL("VAB decoded samples exceed memory limit");
        if (!sample_decode(&b->samples[i], vb + offset, bytes, error, error_size)) return false;
        offset += bytes;
    }
    return true;
}

static bool vlq(const uint8_t *data, size_t size, size_t *p, uint32_t *value)
{
    *value = 0;
    for (unsigned i = 0; i < 4 && *p < size; ++i) {
        const unsigned byte = data[(*p)++];
        *value = (*value << 7) | (byte & 127);
        if (!(byte & 128)) return true;
    }
    return false;
}

bool hmbtn_sound_sequence(HmbtnSound *s, const uint8_t *seq, size_t size,
    char *error, size_t error_size)
{
    if (size < 16 || memcmp(seq, "pQES\0\0\0\1", 8)) FAIL("Invalid SEQ header");
    const unsigned resolution = (unsigned)seq[8] * 256 + seq[9];
    unsigned tempo = ((unsigned)seq[10] << 16) | ((unsigned)seq[11] << 8) | seq[12];
    if (!resolution || !tempo) FAIL("Invalid SEQ timing");
    s->playing = false;
    free(s->events);
    s->events = calloc(MAX_EVENTS, sizeof(*s->events));
    s->event_count = 0;
    if (!s->events) FAIL("Cannot allocate SEQ events");
    size_t p = 15;
    unsigned status = 0;
    uint64_t time = 0;
    const uint64_t denominator = (uint64_t)resolution * 1000000;
    while (p < size && s->event_count < MAX_EVENTS) {
        uint32_t delta;
        if (!vlq(seq, size, &p, &delta) || p >= size) FAIL("Truncated SEQ delta");
        if (delta > 1000000 || time > UINT64_MAX - (uint64_t)delta * tempo * HMBTN_AUDIO_RATE)
            FAIL("SEQ duration exceeds limit");
        time += (uint64_t)delta * tempo * HMBTN_AUDIO_RATE;
        if (seq[p] & 128) status = seq[p++];
        Event event = {time / denominator, (uint8_t)status, 0, 0};
        if (status == 255) {
            if (p >= size) FAIL("Truncated SEQ meta event");
            event.a = seq[p++];
            if (event.a == 47) {
                s->events[s->event_count++] = event;
                return true;
            }
            if (event.a != 81 || size - p < 3) FAIL("Unsupported SEQ meta event");
            tempo = ((unsigned)seq[p] << 16) | ((unsigned)seq[p+1] << 8) | seq[p+2];
            p += 3;
            if (!tempo) FAIL("Invalid SEQ tempo");
            continue;
        }
        const unsigned kind = status & 240, count = kind == 192 ? 1 : 2;
        if (kind != 128 && kind != 144 && kind != 176 && kind != 192)
            FAIL("Unsupported SEQ event (title sequencer scope)");
        if (size - p < count || seq[p] > 127 || (count == 2 && seq[p+1] > 127))
            FAIL("Invalid SEQ event data");
        event.a = seq[p++];
        if (count == 2) event.b = seq[p++];
        if (kind == 176 && event.a != 7 && event.a != 10 && event.a != 99 && event.a != 6)
            FAIL("Unsupported SEQ controller");
        s->events[s->event_count++] = event;
    }
    FAIL("SEQ end missing or event limit exceeded");
}

void hmbtn_sound_start(HmbtnSound *s)
{
    s->next_event = s->loop_event = 0;
    s->frame = s->loop_frame = 0;
    s->loops = 0;
    for (unsigned i = 0; i < 16; ++i) s->channels[i] = (Channel){0,127,64};
    for (unsigned i = 0; i < HMBTN_AUDIO_VOICES; ++i)
        if (!s->voices[i].effect) s->voices[i].active = false;
    s->playing = s->event_count > 0;
}
void hmbtn_sound_music_volume(HmbtnSound *s, unsigned volume)
{
    s->music_volume = volume > 127 ? 127 : volume;
}

static void gains(HmbtnSound *s, Voice *v)
{
    const Bank *b = &s->banks[v->effect ? 0 : 1];
    const Program *p = &b->programs[v->program];
    const Channel *c = &s->channels[v->channel];
    const double volume = (double)v->velocity * b->volume * p->volume * v->tone->volume
        / (127.0 * 127.0 * 127.0 * 127.0) * (v->effect ? 1.0 : c->volume / 127.0);
    int pan = b->pan + p->pan + v->tone->pan + (v->effect ? 64 : (int)c->pan) - 192;
    if (pan < 0) pan = 0;
    if (pan > 127) pan = 127;
    v->left = volume * (pan <= 64 ? 1.0 : (127 - pan) / 63.0);
    v->right = volume * (pan >= 64 ? 1.0 : pan / 64.0);
}

static void note_on(HmbtnSound *s, unsigned channel, unsigned note, unsigned velocity, int effect_tone)
{
    const bool effect = effect_tone >= 0;
    const unsigned program = effect ? 0 : s->channels[channel].program;
    const Bank *bank = &s->banks[effect ? 0 : 1];
    const Program *p = &bank->programs[program];
    for (unsigned j = 0; j < p->count; ++j) {
        const Tone *tone = &p->tones[j];
        if (effect ? j != (unsigned)effect_tone : note < tone->min || note > tone->max) continue;
        unsigned chosen = 0;
        for (unsigned i = 0; i < HMBTN_AUDIO_VOICES; ++i) {
            if (!s->voices[i].active) { chosen = i; break; }
            if (s->voices[i].serial < s->voices[chosen].serial) chosen = i;
        }
        Voice *v = &s->voices[chosen];
        memset(v, 0, sizeof(*v));
        v->sample = &bank->samples[tone->sample];
        v->tone = tone;
        v->pitch = pow(2.0, ((double)note - tone->center - tone->shift / 128.0) / 12.0);
        if (v->pitch > 4.0) v->pitch = 4.0;
        v->effect = effect; v->channel = channel; v->note = note;
        v->velocity = velocity; v->program = program;
        v->active = v->sample->count > 0;
        v->serial = ++s->serial;
        gains(s, v);
    }
}
void hmbtn_sound_effect(HmbtnSound *s, unsigned tone)
{
    if (tone < 16) note_on(s, 0, 60, tone == 4 ? 30 : 60, (int)tone);
}

static void envelope_tick(Voice *v)
{
    const unsigned a = v->tone->adsr1, b = v->tone->adsr2;
    unsigned shift, step_value;
    bool decrease, exponential;
    switch (v->phase) {
    case 0: shift = (a >> 10) & 31; step_value = (a >> 8) & 3;
        decrease = false; exponential = (a & 32768) != 0; break;
    case 1: shift = (a >> 4) & 15; step_value = 0;
        decrease = true; exponential = true; break;
    case 2: shift = (b >> 8) & 31; step_value = (b >> 6) & 3;
        decrease = (b & 16384) != 0; exponential = (b & 32768) != 0; break;
    default: shift = b & 31; step_value = 0;
        decrease = true; exponential = (b & 32) != 0; break;
    }
    int step = decrease ? -8 + (int)step_value : 7 - (int)step_value;
    step *= 1 << (shift < 11 ? 11 - shift : 0);
    uint32_t increment = 32768u >> (shift > 11 ? shift - 11 : 0);
    if (exponential && !decrease && v->envelope > 0x6000) {
        if (shift < 10) step /= 4;
        else if (shift >= 11) increment /= 4;
        else { step /= 2; increment /= 2; }
    } else if (exponential && decrease) step = sar(step * v->envelope, 15);
    if (increment == 0 && !(shift == 31 && step_value == 3)) increment = 1;
    v->counter += increment;
    if (v->counter < 32768) return;
    v->counter &= 32767;
    v->envelope += step;
    if (v->envelope < 0) v->envelope = 0;
    if (v->envelope > 32767) v->envelope = 32767;
    if ((v->phase == 0 && v->envelope == 32767)
            || (v->phase == 1 && v->envelope <= (int)((a & 15) + 1) * 2048)) {
        ++v->phase; v->counter = 0;
    }
    if (v->phase == 3 && !v->envelope) v->active = false;
}

static void events_tick(HmbtnSound *s)
{
    /* A malformed zero-duration loop must never spin the audio/main thread. */
    unsigned budget = MAX_EVENTS;
    while (s->playing && s->next_event < s->event_count && budget) {
        const Event *e = &s->events[s->next_event];
        if (e->frame > s->frame) break;
        --budget;
        ++s->next_event;
        const unsigned kind = e->status & 240, channel = e->status & 15;
        if (e->status == 255 || (kind == 176 && e->a == 99 && e->b == 30)) {
            if (s->loop_frame >= s->frame) { s->playing = false; break; }
            s->frame = s->loop_frame; s->next_event = s->loop_event; ++s->loops;
        } else if (kind == 192) s->channels[channel].program = e->a;
        else if (kind == 144 && e->b) note_on(s, channel, e->a, e->b, -1);
        else if (kind == 128 || (kind == 144 && !e->b)) {
            for (unsigned i = 0; i < HMBTN_AUDIO_VOICES; ++i) {
                Voice *v = &s->voices[i];
                if (v->active && !v->effect && v->channel == channel && v->note == e->a) {
                    v->phase = 3; v->counter = 0;
                }
            }
        } else if (kind == 176) {
            if (e->a == 99 && e->b == 20) {
                s->loop_frame = s->frame; s->loop_event = s->next_event;
            } else if (e->a == 7 || e->a == 10) {
                if (e->a == 7) s->channels[channel].volume = e->b;
                else s->channels[channel].pan = e->b;
                for (unsigned i = 0; i < HMBTN_AUDIO_VOICES; ++i)
                    if (s->voices[i].active && !s->voices[i].effect && s->voices[i].channel == channel)
                        gains(s, &s->voices[i]);
            }
        }
    }
    if (!budget) s->playing = false;
}

void hmbtn_sound_render(HmbtnSound *s, int16_t *stereo, size_t frames)
{
    for (size_t frame = 0; frame < frames; ++frame) {
        events_tick(s);
        double left = 0, right = 0;
        for (unsigned i = 0; i < HMBTN_AUDIO_VOICES; ++i) {
            Voice *v = &s->voices[i];
            if (!v->active) continue;
            const Sample *sample = v->sample;
            if (v->position >= sample->count) {
                if (!sample->repeats || sample->loop >= sample->count) { v->active = false; continue; }
                v->position = sample->loop + fmod(v->position - sample->loop, sample->count - sample->loop);
            }
            const unsigned pos = (unsigned)v->position;
            unsigned next = pos + 1;
            if (next >= sample->count) next = sample->repeats ? sample->loop : pos;
            const double value = sample->pcm[pos] + (sample->pcm[next] - sample->pcm[pos]) * (v->position - pos);
            const double gain = v->envelope / 32768.0 * (v->effect ? 1.0 : s->music_volume / 127.0);
            left += value * gain * v->left;
            right += value * gain * v->right;
            v->position += v->pitch;
            envelope_tick(v);
        }
        /* Original SPU master volume is 0x3fff (about half of full scale). */
        stereo[frame * 2] = (int16_t)clamp16((int)(left * 0.5));
        stereo[frame * 2 + 1] = (int16_t)clamp16((int)(right * 0.5));
        if (s->playing) ++s->frame;
    }
}
uint32_t hmbtn_sound_loop_count(const HmbtnSound *s) { return s->loops; }
