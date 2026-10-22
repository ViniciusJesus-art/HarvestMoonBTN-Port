/* Native event_commands.c subset for events 0x74..0x79. Resource requests keep
 * the cursor pending until the host loads them; unsupported scenes stop. */
#include "farm_intro.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static void select_sprite(HmbtnIntroActor *a, unsigned animation)
{
    if (a->sprite.animation != animation) {
        a->sprite = (HmbtnAnimationPlayer){(uint16_t)animation,0,0};
        a->animation_done = false;
    }
}
HmbtnIntroActor *hmbtn_farm_intro_actor(HmbtnFarmIntro *s, int32_t id)
{
    if (id == 17) return &s->mayor;
    if (id >= 1 && id <= 4) return &s->childhood[id-1];
    if (id == 27) return &s->boy;
    if (id == 33) return &s->girl;
    return NULL;
}
static bool slot(int32_t i) { return i >= 0 && i < 32; }
static int compare(int32_t a, int32_t b, int32_t op)
{
    switch (op) {
    case 0: return !(a < b); case 1: return !(a <= b);
    case 2: return !(a > b); case 3: return !(a >= b); case 4: return a == b;
    default: return -1;
    }
}
static HmbtnEventResult command(void *ctx, const HmbtnEventCommand *c,
    char *error, size_t error_size)
{
    HmbtnFarmIntro *s = ctx;
    const int32_t a = c->command, b = c->operand, x = c->x, y = c->y;
    HmbtnIntroActor *actor = hmbtn_farm_intro_actor(s,a);
    switch (c->type) {
    case 0x23: {
        const int32_t ids[4] = {a,b,x,y};
        for (unsigned i = 0; i < 4; ++i) {
            if (ids[i] == -1) continue;
            HmbtnIntroActor *reset = hmbtn_farm_intro_actor(s,ids[i]);
            if (!reset) goto unsupported;
            memset(reset->velocity,0,sizeof(reset->velocity));
            reset->sprite = (HmbtnAnimationPlayer){0}; reset->animation_done = false;
        }
        return HMBTN_EVENT_DONE;
    }
    case 6:
        if (!actor || x || y || (b != 0 && b != 1)) break;
        actor->visible = b == 0; return HMBTN_EVENT_DONE;
    case 0x3D:
        if (a == 2 && !b && !x && !y && s->area == 80 && !s->area_pending) {
            /* house_door_positions[0]: initial, unexpanded house. */
            s->player.position[0] = -90; s->player.position[1] = 0; s->player.position[2] = 541;
            s->player.facing = 0;
        } else if (!a && x >= -32768 && x <= 32767 && y >= -32768 && y <= 32767) {
            s->player.position[0] = x; s->player.position[1] = b; s->player.position[2] = y;
        } else break;
        s->player.pose = false; return HMBTN_EVENT_DONE;
    case 4:
        if (!actor || x < -32768 || x > 32767 || y < -32768 || y > 32767) break;
        actor->position[0] = x; actor->position[1] = b; actor->position[2] = y;
        actor->visible = true; return HMBTN_EVENT_DONE;
    case 0x15: {
        if (!slot(a) || (b && !slot(x)) || y < 0 || y > 5) break;
        const int32_t value = b ? s->slots[x] : x;
        int64_t result = s->slots[a];
        switch (y) {
        case 0: result = value; break; case 1: result += value; break;
        case 2: result -= value; break; case 3: result *= value; break;
        case 4: if (value) result /= value; break;
        case 5: if (value) result %= value; break;
        }
        if (result < INT32_MIN || result > INT32_MAX) break;
        s->slots[a] = (int32_t)result; return HMBTN_EVENT_DONE;
    }
    case 0xF: case 0x10: {
        if (!slot(a) || (c->type == 0x10 && !slot(b))) break;
        const int result = compare(s->slots[a],c->type == 0x10 ? s->slots[b] : y,x);
        if (result < 0) break;
        return result ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
    }
    case 3: {
        if (!slot(b) || a < 0 || x || y || s->slots[b] == INT32_MAX) break;
        const bool done = s->slots[b]++ > a/2;
        return done ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
    }
    case 0x1B:
        if (a < 0 || a > 3 || b < 0 || b > 300 || y < 0 || y > 1) break;
        if (x) {
            s->fade_white = y != 0;
            s->fade_mode = (a&1) ? 1 : 2;
            s->fade_length = (unsigned)b + ((a&1) ? 0u : 1u);
            s->fade_level = (a&1) && b ? (unsigned)b-1 : 0;
            return HMBTN_EVENT_WAIT;
        }
        return (!s->fade_level || s->fade_level >= s->fade_length+1) ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
    case 0x40:
        if (a < 0 || a > 3 || b || x || y) break;
        s->player.facing = (unsigned)a; s->player.pose = false;
        s->player.sprite = (HmbtnAnimationPlayer){0,(uint16_t)a,0}; return HMBTN_EVENT_DONE;
    case 0x3E: { /* scripted buttons use the original 41-unit walk step */
        if (a < 0 || a > 1 || b < 0 || b > 3 || y || x < -32768 || x > 32767) break;
        const unsigned axis = (b&1) ? 0 : 2;
        const bool snap = b < 2 ? x >= s->player.position[axis] : x <= s->player.position[axis];
        if (snap) { s->player.position[axis] = x; return HMBTN_EVENT_DONE; }
        s->player.facing = (unsigned)b;
        s->player.velocity[axis] = b < 2 ? -41 : 41;
        s->player.pose = false; select_sprite(&s->player,(unsigned)b+1);
        return HMBTN_EVENT_WAIT;
    }
    case 1: case 0x52: { /* farm_nudge_cue / walk_axis: amount*2 per tick */
        if (!actor || b < 0 || b > 3 || x < -32768 || x > 32767 || y < 0 || y > 255) break;
        select_sprite(actor,(unsigned)y);
        const unsigned axis = (b&1) ? 0 : 2;
        const bool snap = b < 2 ? x >= actor->position[axis] : x <= actor->position[axis];
        memset(actor->velocity,0,sizeof(actor->velocity));
        if (snap) { actor->position[axis] = x; return HMBTN_EVENT_DONE; }
        const int32_t speed = c->type == 1 ? 24 : 40;
        actor->velocity[axis] = b < 2 ? -speed : speed; return HMBTN_EVENT_WAIT;
    }
    case 5:
        if (!actor || b < 0 || b > 255 || x < 0 || x > 255 || (y != 0 && y != 1)) break;
        if (!y) {
            if (actor->sprite.animation != (unsigned)b) { select_sprite(actor,(unsigned)b); return HMBTN_EVENT_WAIT; }
            return actor->animation_done ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
        }
        select_sprite(actor,(unsigned)b);
        actor->sprite.frame = (uint16_t)x;
        return HMBTN_EVENT_DONE;
    case 0x49:
        if (!actor || !slot(b) || !slot(x) || !slot(y)) break;
        s->slots[b] = actor->position[0]; s->slots[x] = actor->position[1]; s->slots[y] = actor->position[2];
        return HMBTN_EVENT_DONE;
    case 2:
        if (a != 1 || b < 0 || x != -1 || y) break;
        if (s->message_done && s->message == (unsigned)b) {
            s->message = 0; s->message_done = false; return HMBTN_EVENT_DONE;
        }
        s->message = (unsigned)b; return HMBTN_EVENT_WAIT;
    case 0x3F:
        /* Queued records are (player mode, animation, action-bank flag), not
         * (animation, frame). Mode 0x47 loads sector row 12 for the nod. */
        if (a) break;
        if (b == 71 && x >= 0 && x <= 3 && y == 11) {
            if (!s->action_ready) { s->action_pending = true; return HMBTN_EVENT_PENDING; }
            s->action_active = true; s->action_finished = false;
            s->player.sprite = (HmbtnAnimationPlayer){(uint16_t)x,0,0};
        } else if (b == 5 && x == 10 && y == 10) {
            s->action_active = false;
            s->player.sprite = (HmbtnAnimationPlayer){10,(uint16_t)s->player.facing,0};
        } else break;
        s->player.pose = true; return HMBTN_EVENT_WAIT;
    case 0x1A:
        if (a < 0 || a > 300 || b < 0 || b > 2 || x || y) break;
        if (!b) {
            s->music_from = s->music_volume; s->music_to = 0;
            s->music_length = a ? (unsigned)a : 10; s->music_time = 0;
            return HMBTN_EVENT_WAIT;
        }
        if (b == 2 || s->music_time >= s->music_length) { s->stop_music = true; return HMBTN_EVENT_DONE; }
        return HMBTN_EVENT_WAIT;
    case 0x4D:
        if (a < 0 || a > 1 || b < 0 || b > 300 || y) break;
        if (x) {
            s->music_from = s->music_volume; s->music_to = a ? 127 : 0;
            s->music_length = b ? (unsigned)b : 1; s->music_time = 0;
            return HMBTN_EVENT_DONE;
        }
        return s->music_time >= s->music_length ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
    case 0x33: { /* event_cmd_load_record: follow an actor, optionally glide first. */
        if (a == -2 && !b && !x && !y) {
            s->camera_actor = 0; s->camera_gliding = false;
            memcpy(s->camera,s->player.position,sizeof(s->camera)); return HMBTN_EVENT_DONE;
        }
        if (!actor || b || x < 0 || x > 1 || y < 0 || y > 3) break;
        if (!x || s->camera_actor == (unsigned)a) {
            s->camera_actor = (unsigned)a; s->camera_gliding = false;
            memcpy(s->camera,actor->position,sizeof(s->camera)); return HMBTN_EVENT_DONE;
        }
        s->camera_gliding = true;
        const int32_t dx = actor->position[0]-s->camera[0], dz = actor->position[2]-s->camera[2];
        int32_t distance = (int32_t)sqrt((double)dx*dx+(double)dz*dz);
        int32_t dy = actor->position[1]-s->camera[1]; if (dy < 0) dy = -dy;
        if (dy > distance) distance = dy;
        const int32_t speeds[4] = {24,36,48,72};
        const int32_t steps = distance/speeds[y];
        bool arrived = true;
        for (unsigned axis = 0; axis < 3; ++axis) {
            arrived &= s->camera[axis] == actor->position[axis];
            if (steps) s->camera[axis] += (actor->position[axis]-s->camera[axis])/steps;
            else s->camera[axis] = actor->position[axis];
        }
        if (arrived) { s->camera_actor = (unsigned)a; s->camera_gliding = false; return HMBTN_EVENT_DONE; }
        return HMBTN_EVENT_WAIT;
    }
    case 8:
        if ((((a == 0 || a == 230 || a == 231 || a == 232) && b == 1)
                || (a == -2 && !b)) && !x && !y) {
            /* EVENT_TARGET_HOUSE, spring and house level zero only. */
            const unsigned target = a == -2 ? 80u : (unsigned)a;
            if (s->area == target) return HMBTN_EVENT_DONE;
            s->target_area = target; s->area_pending = true;
            return HMBTN_EVENT_PENDING;
        }
        s->target_area = a == -2 ? 80u : a >= 0 ? (unsigned)a : 0;
        snprintf(error,error_size,"Opening reached area %u (%s); the next stage is not ported yet",
            s->target_area,a == -2 ? "farmhouse interior" : "next scene");
        return HMBTN_EVENT_BOUNDARY;
    case 0x25: /* event_cmd_enter_mode(0,0,0,0): finish the opening event. */
        if (a || b || x || y || s->event != 121 || s->area != 80 || s->area_pending) break;
        s->event_active = false; s->finish_requested = true;
        return HMBTN_EVENT_DONE;
    case 0x32:
        if (b || x || y) break;
        if (a < 117 || a > 121) {
            snprintf(error,error_size,"Opening reached event %d; it is not ported yet",(int)a);
            return HMBTN_EVENT_BOUNDARY;
        }
        s->queued_event = (unsigned)a; s->event_pending = true;
        return HMBTN_EVENT_PENDING;
    case 0x18: /* Load the mode's actual VAB/SEQ before acknowledging it. */
        if (a != 34 || b || x || y) break;
        if (s->music_mode == 34) return HMBTN_EVENT_DONE;
        s->song_pending = true; return HMBTN_EVENT_PENDING;
    case 0x19:
        if (a || b || x || y || s->music_mode != 34) break;
        s->start_music = true; return HMBTN_EVENT_DONE;
    case 0x17: /* CD-XA start/poll: resources must be opened by the host. */
        if ((a != 4 && a != 5) || b || y || (x != 0 && x != 1)) break;
        if (x) {
            if (s->xa_started && s->xa_track == (unsigned)a) {
                s->xa_started = false; return HMBTN_EVENT_WAIT;
            }
            s->xa_track = (unsigned)a; s->xa_pending = true;
            return HMBTN_EVENT_PENDING;
        }
        return s->xa_active ? HMBTN_EVENT_WAIT : HMBTN_EVENT_DONE;
    case 0x1C: /* event bracket; this subset opens it during the song. */
        if (a || b || x || y) break;
        s->event_active = true; return HMBTN_EVENT_WAIT;
    case 0x59:
        if (!actor || b < 0 || b > 2 || x || y) break;
        actor->marker = b == 0 ? 1u : b == 2 ? 2u : 0u; return HMBTN_EVENT_DONE;
    case 0x6D: { /* Original 177-step scalar camera pull-back. */
        if (!slot(a) || !slot(b) || x || y < 0 || y > 1) break;
        if (y) { s->camera_height = -4500; s->camera_distance = -9000; }
        if (s->slots[a] == 0) {
            s->camera_gliding = true; s->camera_actor = 0;
            s->camera_from[0] = s->camera_height; s->camera_from[1] = s->camera_distance;
            s->camera_from[2] = s->camera[1]; s->slots[a] = 1;
        } else if (s->slots[a] == 1) {
            if (s->slots[b] < 0 || s->slots[b] >= 177) break;
            const int32_t remaining = 177-s->slots[b];
            s->camera_height = (s->camera_from[0]+800)*remaining/177-800;
            s->camera_distance = 1100*remaining/177+s->camera_from[1]-1100;
            s->camera[1] = 1000*remaining/177+s->camera_from[2]-1000;
            if (++s->slots[b] >= 177) s->slots[a] = 2;
        } else return HMBTN_EVENT_DONE;
        return HMBTN_EVENT_WAIT;
    }
    case 0x6C: /* event_cmd_result_cell: one original childhood illustration. */
        if (!actor || !slot(b) || x || y || s->slots[b] < 0 || s->slots[b] > 9) break;
        s->cell_actor = (unsigned)a; select_sprite(actor,(unsigned)s->slots[b]);
        return HMBTN_EVENT_DONE;
    default:
        snprintf(error,error_size,"Native intro command 0x%02x at step %u is not ported",c->type,s->script.pc);
        return HMBTN_EVENT_BOUNDARY;
    }
unsupported:
    snprintf(error,error_size,"Unsupported intro arguments at step %u, command 0x%02x",s->script.pc,c->type);
    return HMBTN_EVENT_ERROR;
}

bool hmbtn_farm_intro_open(HmbtnFarmIntro *s, const uint8_t *script, size_t size,
    char *error, size_t error_size)
{
    memset(s,0,sizeof(*s));
    s->player.visible = true; s->music_volume = 127; s->event = 116;
    s->camera_height = -4500; s->camera_distance = -9000;
    s->fade_mode = 1; s->fade_level = s->fade_length = 1; /* Black until the first script tick. */
    return hmbtn_event_script_open(&s->script,script,size,error,error_size);
}
bool hmbtn_farm_intro_tick(HmbtnFarmIntro *s, char *error, size_t error_size)
{
    if (s->script.boundary || s->script.finished) return true;
    if (s->action_active && s->action_finished) {
        s->action_active = s->action_finished = s->player.pose = false;
        s->player.sprite = (HmbtnAnimationPlayer){0,(uint16_t)s->player.facing,0};
    }
    if (s->fade_mode == 1 && s->fade_level) --s->fade_level;
    if (s->fade_mode == 2 && s->fade_level <= s->fade_length+6) ++s->fade_level;
    if (s->music_time < s->music_length) {
        ++s->music_time;
        s->music_volume = (s->music_from*(s->music_length-s->music_time)+s->music_to*s->music_time)/s->music_length;
    }
    memset(s->player.velocity,0,sizeof(s->player.velocity));
    if (s->message && !s->message_done) return true;
    if (!hmbtn_event_tick(&s->script,command,s,error,error_size)) return false;
    if (!s->message) {
        const unsigned ids[] = {1,2,3,4,17,27,33};
        for (unsigned a = 0; a < sizeof(ids)/sizeof(ids[0]); ++a) {
            HmbtnIntroActor *actor = hmbtn_farm_intro_actor(s,(int32_t)ids[a]);
            for (unsigned axis = 0; axis < 3; ++axis) {
                const int64_t value = (int64_t)actor->position[axis]+actor->velocity[axis];
                if (value < -32768 || value > 32767) {
                    snprintf(error,error_size,"Opening actor moved outside signed 16-bit world bounds"); return false;
                }
                actor->position[axis] = (int32_t)value;
            }
        }
        bool moving = false;
        for (unsigned i = 0; i < 3; ++i) {
            const int64_t player = (int64_t)s->player.position[i]+s->player.velocity[i];
            if (player < -32768 || player > 32767) {
                snprintf(error,error_size,"Opening player moved outside signed 16-bit world bounds"); return false;
            }
            s->player.position[i] = (int32_t)player;
            moving |= s->player.velocity[i] != 0;
        }
        if (!moving && !s->player.pose) s->player.sprite = (HmbtnAnimationPlayer){0,(uint16_t)s->player.facing,0};
    }
    if (!s->camera_gliding) {
        const HmbtnIntroActor *follow = s->camera_actor ? hmbtn_farm_intro_actor(s,(int32_t)s->camera_actor) : &s->player;
        if (follow) memcpy(s->camera,follow->position,sizeof(s->camera));
    }
    return true;
}
unsigned hmbtn_farm_intro_brightness(const HmbtnFarmIntro *s)
{
    if (!s->fade_length) return 255;
    return s->fade_level >= s->fade_length ? 0 : 255-s->fade_level*255/s->fade_length;
}
