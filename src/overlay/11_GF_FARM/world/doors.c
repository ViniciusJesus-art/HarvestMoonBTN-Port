/* Farm doors that swing open and shut. */

#include <abs.h>

#include "types.h"
#include "doors.h"
#include "farm_init.h"
#include "farm_state.h"
#include "scene.h"
#include "sound_request.h"
#include "villager.h"

/* A door swings to this angle (4096 to a turn) and holds there for DOOR_HOLD_FRAMES. */
#define DOOR_OPEN_TURN 0x420
#define DOOR_HOLD_FRAMES 11
#define DOOR_SWING_ACCEL 8

/* Open angle for each door facing. */
static u16 door_open_angles[4][2] = {
    {0x0420, 0x0000}, {0xFBE0, 0xFFFF}, {0xFBE0, 0xFFFF}, {0x0420, 0x0000},
};

/* Step a swinging door. While opening or closing its speed grows by 8 and the
 * angle moves by that speed, the other way for the left and up facings. The
 * door's model turns, and so does its twin's (mirrored) if it has one. Opening
 * stops at DOOR_OPEN_TURN with the facing's open angle; closing stops at 0 with
 * sound 0x4C. */
static void door_step(FarmZoneEntry *door)
{
    s16 index;
    s32 linked;
    s32 reversed;
    s16 twin;
    s16 mirrored[3];

    index = door->zone->modelIndex;
    linked = farm_area.mode.data->models[index].flags & SCENE_ENTRY_TWIN;
    reversed = door->facing == FACING_LEFT || door->facing == FACING_UP;
    if (linked) {
        twin = farm_area.mode.data->models[index].link.twin;
    } else {
        twin = -1;
    }
    if (door->id == FARM_ZONE_OPENING) {
        door->swingSpeed += DOOR_SWING_ACCEL;
    }
    if (door->id == FARM_ZONE_CLOSING) {
        door->swingSpeed -= DOOR_SWING_ACCEL;
    }
    if (reversed) {
        door->turnY -= door->swingSpeed;
    } else {
        door->turnY += door->swingSpeed;
    }
    scene_entry_set_turn(index, &door->turnX);
    if (linked) {
        mirrored[0] = door->turnX;
        mirrored[1] = door->turnY * -1;
        mirrored[2] = door->turnZ;
        scene_entry_set_turn(twin, mirrored);
    }
    if (door->id == FARM_ZONE_OPENING && abs(door->turnY) >= DOOR_OPEN_TURN) {
        door->holdTimer = DOOR_HOLD_FRAMES;
        door->id = FARM_ZONE_HELD_OPEN;
        door->swingSpeed = 0;
        door->turnY = door_open_angles[door->facing][0];
    } else if (door->id == FARM_ZONE_CLOSING) {
        if (reversed) {
            if (door->turnY < 0) {
                return;
            }
        } else if (door->turnY > 0) {
            return;
        }
        sound_play(0x4C);
        door->id = FARM_ZONE_CLOSED;
        door->swingSpeed = 0;
        door->turnY = 0;
        scene_entry_set_turn(index, &door->turnX);
        if (linked) {
            scene_entry_set_turn(twin, &door->turnX);
        }
    }
}

/* Process each farm entry according to its current state. */
void door_entries_update(void) {
    FarmZoneEntry *entry = farm_zone_entries;
    u32 index;
    s32 timer;

    if (game_state.globalFlags & GLOBAL_FLAG_PAUSED) {
        return;
    }
    for (index = 0; index < 10; index++, entry++) {
        if (entry->id == FARM_ZONE_FREE) {
            continue;
        }
        switch (entry->id) {
        case FARM_ZONE_OPENING:
            door_step(entry);
            break;
        case FARM_ZONE_HELD_OPEN:
            timer = entry->holdTimer;
            timer--;
            entry->holdTimer = timer;
            if ((s16)timer == 0) {
                entry->id = FARM_ZONE_CLOSING;
            }
            break;
        case FARM_ZONE_CLOSING:
            door_step(entry);
            break;
        }
    }
}

/* The state of the first door whose zone is tied to `event`, or 0 when none is. */
u16 door_entry_for_event(s32 event) {
    FarmZoneEntry *entry = farm_zone_entries;
    u32 i;

    for (i = 0; i < 10; i++, entry++) {
        if (entry->zone->event == event) return entry->id;
    }
    return 0;
}
