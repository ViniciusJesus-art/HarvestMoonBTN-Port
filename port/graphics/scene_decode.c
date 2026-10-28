/* Serialized FarmModeScriptImage/FarmZoneModel from world/scene.h. */
#include "scene_decode.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>

static int16_t signed_half(const uint8_t *p)
{
    const uint16_t v = hmbtn_read_le16(p);
    return (int16_t)(v < 32768 ? (int32_t)v : (int32_t)v-65536);
}
bool hmbtn_scene_data_open(HmbtnSceneData *s, const uint8_t *bytes, size_t size,
    char *error, size_t error_size)
{
    if (!s || !bytes || size < 0x200C) {
        snprintf(error,error_size,"Scene section is truncated"); return false;
    }
    memset(s,0,sizeof(*s));
    if (!hmbtn_model_open(&s->terrain,bytes+0x2000,size-0x2000,error,error_size)) return false;
    s->primitive_capacity = s->terrain.primitive_count;
    for (unsigned i = 0; i < HMBTN_SCENE_MODELS; ++i) {
        const uint8_t *p = bytes+0x948+i*44;
        const uint32_t flags = hmbtn_read_le32(p), offset = hmbtn_read_le32(p+4);
        if (flags > UINT16_MAX) goto invalid;
        s->flags[i] = (uint16_t)flags;
        if (flags&0x8000u) continue;
        if (offset > size-0x2000 || (offset&3u)) goto invalid;
        if (!hmbtn_model_open(&s->models[i],bytes+0x2000+offset,
                size-0x2000-offset,error,error_size)) return false;
        if(flags&0x4000u) {
            HmbtnModel from=s->models[i],to;
            const uint32_t key=hmbtn_read_le32(p+8), output=hmbtn_read_le32(p+12);
            if(key>size-0x2000 || output>size-0x2000 || (key&3u) || (output&3u)) goto invalid;
            if(!hmbtn_model_open(&to,bytes+0x2000+key,size-0x2000-key,error,error_size)
                    || !hmbtn_model_open(&s->models[i],bytes+0x2000+output,size-0x2000-output,error,error_size)) return false;
            if(!hmbtn_model_morph_bind(&s->models[i],&from,&to)) goto invalid;
        }
        for (unsigned axis = 0; axis < 3; ++axis) {
            s->position[i][axis] = signed_half(p+20+axis*2);
            s->angle[i][axis] = hmbtn_read_le16(p+28+axis*2);
            s->spin[i][axis] = signed_half(p+36+axis*2);
        }
        /* Initial slot 0 is visible; upgrade slots 1 and 2 are hidden. */
        if (flags&0x100u) {
            if (flags&15u) s->flags[i] |= 0x1000u;
            else s->flags[i] &= (uint16_t)~0x1000u;
        }
        /* Linked bed alternatives can become visible during sleep. Reserve
         * their decoded primitives as well as initially visible models. */
        if (!(s->flags[i]&0x1000u) || (flags&0x80u)) {
            if (s->models[i].primitive_count > 16384-s->primitive_capacity) goto invalid;
            s->primitive_capacity += s->models[i].primitive_count;
        }
    }
    return true;
invalid:
    snprintf(error,error_size,"Scene model flags or primitive capacity outside bounds"); return false;
}
void hmbtn_scene_data_tick(HmbtnSceneData *s)
{
    for (unsigned i = 0; i < HMBTN_SCENE_MODELS; ++i)
        if ((s->flags[i]&0xB000u) == 0x2000u)
            for (unsigned axis = 0; axis < 3; ++axis)
                s->angle[i][axis] = (uint16_t)((s->angle[i][axis]+s->spin[i][axis])&4095);
}
unsigned hmbtn_scene_data_instances(const HmbtnSceneData *s,
    HmbtnModelInstance instances[HMBTN_SCENE_MODELS+1])
{
    unsigned count = 0;
    /* scene_spin_entries runs before scroll_camera_draw; preserve tie ordering. */
    for (unsigned i = 0; i < HMBTN_SCENE_MODELS; ++i) {
        if (s->flags[i]&0x9000u) continue;
        instances[count].model = &s->models[i];
        memcpy(instances[count].position,s->position[i],sizeof(instances[count].position));
        memcpy(instances[count].angle,s->angle[i],sizeof(instances[count].angle));
        ++count;
    }
    instances[count++] = (HmbtnModelInstance){&s->terrain,{0},{0}};
    return count;
}
