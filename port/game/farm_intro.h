#ifndef HMBTN_FARM_INTRO_H
#define HMBTN_FARM_INTRO_H
#include "event_script.h"
#include "asset_decode.h"
#include "model_decode.h"

typedef struct HmbtnIntroActor {
    int32_t position[3], velocity[3];
    HmbtnAnimationPlayer sprite;
    unsigned facing;
    bool visible, pose, animation_done;
    unsigned marker; /* 0 normal, 1 hidden, 2 alternate, as villager place flags. */
} HmbtnIntroActor;
typedef struct HmbtnFarmIntro {
    HmbtnEventScript script;
    HmbtnIntroActor player, mayor;
    HmbtnIntroActor childhood[4], boy, girl; /* Event actors, not NPC gameplay state. */
    HmbtnIntroActor ellen, zack;
    HmbtnIntroActor elli,stu;
    HmbtnIntroActor carter,barley,rick;
    int32_t slots[32];
    unsigned message, message_bank, target_area;
    int message_result_slot; /* Original show-message result destination, -1 discards. */
    unsigned area, event, queued_event, camera_actor;
    int32_t camera[3];
    bool area_pending, event_pending, camera_gliding;
    bool finish_requested; /* Original FARM_REQUEST_FINISH. */
    bool player_free; /* A waiting visitor yields player animation/movement to house_control. */
    int32_t camera_height, camera_distance, camera_from[3];
    unsigned xa_track;
    bool xa_pending, xa_started, xa_active, event_active;
    unsigned cell_actor;
    bool message_done;
    unsigned fade_mode, fade_level, fade_length;
    bool fade_white;
    unsigned music_volume, music_from, music_to, music_time, music_length;
    bool stop_music;
    unsigned music_mode;
    bool song_pending, start_music;
    bool action_pending, action_ready, action_active, action_finished;
    unsigned text_bank;
    unsigned advanced_minutes;
    bool zone_exited, player_marker;
    const HmbtnModel *ground; /* Borrowed native model, updated by the scene loader. */
    bool visit_approach;
    /* Game-state commands supplied by the scene; unsupported commands still stop. */
    HmbtnEventHandler game_command;
    void *game_context;
} HmbtnFarmIntro;

bool hmbtn_farm_intro_open(HmbtnFarmIntro *intro, const uint8_t *script, size_t size,
    char *error, size_t error_size);
bool hmbtn_farm_intro_tick(HmbtnFarmIntro *intro, char *error, size_t error_size);
unsigned hmbtn_farm_intro_brightness(const HmbtnFarmIntro *intro);
HmbtnIntroActor *hmbtn_farm_intro_actor(HmbtnFarmIntro *intro, int32_t id);
#endif
