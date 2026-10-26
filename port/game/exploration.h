#ifndef HMBTN_EXPLORATION_H
#define HMBTN_EXPLORATION_H
#include "session.h"
/* Extracted from scene_preview.c at d3c0f38 + MA-04/05 working tree.
 * Algorithms remain the current adaptations; original references:
 * 11_GF_FARM/{farm_main.c,people/player.c,field/field.c,world/clock.c}.
 * No SDL, files, renderer, host voice handles or host clock in this interface. */
typedef enum HmbtnExplorationAnimation {
    HMBTN_EXP_PLAYER,HMBTN_EXP_ACTION,HMBTN_EXP_EVENT_PLAYER,
    HMBTN_EXP_VILLAGER,HMBTN_EXP_CHICK,HMBTN_EXP_HEN
} HmbtnExplorationAnimation;
typedef enum HmbtnExplorationEffectKind {
    HMBTN_EXP_UI_SOUND,HMBTN_EXP_KEY_SOUND,HMBTN_EXP_RELEASE_WATER,
    HMBTN_EXP_RETEXTURE,HMBTN_EXP_MESSAGE_ART,HMBTN_EXP_STEREO,
    HMBTN_EXP_HOUSE_ART,HMBTN_EXP_UNSUPPORTED_HOUSE,HMBTN_EXP_MESSAGE_LOADED
} HmbtnExplorationEffectKind;
typedef struct HmbtnExplorationEffect {
    HmbtnExplorationEffectKind kind;
    unsigned id,bank,program,tone,note,volume;
    bool water_loop;
    const uint8_t *record;size_t bytes; /* Synchronous read-only presentation loan. */
} HmbtnExplorationEffect;
typedef enum HmbtnExplorationBoundary {
    HMBTN_EXP_TALK,HMBTN_EXP_START_TALK,HMBTN_EXP_START_VISIT,
    HMBTN_EXP_TRANSITION,HMBTN_EXP_DOOR,HMBTN_EXP_SLEEP
} HmbtnExplorationBoundary;
typedef struct HmbtnExplorationBoundaryResult { bool opened,consumed; } HmbtnExplorationBoundaryResult;
typedef struct HmbtnExplorationFrame {
    const uint8_t *section;size_t section_size;
    const HmbtnModel *ground;
    HmbtnSceneData *scene; /* Optional decoded door-model presentation loan. */
    const HmbtnAnimation *field_art; /* Optional visual field effects, seven entries. */
    const HmbtnAnimation *grass_cut_art; /* Seasonal estate sprite, borrowed. */
    void *context;
    void *boundary_context;
    /* Resource preparation is synchronous for the existing free-world host.
     * Failure/pending must return false, never pretend resources are installed.
     * Resumable scripted loads remain on the MA-05 service protocol. */
    bool (*action_bank)(void *,unsigned,char *,size_t);
    bool (*text)(void *,unsigned,unsigned,uint8_t *,size_t,char *,size_t);
    bool (*village_assets)(void *,unsigned,const HmbtnFarmClock *,char *,size_t);
    const HmbtnAnimation *(*animation)(void *,HmbtnExplorationAnimation,unsigned);
    /* Pure presentation may be omitted by a headless host; no gameplay result
     * depends on sound, texture upload or drawing. Resource/ANI loans may not. */
    bool (*effect)(void *,const HmbtnExplorationEffect *,char *,size_t);
    /* Existing MA-07 paths, explicitly delegated; absence is an error only
     * when one of these boundaries is actually reached. No fake transitions. */
    bool (*boundary)(void *,HmbtnSession *,HmbtnExplorationBoundary,unsigned,
        HmbtnLogicalInput,HmbtnExplorationBoundaryResult *,char *,size_t);
} HmbtnExplorationFrame;
/* One logical tick; caller schedules the original ~29.97 Hz cadence and
 * supplies decoded ground/ANI regardless of whether a frame is presented.
 * This complete entry is for free exploration; scripted ticks are MA-07. */
bool hmbtn_session_exploration_tick(HmbtnSession *,const HmbtnExplorationFrame *,HmbtnLogicalInput,char *,size_t);
/* Same phases used by the current coordinator at its original interleaving
 * points until MA-07. These are not alternate gameplay implementations. */
bool hmbtn_session_exploration_controls(HmbtnSession *,const HmbtnExplorationFrame *,HmbtnLogicalInput,char *,size_t);
bool hmbtn_session_exploration_world(HmbtnSession *,const HmbtnExplorationFrame *,unsigned *,bool *,char *,size_t);
bool hmbtn_session_exploration_animate(HmbtnSession *,const HmbtnExplorationFrame *,char *,size_t);
bool hmbtn_actor_animation_tick(const HmbtnAnimation *,HmbtnIntroActor *,char *,size_t);
#endif
