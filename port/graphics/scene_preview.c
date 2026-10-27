/* Farm scenery, arrival and the first childhood events. Geometry-only mode
 * keeps its diagnostic camera; New Game follows the original scripts.
 * Source: farm_load.c/farm_section_table.h, scene_view.c and event_script.c. */
#include "economy.h"
#include "scene_preview.h"
#include "save_file.h"
#include "session.h"
#include "runtime.h"
#include "scene_inspection.h"
#include "exploration.h"
#include "model_render.h"
#include "scene_decode.h"
#include "psx_data.h"
#include "resource_file.h"
#include "text_resources.h"
#include "action_resources.h"
#include "event_resources.h"
#include "service_resources.h"
#include "service_audio.h"
#include "pad_buttons.h"
#include "farm_intro.h"
#include "farm_visit.h"
#include "event_item.h"
#include "farm_clock.h"
#include "world_map.h"
#include "village_art.h"
#include "village_talk.h"
#include "house_control.h"
#include "farm_action.h"
#include "fishing.h"
#include "field_effects.h"
#include "farm_carry.h"
#include "farm_feed.h"
#include "farm_gather.h"
#include "estate_art.h"
#include "poultry_art.h"
#include "feed_shop_ui.h"
#include "farm_menu_ui.h"
#include "house_sleep.h"
#include "house_ui.h"
#include "new_game_ui.h"
#include "title_audio.h"
#include "xa_stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "field_layouts.inc"
#include "scene_sections.inc"

enum { FRAME_BYTES = 320 * 240 * 4 };

struct HmbtnScenePreview {
    HmbtnSession *session;
    uint8_t *section, *pixels;
    HmbtnImageAtlas *atlas;
    HmbtnSceneData scene;
    HmbtnModelRenderer *geometry;
    SDL_Texture *texture;
    int32_t centre_x, centre_z;
    bool dirty;
    bool opening,restoring; /* Resource restoration must not run area-entry rules. */
    char *data_root;
    uint8_t *player_block, *event_block;
    HmbtnImageAtlas *player_atlas, *mayor_atlas;
    HmbtnAnimation player_animation, actor_animation[35];
    HmbtnAnimation event_player_animation;
    HmbtnHouseUI *house_ui;
    unsigned tv_frame;
    uint8_t tv_sectors[0x1800];
    HmbtnNewGameUI *ui;
    HmbtnTitleAudio audio;
    HmbtnXAStream xa;
    uint8_t *field_block;
    HmbtnImageAtlas *field_atlas;
    HmbtnAnimation field_art[7];
    unsigned field_layout;
    uint64_t watering_voice;
    HmbtnEstateArt estate_art;
    HmbtnPoultryArt poultry_art;
    uint8_t *feed_block;
    HmbtnImageAtlas *feed_atlas;
    HmbtnAnimation feed_art[2];
    HmbtnVillageArt village_art;
    uint8_t *schedule_block;
    HmbtnFarmMenuUI *menu_ui;
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

/* The current host is immediate. A claimed request may instead be completed
 * later by an injected provider; polling never dispatches it twice. */
static const HmbtnServiceResult *service_resource(HmbtnScenePreview *s,HmbtnServiceKind kind,
    unsigned id,unsigned bank,char *error,size_t size)
{
    if(!hmbtn_service_begin(&s->session->services,HMBTN_SERVICE_RESOURCE,(HmbtnServiceRequest){kind,id,bank,0,0})) {
        snprintf(error,size,"Conflicting resource service request");return NULL;
    }
    hmbtn_resource_service(s->data_root,&s->session->services);
    const HmbtnServiceResult *r=hmbtn_service_poll(&s->session->services,HMBTN_SERVICE_RESOURCE);
    if(r->status==HMBTN_SERVICE_FAILED || r->status==HMBTN_SERVICE_CANCELLED) {
        snprintf(error,size,"%s",r->error[0] ? r->error : "Resource request cancelled");return NULL;
    }
    return r;
}
static bool service_read(HmbtnScenePreview *s,HmbtnServiceKind kind,unsigned id,unsigned bank,
    uint8_t *data,size_t bytes,uint32_t *value,char *error,size_t size)
{
    const HmbtnServiceResult *r=service_resource(s,kind,id,bank,error,size);
    if(!r) return false;
    if(r->status!=HMBTN_SERVICE_DONE) { snprintf(error,size,"Resource is pending at synchronous loader");return false; }
    if(!data || r->bytes!=bytes || (kind==HMBTN_SERVICE_ACTION_DATA && (!r->value || r->value>=bytes))) {
        snprintf(error,size,"Resource result has unexpected length");return false;
    }
    memcpy(data,r->data,bytes);if(value) *value=r->value;
    return hmbtn_service_ack(&s->session->services,r->ticket);
}
static const HmbtnServiceResult *service_audio(HmbtnScenePreview *s,HmbtnServiceKind kind,
    unsigned id,char *error,size_t size)
{
    if(!hmbtn_service_begin(&s->session->services,HMBTN_SERVICE_AUDIO,
            (HmbtnServiceRequest){kind,id,0,0,s->session->intro.script.pc})) {
        snprintf(error,size,"Conflicting audio service request");return NULL;
    }
    hmbtn_audio_service(s->data_root,&s->audio,&s->xa,&s->session->services);
    const HmbtnServiceResult *r=hmbtn_service_poll(&s->session->services,HMBTN_SERVICE_AUDIO);
    if(r->status==HMBTN_SERVICE_FAILED || r->status==HMBTN_SERVICE_CANCELLED) {
        snprintf(error,size,"%s",r->error[0] ? r->error : "Audio request cancelled");return NULL;
    }
    return r;
}

void hmbtn_scene_preview_destroy(HmbtnScenePreview *s)
{
    if (!s) return;
    hmbtn_session_destroy(s->session);
    hmbtn_estate_art_close(&s->estate_art);hmbtn_poultry_art_close(&s->poultry_art);
    hmbtn_village_art_close(&s->village_art);free(s->schedule_block);
    free(s->feed_block);free(s->feed_atlas);
    SDL_DestroyTexture(s->texture);
    hmbtn_model_renderer_destroy(s->geometry);
    hmbtn_xa_stream_close(&s->xa);
    hmbtn_title_audio_close(&s->audio);
    hmbtn_new_game_ui_destroy(s->ui);
    hmbtn_house_ui_destroy(s->house_ui);
    hmbtn_farm_menu_ui_destroy(s->menu_ui);
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

static bool load_action_bank(HmbtnScenePreview *s,unsigned row,char *error,size_t size)
{
    if(!s->action_block) s->action_block=malloc(HMBTN_ACTION_RESOURCE_BYTES);
    if(!s->action_atlas) s->action_atlas=malloc(sizeof(*s->action_atlas));
    if(!s->action_block || !s->action_atlas) { snprintf(error,size,"Cannot allocate original player action bank");return false; }
    uint32_t animation_bytes=0;
    return service_read(s,HMBTN_SERVICE_ACTION_DATA,row,0,s->action_block,HMBTN_ACTION_RESOURCE_BYTES,&animation_bytes,error,size)
        && hmbtn_animation_open(&s->action_animation,s->action_block,animation_bytes,error,size)
        && hmbtn_bank_decode(s->action_block+animation_bytes,HMBTN_ACTION_RESOURCE_BYTES-animation_bytes,s->action_atlas,error,size);
}
static bool load_area(HmbtnScenePreview *s, const char *root, unsigned area,
    char *error, size_t error_size)
{
    unsigned logical,resource_area;
    if(!hmbtn_world_area(area,s->session->game.clock.season,&logical,&resource_area)) {
        if(area==230 || area==231 || area==232) logical=resource_area=area;
        else {
            snprintf(error,error_size,"Area %u is not integrated",area);return false;
        }
    }
    area=logical;
    /* Seasonal section packs can leave texture padding used by their model
     * untouched (e.g. summer farm page 8, UV 128,242). Compose the area's base
     * pack before the seasonal upload instead of depending on earlier visits
     * or retaining renderer memory in a save. Keep strict missing-data checks. */
    if(resource_area!=logical) {
        const uint32_t base=scene_sections[logical][1];
        const size_t bytes=scene_sections[logical][2]-base;
        uint8_t *baseline=malloc(bytes);
        if(!baseline) { snprintf(error,error_size,"Cannot allocate base area texture pack");return false; }
        const bool ready=hmbtn_resource_read(root,"A_FILE/00_gf.bin",base,baseline,bytes,error,error_size)
            && hmbtn_bank_apply(baseline,bytes,s->atlas,error,error_size);
        free(baseline);if(!ready) return false;
    }
    if((area==96 || area==112) && !s->feed_block) {
        /* Interior resourceSet 7: estate_code_value selects item sprites 4/7. */
        s->feed_block=malloc(0x2800);s->feed_atlas=malloc(sizeof(*s->feed_atlas));
        if(!s->feed_block || !s->feed_atlas) { snprintf(error,error_size,"Cannot allocate estate feed art");return false; }
        if(!hmbtn_resource_read(root,"A_FILE/00_gf.bin",0x38A7800,s->feed_block,0x2800,error,error_size)
                || !hmbtn_animation_open(&s->feed_art[0],s->feed_block+476,560-476,error,error_size)
                || !hmbtn_animation_open(&s->feed_art[1],s->feed_block+728,812-728,error,error_size)
                || !hmbtn_bank_decode(s->feed_block+1508,0x2800-1508,s->feed_atlas,error,error_size)) return false;
    }
    const uint32_t start=scene_sections[resource_area][0],texture=scene_sections[resource_area][1],end=scene_sections[resource_area][2];
    const size_t size = texture-start, bank_size = end-texture;
    uint8_t *section = malloc(size), *bank = malloc(bank_size);
    HmbtnSceneData scene;
    bool ok = section && bank;
    if (!ok) snprintf(error,error_size,"Cannot allocate bounded scene buffers");
    /* farm_load_season_art is independent of the section's texture upload. */
    const uint32_t seasonal_starts[5]={0x3845800,0x3849800,0x384d000,0x3852800,0x3859000};
    const unsigned seasonal_ani[4]={3604,3608,4748,5012};
    const size_t seasonal_size=seasonal_starts[s->session->game.clock.season+1]-seasonal_starts[s->session->game.clock.season];
    uint8_t *seasonal=ok && s->session->game.clock.season ? malloc(seasonal_size) : NULL;
    if(ok && s->session->game.clock.season && !seasonal) { snprintf(error,error_size,"Cannot allocate seasonal texture pack");ok=false; }
    if(ok && s->session->game.clock.season) {
        ok=hmbtn_resource_read(root,"A_FILE/00_gf.bin",seasonal_starts[s->session->game.clock.season],seasonal,seasonal_size,error,error_size)
            && hmbtn_bank_apply(seasonal+seasonal_ani[s->session->game.clock.season],seasonal_size-seasonal_ani[s->session->game.clock.season],s->atlas,error,error_size);
    }
    free(seasonal);
    if (ok) ok = hmbtn_resource_read(root,"A_FILE/00_gf.bin",start,section,size,error,error_size)
        && hmbtn_resource_read(root,"A_FILE/00_gf.bin",texture,bank,bank_size,error,error_size)
        && hmbtn_scene_data_open(&scene,section,size,error,error_size)
        && hmbtn_bank_apply(bank,bank_size,s->atlas,error,error_size);
    free(bank);
    if (!ok) { free(section); return false; }
    HmbtnModelRenderer *geometry = hmbtn_model_renderer_create(scene.primitive_capacity);
    if (!geometry) { free(section); snprintf(error,error_size,"Cannot allocate bounded scene ordering"); return false; }
    if(!s->restoring) hmbtn_session_area_begin(s->session,area,s->section ? hmbtn_read_le16(s->section+2) : 0,s->section!=NULL);
    if(s->opening && (area==0 || area==172 || area==176 || area==204)) {
        HmbtnEventResourceRange range;
        if(!hmbtn_event_resource_locate(HMBTN_EVENT_RESOURCE_TRIGGERS,resource_area,&range)
                || range.bytes>sizeof(s->session->trigger_data)
                || !service_read(s,HMBTN_SERVICE_TRIGGER_DATA,resource_area,0,
                    s->session->trigger_data,range.bytes,NULL,error,error_size)
                || !hmbtn_farm_triggers_open(&s->session->triggers,s->session->trigger_data,range.bytes,error,error_size)) {
            hmbtn_model_renderer_destroy(geometry);free(section);return false;
        }
    }
    hmbtn_model_renderer_destroy(s->geometry); free(s->section);
    s->section = section; s->scene = scene; s->geometry = geometry; s->session->area = area;s->dirty=true;
    if(s->opening && !hmbtn_estate_art_update(&s->estate_art,root,hmbtn_read_le16(section+10),error,error_size)) return false;
    bool hens_here=area==112 || hmbtn_poultry_held_slot(&s->session->poultry)<HMBTN_HENS;
    for(unsigned i=0;i<HMBTN_HENS;++i) hens_here|=s->session->poultry.hens[i].occupied && s->session->poultry.hens[i].area==area;
    if(s->opening && hens_here && !hmbtn_poultry_art_load(&s->poultry_art,root,error,error_size)) return false;
    if(!s->restoring) hmbtn_session_area_ready(s->session,area,&s->scene);
    else s->session->intro.ground=&s->scene.terrain;
    if (area != 0) { free(s->field_block); s->field_block = NULL; free(s->field_atlas); s->field_atlas = NULL;
        memset(s->session->field_effects,0,sizeof(s->session->field_effects)); }
    else if (!load_field(s,root,error,error_size)
            || !hmbtn_farm_field_bind(&s->session->field,&s->scene.terrain)) {
        if(!error[0]) snprintf(error,error_size,"Cannot bind original farm field quads");
        return false;
    }
    if(area==0 && !s->restoring) s->session->field.season=s->session->game.clock.season;
    if(area==0 && (s->session->control.enabled || s->session->first_visit_started) && !hmbtn_farm_field_retexture(&s->session->field,
            s->section+(s->scene.terrain.data-s->section),s->scene.terrain.size)) {
        snprintf(error,error_size,"Cannot restore field soil textures");return false;
    }
    printf("Farm geometry loaded: area %u, %u objects, %u primitives; native C renderer.\n",
        area,(unsigned)s->scene.terrain.object_count,(unsigned)s->scene.primitive_capacity);
    return true;
}

/* Takes ownership of a decoded session, including on failure. */
static HmbtnScenePreview *scene_create_with_session(SDL_Renderer *renderer,
    const char *root,bool opening,HmbtnSession *saved,char *error,size_t error_size)
{
    HmbtnScenePreview *s = calloc(1,sizeof(*s));
    if (!s) { hmbtn_session_destroy(saved);goto allocation; }
    s->restoring=saved!=NULL;s->opening=s->restoring && opening;
    s->session=saved ? saved : hmbtn_session_create(title_layout0_cells,1125);
    s->data_root=SDL_strdup(root);
    if(!s->session || !s->data_root) goto allocation;
    s->menu_ui=hmbtn_farm_menu_ui_create();
    if(!s->menu_ui) goto allocation;
    /* Texture uploads preserve the rest of PS1 VRAM. In particular, seasonal
     * packs can leave padding from earlier sections outside their rectangles. */
    s->pixels = malloc(FRAME_BYTES); s->atlas = calloc(1,sizeof(*s->atlas));
    if (!s->pixels || !s->atlas) goto allocation;
    if (!load_area(s,root,s->restoring ? s->session->area : 0,error,error_size)) goto fail;
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

static HmbtnScenePreview *scene_create(SDL_Renderer *renderer,const char *root,
    bool opening,char *error,size_t size)
{ return scene_create_with_session(renderer,root,opening,NULL,error,size); }

HmbtnScenePreview *hmbtn_scene_preview_create(SDL_Renderer *renderer,
    const char *root, char *error, size_t error_size)
{
    return scene_create(renderer,root,false,error,error_size);
}

static bool load_event(HmbtnScenePreview *s, unsigned event, char *error, size_t error_size)
{
    if (event!=12 && !hmbtn_farm_visit_supported(event) && event!=172 && event!=299 && (event < 116 || event > 121)) { snprintf(error,error_size,"Event %u is not integrated",event); return false; }
    HmbtnEventResourceRange range;
    if(!hmbtn_event_resource_locate(HMBTN_EVENT_RESOURCE_SCRIPT,event,&range)) { snprintf(error,error_size,"Invalid event resource id %u",event);return false; }
    const size_t bytes=range.bytes;
    free(s->event_block); s->event_block = malloc(bytes);
    if (!s->event_block) { snprintf(error,error_size,"Cannot allocate bounded opening event"); return false; }
    HmbtnEventBlock block;
    if (!service_read(s,HMBTN_SERVICE_EVENT_DATA,event,0,s->event_block,bytes,NULL,error,error_size)
            || !hmbtn_event_block_open(&block,s->event_block,bytes,error,error_size)) return false;
    memset(s->mayor_atlas,0,sizeof(*s->mayor_atlas));
    memset(s->actor_animation,0,sizeof(s->actor_animation));
    memset(&s->event_player_animation,0,sizeof(s->event_player_animation));
    const uint8_t *script = NULL; size_t script_size = 0;
    uint64_t actor_mask = 0; unsigned banks = 0;
    for (unsigned i = 0; i < block.count; ++i) {
        const uint8_t *entry; size_t size; uint16_t flags;
        if (!hmbtn_event_block_entry(&block,i,&entry,&size,&flags)) return false;
        if (flags == 0x4000) { script = entry; script_size = size; }
        else if (flags == 0x8000) {
            ++banks;
            if (!hmbtn_bank_apply(entry,size,s->mayor_atlas,error,error_size)) return false;
        } else if ((flags&0xFF00u) == 0x2000u && (hmbtn_farm_intro_actor(&s->session->intro,flags&255u)
                || (hmbtn_farm_visit_supported(event) && (flags&255u)>0 && (flags&255u)<HMBTN_VILLAGERS))) {
            const unsigned id = flags&255u;
            const uint64_t bit = UINT64_C(1) << id;
            if (actor_mask&bit) { snprintf(error,error_size,"Duplicate opening actor entry"); return false; }
            actor_mask |= bit;
            HmbtnAnimation *animation = &s->actor_animation[id];
            if (!hmbtn_animation_open(animation,entry,size,error,error_size)) return false;
        } else if(event==12 && flags==0x401) {
            if(!hmbtn_animation_open(&s->event_player_animation,entry,size,error,error_size)) return false;
        } else { snprintf(error,error_size,"Unsupported opening actor flags 0x%04x",flags); return false; }
    }
    const bool world_event=event==129 || event==130 || event==289;
    const uint64_t expected = event==140 ? UINT64_C(1)<<21 : event==299 || world_event ? 0 : event==172 ? (UINT64_C(1)<<10)|(UINT64_C(1)<<15)|(UINT64_C(1)<<19)
        : event==12 ? (UINT64_C(1)<<17)|(UINT64_C(1)<<23)|(UINT64_C(1)<<26)
        : event == 116 || event == 121 || hmbtn_farm_visit_supported(event) ? UINT64_C(1)<<17
        : event == 117 ? 14u : event == 118 ? 2u : event == 120 ? 30u
        : (UINT64_C(1)<<27)|(UINT64_C(1)<<33);
    if (!script || actor_mask != expected || banks != (event==299 || world_event ? 0u : 1u)) {
        snprintf(error,error_size,"Opening event is missing its expected actors, textures or script"); return false;
    }
    if(!hmbtn_session_open_event(s->session,event,script,script_size,error,error_size)) return false;
    s->session->intro.ground=&s->scene.terrain;
    s->session->runtime.loaded_cell=UINT32_MAX;s->session->runtime.loaded_message=0;
    printf("Opening event 0x%02x loaded: original actors, textures and native commands.\n",event); fflush(stdout);
    return true;
}

static bool player_resources(HmbtnScenePreview *s,const char *root,char *error,size_t error_size)
{
    s->player_block = malloc(0x49000);
    s->player_atlas = malloc(sizeof(*s->player_atlas));
    s->mayor_atlas = malloc(sizeof(*s->mayor_atlas));
    /* Resource root is owned from scene_create, before its first load. */
    if (!s->player_block || !s->player_atlas || !s->mayor_atlas || !s->data_root) {
        snprintf(error,error_size,"Cannot allocate bounded intro resources"); return false;
    }
    if (!hmbtn_resource_read(root,"A_FILE/00_gf.bin",0x2A800,s->player_block,0x49000,error,error_size)
            || !hmbtn_animation_open(&s->player_animation,s->player_block,0xBE08,error,error_size)
            || !hmbtn_bank_decode(s->player_block+0xBE08,0x49000-0xBE08,s->player_atlas,error,error_size)) return false;
    s->ui=hmbtn_new_game_ui_create(root,error,error_size);return s->ui!=NULL;
}
HmbtnScenePreview *hmbtn_scene_intro_create(SDL_Renderer *renderer,
    const char *root,char *error,size_t error_size)
{
    HmbtnScenePreview *s=scene_create(renderer,root,true,error,error_size);
    if(!s) return NULL;
    if(!player_resources(s,root,error,error_size) || !load_event(s,116,error,error_size)
        || !hmbtn_title_audio_open_song(&s->audio,root,31,error,error_size)) goto fail;
    hmbtn_sound_start(s->audio.sound);
    s->session->runtime.audio_song = 31; s->audio.xa = &s->xa;
    s->opening = true;
    puts("Opening event 0x74 ready: arrival with scenery, followed by childhood events 0x75..0x78 and the return to Thomas.");
    return s;
fail:
    hmbtn_scene_preview_destroy(s); return NULL;
}

bool hmbtn_scene_preview_take_rumble(HmbtnScenePreview *s)
{
    if(!s) return false;
    const bool pending=s->session->house.options.rumble;
    s->session->house.options.rumble=false;
    return pending;
}
HmbtnScenePreview *hmbtn_scene_house_create(SDL_Renderer *renderer,
    const char *root, char *error, size_t error_size)
{
    HmbtnScenePreview *s=hmbtn_scene_intro_create(renderer,root,error,error_size);
    if(!s) return NULL;
    if(!load_area(s,root,80,error,error_size)) { hmbtn_scene_preview_destroy(s); return NULL; }
    /* Shortcut to the same handoff as the end of event 0x79; no saved state. */
    memset(&s->session->intro,0,sizeof(s->session->intro));
    s->session->intro.area=80; s->session->intro.event=121; s->session->intro.script.finished=true;
    s->session->intro.finish_requested=true; s->session->intro.stop_music=true;
    s->session->intro.camera_height=-4500; s->session->intro.camera_distance=-9000;
    s->session->intro.player.visible=true;
    s->session->intro.player.position[0]=-90; s->session->intro.player.position[2]=541;
    memcpy(s->session->intro.camera,s->session->intro.player.position,sizeof(s->session->intro.camera));
    memset(&s->actor_animation,0,sizeof(s->actor_animation));
    return s;
}
HmbtnScenePreview *hmbtn_scene_area_create(SDL_Renderer *renderer,
    const char *root,unsigned area,char *error,size_t error_size)
{
    return hmbtn_scene_area_create_at(renderer,root,area,NULL,error,error_size);
}
HmbtnScenePreview *hmbtn_scene_area_create_at(SDL_Renderer *renderer,
    const char *root,unsigned area,const HmbtnFarmClock *clock,char *error,size_t error_size)
{
    unsigned logical,resource;
    if(!hmbtn_world_area(area,0,&logical,&resource) || logical==80 || area!=logical) {
        snprintf(error,error_size,"Unsupported test region %u",area);return NULL;
    }
    HmbtnScenePreview *s=hmbtn_scene_house_create(renderer,root,error,error_size);
    if(!s) return NULL;
    if(clock) {
        if(clock->season>3 || clock->day>29 || clock->weekday>6 || clock->hour>23 || clock->minute>59 || clock->frames>14) {
            snprintf(error,error_size,"Invalid preview calendar or time");goto fail;
        }
        s->session->game.clock=*clock;
        s->session->daily_date=hmbtn_session_date(clock);
        hmbtn_weather_init(&s->session->game.weather,clock,&s->session->game.registration);
    }
    HmbtnHouseExit arrival={0};bool found=false;
    unsigned facing=0;
    /* Use a real incoming passage, rather than a guessed point in the map.
     * The house's exit is first so region 0 starts beside the front door. */
    const unsigned sources[]={80,0,24,28,32,36,40,44,48,52,56,60,64,68,72,76,
        128,132,136,140,144,148,152,156,160,164,168,172,176,180,184,188,192,196,200,204,208,212};
    for(unsigned n=0;n<sizeof(sources)/sizeof(sources[0]) && !found;++n) {
        uint8_t zones[16+50*44];
        if(!hmbtn_resource_read(root,"A_FILE/00_gf.bin",scene_sections[sources[n]+s->session->game.clock.season][0],
                zones,sizeof(zones),error,error_size)) goto fail;
        for(unsigned i=0;i<50 && !found;++i) {
            const uint8_t *p=zones+16+i*44;
            if((hmbtn_read_le16(p+2)&~3u)!=area) continue;
            int32_t point[3];
            for(unsigned a=0;a<3;++a) {
                const unsigned lo=hmbtn_read_le16(p+14+a*2),hi=hmbtn_read_le16(p+20+a*2);
                point[a]=((lo<32768 ? (int32_t)lo : (int32_t)lo-65536)
                    +(hi<32768 ? (int32_t)hi : (int32_t)hi-65536))/2;
            }
            found=hmbtn_house_exit_at(zones,sizeof(zones),point,&arrival) && arrival.zone==i;
            if(!found) found=hmbtn_world_gate_at(zones,sizeof(zones),point,&arrival) && arrival.zone==i;
            if(!found) {
                HmbtnIntroActor actor={0};unsigned model;
                memcpy(actor.position,point,sizeof(point));actor.facing=hmbtn_read_le16(p+8);
                found=hmbtn_world_door_at(zones,sizeof(zones),&actor,&arrival,&model) && arrival.zone==i;
            }
            if(found) facing=hmbtn_read_le16(p+8)&3u;
        }
    }
    if(!found) { snprintf(error,error_size,"No original incoming passage for region %u",area);goto fail; }
    if(!load_area(s,root,area,error,error_size)) goto fail;
    s->session->intro.area=area;s->session->first_visit_started=true;
    memcpy(s->session->intro.player.position,arrival.entry,sizeof(arrival.entry));
    HmbtnGroundHit ground;
    if(hmbtn_ground_find(&s->scene.terrain,s->session->intro.player.position,0xC080,200,&ground))
        s->session->intro.player.position[1]=ground.height;
    s->session->intro.player.facing=facing;s->session->intro.player.sprite=(HmbtnAnimationPlayer){0,(uint16_t)facing,0};
    memcpy(s->session->intro.camera,s->session->intro.player.position,sizeof(s->session->intro.camera));
    hmbtn_world_camera(s->section,s->scene.terrain.size+0x2000,s->session->intro.camera);
    s->centre_x=s->session->intro.camera[0];s->centre_z=s->session->intro.camera[2];
    printf("Region test entry: area %u; season %u, day %u at %02u:%02u; fresh session, Thomas visit skipped.\n",
        area,s->session->game.clock.season,s->session->game.clock.day+1,s->session->game.clock.hour,s->session->game.clock.minute);
    return s;
fail:
    hmbtn_scene_preview_destroy(s);return NULL;
}
HmbtnScenePreview *hmbtn_scene_childhood_create(SDL_Renderer *renderer,
    const char *root, char *error, size_t error_size)
{
    HmbtnScenePreview *s = hmbtn_scene_intro_create(renderer,root,error,error_size);
    if (!s) return NULL;
    if (!load_area(s,root,231,error,error_size) || !load_event(s,119,error,error_size)) goto fail;
    s->session->intro.area = 231;
    hmbtn_title_audio_close(&s->audio);
    if (!hmbtn_title_audio_open_song(&s->audio,root,33,error,error_size)) goto fail;
    s->session->runtime.audio_song = 33; s->audio.xa = &s->xa; hmbtn_sound_start(s->audio.sound);
    return s;
fail:
    hmbtn_scene_preview_destroy(s); return NULL;
}
void hmbtn_scene_intro_registration(HmbtnScenePreview *s, const HmbtnRegistration *registration)
{
    if (!s || !registration) return;
    /* One-time value transfer after title teardown, never copy editor pointers. */
    s->session->game.registration=*registration;
    puts("Registration retained in native opening: player/farm/dog names and birthday.");
}

static bool farm_song(HmbtnScenePreview *s,unsigned song,char *error,size_t error_size)
{
    if(s->session->runtime.audio_song==song && s->session->completion_reported) return true;
    hmbtn_title_audio_close(&s->audio);
    if(!hmbtn_title_audio_open_song(&s->audio,s->data_root,song,error,error_size)) return false;
    uint8_t *fx=malloc(227328);
    if(!fx) { snprintf(error,error_size,"Cannot allocate farm effects bank");return false; }
    const bool ok=hmbtn_resource_read(s->data_root,"A_FILE/09_sound.stm",24576,fx,227328,error,error_size)
        && hmbtn_sound_bank(s->audio.sound,2,fx,10240,fx+10240,217088,error,error_size);
    free(fx);if(!ok) return false;
    s->watering_voice=0;s->session->runtime.audio_song=song;s->audio.xa=&s->xa;hmbtn_sound_start(s->audio.sound);
    hmbtn_sound_stereo(s->audio.sound,s->session->house.options.stereo);return true;
}
HmbtnScenePreview *hmbtn_scene_saved_create(SDL_Renderer *renderer,const char *root,
    const char *path,char *error,size_t size)
{
    HmbtnSession *saved=hmbtn_save_file_read(path,error,size);
    if(!saved) return NULL;
    HmbtnScenePreview *s=scene_create_with_session(renderer,root,true,saved,error,size);
    if(!s) return NULL;
    if(!player_resources(s,root,error,size)) goto fail;
    s->house_ui=hmbtn_house_ui_create();s->schedule_block=malloc(0xA800);
    if(!s->house_ui || !s->schedule_block) { snprintf(error,size,"Cannot allocate saved session resources");goto fail; }
    if(!hmbtn_resource_read(root,"A_FILE/00_gf.bin",0x74800,s->schedule_block,0xA800,error,size)
        || !hmbtn_session_bind_schedules(saved,s->schedule_block,0xA800)
        || !hmbtn_village_art_update(&s->village_art,root,hmbtn_read_le16(s->section+12),&saved->game.clock,error,size)
        || !farm_song(s,hmbtn_world_song_weather(s->section,16,saved->game.clock.season,saved->game.clock.hour,saved->game.weather.today),error,size)) goto fail;
    s->restoring=false;s->centre_x=saved->intro.camera[0];s->centre_z=saved->intro.camera[2];
    hmbtn_sound_music_volume(s->audio.sound,hmbtn_world_music_volume_weather(s->section,16,saved->game.clock.season,saved->game.clock.hour,saved->game.weather.today));
    /* Validate resource-specific ANI frames before publishing the candidate. */
    if(!hmbtn_scene_preview_draw(s,renderer,error,size)) goto fail;
    return s;
fail:hmbtn_scene_preview_destroy(s);return NULL;
}
bool hmbtn_scene_preview_save(HmbtnScenePreview *s,const char *path,char *error,size_t size)
{
    if(!s || !s->opening) { snprintf(error,size,"No gameplay session to save");return false; }
    return hmbtn_save_file_write(path,s->session,error,size);
}
bool hmbtn_scene_preview_save_pending(HmbtnScenePreview *s)
{
    char reason[192];return s && s->session->morning_save_requested && hmbtn_save_available(s->session,reason,sizeof(reason));
}
bool hmbtn_scene_preview_take_save_request(HmbtnScenePreview *s)
{
    if(!hmbtn_scene_preview_save_pending(s)) return false;
    s->session->morning_save_requested=false;return true;
}
bool hmbtn_scene_preview_load_pending(const HmbtnScenePreview *s) { return s && s->session->load_requested; }
void hmbtn_scene_preview_ack_load(HmbtnScenePreview *s) { if(s) s->session->load_requested=false; }
static HmbtnExplorationFrame exploration_frame(HmbtnScenePreview *);
static bool exploration_action(void *context,unsigned row,char *error,size_t size)
{ return load_action_bank(context,row,error,size); }
static bool exploration_text(void *context,unsigned bank,unsigned id,uint8_t *record,size_t bytes,char *error,size_t size)
{
    HmbtnScenePreview *s=context;
    return service_read(s,HMBTN_SERVICE_TEXT_DATA,id,bank,record,bytes,NULL,error,size);
}
static bool exploration_village_assets(void *context,unsigned primary,const HmbtnFarmClock *clock,char *error,size_t size)
{
    HmbtnScenePreview *s=context;
    return hmbtn_village_art_update(&s->village_art,s->data_root,primary,clock,error,size);
}
static const HmbtnAnimation *exploration_animation(void *context,HmbtnExplorationAnimation kind,unsigned id)
{
    HmbtnScenePreview *s=context;const HmbtnImageAtlas *atlas;
    switch(kind) {
    case HMBTN_EXP_PLAYER:return &s->player_animation;
    case HMBTN_EXP_ACTION:return &s->action_animation;
    case HMBTN_EXP_EVENT_PLAYER:return &s->event_player_animation;
    case HMBTN_EXP_VILLAGER:return hmbtn_village_art_actor(&s->village_art,id,&atlas);
    case HMBTN_EXP_CHICK:return &s->poultry_art.chick;
    case HMBTN_EXP_HEN:return &s->poultry_art.adult;
    default:return NULL;
    }
}
static bool exploration_effect(void *context,const HmbtnExplorationEffect *effect,char *error,size_t error_size)
{
    HmbtnScenePreview *s=context;
    switch(effect->kind) {
    case HMBTN_EXP_UI_SOUND:hmbtn_sound_effect(s->audio.sound,effect->id);return true;
    case HMBTN_EXP_KEY_SOUND: {
        const uint64_t voice=hmbtn_sound_effect_start(s->audio.sound,effect->bank,effect->program,effect->tone,effect->note,effect->volume);
        if(!voice) { snprintf(error,error_size,"Original exploration tone unavailable");return false; }
        if(effect->water_loop) s->watering_voice=voice;
        return true;
    }
    case HMBTN_EXP_RELEASE_WATER:hmbtn_sound_release_voice(s->audio.sound,s->watering_voice);s->watering_voice=0;return true;
    case HMBTN_EXP_RETEXTURE:
        return hmbtn_farm_field_retexture(&s->session->field,s->section+(s->scene.terrain.data-s->section),s->scene.terrain.size);
    case HMBTN_EXP_MESSAGE_ART:return hmbtn_ui_message_art(s->ui,effect->record,effect->bytes,error,error_size);
    case HMBTN_EXP_STEREO:hmbtn_sound_stereo(s->audio.sound,effect->id!=0);return true;
    case HMBTN_EXP_MESSAGE_LOADED:
        printf("House message: 0x%03x, mode %u\n",effect->id,effect->bank);fflush(stdout);return true;
    case HMBTN_EXP_UNSUPPORTED_HOUSE:
        printf("House boundary: %s is not ported yet; no save file was read or written. The bed menu remains available.\n",
            effect->id==2 ? "diary/load overlay" : "livestock exchange overlay");fflush(stdout);return true;
    case HMBTN_EXP_HOUSE_ART: {
        if(s->session->area==80) s->scene.angle[0][2]=(uint16_t)s->session->house.pan;
        if(s->session->house.tv_dirty) {
            if(!hmbtn_resource_read(s->data_root,"A_FILE/00_gf.bin",0x3D5A800+s->session->house.tv_group*0x1800,
                    s->tv_sectors,sizeof(s->tv_sectors),error,error_size)) return false;
            s->session->house.tv_dirty=false; s->tv_frame=UINT32_MAX;
        }
        if(s->tv_sectors[0] && (s->session->house.mode==HMBTN_HOUSE_TV || s->session->house.tv_group==18)) {
            const unsigned frame=s->session->house.mode==HMBTN_HOUSE_TV ? (s->session->house.tv_ticks/16)%3 : 0;
            if(s->tv_frame!=frame) {
                if(!hmbtn_bank_apply_at(s->tv_sectors+frame*2048,2048,s->atlas,320,1,320,-48,error,error_size)) return false;
                s->tv_frame=frame;
            }
        }
        return true;
    }
    default:snprintf(error,error_size,"Unknown exploration presentation request");return false;
    }
}
static HmbtnExplorationFrame exploration_frame(HmbtnScenePreview *s)
{
    return (HmbtnExplorationFrame){.section=s->section,.section_size=s->scene.terrain.size+0x2000,
        .ground=&s->scene.terrain,.scene=&s->scene,.field_art=s->field_art,.context=s,.action_bank=exploration_action,
        .grass_cut_art=hmbtn_estate_art_sprite(&s->estate_art,s->session->game.clock.season==0 ? 14 : 19),
        .text=exploration_text,.village_assets=exploration_village_assets,.animation=exploration_animation,
        .effect=exploration_effect};
}
static void runtime_refresh(HmbtnSessionFrame *f)
{
    HmbtnScenePreview *s=f->context;f->section=s->section;f->scene=&s->scene;
    f->exploration=exploration_frame(s);
}
static bool runtime_area(HmbtnSessionFrame *f,unsigned id,char *error,size_t size)
{
    HmbtnScenePreview *s=f->context;
    if(!load_area(s,s->data_root,id,error,size)) return false;
    runtime_refresh(f);return true;
}
static bool runtime_event(HmbtnSessionFrame *f,unsigned id,char *error,size_t size)
{ return load_event(f->context,id,error,size); }
static bool runtime_song(void *context,unsigned id,bool effects,char *error,size_t size)
{
    HmbtnScenePreview *s=context;
    if(effects) return farm_song(s,id,error,size);
    hmbtn_title_audio_close(&s->audio);
    if(!hmbtn_title_audio_open_song(&s->audio,s->data_root,id,error,size)) return false;
    s->session->runtime.audio_song=id;s->audio.xa=&s->xa;return true;
}
static const HmbtnAnimation *runtime_animation(void *context,unsigned id,bool resident)
{
    HmbtnScenePreview *s=context;const HmbtnImageAtlas *atlas;
    if(!resident && id<35 && s->actor_animation[id].data) return &s->actor_animation[id];
    if(resident || (s->session->visit.active && (s->session->visit.event==130 || s->session->visit.event==289 || s->session->visit.event==140)))
        return hmbtn_village_art_actor(&s->village_art,id,&atlas);
    return NULL;
}
static const HmbtnServiceResult *runtime_resource(void *c,HmbtnServiceKind kind,unsigned id,unsigned bank,char *e,size_t n)
{ return service_resource(c,kind,id,bank,e,n); }
static const HmbtnServiceResult *runtime_audio(void *c,HmbtnServiceKind kind,unsigned id,char *e,size_t n)
{ return service_audio(c,kind,id,e,n); }
static bool runtime_read(void *c,HmbtnServiceKind kind,unsigned id,unsigned bank,uint8_t *data,size_t bytes,uint32_t *value,char *e,size_t n)
{ return service_read(c,kind,id,bank,data,bytes,value,e,n); }
static bool runtime_conversation(void *c,unsigned id,uint8_t *data,size_t capacity,size_t *bytes,char *error,size_t size)
{
    HmbtnScenePreview *s=c;uint32_t offset;
    return hmbtn_village_talk_range(id,&offset,bytes) && *bytes<=capacity
        && hmbtn_resource_read(s->data_root,"A_FILE/12_event_s.bin",offset,data,*bytes,error,size);
}
static bool runtime_effect(HmbtnSessionFrame *f,const HmbtnRuntimeEffect *e,char *error,size_t size)
{
    HmbtnScenePreview *s=f->context;
    switch(e->kind) {
    case HMBTN_RUNTIME_SOUND:hmbtn_sound_effect(s->audio.sound,e->id);return true;
    case HMBTN_RUNTIME_KEY_SOUND:return hmbtn_sound_key_on(s->audio.sound,e->bank,e->program,e->tone,e->note,e->volume);
    case HMBTN_RUNTIME_START_MUSIC:hmbtn_sound_start(s->audio.sound);return true;
    case HMBTN_RUNTIME_STOP_MUSIC:hmbtn_sound_stop_music(s->audio.sound);return true;
    case HMBTN_RUNTIME_VOLUME:hmbtn_sound_music_volume(s->audio.sound,e->id);return true;
    case HMBTN_RUNTIME_XA_TICK:hmbtn_xa_stream_tick(&s->xa);return true;
    case HMBTN_RUNTIME_AUDIO_POLL:hmbtn_audio_service(s->data_root,&s->audio,&s->xa,&s->session->services);return true;
    case HMBTN_RUNTIME_MESSAGE_ART:return hmbtn_ui_message_art(s->ui,e->data,e->bytes,error,size);
    case HMBTN_RUNTIME_PORTRAIT:return hmbtn_ui_message_portrait(s->ui,e->data,error,size);
    case HMBTN_RUNTIME_TV_CLEAR:memset(s->tv_sectors,0,sizeof(s->tv_sectors));s->tv_frame=UINT32_MAX;return true;
    case HMBTN_RUNTIME_READY:
        if(!s->house_ui) s->house_ui=hmbtn_house_ui_create();
        if(!s->house_ui) { snprintf(error,size,"Cannot allocate house interface");return false; }
        if(!s->schedule_block) {
            s->schedule_block=malloc(0xA800);
            if(!s->schedule_block || !hmbtn_resource_read(s->data_root,"A_FILE/00_gf.bin",0x74800,s->schedule_block,0xA800,error,size)) return false;
        }
        f->schedules=s->schedule_block;f->schedule_size=0xA800;return true;
    default:snprintf(error,size,"Unknown session presentation effect");return false;
    }
}
HmbtnSessionFrame hmbtn_scene_preview_frame(HmbtnScenePreview *s)
{
    HmbtnSessionFrame f={.session=s->session,.context=s,.area=runtime_area,.event=runtime_event,
        .song=runtime_song,.effect=runtime_effect,.animation=runtime_animation,.resource=runtime_resource,
        .audio=runtime_audio,.read=runtime_read,.conversation=runtime_conversation};
    runtime_refresh(&f);return f;
}
/* Shared by the host tick's projected weather scroll and the renderer. */
static HmbtnCamera scene_camera(const HmbtnScenePreview *s,const int32_t centre[3])
{
    const int32_t distance=s->opening ? s->session->intro.camera_distance : -9000;
    const int32_t height=s->opening ? s->session->intro.camera_height : -4500;
    const int32_t z_offset=distance<0 ? -((-distance*2896+4095)/4096) : distance*2896/4096;
    return (HmbtnCamera){{centre[0]-distance*2896/4096,centre[1]+height,centre[2]+z_offset},
        {centre[0],centre[1],centre[2]},1000};
}
static bool runtime_tick(HmbtnScenePreview *s,HmbtnLogicalInput input,char *error,size_t size)
{
    HmbtnSessionFrame f=hmbtn_scene_preview_frame(s);
    const HmbtnCamera previous=scene_camera(s,s->session->intro.camera);
    const unsigned area=s->session->area;
    if(!hmbtn_session_tick(s->session,&f,input,error,size)) return false;
    /* player_update_position projects the new clamped camera centre through
     * the previous view. Moving at a camera boundary yields zero scroll.
     * Subtract the native projected old centre instead of literal 160/120 to
     * cancel projection rounding. Area loads must not drag old particles. */
    const int32_t *centre=s->session->intro.camera;
    if(s->session->completion_reported && area==s->session->area && s->session->control.enabled
            && !s->session->travel_tick && !(hmbtn_read_le16(s->section+8)&0x1800u)
            && memcmp(previous.target,centre,sizeof(previous.target))) {
        int old_x,old_y,new_x,new_y;
        if(hmbtn_model_project_point(&previous,previous.target,&old_x,&old_y)
                && hmbtn_model_project_point(&previous,centre,&new_x,&new_y))
            hmbtn_weather_effects_scroll(&s->session->weather_effects,old_x-new_x,old_y-new_y);
    }
    s->centre_x=centre[0];s->centre_z=centre[2];s->dirty=true;
    return true;
}
bool hmbtn_scene_preview_tick(HmbtnScenePreview *s,HmbtnLogicalInput input,char *error,size_t error_size)
{
    if(!s) { snprintf(error,error_size,"Missing native scene");return false; }
    const uint32_t pressed=input.pressed,held=input.held;
    if(!s->opening && (pressed&HMBTN_PAD_START)) reset_camera(s);
    if (s->session->menu.mode==HMBTN_MENU_NONE && (!s->opening || (!s->session->intro.message && !s->session->intro.script.boundary)))
        hmbtn_scene_data_tick(&s->scene);
    if (!s->opening || !s->session->intro.script.boundary) s->dirty = true;
    if (s->opening) {
        if (!s->session->intro.script.boundary && !runtime_tick(s,input,error,error_size)) return false;
        return hmbtn_title_audio_pump(&s->audio,error,error_size);
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
    return !s->opening || hmbtn_title_audio_pump(&s->audio,error,error_size);
}
bool hmbtn_scene_preview_draw(HmbtnScenePreview *s,SDL_Renderer *renderer,char *error,size_t error_size)
{
    if(s->opening) { s->centre_x=s->session->intro.camera[0];s->centre_z=s->session->intro.camera[2]; }
    if (s->dirty) {
        for (size_t i = 0; i < FRAME_BYTES; i += 4) {
            memset(s->pixels+i,0,3); s->pixels[i+3] = 255;
        }
        if (!s->opening || !s->session->intro.cell_actor) {
        const int32_t centre[3]={s->centre_x,s->opening ? s->session->intro.camera[1] : 0,s->centre_z};
        const HmbtnCamera camera=scene_camera(s,centre);
        HmbtnWorldSprite actors[512]; unsigned count = 0;
        if(s->session->control.enabled) for(unsigned i=125;i<=172;++i) {
            const unsigned item=hmbtn_farm_gather_item(&s->session->gather,i,hmbtn_read_le16(s->section+2),s->session->game.clock.season);
            const int art=hmbtn_estate_item_art(item);
            const HmbtnAnimation *ani=art>=0 ? hmbtn_estate_art_sprite(&s->estate_art,(unsigned)art) : NULL;
            if(!item || !ani) continue;
            actors[count]=(HmbtnWorldSprite){ani,s->estate_art.atlas,{0},{0},0};
            memcpy(actors[count++].position,hmbtn_farm_gather_record(i)->position,sizeof(actors[0].position));
        }
        /* estate.c:estate_record_tables_refresh / estate_code_status and
         * estate_search_records: seasonal table bundles, original ANI 15..17
         * and projection offsets. Greenhouse availability is not implemented. */
        if(s->session->area==180 && s->session->game.clock.season<3) {
            for(unsigned i=0;i<HMBTN_GATHER_RECORDS;++i) {
                const HmbtnGatherRecord *record=hmbtn_farm_gather_record(i);
                if(record->location!=30 || record->item!=0x128+s->session->game.clock.season) continue;
                const HmbtnAnimation *ani=hmbtn_estate_art_sprite(&s->estate_art,15+s->session->game.clock.season);
                if(!ani || count>=512) { snprintf(error,error_size,"Original supermarket seed sprites unavailable");return false; }
                actors[count++]=(HmbtnWorldSprite){ani,s->estate_art.atlas,{0},
                    {record->position[0]+150,record->position[1]-100,record->position[2]-150},0};
            }
        }
        for(unsigned i=0;i<HMBTN_HENS;++i) {
            const HmbtnHen *hen=&s->session->poultry.hens[i];
            if(hen->occupied && !hen->dead && !hen->held && hen->area==s->session->area && hen->stage!=HMBTN_HEN_EGG) {
                actors[count]=(HmbtnWorldSprite){hen->stage==HMBTN_HEN_CHICK ? &s->poultry_art.chick : &s->poultry_art.adult,
                    s->poultry_art.atlas,hen->actor.sprite,{0},0};
                memcpy(actors[count++].position,hen->actor.position,sizeof(actors[0].position));
                if(hen->praise && hmbtn_new_game_ui_cue(s->ui,&actors[count],hen->actor.position,37,hen->praise,-20)) ++count;
            }
            if(hen->occupied && hen->area==s->session->area && hen->stage==HMBTN_HEN_EGG) {
                const HmbtnAnimation *ani=hmbtn_estate_art_sprite(&s->estate_art,hen->golden_egg ? 6 : 5);
                if(ani) {
                    actors[count]=(HmbtnWorldSprite){ani,s->estate_art.atlas,{0},{0},0};
                    memcpy(actors[count++].position,hen->actor.position,sizeof(actors[0].position));
                }
            }
            const HmbtnCoopEgg *egg=&s->session->poultry.eggs[i];
            /* estate_code_lookup: the outdoor egg sprites are seasonal. */
            static const unsigned farm_egg[4]={0x16,0x1B,0x1B,0x0C};
            const int art=s->session->area==0 ? (int)(farm_egg[s->session->game.clock.season]+(egg->item==0xE8)) : hmbtn_estate_item_art(egg->item);
            const HmbtnAnimation *ani=art>=0 ? hmbtn_estate_art_sprite(&s->estate_art,(unsigned)art) : NULL;
            if(egg->item && egg->area==s->session->area && ani) {
                actors[count]=(HmbtnWorldSprite){ani,s->estate_art.atlas,{0},{0},0};
                memcpy(actors[count++].position,egg->position,sizeof(actors[0].position));
            }
        }
        if(s->feed_block && (s->session->area==96 || s->session->area==112)) {
            const bool barn=s->session->area==96;const bool *bins=barn ? s->session->feed.barn : s->session->feed.coop;
            const unsigned capacity=barn ? 22 : 10;
            for(unsigned i=0;i<capacity;++i) if(bins[i]) {
                actors[count]=(HmbtnWorldSprite){&s->feed_art[barn ? 0 : 1],s->feed_atlas,{0},{0},0};
                hmbtn_farm_feed_position(barn,i,actors[count++].position);
            }
        }
        if((s->session->control.enabled || s->session->visit.active) && s->session->village_ready) for(unsigned id=1;id<37;++id) {
            if((s->session->village.event_actors[id] && s->session->visit.event!=289)
                    || (s->session->village.event_actor && s->session->village.event_actor_id==id)) continue;
            const HmbtnIntroActor *a=hmbtn_village_actor(&s->session->village,id);
            const HmbtnImageAtlas *atlas;
            const HmbtnAnimation *ani=hmbtn_village_art_actor(&s->village_art,id,&atlas);
            if(!a->visible || !ani) continue;
            actors[count]=(HmbtnWorldSprite){ani,atlas,a->sprite,{0},0};
            memcpy(actors[count++].position,a->position,sizeof(a->position));
        }
        unsigned player_sprite=0;bool player_drawn=false;
        if (s->opening && s->session->intro.player.visible) {
            player_sprite=count;player_drawn=true;
            actors[count] = (HmbtnWorldSprite){s->session->intro.action_active ? &s->action_animation : s->session->first_visit_active ? &s->event_player_animation : &s->player_animation,
                s->session->intro.action_active ? s->action_atlas : s->session->first_visit_active ? s->mayor_atlas : s->player_atlas,s->session->intro.player.sprite,{0},0};
            memcpy(actors[count++].position,s->session->intro.player.position,sizeof(actors[0].position));
            hmbtn_new_game_ui_cursor(s->ui,&actors[count++],s->session->intro.player.position,0);
        }
        if (s->opening) for (unsigned id = 1; id < 35; ++id) {
            if(s->session->visit.event==289 && s->session->village.event_actors[id]) continue;
            const HmbtnIntroActor *actor = hmbtn_farm_intro_actor(&s->session->intro,(int32_t)id);
            const HmbtnImageAtlas *atlas=s->mayor_atlas;
            const HmbtnAnimation *animation=s->actor_animation[id].data ? &s->actor_animation[id]
                : s->session->visit.active && (s->session->visit.event==130 || s->session->visit.event==289 || s->session->visit.event==140) ? hmbtn_village_art_actor(&s->village_art,id,&atlas) : NULL;
            if (!actor || !actor->visible || !animation) continue;
            actors[count] = (HmbtnWorldSprite){animation,atlas,actor->sprite,{0},0};
            memcpy(actors[count++].position,actor->position,sizeof(actors[0].position));
            if (actor->marker != 1) hmbtn_new_game_ui_cursor(s->ui,&actors[count++],actor->position,actor->marker == 2);
        }
        const uint16_t *layout = s->field_layout == 1 ? title_layout1_cells
            : s->field_layout == 2 ? title_layout2_cells : title_layout0_cells;
        if (s->session->area == 0) for (unsigned row = 0; row < 25; ++row) for (unsigned col = 0; col < 45; ++col) {
            const int32_t x = (int32_t)col*400-4200, z = 1400-(int32_t)row*400;
            if (x < s->centre_x-3600 || x >= s->centre_x+3600
                    || z > s->centre_z+3600 || z <= s->centre_z-3600) continue;
            const HmbtnFarmTile *tile=&s->session->field.cells[row*45+col];
            if((s->session->control.enabled || s->session->first_visit_started)
                    && hmbtn_farm_crop_supported(tile->value) && tile->value!=0x36 && tile->kind) {
                unsigned sprite,anim;
                if(!hmbtn_estate_crop_look(tile->value,tile->kind,s->session->game.clock.season,&sprite,&anim)) continue;
                const HmbtnAnimation *ani=hmbtn_estate_art_sprite(&s->estate_art,sprite);
                if(!ani) { snprintf(error,error_size,"Original crop art unavailable");return false; }
                if(count>=512) { snprintf(error,error_size,"Field sprite capacity exceeded");return false; }
                actors[count++]=(HmbtnWorldSprite){ani,s->estate_art.atlas,{(uint16_t)anim,0,0},{x,0,z},0};
                continue;
            }
            unsigned art; int lift = 0;
            switch (s->session->control.enabled || s->session->first_visit_started ? s->session->field.cells[row*45+col].value : layout[row*45+col]) {
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
        if(s->session->area==0) for(unsigned i=0;i<HMBTN_FIELD_EFFECTS;++i) if(s->session->field_effects[i].active) {
            const HmbtnFieldEffect *e=&s->session->field_effects[i];
            if(count>=512) { snprintf(error,error_size,"Field effect capacity exceeded");return false; }
            const bool grass=e->art==HMBTN_FIELD_EFFECT_GRASS;
            const HmbtnAnimation *ani=grass ? hmbtn_estate_art_sprite(&s->estate_art,
                s->session->game.clock.season==0 ? 14 : 19) : &s->field_art[e->art];
            if(!ani) { snprintf(error,error_size,"Original grass cut art unavailable");return false; }
            actors[count]=(HmbtnWorldSprite){ani,grass ? s->estate_art.atlas : s->field_atlas,{1,(uint16_t)(e->tick/3),0},{0},0};
            memcpy(actors[count++].position,e->position,sizeof(e->position));
        }
        HmbtnWorldAttachment attachment={0};unsigned attached=0;
        if(player_drawn && ((s->session->quick_ticks && s->session->quick_id) || s->session->house.inventory->held_item.id || s->session->fishing.shown || s->session->event_item.shown || s->session->farm_action.phase==HMBTN_FARM_MEDICINE)
                && !(s->session->fishing.phase==HMBTN_FISHING_EAT && s->session->intro.player.sprite.frame>=2)
                && !(s->session->event_item.phase==HMBTN_EVENT_ITEM_EAT && s->session->intro.player.sprite.frame>=2)) {
            int offset_x=-8,offset_y=-10;
            if(s->session->control.carrying_big && s->session->control.mode!=4) {
                static const int offset[4][2]={{-8,-10},{-6,-18},{6,-18},{8,-10}};
                offset_x=offset[s->session->intro.player.facing&3][0];offset_y=offset[s->session->intro.player.facing&3][1];
            }
            else if(s->session->fishing.shown || s->session->event_item.shown) {
                static const int offsets[4][2]={{-8,-10},{-6,-18},{6,-18},{8,-10}};
                offset_x=offsets[s->session->intro.player.facing&3][0];offset_y=offsets[s->session->intro.player.facing&3][1];
            }
            else if(s->session->house.inventory->held_item.id && !(s->session->quick_ticks && s->session->quick_tool))
                hmbtn_farm_carried_offset(s->session->intro.player.facing,s->session->control.mode==4,&offset_x,&offset_y);
            attachment.sprite=player_sprite;attachment.screen_x=offset_x;attachment.screen_y=offset_y;
            if(!hmbtn_house_ui_picture_art(s->house_ui,s->data_root,
                        s->session->event_item.shown ? s->session->event_item.shown : s->session->fishing.shown ? s->session->fishing.shown : s->session->quick_ticks && s->session->quick_id ? s->session->quick_id
                            : s->session->farm_action.phase==HMBTN_FARM_MEDICINE ? s->session->house.inventory->equipped.id : s->session->house.inventory->held_item.id,
                        &attachment.animation,&attachment.atlas,error,error_size)) return false;
            attached=1;
        }
        const unsigned model_count = hmbtn_scene_data_instances(&s->scene,instances);
        if (!hmbtn_model_render_instances_attached(s->geometry,instances,model_count,s->atlas,&camera,actors,count,
                &attachment,attached,s->pixels,FRAME_BYTES,NULL,error,error_size)) return false;
        if(s->session->fishing.marker_visible) {
            int x,y;const HmbtnFishing *f=&s->session->fishing;
            const int32_t point[3]={f->marker[0]+150,f->marker[1]-100,f->marker[2]-150};
            if(hmbtn_model_project_point(&camera,point,&x,&y)) {
                if(f->phase==HMBTN_FISHING_RELEASE_FLIGHT) {
                    if(!hmbtn_house_ui_picture(s->house_ui,s->data_root,s->pixels,f->caught,x,y,error,error_size)) return false;
                } else {
                    const int bob=f->phase==HMBTN_FISHING_WAIT ? (int)(cos(f->bob_phase*6.283185307179586/4096)*4) : (int)(f->bob_frame&3u);
                    if(!hmbtn_ui_icon(s->ui,s->pixels,s->session->house.inventory->equipped.id==0x39 ? 28 : 27,0,x,y+bob,error,error_size)) return false;
                }
            }
        }
        if(s->session->intro.player_marker) {
            int x,y;const int32_t point[3]={s->session->intro.player.position[0]+150,s->session->intro.player.position[1]-100,s->session->intro.player.position[2]-150};
            if(hmbtn_model_project_point(&camera,point,&x,&y)
                    && !hmbtn_ui_icon(s->ui,s->pixels,14,(s->session->runtime.ticks/2)&1u,x,y-50,error,error_size)) return false;
        }
        for(unsigned i=0;i<3;++i) if(s->session->carry.thrown[i].item.id && s->session->carry.thrown[i].area==s->session->area) {
            const HmbtnThrownItem *t=&s->session->carry.thrown[i];int x,y;
            const int32_t point[3]={t->position[0]+150,t->position[1]-100,t->position[2]-150};
            if(hmbtn_model_project_point(&camera,point,&x,&y)
                    && !hmbtn_house_ui_picture(s->house_ui,s->data_root,s->pixels,t->item.id,x-8,y-10,error,error_size)) return false;
        }
        }
        if (s->opening) {
            if (s->session->intro.cell_actor) {
                const HmbtnIntroActor *cell = hmbtn_farm_intro_actor(&s->session->intro,(int32_t)s->session->intro.cell_actor);
                if (!cell || s->session->intro.cell_actor > 4) { snprintf(error,error_size,"Invalid childhood illustration actor"); return false; }
                for (size_t i = 0; i < FRAME_BYTES; i += 4) memset(s->pixels+i,0,3);
                if (!hmbtn_animation_draw(&s->actor_animation[s->session->intro.cell_actor],s->mayor_atlas,
                        &cell->sprite,80,60,s->pixels,320,240,FRAME_BYTES,error,error_size)) return false;
            }
            if(s->session->completion_reported && !(hmbtn_read_le16(s->section+8)&0x1800u))
                for(unsigned i=0;i<HMBTN_WEATHER_PARTICLES;++i) {
                    const HmbtnWeatherParticle *p=&s->session->weather_effects.particles[i];
                    if(p->sprite && p->delay<0 && p->x>=-16 && p->x<336 && p->y>=-16 && p->y<256
                        && !hmbtn_ui_icon(s->ui,s->pixels,p->sprite,p->frame,p->x,p->y,error,error_size)) return false;
                }
            /* main_draw_ots: weather sprites are in world OT; screen_fill_quad
             * is in otTags[1], rendered afterwards and before UI in otTags[0].
             * Apply original subtractive weather light to the blue palette too. */
            if(s->session->completion_reported && !(hmbtn_read_le16(s->section+8)&0x1800u))
                hmbtn_daylight_apply(&s->session->daylight,s->pixels,FRAME_BYTES);
            const unsigned brightness = s->session->morning_fade ? 255*(45-s->session->morning_fade)/45 : s->session->overnight_tick ? 255*(45-s->session->overnight_tick)/45 : s->session->travel_tick ? (s->session->travel_tick<=11 ? 255*(11-s->session->travel_tick)/10 : 255*(s->session->travel_tick-11)/10) : s->session->sleep.active ? 255*(30-s->session->sleep.fade)/30 : hmbtn_farm_intro_brightness(&s->session->intro);
            for (size_t i = 0; i < FRAME_BYTES; ++i)
                if (i%4 != 3) s->pixels[i] = (uint8_t)((s->pixels[i]*brightness+(s->session->intro.fade_white ? 255*(255-brightness) : 0))/255);
            if(s->session->control.enabled && !s->session->travel_tick && !s->session->morning_fade && !s->session->overnight_tick && !hmbtn_house_ui_draw(s->house_ui,s->ui,&s->session->house,
                    s->data_root,s->session->runtime.ticks,s->pixels,error,error_size)) return false;
            if(s->session->menu.mode==HMBTN_MENU_RUCKSACK && !hmbtn_farm_menu_ui_draw(s->menu_ui,s->house_ui,s->ui,&s->session->menu,&s->session->house,&s->session->farm_action,
                    hmbtn_game_registration_const(&s->session->game),s->data_root,s->session->runtime.ticks,s->pixels,error,error_size)) return false;
            if(s->session->feed_shop.phase==HMBTN_SHOP_SELL_LIST || s->session->feed_shop.phase==HMBTN_SHOP_SELL_CONFIRM)
                for(size_t i=0;i<FRAME_BYTES;++i) if(i%4!=3) s->pixels[i]/=2;
            if(!hmbtn_feed_shop_ui_draw(&s->session->feed_shop,hmbtn_game_economy_const(&s->session->game),hmbtn_game_registration_const(&s->session->game),s->ui,s->pixels,s->session->runtime.ticks,error,error_size)) return false;
            if (!hmbtn_new_game_ui_draw(s->ui,s->session->animal_name_active ? &s->session->animal_name : s->session->feed_shop.phase==HMBTN_SHOP_NAME ? &s->session->feed_shop.editor : &s->session->dialogue,
                    s->session->runtime.ticks,s->pixels,error,error_size)) return false;
            if(s->session->menu.mode==HMBTN_MENU_STATUS && !hmbtn_farm_menu_ui_draw(s->menu_ui,s->house_ui,s->ui,&s->session->menu,&s->session->house,&s->session->farm_action,
                    hmbtn_game_registration_const(&s->session->game),s->data_root,s->session->runtime.ticks,s->pixels,error,error_size)) return false;
        }
        if (!SDL_UpdateTexture(s->texture,NULL,s->pixels,320*4)) goto sdl_error;
        s->dirty = false;
    }
    if (!SDL_SetRenderDrawColor(renderer,0,0,0,255) || !SDL_RenderClear(renderer)
            || !SDL_RenderTexture(renderer,s->texture,NULL,NULL)) goto sdl_error;
    return true;
sdl_error:
    snprintf(error,error_size,"Cannot present scene preview: %s",SDL_GetError()); return false;
}

HmbtnSession *hmbtn_scene_preview_session(HmbtnScenePreview *s) { return s ? s->session : NULL; }
HmbtnSceneResources hmbtn_scene_preview_resources(HmbtnScenePreview *s)
{
    return (HmbtnSceneResources){s->section,s->schedule_block,&s->scene,s->data_root,s->ui,s->house_ui,s->menu_ui,
        &s->estate_art,&s->village_art,&s->audio,&s->xa};
}
bool hmbtn_scene_preview_load_area(HmbtnScenePreview *s,unsigned area,char *error,size_t size)
{ return load_area(s,s->data_root,area,error,size); }
void hmbtn_scene_preview_invalidate(HmbtnScenePreview *s) { if(s) s->dirty=true; }
bool hmbtn_scene_preview_rebind(HmbtnScenePreview *s,const uint8_t *data,size_t bytes,char *error,size_t size)
{
    if(!s || !data || !bytes) { snprintf(error,size,"Missing schedule resources");return false; }
    uint8_t *copy=malloc(bytes);HmbtnHouseUI *ui=hmbtn_house_ui_create();
    if(!copy || !ui) { free(copy);hmbtn_house_ui_destroy(ui);snprintf(error,size,"Cannot rebind host resources");return false; }
    memcpy(copy,data,bytes);
    if(!hmbtn_session_bind_schedules(s->session,copy,bytes)) { free(copy);hmbtn_house_ui_destroy(ui);return false; }
    free(s->schedule_block);s->schedule_block=copy;
    hmbtn_house_ui_destroy(s->house_ui);s->house_ui=ui;
    s->session->intro.ground=&s->scene.terrain;
    hmbtn_session_runtime_rebind(s->session);s->dirty=true;return true;
}
bool hmbtn_scene_preview_area_location(HmbtnScenePreview *s,unsigned area,unsigned *location,char *error,size_t size)
{
    unsigned logical,resource;uint8_t header[16];
    if(!location || !hmbtn_world_area(area,s->session->game.clock.season,&logical,&resource)) return false;
    if(!hmbtn_resource_read(s->data_root,"A_FILE/00_gf.bin",scene_sections[resource][0],header,sizeof(header),error,size)) return false;
    *location=hmbtn_read_le16(header+2);return true;
}

bool hmbtn_scene_preview_set_weather(HmbtnScenePreview *s,unsigned today,unsigned tomorrow)
{
    if(!s || today>=HMBTN_WEATHER_KINDS || tomorrow>=HMBTN_WEATHER_KINDS) return false;
    s->session->game.weather.today=today;s->session->game.weather.tomorrow=tomorrow;
    hmbtn_village_refresh_weather(&s->session->village);
    hmbtn_daylight_weather_select(&s->session->daylight,&s->session->game.clock,today);
    hmbtn_weather_effects_tick(&s->session->weather_effects,today,true);
    s->session->house.tv_dirty=true;s->dirty=true;return true;
}
