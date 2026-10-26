#ifndef HMBTN_RUNTIME_H
#define HMBTN_RUNTIME_H
#include "exploration.h"
#include "scene_decode.h"
/* Session coordinator. Resource/model loans are valid for one tick and refreshed
 * after a load. No SDL, files, audio voices or renderer are owned by gameplay. */
typedef enum HmbtnRuntimeEffectKind {
    HMBTN_RUNTIME_SOUND,HMBTN_RUNTIME_KEY_SOUND,HMBTN_RUNTIME_START_MUSIC,
    HMBTN_RUNTIME_STOP_MUSIC,HMBTN_RUNTIME_VOLUME,HMBTN_RUNTIME_XA_TICK,
    HMBTN_RUNTIME_AUDIO_POLL,HMBTN_RUNTIME_MESSAGE_ART,HMBTN_RUNTIME_PORTRAIT,
    HMBTN_RUNTIME_TV_CLEAR,HMBTN_RUNTIME_READY
} HmbtnRuntimeEffectKind;
typedef struct HmbtnRuntimeEffect {
    HmbtnRuntimeEffectKind kind;
    unsigned id,bank,program,tone,note,volume;
    const void *data;size_t bytes;
} HmbtnRuntimeEffect;
typedef struct HmbtnSessionFrame {
    HmbtnSession *session;
    HmbtnLogicalInput input;
    const uint8_t *section;
    HmbtnSceneData *scene; /* Decoded animation/door/morph state; no renderer. */
    HmbtnExplorationFrame exploration;
    void *context;
    const uint8_t *schedules;size_t schedule_size;
    char *error;size_t error_size;
    bool (*area)(struct HmbtnSessionFrame *,unsigned,char *,size_t);
    bool (*event)(struct HmbtnSessionFrame *,unsigned,char *,size_t);
    bool (*song)(void *,unsigned,bool,char *,size_t);
    bool (*effect)(struct HmbtnSessionFrame *,const HmbtnRuntimeEffect *,char *,size_t);
    const HmbtnAnimation *(*animation)(void *,unsigned,bool);
    const HmbtnServiceResult *(*resource)(void *,HmbtnServiceKind,unsigned,unsigned,char *,size_t);
    const HmbtnServiceResult *(*audio)(void *,HmbtnServiceKind,unsigned,char *,size_t);
    bool (*read)(void *,HmbtnServiceKind,unsigned,unsigned,uint8_t *,size_t,uint32_t *,char *,size_t);
    bool (*conversation)(void *,unsigned,uint8_t *,size_t,size_t *,char *,size_t);
} HmbtnSessionFrame;
/* Resource handoff: preserves the current area-entry rules in game/, while
 * resource providers retain ownership of all buffers and renderer caches. */
void hmbtn_session_area_begin(HmbtnSession *,unsigned,unsigned,bool);
void hmbtn_session_area_ready(HmbtnSession *,unsigned,const HmbtnSceneData *);
void hmbtn_session_runtime_rebind(HmbtnSession *session);
HmbtnEventResult hmbtn_session_visit_command(HmbtnSession *,HmbtnSessionFrame *,const HmbtnEventCommand *,char *,size_t);
/* Exactly one logical tick. No composition, upload or presentation is required. */
bool hmbtn_session_tick(HmbtnSession *session,HmbtnSessionFrame *frame,HmbtnLogicalInput input,char *error,size_t error_size);
/* Explicit entry points for resource-bound hosts and test fixtures. */
bool hmbtn_session_start_visit(HmbtnSession *,HmbtnSessionFrame *,unsigned,char *,size_t);
bool hmbtn_session_prepare_morning(HmbtnSession *,HmbtnSessionFrame *,bool,char *,size_t);
#endif
