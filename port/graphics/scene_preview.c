/* Farm scenery, arrival and the first childhood events. Geometry-only mode
 * keeps its diagnostic camera; New Game follows the original scripts.
 * Source: farm_load.c/farm_section_table.h, scene_view.c and event_script.c. */
#include "scene_preview.h"
#include "model_render.h"
#include "scene_decode.h"
#include "psx_data.h"
#include "resource_file.h"
#include "pad_buttons.h"
#include "farm_intro.h"
#include "house_control.h"
#include "house_sleep.h"
#include "house_ui.h"
#include "new_game_ui.h"
#include "title_audio.h"
#include "xa_stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_layouts.inc"

enum { FRAME_BYTES = 320 * 240 * 4 };

struct HmbtnScenePreview {
    uint8_t *section, *pixels;
    HmbtnImageAtlas *atlas;
    HmbtnSceneData scene;
    HmbtnModelRenderer *geometry;
    SDL_Texture *texture;
    int32_t centre_x, centre_z;
    Uint64 accumulated_ns;
    bool dirty;
    bool opening, completion_reported;
    char *data_root;
    uint8_t *player_block, *event_block;
    HmbtnImageAtlas *player_atlas, *mayor_atlas;
    HmbtnAnimation player_animation, actor_animation[35];
    unsigned area;
    HmbtnFarmIntro intro;
    HmbtnHouseControl control;
    HmbtnHouseInteraction house;
    HmbtnHouseSleep sleep;
    bool sleep_reported;
    HmbtnHouseUI *house_ui;
    unsigned house_music_ticks, tv_frame;
    uint8_t tv_sectors[0x1800];
    HmbtnNewGame dialogue;
    HmbtnNewGameUI *ui;
    HmbtnTitleAudio audio;
    HmbtnXAStream xa;
    unsigned audio_song;
    uint32_t pending_pressed, ticks;
    unsigned loaded_message, loaded_cell;
    uint8_t *field_block;
    HmbtnImageAtlas *field_atlas;
    HmbtnAnimation field_art[7];
    unsigned field_layout;
    uint8_t *action_block;
    HmbtnImageAtlas *action_atlas;
    HmbtnAnimation action_animation;
};

static void reset_camera(HmbtnScenePreview *s)
{
    /* Development viewpoint near the farmhouse, not the event 0x74 camera. */
    s->centre_x = -4800;
    s->centre_z = 3200;
    s->dirty = true;
}

void hmbtn_scene_preview_destroy(HmbtnScenePreview *s)
{
    if (!s) return;
    SDL_DestroyTexture(s->texture);
    hmbtn_model_renderer_destroy(s->geometry);
    hmbtn_xa_stream_close(&s->xa);
    hmbtn_title_audio_close(&s->audio);
    hmbtn_new_game_ui_destroy(s->ui);
    hmbtn_house_ui_destroy(s->house_ui);
    SDL_free(s->data_root);
    free(s->player_block); free(s->event_block); free(s->player_atlas); free(s->mayor_atlas);
    free(s->field_block); free(s->field_atlas);
    free(s->action_block); free(s->action_atlas);
    free(s->section); free(s->pixels); free(s->atlas); free(s);
}

static bool load_field(HmbtnScenePreview *s, const char *root, char *error, size_t error_size)
{
    if (s->field_block && s->field_atlas) return true;
    s->field_block = malloc(0xB000); s->field_atlas = malloc(sizeof(*s->field_atlas));
    if (!s->field_block || !s->field_atlas) { snprintf(error,error_size,"Cannot allocate bounded field resources"); return false; }
    if (!hmbtn_resource_read(root,"A_FILE/00_gf.bin",0x385F800,s->field_block,0xB000,error,error_size)
            || !hmbtn_bank_decode(s->field_block+5588,0xB000-5588,s->field_atlas,error,error_size)) return false;
    const unsigned offsets[8] = {0,200,400,632,832,1032,1196,1396};
    for (unsigned i = 0; i < 7; ++i)
        if (!hmbtn_animation_open(&s->field_art[i],s->field_block+offsets[i],
                offsets[i+1]-offsets[i],error,error_size)) return false;
    return true;
}

static bool load_area(HmbtnScenePreview *s, const char *root, unsigned area,
    char *error, size_t error_size)
{
    /* farm_section_table.h: only scenes with implemented native events. */
    static const struct { unsigned area; uint32_t start, texture, end; } rows[] = {
        {0,0x83800,0xBA800,0xDD800}, {230,0x3483800,0x34BA000,0x34D9000},
        {231,0x34EC800,0x3509800,0x351D800}, {232,0x3530800,0x3544000,0x3550800},
        {80,0x1C52800,0x1C5E000,0x1C66000}
    };
    unsigned row = 0;
    while (row < sizeof(rows)/sizeof(rows[0]) && rows[row].area != area) ++row;
    if (row == sizeof(rows)/sizeof(rows[0])) { snprintf(error,error_size,"Area %u is not integrated",area); return false; }
    const uint32_t start = rows[row].start, texture = rows[row].texture, end = rows[row].end;
    const size_t size = texture-start, bank_size = end-texture;
    uint8_t *section = malloc(size), *bank = malloc(bank_size);
    HmbtnSceneData scene;
    bool ok = section && bank;
    if (!ok) snprintf(error,error_size,"Cannot allocate bounded scene buffers");
    if (ok) ok = hmbtn_resource_read(root,"A_FILE/00_gf.bin",start,section,size,error,error_size)
        && hmbtn_resource_read(root,"A_FILE/00_gf.bin",texture,bank,bank_size,error,error_size)
        && hmbtn_scene_data_open(&scene,section,size,error,error_size)
        && hmbtn_bank_decode(bank,bank_size,s->atlas,error,error_size);
    free(bank);
    if (!ok) { free(section); return false; }
    HmbtnModelRenderer *geometry = hmbtn_model_renderer_create(scene.primitive_capacity);
    if (!geometry) { free(section); snprintf(error,error_size,"Cannot allocate bounded scene ordering"); return false; }
    hmbtn_model_renderer_destroy(s->geometry); free(s->section);
    s->section = section; s->scene = scene; s->geometry = geometry; s->area = area;
    if (area != 0) { free(s->field_block); s->field_block = NULL; free(s->field_atlas); s->field_atlas = NULL; }
    else if (!load_field(s,root,error,error_size)) return false;
    s->intro.camera_height = -4500; s->intro.camera_distance = -9000;
    printf("Farm geometry loaded: area %u, %u objects, %u primitives; native C renderer.\n",
        area,(unsigned)s->scene.terrain.object_count,(unsigned)s->scene.primitive_capacity);
    return true;
}

static HmbtnScenePreview *scene_create(SDL_Renderer *renderer,
    const char *root, bool opening, char *error, size_t error_size)
{
    HmbtnScenePreview *s = calloc(1,sizeof(*s));
    if (!s) goto allocation;
    hmbtn_house_interaction_init(&s->house);
    s->pixels = malloc(FRAME_BYTES); s->atlas = malloc(sizeof(*s->atlas));
    if (!s->pixels || !s->atlas) goto allocation;
    if (!load_area(s,root,0,error,error_size)) goto fail;
    s->texture = SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STREAMING,320,240);
    if (!s->texture || !SDL_SetTextureScaleMode(s->texture,SDL_SCALEMODE_NEAREST)) {
        snprintf(error,error_size,"Cannot create scene texture: %s",SDL_GetError()); goto fail;
    }
    reset_camera(s);
    if (!opening) puts("Development view only: arrows/gamepad pan camera, Enter/Start resets. Use --intro-preview for the player, Thomas and the arrival event.");
    return s;
allocation:
    snprintf(error,error_size,"Cannot allocate bounded scene preview buffers");
fail:
    hmbtn_scene_preview_destroy(s);
    return NULL;
}

HmbtnScenePreview *hmbtn_scene_preview_create(SDL_Renderer *renderer,
    const char *root, char *error, size_t error_size)
{
    return scene_create(renderer,root,false,error,error_size);
}

static bool load_event(HmbtnScenePreview *s, unsigned event, char *error, size_t error_size)
{
    static const uint32_t offsets[] = {0x2CC800,0x2CE800,0x2DA000,0x309800,0x315000,0x321000,0x323000};
    if (event < 116 || event > 121) { snprintf(error,error_size,"Opening event %u is not integrated",event); return false; }
    const uint32_t start = offsets[event-116];
    const size_t bytes = offsets[event-115]-start;
    free(s->event_block); s->event_block = malloc(bytes);
    if (!s->event_block) { snprintf(error,error_size,"Cannot allocate bounded opening event"); return false; }
    HmbtnEventBlock block;
    if (!hmbtn_resource_read(s->data_root,"A_FILE/12_event_s.bin",start,s->event_block,bytes,error,error_size)
            || !hmbtn_event_block_open(&block,s->event_block,bytes,error,error_size)) return false;
    memset(s->mayor_atlas,0,sizeof(*s->mayor_atlas));
    memset(s->actor_animation,0,sizeof(s->actor_animation));
    memset(s->intro.childhood,0,sizeof(s->intro.childhood));
    memset(&s->intro.mayor,0,sizeof(s->intro.mayor));
    memset(&s->intro.boy,0,sizeof(s->intro.boy)); memset(&s->intro.girl,0,sizeof(s->intro.girl));
    const uint8_t *script = NULL; size_t script_size = 0;
    uint64_t actor_mask = 0; unsigned banks = 0;
    for (unsigned i = 0; i < block.count; ++i) {
        const uint8_t *entry; size_t size; uint16_t flags;
        if (!hmbtn_event_block_entry(&block,i,&entry,&size,&flags)) return false;
        if (flags == 0x4000) { script = entry; script_size = size; }
        else if (flags == 0x8000) {
            ++banks;
            if (!hmbtn_bank_apply(entry,size,s->mayor_atlas,error,error_size)) return false;
        } else if ((flags&0xFF00u) == 0x2000u && hmbtn_farm_intro_actor(&s->intro,flags&255u)) {
            const unsigned id = flags&255u;
            const uint64_t bit = UINT64_C(1) << id;
            if (actor_mask&bit) { snprintf(error,error_size,"Duplicate opening actor entry"); return false; }
            actor_mask |= bit;
            HmbtnAnimation *animation = &s->actor_animation[id];
            if (!hmbtn_animation_open(animation,entry,size,error,error_size)) return false;
        } else { snprintf(error,error_size,"Unsupported opening actor flags 0x%04x",flags); return false; }
    }
    const uint64_t expected = event == 116 || event == 121 ? UINT64_C(1)<<17
        : event == 117 ? 14u : event == 118 ? 2u : event == 120 ? 30u
        : (UINT64_C(1)<<27)|(UINT64_C(1)<<33);
    if (!script || actor_mask != expected || banks != 1) {
        snprintf(error,error_size,"Opening event is missing its expected actors, textures or script"); return false;
    }
    if (event == 116) {
        if (!hmbtn_farm_intro_open(&s->intro,script,script_size,error,error_size)) return false;
    } else if (!hmbtn_event_script_open(&s->intro.script,script,script_size,error,error_size)) return false;
    memset(s->intro.slots,0,sizeof(s->intro.slots));
    s->intro.event = event; s->intro.event_pending = false; s->intro.queued_event = 0;
    s->loaded_cell = UINT32_MAX;
    s->intro.cell_actor = 0; s->intro.camera_actor = 0; s->intro.camera_gliding = false;
    s->intro.message = 0; s->intro.message_done = false; s->loaded_message = 0;
    s->dialogue.message.phase = HMBTN_MESSAGE_CLOSED;
    printf("Opening event 0x%02x loaded: original actors, textures and native commands.\n",event); fflush(stdout);
    return true;
}

static bool actor_animation_tick(const HmbtnAnimation *a, HmbtnIntroActor *actor,
    char *error, size_t error_size)
{
    /* Preserve the original ENDED/JUMPED signal for event_cmd_set_sprite waits. */
    const HmbtnAnimationPlayer *p = &actor->sprite;
    if (p->animation >= a->count[3]) { snprintf(error,error_size,"Opening animation outside range"); return false; }
    const uint8_t *range = a->data+a->section[3]+p->animation*4;
    const unsigned count = hmbtn_read_le16(range);
    if (p->frame >= count) { snprintf(error,error_size,"Opening animation frame outside range"); return false; }
    const uint8_t *frame = a->data+a->section[4]+hmbtn_read_le16(range+2)+p->frame*10;
    actor->animation_done = p->ticks+1u >= ((unsigned)frame[6]+1)/2
        && (frame[7] != 0 || p->frame+1u >= count);
    return hmbtn_animation_tick(a,&actor->sprite,error,error_size);
}

HmbtnScenePreview *hmbtn_scene_intro_create(SDL_Renderer *renderer,
    const char *root, char *error, size_t error_size)
{
    HmbtnScenePreview *s = scene_create(renderer,root,true,error,error_size);
    if (!s) return NULL;
    s->player_block = malloc(0x49000);
    s->player_atlas = malloc(sizeof(*s->player_atlas));
    s->mayor_atlas = malloc(sizeof(*s->mayor_atlas));
    s->data_root = SDL_strdup(root);
    if (!s->player_block || !s->player_atlas || !s->mayor_atlas || !s->data_root) {
        snprintf(error,error_size,"Cannot allocate bounded intro resources"); goto fail;
    }
    if (!hmbtn_resource_read(root,"A_FILE/00_gf.bin",0x2A800,s->player_block,0x49000,error,error_size)
            || !hmbtn_animation_open(&s->player_animation,s->player_block,0xBE08,error,error_size)
            || !hmbtn_bank_decode(s->player_block+0xBE08,0x49000-0xBE08,s->player_atlas,error,error_size)
            || !load_event(s,116,error,error_size)) goto fail;
    s->ui = hmbtn_new_game_ui_create(root,error,error_size);
    if (!s->ui || !hmbtn_title_audio_open_song(&s->audio,root,31,error,error_size)) goto fail;
    hmbtn_sound_start(s->audio.sound);
    s->audio_song = 31; s->audio.xa = &s->xa;
    s->opening = true;
    puts("Opening event 0x74 ready: arrival with scenery, followed by childhood events 0x75..0x78 and the return to Thomas.");
    return s;
fail:
    hmbtn_scene_preview_destroy(s); return NULL;
}

void hmbtn_scene_preview_discard_input(HmbtnScenePreview *s)
{
    if (s) s->pending_pressed = 0;
}
bool hmbtn_scene_preview_take_rumble(HmbtnScenePreview *s)
{
    if(!s) return false;
    const bool pending=s->house.options.rumble;
    s->house.options.rumble=false;
    return pending;
}
HmbtnScenePreview *hmbtn_scene_house_create(SDL_Renderer *renderer,
    const char *root, char *error, size_t error_size)
{
    HmbtnScenePreview *s=hmbtn_scene_intro_create(renderer,root,error,error_size);
    if(!s) return NULL;
    if(!load_area(s,root,80,error,error_size)) { hmbtn_scene_preview_destroy(s); return NULL; }
    /* Shortcut to the same handoff as the end of event 0x79; no saved state. */
    memset(&s->intro,0,sizeof(s->intro));
    s->intro.area=80; s->intro.event=121; s->intro.script.finished=true;
    s->intro.finish_requested=true; s->intro.stop_music=true;
    s->intro.camera_height=-4500; s->intro.camera_distance=-9000;
    s->intro.player.visible=true;
    s->intro.player.position[0]=-90; s->intro.player.position[2]=541;
    memcpy(s->intro.camera,s->intro.player.position,sizeof(s->intro.camera));
    memset(&s->actor_animation,0,sizeof(s->actor_animation));
    return s;
}
HmbtnScenePreview *hmbtn_scene_childhood_create(SDL_Renderer *renderer,
    const char *root, char *error, size_t error_size)
{
    HmbtnScenePreview *s = hmbtn_scene_intro_create(renderer,root,error,error_size);
    if (!s) return NULL;
    if (!load_area(s,root,231,error,error_size) || !load_event(s,119,error,error_size)) goto fail;
    s->intro.area = 231;
    hmbtn_title_audio_close(&s->audio);
    if (!hmbtn_title_audio_open_song(&s->audio,root,33,error,error_size)) goto fail;
    s->audio_song = 33; s->audio.xa = &s->xa; hmbtn_sound_start(s->audio.sound);
    return s;
fail:
    hmbtn_scene_preview_destroy(s); return NULL;
}
void hmbtn_scene_intro_registration(HmbtnScenePreview *s, const HmbtnNewGame *g)
{
    if (!s || !g) return;
    memcpy(s->dialogue.player_name,g->player_name,sizeof(g->player_name));
    memcpy(s->dialogue.farm_name,g->farm_name,sizeof(g->farm_name));
    memcpy(s->dialogue.dog_name,g->dog_name,sizeof(g->dog_name));
    s->dialogue.season = g->season; s->dialogue.day = g->day;
    puts("Registration retained in native opening: player/farm/dog names and birthday.");
}

static bool opening_tick(HmbtnScenePreview *s, uint32_t held, char *error, size_t error_size)
{
    if (s->intro.message && s->loaded_message == s->intro.message) {
        const int sound = hmbtn_message_tick(&s->dialogue.message,s->pending_pressed,held);
        if (sound >= 0) hmbtn_sound_effect(s->audio.sound,(unsigned)sound);
        if (s->dialogue.message.phase == HMBTN_MESSAGE_CLOSED) s->intro.message_done = true;
    }
    hmbtn_xa_stream_tick(&s->xa); s->intro.xa_active = s->xa.active;
    const unsigned previous = s->intro.script.pc;
    if (!hmbtn_farm_intro_tick(&s->intro,error,error_size)) return false;
    if (s->intro.script.pc != previous) {
        printf("Opening event 0x%02x step: %u\n",s->intro.event,s->intro.script.pc); fflush(stdout);
    }
    if (s->intro.area_pending) {
        if (!load_area(s,s->data_root,s->intro.target_area,error,error_size)) return false;
        s->intro.area = s->area; s->intro.area_pending = false;
        const unsigned song = s->area == 230 ? 32u : s->area == 231 || s->area == 232 ? 33u : s->audio_song;
        if (song != s->audio_song) {
            hmbtn_title_audio_close(&s->audio);
            if (!hmbtn_title_audio_open_song(&s->audio,s->data_root,song,error,error_size)) return false;
            s->audio_song = song; s->audio.xa = &s->xa;
            hmbtn_sound_start(s->audio.sound); s->intro.stop_music = false;
        }
    }
    if (s->intro.xa_pending) {
        if (!hmbtn_xa_stream_start(&s->xa,s->data_root,s->intro.xa_track,error,error_size)) return false;
        s->intro.xa_pending = false; s->intro.xa_started = s->intro.xa_active = true;
    }
    if (s->intro.action_pending) {
        /* player_load_tool_anim: mode 71 -> row 12, seven sectors, ANI 352 bytes. */
        s->action_block = malloc(0x3800); s->action_atlas = malloc(sizeof(*s->action_atlas));
        if (!s->action_block || !s->action_atlas) {
            snprintf(error,error_size,"Cannot allocate the player's action bank"); return false;
        }
        if (!hmbtn_resource_read(s->data_root,"A_FILE/00_gf.bin",0x3CE2800,s->action_block,0x3800,error,error_size)
                || !hmbtn_animation_open(&s->action_animation,s->action_block,352,error,error_size)
                || !hmbtn_bank_decode(s->action_block+352,0x3800-352,s->action_atlas,error,error_size)) return false;
        s->intro.action_pending = false; s->intro.action_ready = true;
    }
    if (s->intro.song_pending) {
        hmbtn_title_audio_close(&s->audio);
        if (!hmbtn_title_audio_open_song(&s->audio,s->data_root,34,error,error_size)) return false;
        s->audio_song = s->intro.music_mode = 34; s->audio.xa = &s->xa; s->intro.song_pending = false;
    }
    if (s->intro.start_music) {
        hmbtn_sound_start(s->audio.sound); s->intro.start_music = s->intro.stop_music = false;
        s->intro.music_volume = 127; s->intro.music_time = s->intro.music_length = 0;
    }
    if (s->intro.event_pending && !load_event(s,s->intro.queued_event,error,error_size)) return false;
    if (s->intro.script.boundary) {
        printf("Opening boundary: %s. Escape closes the window.\n",error); fflush(stdout);
        error[0] = '\0';
    }
    if (s->intro.script.finished && s->intro.finish_requested && !s->completion_reported) {
        /* farm_track_id: sunny spring, sceneId 999 -> song 1. The farm's
         * effects VAB is independent of both common UI effects and music. */
        hmbtn_title_audio_close(&s->audio);
        if(!hmbtn_title_audio_open_song(&s->audio,s->data_root,1,error,error_size)) return false;
        uint8_t *fx=malloc(227328);
        if(!fx) { snprintf(error,error_size,"Cannot allocate farm effects bank"); return false; }
        const bool loaded=hmbtn_resource_read(s->data_root,"A_FILE/09_sound.stm",24576,fx,227328,error,error_size)
            && hmbtn_sound_bank(s->audio.sound,2,fx,10240,fx+10240,217088,error,error_size);
        free(fx); if(!loaded) return false;
        s->house_ui=hmbtn_house_ui_create();
        if(!s->house_ui) { snprintf(error,error_size,"Cannot allocate house interface"); return false; }
        s->audio_song=1; s->audio.xa=&s->xa; hmbtn_sound_start(s->audio.sound);
        s->intro.stop_music=false; s->intro.music_volume=0;
        s->control.enabled=true;
        s->intro.action_active=false; s->intro.player.pose=false;
        puts("Opening complete: farmhouse interior (area 80). Arrows walk, Circle/X runs, Cross/Z interacts. TV, toolbox, calendar, bookshelf, options and bed menu enabled; overnight world updates, save/load and farm exit remain pending.");
        fflush(stdout); s->completion_reported = true;
    }
    if(s->control.enabled) {
        if(s->house_music_ticks<100) ++s->house_music_ticks;
        s->intro.music_volume=s->house_music_ticks*127/100;
        bool opened=false;
        if(!s->control.exit_pending && s->house.mode==HMBTN_HOUSE_FREE && (s->pending_pressed&HMBTN_PAD_CROSS))
            opened=hmbtn_house_interact(&s->house,s->section,s->scene.terrain.size+0x2000,&s->intro.player);
        if(s->house.mode!=HMBTN_HOUSE_FREE) {
            if(!opened) hmbtn_house_interaction_tick(&s->house,s->pending_pressed,held);
            if(s->house.sound>=0) hmbtn_sound_effect(s->audio.sound,(unsigned)s->house.sound);
            if(s->house.request && s->house.loaded!=s->house.request) {
                if(s->house.request==21) {
                    /* text_find_staged_value: the initial can has zero water.
                     * There is no refill/tool-use branch in the house yet. */
                    uint8_t record[8192];
                    if(!hmbtn_resource_read(s->data_root,"A_FILE/05_messege3.bin",21*8192u,
                            record,sizeof(record),error,error_size)) return false;
                    for(unsigned i=0;i<HMBTN_MESSAGE_WORDS;++i)
                        if(hmbtn_read_le16(record+0x1800+i*2)==0x4010)
                            hmbtn_write_le16(record+0x1800+i*2,0x40);
                    if(!hmbtn_ui_message_record(s->ui,record,sizeof(record),&s->house.message,error,error_size)) return false;
                } else if(!hmbtn_new_game_ui_message(s->ui,s->data_root,"A_FILE/05_messege3.bin",
                            s->house.request,&s->house.message,error,error_size)) return false;
                s->house.loaded=s->house.request;
                s->house.message.suppress_cursor=s->house.mode==HMBTN_HOUSE_TV_SELECT
                    || s->house.mode==HMBTN_HOUSE_TOOLBOX
                    || s->house.mode==HMBTN_HOUSE_BED
                    || (s->house.mode==HMBTN_HOUSE_OPTIONS && !s->house.detail)
                    || (s->house.mode==HMBTN_HOUSE_CALENDAR && !s->house.detail);
                printf("House message: 0x%03x, mode %u\n",s->house.request,(unsigned)s->house.mode); fflush(stdout);
            }
            if(s->house.mode!=HMBTN_HOUSE_SLEEP)
                s->intro.player.sprite=(HmbtnAnimationPlayer){0,(uint16_t)s->intro.player.facing,0};
        } else if(!opened) {
            hmbtn_house_control_tick(&s->control,&s->intro.player,&s->scene.terrain,
                hmbtn_house_world_buttons(&s->house.options,held));
            hmbtn_house_options_idle(&s->house.options,s->control.mode!=0,s->intro.player.facing);
            if(s->control.mode && !s->control.exit_pending) {
                if(s->house.steps>(s->control.mode==4 ? 8u : 12u)) {
                    if(!hmbtn_sound_key_on(s->audio.sound,2,8,0,68,20)) {
                        snprintf(error,error_size,"Original footstep tone is unavailable"); return false;
                    }
                    s->house.steps=0;
                } else ++s->house.steps;
            }
        }
        hmbtn_sound_stereo(s->audio.sound,s->house.options.stereo);
        if(s->house.boundary) {
            printf("House boundary: %s is not ported yet; no save file was read or written. The bed menu remains available.\n",
                s->house.boundary==2 ? "diary/load overlay" : "livestock exchange overlay");
            fflush(stdout); s->house.boundary=0;
        }
        if(s->house.mode==HMBTN_HOUSE_SLEEP) {
            if(!s->sleep.active) {
                /* player_load_tool_anim: mode 74 -> row 15, aligned ANI 312. */
                if(!s->action_block) s->action_block=malloc(0x3800);
                if(!s->action_atlas) s->action_atlas=malloc(sizeof(*s->action_atlas));
                if(!s->action_block || !s->action_atlas) {
                    snprintf(error,error_size,"Cannot allocate sleep animation bank"); return false;
                }
                if(!hmbtn_resource_read(s->data_root,"A_FILE/00_gf.bin",0x3CED000,s->action_block,0x3800,error,error_size)
                        || !hmbtn_animation_open(&s->action_animation,s->action_block,312,error,error_size)
                        || !hmbtn_bank_decode(s->action_block+312,0x3800-312,s->action_atlas,error,error_size)) return false;
                hmbtn_house_sleep_start(&s->sleep,&s->intro.player); s->intro.action_active=true;
            }
            hmbtn_house_sleep_tick(&s->sleep,&s->intro.player);
            /* scene_link_handback: eight steps to lift, then twelve to settle. */
            s->scene.flags[1]=(uint16_t)((s->scene.flags[1]&~0x1000u)|(s->sleep.second_bed ? 0x1000u : 0u));
            s->scene.flags[2]=(uint16_t)((s->scene.flags[2]&~0x1000u)|(s->sleep.second_bed ? 0u : 0x1000u));
            s->scene.models[1].morph_steps=8;
            s->scene.models[1].morph_step=s->sleep.second_bed ? 8 : s->sleep.link_step;
            s->scene.models[2].morph_steps=12;
            s->scene.models[2].morph_step=s->sleep.link_state==2 ? s->sleep.link_step : 0;
            s->intro.music_volume=s->intro.music_volume*(30-s->sleep.fade)/30;
            if(s->sleep.boundary && !s->sleep_reported) {
                hmbtn_sound_stop_music(s->audio.sound);
                printf("Overnight boundary: original bed animation and fade completed%s. Field/animals/weather/day events must be ported before advancing time or saving. No date change or save write occurred. Escape closes the window.\n",
                    s->house.save_requested ? " (diary requested)" : "");
                fflush(stdout); s->sleep_reported=true;
            }
        }
        s->scene.angle[0][2]=(uint16_t)s->house.pan;
        if(s->house.tv_dirty) {
            if(!hmbtn_resource_read(s->data_root,"A_FILE/00_gf.bin",0x3D5A800+s->house.tv_group*0x1800,
                    s->tv_sectors,sizeof(s->tv_sectors),error,error_size)) return false;
            s->house.tv_dirty=false; s->tv_frame=UINT32_MAX;
        }
        if(s->tv_sectors[0] && (s->house.mode==HMBTN_HOUSE_TV || s->house.tv_group==18)) {
            const unsigned frame=s->house.mode==HMBTN_HOUSE_TV ? (s->house.tv_ticks/16)%3 : 0;
            if(s->tv_frame!=frame) {
                if(!hmbtn_bank_apply_at(s->tv_sectors+frame*2048,2048,s->atlas,320,1,320,-48,error,error_size)) return false;
                s->tv_frame=frame;
            }
        }
        s->dialogue.message=s->house.message;
        HmbtnHouseExit exit;
        if(s->house.mode==HMBTN_HOUSE_FREE && !s->control.exit_pending && hmbtn_house_exit_at(s->section,
                s->scene.terrain.size+0x2000,s->intro.player.position,&exit)) {
            s->control.exit_pending=true; s->control.exit_zone=exit.zone;
            s->intro.player.sprite=(HmbtnAnimationPlayer){0,(uint16_t)s->intro.player.facing,0};
            printf("House exit boundary: zone %u, target area %u, entry (%d,%d,%d). Transition and farm events are not ported yet. Escape closes the window.\n",
                exit.zone,exit.target,(int)exit.entry[0],(int)exit.entry[1],(int)exit.entry[2]);
            fflush(stdout);
        }
        memcpy(s->intro.camera,s->sleep.active ? s->sleep.camera : s->intro.player.position,sizeof(s->intro.camera));
    }
    if (s->intro.message && s->loaded_message != s->intro.message) {
        if (!hmbtn_new_game_ui_message(s->ui,s->data_root,"A_FILE/03_messege1.bin",
                s->intro.message,&s->dialogue.message,error,error_size)) return false;
        s->loaded_message = s->intro.message;
        printf("Opening message: 0x%03x\n",s->loaded_message); fflush(stdout);
    }
    if (!s->intro.message) {
        s->loaded_message = 0;
        if ((!s->sleep.active || s->sleep.phase<4) && !actor_animation_tick(s->intro.action_active ? &s->action_animation : &s->player_animation,
                &s->intro.player,error,error_size)) return false;
        if (s->intro.action_active) s->intro.action_finished = s->intro.player.animation_done;
        for (unsigned id = 1; id < 35; ++id) {
            HmbtnIntroActor *actor = hmbtn_farm_intro_actor(&s->intro,(int32_t)id);
            if (actor && actor->visible && s->actor_animation[id].data
                    && !actor_animation_tick(&s->actor_animation[id],actor,error,error_size)) return false;
        }
    }
    if (s->intro.cell_actor) {
        const HmbtnIntroActor *cell = hmbtn_farm_intro_actor(&s->intro,(int32_t)s->intro.cell_actor);
        if (cell && s->loaded_cell != cell->sprite.animation) {
            s->loaded_cell = cell->sprite.animation;
            printf("Opening memory image: %u\n",s->loaded_cell); fflush(stdout);
        }
    }
    hmbtn_sound_music_volume(s->audio.sound,s->intro.music_volume);
    if (s->intro.stop_music) hmbtn_sound_stop_music(s->audio.sound);
    s->centre_x = s->intro.camera[0]; s->centre_z = s->intro.camera[2];
    s->dirty = true; ++s->ticks;
    return true;
}

bool hmbtn_scene_preview_draw(HmbtnScenePreview *s, SDL_Renderer *renderer,
    Uint64 elapsed_ns, uint32_t pressed, uint32_t held, char *error, size_t error_size)
{
    const Uint64 tick_ns = SDL_NS_PER_SECOND * 1001 / 30000;
    if (!s->opening && (pressed & HMBTN_PAD_START)) reset_camera(s);
    s->pending_pressed |= pressed;
    s->accumulated_ns += elapsed_ns > tick_ns*8 ? tick_ns*8 : elapsed_ns;
    while (s->accumulated_ns >= tick_ns) {
        s->accumulated_ns -= tick_ns;
        if (!s->opening || (!s->intro.message && !s->intro.script.boundary))
            hmbtn_scene_data_tick(&s->scene);
        if (!s->opening || !s->intro.script.boundary) s->dirty = true;
        if (s->opening) {
            if (!s->intro.script.boundary && !opening_tick(s,held,error,error_size)) return false;
            s->pending_pressed = 0;
            continue;
        }
        int32_t dx = 0, dz = 0;
        if (held & HMBTN_PAD_UP) { dx -= 100; dz += 100; }
        if (held & HMBTN_PAD_DOWN) { dx += 100; dz -= 100; }
        if (held & HMBTN_PAD_RIGHT) { dx += 100; dz += 100; }
        if (held & HMBTN_PAD_LEFT) { dx -= 100; dz -= 100; }
        if (dx || dz) {
            s->centre_x += dx; s->centre_z += dz;
            if (s->centre_x < -16000) s->centre_x = -16000;
            if (s->centre_x > 16000) s->centre_x = 16000;
            if (s->centre_z < -16000) s->centre_z = -16000;
            if (s->centre_z > 16000) s->centre_z = 16000;
            s->dirty = true;
        }
    }
    if (s->dirty) {
        for (size_t i = 0; i < FRAME_BYTES; i += 4) {
            memset(s->pixels+i,0,3); s->pixels[i+3] = 255;
        }
        if (!s->opening || !s->intro.cell_actor) {
        const int32_t distance = s->opening ? s->intro.camera_distance : -9000;
        const int32_t height = s->opening ? s->intro.camera_height : -4500;
        const int32_t centre_y = s->opening ? s->intro.camera[1] : 0;
        const int32_t z_offset = distance < 0 ? -((-distance*2896+4095)/4096) : distance*2896/4096;
        const HmbtnCamera camera = {{s->centre_x-distance*2896/4096,centre_y+height,s->centre_z+z_offset},
            {s->centre_x,centre_y,s->centre_z},1000};
        HmbtnWorldSprite actors[512]; unsigned count = 0;
        if (s->opening && s->intro.player.visible) {
            actors[count] = (HmbtnWorldSprite){s->intro.action_active ? &s->action_animation : &s->player_animation,
                s->intro.action_active ? s->action_atlas : s->player_atlas,s->intro.player.sprite,{0},0};
            memcpy(actors[count++].position,s->intro.player.position,sizeof(actors[0].position));
            hmbtn_new_game_ui_cursor(s->ui,&actors[count++],s->intro.player.position,0);
        }
        if (s->opening) for (unsigned id = 1; id < 35; ++id) {
            const HmbtnIntroActor *actor = hmbtn_farm_intro_actor(&s->intro,(int32_t)id);
            if (!actor || !actor->visible || !s->actor_animation[id].data) continue;
            actors[count] = (HmbtnWorldSprite){&s->actor_animation[id],s->mayor_atlas,actor->sprite,{0},0};
            memcpy(actors[count++].position,actor->position,sizeof(actors[0].position));
            if (actor->marker != 1) hmbtn_new_game_ui_cursor(s->ui,&actors[count++],actor->position,actor->marker == 2);
        }
        const uint16_t *layout = s->field_layout == 1 ? title_layout1_cells
            : s->field_layout == 2 ? title_layout2_cells : title_layout0_cells;
        if (s->area == 0) for (unsigned row = 0; row < 25; ++row) for (unsigned col = 0; col < 45; ++col) {
            const int32_t x = (int32_t)col*400-4200, z = 1400-(int32_t)row*400;
            if (x < s->centre_x-3600 || x >= s->centre_x+3600
                    || z > s->centre_z+3600 || z <= s->centre_z-3600) continue;
            unsigned art; int lift = 0;
            switch (layout[row*45+col]) {
            case 0x3A: art = 0; break;
            case 0x16D: art = 1; lift = -10; break;
            case 0x170: art = 2; lift = -10; break;
            case 0x173: art = 3; lift = -15; break;
            case 0x3E: art = 4; break;
            case 0x3F: art = 5; break;
            case 0x40: art = 6; break;
            default: continue;
            }
            if (count >= 512) { snprintf(error,error_size,"Field sprite capacity exceeded"); return false; }
            actors[count++] = (HmbtnWorldSprite){&s->field_art[art],s->field_atlas,{0},{x,0,z},lift};
        }
        HmbtnModelInstance instances[HMBTN_SCENE_MODELS+1];
        const unsigned model_count = hmbtn_scene_data_instances(&s->scene,instances);
        if (!hmbtn_model_render_instances(s->geometry,instances,model_count,s->atlas,&camera,actors,count,
                s->pixels,FRAME_BYTES,NULL,error,error_size)) return false;
        }
        if (s->opening) {
            if (s->intro.cell_actor) {
                const HmbtnIntroActor *cell = hmbtn_farm_intro_actor(&s->intro,(int32_t)s->intro.cell_actor);
                if (!cell || s->intro.cell_actor > 4) { snprintf(error,error_size,"Invalid childhood illustration actor"); return false; }
                for (size_t i = 0; i < FRAME_BYTES; i += 4) memset(s->pixels+i,0,3);
                if (!hmbtn_animation_draw(&s->actor_animation[s->intro.cell_actor],s->mayor_atlas,
                        &cell->sprite,80,60,s->pixels,320,240,FRAME_BYTES,error,error_size)) return false;
            }
            const unsigned brightness = s->sleep.active ? 255*(30-s->sleep.fade)/30 : hmbtn_farm_intro_brightness(&s->intro);
            for (size_t i = 0; i < FRAME_BYTES; ++i)
                if (i%4 != 3) s->pixels[i] = (uint8_t)((s->pixels[i]*brightness+(s->intro.fade_white ? 255*(255-brightness) : 0))/255);
            if(s->control.enabled && !hmbtn_house_ui_draw(s->house_ui,s->ui,&s->house,
                    s->data_root,s->ticks,s->pixels,error,error_size)) return false;
            if (!hmbtn_new_game_ui_draw(s->ui,&s->dialogue,s->ticks,s->pixels,error,error_size)) return false;
        }
        if (!SDL_UpdateTexture(s->texture,NULL,s->pixels,320*4)) goto sdl_error;
        s->dirty = false;
    }
    if (s->opening && !hmbtn_title_audio_pump(&s->audio,error,error_size)) return false;
    if (!SDL_SetRenderDrawColor(renderer,0,0,0,255) || !SDL_RenderClear(renderer)
            || !SDL_RenderTexture(renderer,s->texture,NULL,NULL)) goto sdl_error;
    return true;
sdl_error:
    snprintf(error,error_size,"Cannot present scene preview: %s",SDL_GetError()); return false;
}
