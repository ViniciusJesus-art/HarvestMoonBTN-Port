/* Farm scene: area changes, the scene entries and their morphs and the camera pans. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include <memory.h>
#include "types.h"
#include "cd_audio.h"
#include "clock.h"
#include "estate.h"
#include "event_flags.h"
#include "event_script.h"
#include "farm_init.h"
#include "farm_load.h"
#include "farm_probe.h"
#include "farm_state.h"
#include "field.h"
#include "ground.h"
#include "horse.h"
#include "items.h"
#include "main.h"
#include "math_util.h"
#include "milestones.h"
#include "mine.h"
#include "model.h"
#include "player.h"
#include "scene.h"
#include "scene_view.h"
#include "villager.h"
/* The sky fade runs over this many steps, out and back. */
#define SKY_FADE_STEPS 512
/* The TMD header, before its object table. */
#define TMD_HEADER_SIZE 0xC

/* Where the player stands after leaving the mine's return points. */
#define MINE_RETURN_37_X -10911
#define MINE_RETURN_37_Y -1685
#define MINE_RETURN_37_Z 9242
#define MINE_RETURN_43_X -11453
#define MINE_RETURN_43_Y -1892
#define MINE_RETURN_43_Z 9706
static void frame_set_clear_color(s32 r, s32 g, s32 b);
static void scene_entry_reset(FarmSceneModel *p);
static void morph_reset(FarmMorph *item);
static FarmMorph *morph_claim(char *model, char *from, char *to, SVECTOR *position);
static void morph_bind_models(FarmMorph *morph);
static void morph_blend_vertices(s32 count, FarmMorph *morph);
static void scene_assign_slots(void);
static void scene_flag_stage_tick(void);
static void scene_link_slot1_reset(void);
static void scene_link_handback(void);
static void scene_pan_object_find(void);
static void scene_camera_transition(void);
static void scene_tilt_object_find(void);
static void scene_tilt_transition(void);
static void scene_interp_start(void);
static void scene_interp_step(void);
static void scene_slot_flag_move(void);
static void scene_spouse_marker_update(void);
static SVECTOR scene_project_vertex;
static FarmScreenXY scene_projected_xy;
static FarmSceneModel scene_entries[SCENE_MODEL_COUNT];
static FarmMorph scene_morphs[SCENE_MORPH_COUNT];
static u32 scene_unused_80129174;
/* The entries selected for slots 0..2. */
static FarmSceneModel *scene_slot_entries[3];
/* The slot a scene entry is in, from the low nibble of its flags. */
enum SceneSlot
{
    SCENE_SLOT_0 = 0,
    SCENE_SLOT_1 = 1,
    SCENE_SLOT_2 = 2,
};
/* Stage of the three-owner flag rotation. */
enum SceneFlagStage
{
    SCENE_FLAG_WAIT_SHIPPED = 0,
    SCENE_FLAG_COUNT_SLOT0 = 1,
    SCENE_FLAG_COUNT_SLOT1 = 2,
    SCENE_FLAG_COUNT_SLOT2 = 3,
};
static u32 scene_flag_stage;
/* The two link entries. */
static FarmSceneModel *scene_link_entries[2];
/* Stage of the link handback between the two link entries. */
enum SceneHandbackState
{
    SCENE_HANDBACK_WAIT = 0,
    SCENE_HANDBACK_COUNT_FIRST = 1,
    SCENE_HANDBACK_COUNT_SECOND = 2,
    SCENE_HANDBACK_RETURN = 3,
};
static u32 scene_handback_state;
static FarmSceneModel *scene_pan_object;
/* Camera pan state. */
enum ScenePanState
{
    PAN_IDLE = 0,
    PAN_OUT = 1,
    PAN_HELD = 2,
    PAN_BACK = 3,
};
static u32 scene_pan_state;
static FarmSceneModel *scene_tilt_object;
/* Tilt state, with the same values as the pan state. */
static u32 scene_tilt_state;
/* The two marked entries, [0] and [1]. */
static FarmSceneModel *scene_marked_entries[2];

/* Clear the exit and forget the entered and absent areas. */
void scene_exit_clear(void)
{
    farm_area.mode.exit = 0;
    farm_area.mode.enteredArea = 0xFFFF;
    farm_area.mode.absentArea = 0xFFFF;
}

void scene_ground_tolerance_set(void)
{
    ground_set_tolerance(200);
}

static void scene_stub_1(void)
{
}

/* Save the current area's head as the previous one, load the new area's section and
 * model, then start the handler of its place (the milestone result, the sky fade). */
void scene_change_area(u16 area, s32 unused, u16 cached)
{
    FarmLoadArea *farm;
    FarmModeScript *script;

    farm = &farm_area;
    farm->mode.previousHead.place = farm->mode.script.head.place;
    farm->mode.previousHead.scene = farm->mode.script.head.scene;
    farm->mode.previousHead.flags = farm->mode.script.head.flags;
    farm->mode.previousHead.resourceSet = farm->mode.script.head.resourceSet;
    farm->mode.previousHead.resourceId = farm->mode.script.head.resourceId;
    farm->mode.previousHead.sceneId = farm->mode.script.head.sceneId;
    farm_load_section(area, cached);
    model_bind_tmd(&farm_model, farm_area.sectionBuffer + SECTION_STRINGS_OFFSET);
    scene_clamp_bounds_set(farm_area.mode.script.clampMin, farm_area.mode.script.clampMax);
    ground_enable_objects(&farm_model);
    frame_set_clear_color(0, 0, 0);
    script = farm->mode.data;
    switch (script->head.place.location)
    {
    case PLACE_F:
        if (script->head.scene == AREA_MILESTONE_RESULT)
        {
            milestone_result_begin();
        }
        else
        {
            milestone_result_show();
        }
        break;
    case PLACE_2C:
        scene_interp_start();
        break;
    }
}

/* Set the clear colour of both frame buffers to (r, g, b). */
static void frame_set_clear_color(s32 r, s32 g, s32 b)
{
    FrameState *fb0 = frame_records;
    FrameState *fb1 = fb0 + 1;

    fb0->drawEnv.r0 = r;
    fb0->drawEnv.g0 = g;
    fb0->drawEnv.b0 = b;
    fb1->drawEnv.r0 = r;
    fb1->drawEnv.g0 = g;
    fb1->drawEnv.b0 = b;
    fb0->drawEnv.isbg = 1;
    fb0[1].drawEnv.isbg = 1;
}

/* Per-frame update of the current scene. On the farm it plays the chicken notice or the
 * visitors track when the estate sets them and draws the field grid; the place's own step
 * runs after (greenhouse, milestone result or timer, sky fade). The noon and 18:00 chimes
 * play once each, then the flag, link, camera, tilt and marker updates run. */
void scene_daily_announce(void)
{
    s32 track;
    s32 i;
    FarmState *noon;
    FarmState *evening;

    switch (farm_area.mode.data->head.place.location)
    {
    case PLACE_FARM:
        if (game_state.progress.estate.estateFlags & ESTATE_FLAG_CHICKEN_NOTICE)
        {
            track = -1;
            game_state.progress.estate.estateFlags &= ~ESTATE_FLAG_CHICKEN_NOTICE;
            for (i = 0; i < 10; i++)
            {
                if (game_state.animals.chickens[i].actor.kind == ITEM_CHICKEN && game_state.animals.chickens[i].daysSick == 0)
                {
                    track = CD_TRACK_CHICKEN_NOTICE;
                    break;
                }
            }
            if (game_state.progress.estate.estateFlags & ESTATE_FLAG_VISITORS)
            {
                track = CD_TRACK_VISITORS;
            }
            if (track != -1)
            {
                cd_audio_play_track(track);
            }
        }
        field_draw_grid();
        break;
    case PLACE_2:
        if (!(game_state.progress.estate.estateFlags & ESTATE_FLAG_BIT16))
        {
            game_state.progress.estate.estateFlags |= ESTATE_FLAG_BIT16;
        }
        break;
    case PLACE_F:
        if (farm_area.mode.data->head.scene == AREA_MILESTONE_RESULT)
        {
            milestone_result_step();
        }
        else
        {
            milestone_timer_tick();
        }
        break;
    case PLACE_GREENHOUSE:
        greenhouse_draw();
        break;
    case PLACE_2C:
        scene_interp_step();
        break;
    }
    noon = &game_state;
    if (*(s32 *)&noon->progress.clock.time.hm.hour == CLOCK_NOON_HOUR && !(noon->progress.estate.estateFlags & ESTATE_FLAG_NOON_CHIME))
    {
        noon->progress.estate.estateFlags |= ESTATE_FLAG_NOON_CHIME;
        cd_audio_play_track(CD_TRACK_CHIME);
    }
    evening = &game_state;
    if (*(s32 *)&evening->progress.clock.time.hm.hour == 18 && !(evening->progress.estate.estateFlags & ESTATE_FLAG_EVENING_CHIME))
    {
        evening->progress.estate.estateFlags |= ESTATE_FLAG_EVENING_CHIME;
        cd_audio_play_track(CD_TRACK_CHIME);
    }
    scene_flag_stage_tick();
    scene_link_handback();
    scene_camera_transition();
    scene_tilt_transition();
    scene_spouse_marker_update();
}

/* Pick the scene id for a selection. Script 0x12D always gets its own id, one per
 * season. The house, coop and barn ids (HOUSE_SCENE_BASE, COOP_SCENE_BASE,
 * BARN_SCENE_BASE) move with their upgrade level and the season. The mine entrances
 * (MINE_43_SCENE_BASE, MINE_37_SCENE_BASE) override them while the layer below is not
 * open at that entrance, or while the player is returning from it. */
s32 scene_resolve_selection(s32 selection)
{
    s32 offset;

    if (event_current_script_id() == 0x12D)
    {
        return game_state.progress.clock.date.season + 0xBC;
    }
    if (selection == 0)
    {
        selection = farm_layout_index();
    }
    if (selection == HOUSE_SCENE_BASE)
    {
        offset = game_state.progress.estate.houseLevel * 4 + HOUSE_SCENE_BASE;
        selection = game_state.progress.clock.date.season + offset;
    }
    if (selection == COOP_SCENE_BASE)
    {
        selection = game_state.progress.estate.coopLevel * 4 + game_state.progress.clock.date.season + COOP_SCENE_BASE;
    }
    if (selection == BARN_SCENE_BASE)
    {
        selection = game_state.progress.estate.barnLevel * 4 + game_state.progress.clock.date.season + BARN_SCENE_BASE;
    }
    if (farm_area.mode.data->head.place.value == PLACE_MINE_43)
    {
        if (event_flag_get(EVENT_FLAG_MINE_NEXT_LAYER) == 0 && farm_area.mode.data->head.place.location == PLACE_44)
        {
            selection = game_state.progress.clock.date.season + MINE_43_SCENE_BASE;
        }
        if (event_flag_get(EVENT_FLAG_MINE_RETURN_43) == 1)
        {
            selection = game_state.progress.clock.date.season + MINE_43_SCENE_BASE;
        }
    }
    if (farm_area.mode.data->head.place.value == PLACE_MINE)
    {
        if (event_flag_get(EVENT_FLAG_MINE_NEXT_LAYER) == 0 && farm_area.mode.data->head.place.location == PLACE_38)
        {
            selection = game_state.progress.clock.date.season + MINE_37_SCENE_BASE;
        }
        if (event_flag_get(EVENT_FLAG_MINE_RETURN_37) == 1)
        {
            selection = game_state.progress.clock.date.season + MINE_37_SCENE_BASE;
        }
    }
    return clock_remap_selection((s16)selection);
}

/* Enter the picked scene: place the player at the scene's entry (when `enter` is set;
 * the mine return points override it), record the scene as entered or, when the entry
 * is absent, as the absent area, then load the scene's resources, queue its scripts,
 * register its zones and load its sprites. */
void scene_enter(s32 enter, s32 cached)
{
    FarmZone *entry;
    u16 flags;
    s16 oldResourceSet;
    s16 oldPlace;
    FarmLoadArea *far;

    oldResourceSet = farm_area.mode.script.head.resourceSet;
    oldPlace = farm_area.mode.script.head.place.value;
    farm_clear_resource_buffer();
    farm_area.mode.area = scene_resolve_selection(farm_area.mode.area);
    entry = farm_area.mode.exit;
    if (entry != 0)
    {
        flags = entry->flags;
        if (enter)
        {
            game_state.player.position.vx = entry->entry.x;
            game_state.player.position.vy = entry->entry.y;
            game_state.player.position.vz = entry->entry.z;
        }
        scene_change_area(farm_area.mode.area, flags, cached);
        if (enter)
        {
            mine_next_layer_check();
            if (event_flag_get(EVENT_FLAG_MINE_RETURN_43) == 1)
            {
                game_state.player.position.vx = MINE_RETURN_43_X;
                game_state.player.position.vy = MINE_RETURN_43_Y;
                game_state.player.position.vz = MINE_RETURN_43_Z;
                event_flag_set(EVENT_FLAG_MINE_RETURN_43, 0);
            }
            if (event_flag_get(EVENT_FLAG_MINE_RETURN_37) == 1)
            {
                game_state.player.position.vx = MINE_RETURN_37_X;
                game_state.player.position.vy = MINE_RETURN_37_Y;
                game_state.player.position.vz = MINE_RETURN_37_Z;
                event_flag_set(EVENT_FLAG_MINE_RETURN_37, 0);
            }
        }
        if (flags & SCENE_ENTRY_ABSENT)
        {
            farm_area.mode.absentArea = farm_area.mode.area;
        }
        else
        {
            farm_area.mode.enteredArea = farm_area.mode.area;
        }
    }
    else
    {
        scene_change_area(farm_area.mode.area, 0, cached);
        farm_area.mode.enteredArea = farm_area.mode.area;
        if (game_state.globalFlags & GLOBAL_FLAG_NEW_GAME)
        {
            farm_load_item_art(0);
        }
    }
    far = &farm_area;
    far->mode.place = far->mode.script.head.place;
    far->mode.data = &farm_area.mode.script;
    far->mode.previous = &farm_area.mode.previousHead;
    if (far->mode.place.location == PLACE_3)
    {
        far->mode.data->head.resourceSet = game_state.progress.clock.date.season;
    }
    if (oldResourceSet != far->mode.data->head.resourceSet)
    {
        farm_load_item_art(far->mode.data->head.resourceSet);
    }
    if (!(game_state.statusFlags & STATUS_FLAG_EVENT_HOLDS_RESOURCE_E))
    {
        if (oldPlace == 0 && far->mode.data->head.place.value != 0)
        {
            if (far->mode.data->head.resourceId != 0)
            {
                farm_load_resource_id(FARM_RESOURCE_DAY, 0);
            }
        }
        else if (farm_area.mode.eventMode == FARM_MODE_TARGET_ARRIVE && (game_state.statusFlags & STATUS_FLAG_TARGET_RELOAD))
        {
            farm_load_resource_id(FARM_RESOURCE_DAY, 0);
        }
    }
    if (farm_area.mode.eventMode != FARM_MODE_TARGET_ARRIVE && event_flag_get(EVENT_FLAG_REQUEST_BUSY) != 1)
    {
        if (farm_area.mode.data->head.resourceId != SCENE_NONE)
        {
            farm_load_resource_id(farm_area.mode.data->head.resourceId, 0);
        }
    }
    else if (farm_area.mode.eventMode == FARM_MODE_TARGET_ARRIVE && (game_state.statusFlags & STATUS_FLAG_TARGET_RELOAD))
    {
        if (farm_area.mode.data->head.resourceId != SCENE_NONE)
        {
            farm_load_resource_id(farm_area.mode.data->head.resourceId, 0);
        }
    }
    game_state.statusFlags &= ~STATUS_FLAG_TARGET_RELOAD;
    scene_mode_queue_run(farm_area.mode.data);
    if (farm_area.mode.place.value == 0)
    {
        farm_load_resource(0);
    }
    if (farm_area.mode.place.location == PLACE_5)
    {
        farm_load_resource(1);
    }
    horse_load_sprite();
    farm_item_history_resume();
    farm_zones_register_script(farm_area.mode.data);
    if (farm_area.mode.data->head.place.location == PLACE_FARM && game_state.progress.clock.minuteOfDay >= CLOCK_EVENING_MINUTE)
    {
        farm_load_framebuffer_header();
        event_flag_set(EVENT_FLAG_EVENING_18H, 1);
    }
    player_load_winter_anim();
}

void scene_entries_clear(void)
{
    FarmSceneModel *p = scene_entries;
    s32 i = SCENE_MODEL_COUNT - 1;
    do
    {
        scene_entry_reset(p);
        i--;
        p++;
    } while (i >= 0);
}

/* The first free entry (flagged absent), or 0 when every entry is in use. */
static FarmSceneModel *scene_entry_alloc(void)
{
    FarmSceneModel *p = scene_entries;
    s32 i = 0;
    do
    {
        i++;
        if (!(p->flags & SCENE_ENTRY_ABSENT))
        {
            p++;
            if (i < SCENE_MODEL_COUNT)
                continue;
            return 0;
        }
        return p;
    } while (1);
}

static void scene_entry_reset(FarmSceneModel *p)
{
    p->position.vx = 0;
    p->position.vy = 0;
    p->position.vz = 0;
    p->angle.vx = 0;
    p->angle.vy = 0;
    p->angle.vz = 0;
    p->turn.vx = 0;
    p->turn.vy = 0;
    p->turn.vz = 0;
    p->flags = SCENE_ENTRY_ABSENT;
    memset(&p->model, 0, sizeof(Model));
}

/* Create the scene entries from the zone's model list, skipping absent ones, then set up
 * their slots, links, pan and tilt objects and marked entries. */
void scene_populate_objects(void)
{
    s32 count;
    char *strings;
    FarmZoneModel *source;
    FarmSceneModel *dest;
    u16 flags;

    count = SCENE_MODEL_COUNT - 1;
    source = farm_area.mode.data->models;
    strings = farm_area.sectionBuffer + SECTION_STRINGS_OFFSET;
    do
    {
        if (!(source->flags & SCENE_ENTRY_ABSENT))
        {
            dest = scene_entry_alloc();
            if (dest != 0)
            {
                dest->position = source->position;
                dest->angle = source->angle;
                dest->spin = source->spin;
                model_bind_tmd(&dest->model, strings + source->name);
                flags = source->flags;
                dest->flags = flags;
                if (flags & SCENE_ENTRY_MORPH)
                {
                    dest->morph = morph_claim(
                        strings + source->name, strings + source->link.frames[0],
                        strings + source->link.frames[1], &dest->position);
                    dest->morph->step = 0;
                    dest->morph->steps = source->steps;
                }
            }
        }
        count--;
        source++;
    } while (count >= 0);
    scene_assign_slots();
    scene_link_slot1_reset();
    scene_pan_object_find();
    scene_tilt_object_find();
    scene_slot_flag_move();
}

void scene_entry_set_position(s16 index, const u16 *src)
{
    FarmSceneModel *base = scene_entries;
    FarmSceneModel *entry = &base[index];

    entry->position.vx = src[0];
    entry->position.vy = src[1];
    entry->position.vz = src[2];
}

void scene_entry_set_turn(s16 index, const s16 *src)
{
    FarmSceneModel *base = scene_entries;
    FarmSceneModel *entry = &base[index];

    entry->turn.vx = src[0];
    entry->turn.vy = src[1];
    entry->turn.vz = src[2];
}

/* Draw the scene entries that are neither absent nor hidden, each placed at its
 * position and rotated by its base angle plus its turn. Entries that are not links
 * are skipped when they project off screen. A morphing entry draws its morph's
 * blended model. Spinning entries turn by their spin each frame while the farm is not
 * paused. */
void scene_spin_entries(void)
{
    FarmSceneModel *entry;
    Model *model;
    u16 flags;
    MATRIX m;
    VECTOR position;
    SVECTOR angle;
    s32 i;

    entry = scene_entries;
    for (i = 0; i < SCENE_MODEL_COUNT; i++, entry++)
    {
        flags = entry->flags;
        if (flags & SCENE_ENTRY_ABSENT)
        {
            continue;
        }
        if (flags & SCENE_ENTRY_HIDDEN)
        {
            continue;
        }
        model = &entry->model;
        if (flags & SCENE_ENTRY_MORPH)
        {
            model = &entry->morph->links[2];
            morph_bind_models(entry->morph);
        }
        PushMatrix();
        model_save_gte_rot();
        m = GsIDMATRIX;
        if (!(entry->flags & SCENE_ENTRY_LINK))
        {
            scene_project_vertex.vx = entry->position.vx;
            scene_project_vertex.vy = entry->position.vy;
            scene_project_vertex.vz = entry->position.vz;
            gte_project_vertex(&scene_projected_xy, &scene_project_vertex);
            if ((u32)(scene_projected_xy.x - SCREEN_BOX_LEFT) >= SCREEN_BOX_WIDTH || scene_projected_xy.y >= SCREEN_BOX_BOTTOM || scene_projected_xy.y < SCREEN_BOX_TOP)
            {
                goto pop;
            }
        }
        position.vx = entry->position.vx;
        position.vy = entry->position.vy;
        position.vz = entry->position.vz;
        TransMatrix(&m, &position);
        angle.vx = entry->angle.vx + entry->turn.vx;
        angle.vy = entry->angle.vy + entry->turn.vy;
        angle.vz = entry->angle.vz + entry->turn.vz;
        RotMatrixZXY(&angle, &m);
        model_compose_gte_matrix(&m);
        model_draw(model);
    pop:
        PopMatrix();
        if (!(game_state.globalFlags & GLOBAL_FLAG_PAUSED) && (entry->flags & SCENE_ENTRY_SPIN))
        {
            entry->angle.vx += entry->spin.vx;
            entry->angle.vy += entry->spin.vy;
            entry->angle.vz += entry->spin.vz;
        }
    }
}

static void morph_reset(FarmMorph *item)
{
    memset(item, 0, sizeof(FarmMorph));
    item->flags = MORPH_FLAG_FREE;
}

void scene_morphs_clear(void)
{
    FarmMorph *p = scene_morphs;
    s32 i = SCENE_MORPH_COUNT - 1;
    do
    {
        morph_reset(p);
        i--;
        p++;
    } while (i >= 0);
}

/* Take a free morph, bind its three models and place it at `position`; 0 when none is free. */
static FarmMorph *morph_claim(char *model, char *from, char *to, SVECTOR *position)
{
    FarmMorph *item;
    s32 i;

    item = scene_morphs;
    for (i = 0; i < SCENE_MORPH_COUNT; i++, item++)
    {
        if (!(item->flags & MORPH_FLAG_FREE))
            continue;
        morph_reset(item);
        item->flags &= ~MORPH_FLAG_FREE;
        model_bind_tmd(&item->links[0], model);
        model_bind_tmd(&item->links[1], from);
        model_bind_tmd(&item->links[2], to);
        item->x = position->vx;
        item->y = position->vy;
        item->z = position->vz;
        return item;
    }
    return 0;
}

/* The vertices of a TMD's first object. */
#define TMD_VERTICES(tmd) ((SVECTOR *)((u8 *)(tmd) + ((tmd)->objects[0].vert_top + TMD_HEADER_SIZE)))

/* Point the morph's vertex arrays at its three models' vertices and blend. */
static void morph_bind_models(FarmMorph *morph)
{
    TmdHeader *tmd;

    tmd = morph->links[0].tmd;
    morph->from = TMD_VERTICES(tmd);
    tmd = morph->links[1].tmd;
    morph->to = TMD_VERTICES(tmd);
    tmd = morph->links[2].tmd;
    morph->out = TMD_VERTICES(tmd);
    morph_blend_vertices(tmd->objects[0].n_vert, morph);
}

/* Blend count vertices between two key frames:
 * out = from + (to - from) * step / steps. */
static void morph_blend_vertices(s32 count, FarmMorph *morph)
{
    s32 step = morph->step;
    s32 steps = morph->steps;
    s32 i;
    SVECTOR delta;

    for (i = 0; i < count; i++)
    {
        delta.vx = morph->to[i].vx - morph->from[i].vx;
        delta.vy = morph->to[i].vy - morph->from[i].vy;
        delta.vz = morph->to[i].vz - morph->from[i].vz;
        morph->out[i].vx = delta.vx * step / steps + morph->from[i].vx;
        morph->out[i].vy = delta.vy * step / steps + morph->from[i].vy;
        morph->out[i].vz = delta.vz * step / steps + morph->from[i].vz;
    }
}

/* Return 1 when the area's place differs from the (value, location) pair `p`, else 0. */
s32 scene_place_mismatch(s16 *p)
{
    FarmZoneHeader *q = &farm_area.mode.data->head;
    if (q->place.value == p[0])
    {
        if (q->place.location == p[1])
            return 0;
    }
    return 1;
}

void farm_copy_words(u32 *dst, const u32 *src, u32 count)
{
    u32 i = 0;
    if (count != 0)
    {
        do
        {
            *dst = *src;
            src++;
            i++;
            dst++;
        } while (i < count);
    }
}

void farm_copy_halfwords(u16 *dst, const u16 *src, u32 count)
{
    u32 i = 0;
    if (count != 0)
    {
        do
        {
            *dst++ = *src++;
            i++;
        } while (i < count);
    }
}

static s16 house_level_offsets[4] = {
    0,
    8,
    16,
    0,
};

static s16 barn_layout_offsets[4] = {
    0,
    4,
    8,
    12,
};

static s16 coop_layout_offsets[4] = {
    0,
    4,
    8,
    0,
};

static s16 greenhouse_level_offsets[2] = {
    0,
    4,
};

s32 farm_layout_index(void)
{
    s32 total;
    s16 base;

    total = 0;
    if (game_state.progress.estate.houseLevel == 0)
    {
        base = house_level_offsets[game_state.progress.estate.houseLevel];
        if (game_state.progress.estate.coopLevel == 0)
        {
            total = base + coop_layout_offsets[0];
        }
        else
        {
            total = base + coop_layout_offsets[1];
        }
    }
    if (game_state.progress.estate.houseLevel == 1)
    {
        base = house_level_offsets[game_state.progress.estate.houseLevel];
        if (game_state.progress.estate.barnLevel == 0)
        {
            total = base + barn_layout_offsets[0];
        }
        else
        {
            total = base + barn_layout_offsets[1];
        }
    }
    if (game_state.progress.estate.houseLevel == 2)
    {
        base = house_level_offsets[game_state.progress.estate.houseLevel];
        total = base + greenhouse_level_offsets[game_state.progress.estate.greenhouseLevel];
    }
    return total + game_state.progress.clock.date.season;
}

static int scene_stub_2(void)
{
    return 0;
}

/* Assign each slotted entry to its slot by the low nibble of its flags: slot 0 is shown
 * with a limit of 8, slots 1 and 2 are hidden with limits of 4 and 12. Each entry's
 * counter restarts at 0 against its limit. */
static void scene_assign_slots(void)
{
    s32 i;
    FarmSceneModel *entry;
    s32 limit;
    s32 slot;

    entry = scene_entries;
    scene_slot_entries[0] = 0;
    scene_slot_entries[1] = 0;
    scene_slot_entries[2] = 0;
    scene_flag_stage = SCENE_FLAG_WAIT_SHIPPED;
    for (i = 0; i < SCENE_MODEL_COUNT; i++, entry++)
    {
        if (entry->flags & SCENE_ENTRY_SLOTTED)
        {
            switch (entry->flags & SCENE_ENTRY_SLOT_MASK)
            {
            case SCENE_SLOT_0:
                entry->flags &= ~SCENE_ENTRY_HIDDEN;
                limit = 8;
                slot = SCENE_SLOT_0;
                break;
            case SCENE_SLOT_1:
                entry->flags |= SCENE_ENTRY_HIDDEN;
                limit = 4;
                slot = SCENE_SLOT_1;
                break;
            case SCENE_SLOT_2:
                entry->flags |= SCENE_ENTRY_HIDDEN;
                limit = 12;
                slot = SCENE_SLOT_2;
                break;
            default:
                limit = 0;
                slot = SCENE_SLOT_0;
                break;
            }
            entry->morph->step = 0;
            entry->morph->steps = limit;
            scene_slot_entries[slot] = entry;
        }
    }
}

/* Pass the hidden flag around the three slot entries. The first stage waits until
 * the item is shipped; then each slot counts up to its limit and hands the flag on.
 * The last stage clears the shipped flag when it completes. */
static void scene_flag_stage_tick(void)
{
    /* Count one step on `owner`'s counter; when it reaches its limit, reset both
     * counters, hide `owner`, show `other`, record `nextStage` as the next stage and
     * return 1. */
    s32 scene_slot_handoff_step(FarmSceneModel * owner, FarmSceneModel * other, s32 nextStage)
    {
        FarmMorph *counter;

        owner->morph->step++;
        counter = owner->morph;
        if (counter->step < counter->steps)
        {
            return 0;
        }
        other->morph->step = 0;
        counter->step = 0;
        owner->flags |= SCENE_ENTRY_HIDDEN;
        other->flags &= ~SCENE_ENTRY_HIDDEN;
        scene_flag_stage = nextStage;
        return 1;
    }

    FarmSceneModel *slot0 = scene_slot_entries[0];
    u32 stage;

    if (slot0 == 0)
    {
        return;
    }
    stage = scene_flag_stage;
    switch (stage)
    {
    case SCENE_FLAG_WAIT_SHIPPED:
        if (event_flag_get(EVENT_FLAG_ITEM_SHIPPED) == 1)
        {
            scene_flag_stage = SCENE_FLAG_COUNT_SLOT0;
        }
        break;
    case SCENE_FLAG_COUNT_SLOT0:
        scene_slot_handoff_step(slot0, scene_slot_entries[1], SCENE_FLAG_COUNT_SLOT1);
        break;
    case SCENE_FLAG_COUNT_SLOT1:
        scene_slot_handoff_step(scene_slot_entries[1], scene_slot_entries[2], SCENE_FLAG_COUNT_SLOT2);
        break;
    case SCENE_FLAG_COUNT_SLOT2:
        if (scene_slot_handoff_step(scene_slot_entries[2], slot0, SCENE_FLAG_WAIT_SHIPPED) == 1)
        {
            event_flag_set(EVENT_FLAG_ITEM_SHIPPED, 0);
        }
        break;
    }
}

/* Link the link entries: slot 0 is shown with a limit of 8, slot 1 is hidden with a
 * limit of 12; any other nibble takes slot 0 with limit 0. When sleeping, go straight
 * to the return state: hide slot 0's entry, show slot 1's, and set slot 1's count to
 * its limit. */
static void scene_link_slot1_reset(void)
{
    s32 i;
    FarmSceneModel *entry;
    u16 flags;
    s32 slot;
    s32 steps;

    scene_handback_state = SCENE_HANDBACK_WAIT;
    scene_link_entries[0] = 0;
    scene_link_entries[1] = 0;
    for (i = 0, entry = scene_entries; i < SCENE_MODEL_COUNT; i++, entry++)
    {
        flags = entry->flags;
        if (flags & SCENE_ENTRY_LINK)
        {
            switch (flags & SCENE_ENTRY_SLOT_MASK)
            {
            case SCENE_SLOT_0:
                steps = 8;
                entry->flags = flags & ~SCENE_ENTRY_HIDDEN;
                slot = SCENE_SLOT_0;
                break;
            case SCENE_SLOT_1:
                entry->flags = flags | SCENE_ENTRY_HIDDEN;
                steps = 12;
                slot = SCENE_SLOT_1;
                break;
            default:
                slot = SCENE_SLOT_0;
                steps = 0;
                break;
            }
            entry->morph->step = 0;
            entry->morph->steps = steps;
            scene_link_entries[slot] = entry;
        }
    }
    event_flag_set(EVENT_FLAG_09, 0);
    if (event_flag_get(EVENT_FLAG_SLEEPING) == 1)
    {
        scene_handback_state = SCENE_HANDBACK_RETURN;
        scene_link_entries[0]->flags |= SCENE_ENTRY_HIDDEN;
        scene_link_entries[1]->flags &= ~SCENE_ENTRY_HIDDEN;
        scene_link_entries[1]->morph->step =
            scene_link_entries[1]->morph->steps;
    }
}

/* Step the handback between the two link entries. WAIT waits for event flag 09.
 * COUNT_FIRST counts the first entry up to its limit; then on flag 09 it resets both
 * counts, hides the first entry, shows the second and clears flag 09. Once sleeping
 * is set it moves on. COUNT_SECOND counts the second entry up to its limit. RETURN
 * swaps the hidden flag back when sleeping ends, then returns to WAIT. */
static void scene_link_handback(void)
{
    FarmSceneModel *entry;
    FarmSceneModel *other;
    FarmMorph *link;

    if (scene_link_entries[0] == 0)
    {
        return;
    }
    switch (scene_handback_state)
    {
    case SCENE_HANDBACK_WAIT:
        if (event_flag_get(EVENT_FLAG_09) == 1)
        {
            scene_handback_state = SCENE_HANDBACK_COUNT_FIRST;
        }
        break;
    case SCENE_HANDBACK_COUNT_FIRST:
        entry = scene_link_entries[0];
        link = entry->morph;
        other = scene_link_entries[1];
        if (link->step >= link->steps)
        {
            if (event_flag_get(EVENT_FLAG_09) == 1)
            {
                entry->morph->step = other->morph->step = 0;
                entry->flags |= SCENE_ENTRY_HIDDEN;
                other->flags &= ~SCENE_ENTRY_HIDDEN;
                event_flag_set(EVENT_FLAG_09, 0);
            }
            if (event_flag_get(EVENT_FLAG_SLEEPING) == 1)
            {
                scene_handback_state = SCENE_HANDBACK_COUNT_SECOND;
            }
        }
        else
        {
            link->step++;
        }
        break;
    case SCENE_HANDBACK_COUNT_SECOND:
        entry = scene_link_entries[1];
        entry->morph->step++;
        link = entry->morph;
        if (link->step >= link->steps)
        {
            link->step = link->steps;
            scene_handback_state = SCENE_HANDBACK_RETURN;
        }
        break;
    case SCENE_HANDBACK_RETURN:
        if (event_flag_get(EVENT_FLAG_SLEEPING) == 0)
        {
            entry = scene_link_entries[0];
            other = scene_link_entries[1];
            entry->flags &= ~SCENE_ENTRY_HIDDEN;
            other->flags |= SCENE_ENTRY_HIDDEN;
            entry->morph->step = other->morph->step = 0;
            scene_handback_state = SCENE_HANDBACK_WAIT;
        }
        break;
    }
}

/* Find the first entry with the pan flag, zero its angle and keep it as the pan object. */
static void scene_pan_object_find(void)
{
    s32 i;
    FarmSceneModel *p;
    scene_pan_state = 0;
    i = 0;
    p = scene_entries;
    do
    {
        i++;
        if (p->flags & SCENE_ENTRY_PAN)
        {
            p->angle.vx = 0;
            p->angle.vy = 0;
            p->angle.vz = 0;
            scene_pan_object = p;
            return;
        }
        p++;
    } while (i < SCENE_MODEL_COUNT);
    scene_pan_object = 0;
}

/* Pan the camera: when EVENT_FLAG_ITEM_RESULT_STATE_6 is set, the pan object's vz rises
 * to 640, holds until the flag clears, and falls back to 0. */
static void scene_camera_transition(void)
{
    FarmSceneModel *object;
    u32 *state;
    u32 mode;
    s32 result;
    object = scene_pan_object;
    if (object == 0)
        return;
    state = &scene_pan_state;
    mode = *state;
    switch (mode)
    {
    case PAN_IDLE:
        result = event_flag_get(EVENT_FLAG_ITEM_RESULT_STATE_6);
        if (result == 1)
        {
            *state = PAN_OUT;
        }
        break;
    case PAN_OUT:
        result = object->angle.vz + 0x20;
        object->angle.vz = result;
        if ((s16)result >= 0x280)
        {
            result = 0x280;
            mode = PAN_HELD;
            object->angle.vz = result;
            *state = mode;
        }
        break;
    case PAN_HELD:
        result = event_flag_get(EVENT_FLAG_ITEM_RESULT_STATE_6);
        if (result == 0)
        {
            *state = PAN_BACK;
        }
        break;
    case PAN_BACK:
        result = object->angle.vz - 0x20;
        object->angle.vz = result;
        if ((result << 16) <= 0)
        {
            object->angle.vz = 0;
            *state = PAN_IDLE;
        }
        break;
    }
}

/* 1 while the camera pan is held at its far point. */
int scene_pan_is_held(void)
{
    return scene_pan_state == PAN_HELD;
}

/* Find the first entry with the tilt flag, zero its angle and keep it as the tilt object. */
static void scene_tilt_object_find(void)
{
    FarmSceneModel *p;
    s32 i;
    scene_tilt_state = 0;
    i = 0;
    p = scene_entries;
    do
    {
        if (p->flags & SCENE_ENTRY_TILT)
        {
            p->angle.vx = 0;
            p->angle.vy = 0;
            p->angle.vz = 0;
            scene_tilt_object = p;
            return;
        }
        i++;
        p++;
    } while (i < SCENE_MODEL_COUNT);
    scene_tilt_object = 0;
}

/* Tilt the camera: when flag EVENT_FLAG_ITEM_RESULT_STATE_7 is set, the tilt object's vy offset
 * falls to -448, holds until the flag clears, and rises back to 0. */
static void scene_tilt_transition(void)
{
    FarmSceneModel *object;
    u32 mode;
    s32 result;
    object = scene_tilt_object;
    if (object == 0)
        return;
    mode = scene_tilt_state;
    switch (mode)
    {
    case PAN_IDLE:
        result = event_flag_get(EVENT_FLAG_ITEM_RESULT_STATE_7);
        if (result == 1)
        {
            scene_tilt_state = PAN_OUT;
        }
        break;
    case PAN_OUT:
        result = object->angle.vy - 0x20;
        object->angle.vy = result;
        if ((s16)result < -0x1BF)
        {
            result = -0x1C0;
            mode = PAN_HELD;
            object->angle.vy = result;
            scene_tilt_state = mode;
        }
        break;
    case PAN_HELD:
        result = event_flag_get(EVENT_FLAG_ITEM_RESULT_STATE_7);
        if (result == 0)
        {
            scene_tilt_state = PAN_BACK;
        }
        break;
    case PAN_BACK:
        result = object->angle.vy + 0x20;
        object->angle.vy = result;
        if ((result << 16) >= 0)
        {
            object->angle.vy = 0;
            scene_tilt_state = PAN_IDLE;
        }
        break;
    }
}

/* 1 while the camera tilt is held at its far point. */
int scene_tilt_is_held(void)
{
    return scene_tilt_state == PAN_HELD;
}

static s16 scene_interp_reversed = 0;

static s16 scene_interp_pos = 0;

/* Start the sky fade at its last step, with the clear colour at the fade's end colour. */
static void scene_interp_start(void)
{
    scene_interp_reversed = 0;
    scene_interp_pos = SKY_FADE_STEPS - 1;
    frame_set_clear_color(0, 0xA5, 0xD5);
}

static s32 interp_color_start[3] = {0, 0x0063, 0x007F};

static s32 interp_color_end[3] = {0, 165, 213};

/* Step the sky colour fade, bouncing back and forth between its ends. */
static void scene_interp_step(void)
{
    s32 values[3];

    math_lerp_vec3(scene_interp_pos, SKY_FADE_STEPS - 1, interp_color_start, interp_color_end, values);
    frame_set_clear_color(values[0], values[1], values[2]);
    if (scene_interp_reversed == 0)
    {
        scene_interp_pos++;
        if (scene_interp_pos >= SKY_FADE_STEPS)
        {
            scene_interp_reversed = 1;
            scene_interp_pos = SKY_FADE_STEPS - 1;
        }
    }
    else
    {
        scene_interp_pos--;
        if (scene_interp_pos < 0)
        {
            scene_interp_reversed = 0;
            scene_interp_pos = 0;
        }
    }
}

/* Find the marked entries. The low bit of an entry's flags says which of the two it
 * is; stop at the first slot 1 entry. Show the first and hide the second. */
static void scene_slot_flag_move(void)
{
    s32 i;
    FarmSceneModel *entry;
    u16 flags;
    s32 slot;

    scene_marked_entries[0] = 0;
    scene_marked_entries[1] = 0;
    for (i = 0, entry = scene_entries; i < SCENE_MODEL_COUNT; i++, entry++)
    {
        flags = entry->flags;
        if (flags & SCENE_ENTRY_MARKED)
        {
            slot = flags & 1;
            scene_marked_entries[slot] = entry;
            if (slot)
            {
                break;
            }
        }
    }
    scene_marked_entries[0]->flags &= ~SCENE_ENTRY_HIDDEN;
    scene_marked_entries[1]->flags |= SCENE_ENTRY_HIDDEN;
}

/* Show the marked entry for the spouse's place: entry 1 when the spouse is at home,
 * entry 0 otherwise. */
static void scene_spouse_marker_update(void)
{
    if (scene_marked_entries[0] != 0)
    {
        if (event_flag_get(EVENT_FLAG_SPOUSE_AT_HOME) == 1)
        {
            scene_marked_entries[1]->flags &= ~SCENE_ENTRY_HIDDEN;
            scene_marked_entries[0]->flags |= SCENE_ENTRY_HIDDEN;
        }
        else
        {
            scene_marked_entries[0]->flags &= ~SCENE_ENTRY_HIDDEN;
            scene_marked_entries[1]->flags |= SCENE_ENTRY_HIDDEN;
        }
    }
}
