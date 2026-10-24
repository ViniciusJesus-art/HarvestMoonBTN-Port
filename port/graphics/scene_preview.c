/* Farm scenery, arrival and the first childhood events. Geometry-only mode
 * keeps its diagnostic camera; New Game follows the original scripts.
 * Source: farm_load.c/farm_section_table.h, scene_view.c and event_script.c. */
#include "scene_preview.h"
#include "model_render.h"
#include "scene_decode.h"
#include "psx_data.h"
#include "resource_file.h"
#include "text_resources.h"
#include "pad_buttons.h"
#include "farm_intro.h"
#include "farm_clock.h"
#include "world_map.h"
#include "village_art.h"
#include "village_talk.h"
#include "house_control.h"
#include "farm_action.h"
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
#include "field_layouts.inc"
#include "scene_sections.inc"

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
    HmbtnAnimation event_player_animation;
    unsigned area;
    HmbtnFarmIntro intro;
    HmbtnHouseControl control;
    HmbtnHouseInteraction house;
    HmbtnHouseSleep sleep;
    HmbtnFarmClock clock;
    HmbtnDaylight daylight;
    unsigned overnight_tick,morning_fade;
    HmbtnHouseUI *house_ui;
    unsigned house_music_ticks, tv_frame;
    unsigned travel_tick;
    unsigned door_tick, door_model;
    int door_speed, door_angle;
    uint16_t door_base_angle,door_twin_base;
    int door_twin;
    HmbtnHouseExit travel_exit;
    uint8_t tv_sectors[0x1800];
    HmbtnNewGame dialogue;
    HmbtnVillageTalk talk;
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
    HmbtnFarmField field;
    HmbtnFarmAction farm_action;
    HmbtnFieldEffect field_effects[HMBTN_FIELD_EFFECTS];
    uint64_t watering_voice;
    HmbtnFarmCarry carry;
    HmbtnFarmFeed feed;
    HmbtnFeedShop feed_shop;
    HmbtnFarmGather gather;
    HmbtnEstateArt estate_art;
    HmbtnPoultry poultry;
    HmbtnPoultryArt poultry_art;
    unsigned animal_event;
    HmbtnNewGame animal_name;
    bool animal_name_active;
    uint8_t *feed_block;
    HmbtnImageAtlas *feed_atlas;
    HmbtnAnimation feed_art[2];
    HmbtnFarmShipping shipping;
    HmbtnVillage village;
    HmbtnVillageArt village_art;
    uint8_t *schedule_block;
    bool village_ready;
    int refused_zone;
    bool first_visit_started, first_visit_active;
    unsigned first_exit_tick;
    HmbtnHouseExit first_exit;
    HmbtnFarmMenu menu;
    HmbtnFarmMenuUI *menu_ui;
    unsigned quick_ticks,quick_id;
    bool quick_tool;
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

static bool load_area(HmbtnScenePreview *s, const char *root, unsigned area,
    char *error, size_t error_size)
{
    unsigned logical,resource_area;
    if(!hmbtn_world_area(area,s->clock.season,&logical,&resource_area)) {
        if(area==230 || area==231 || area==232) logical=resource_area=area;
        else {
            snprintf(error,error_size,"Area %u is not integrated",area);return false;
        }
    }
    area=logical;
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
    const size_t seasonal_size=seasonal_starts[s->clock.season+1]-seasonal_starts[s->clock.season];
    uint8_t *seasonal=ok && s->clock.season ? malloc(seasonal_size) : NULL;
    if(ok && s->clock.season && !seasonal) { snprintf(error,error_size,"Cannot allocate seasonal texture pack");ok=false; }
    if(ok && s->clock.season) {
        ok=hmbtn_resource_read(root,"A_FILE/00_gf.bin",seasonal_starts[s->clock.season],seasonal,seasonal_size,error,error_size)
            && hmbtn_bank_apply(seasonal+seasonal_ani[s->clock.season],seasonal_size-seasonal_ani[s->clock.season],s->atlas,error,error_size);
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
    hmbtn_model_renderer_destroy(s->geometry); free(s->section);
    s->section = section; s->scene = scene; s->geometry = geometry; s->area = area;s->dirty=true;
    if(s->opening && !hmbtn_estate_art_update(&s->estate_art,root,hmbtn_read_le16(section+10),error,error_size)) return false;
    bool hens_here=area==112 || hmbtn_poultry_held_slot(&s->poultry)<HMBTN_HENS;
    for(unsigned i=0;i<HMBTN_HENS;++i) hens_here|=s->poultry.hens[i].occupied && s->poultry.hens[i].area==area;
    if(s->opening && hens_here && !hmbtn_poultry_art_load(&s->poultry_art,root,error,error_size)) return false;
    s->carry.area=area;
    s->intro.ground=&s->scene.terrain;
    s->refused_zone=-1;
    if(s->village_ready) hmbtn_village_enter(&s->village);
    hmbtn_daylight_select(&s->daylight,&s->clock);
    if (area != 0) { free(s->field_block); s->field_block = NULL; free(s->field_atlas); s->field_atlas = NULL;
        memset(s->field_effects,0,sizeof(s->field_effects)); }
    else if (!load_field(s,root,error,error_size)
            || !hmbtn_farm_field_bind(&s->field,&s->scene.terrain)) {
        if(!error[0]) snprintf(error,error_size,"Cannot bind original farm field quads");
        return false;
    }
    if(area==0) s->field.season=s->clock.season;
    if(area==0 && (s->control.enabled || s->first_visit_started) && !hmbtn_farm_field_retexture(&s->field,
            s->section+(s->scene.terrain.data-s->section),s->scene.terrain.size)) {
        snprintf(error,error_size,"Cannot restore field soil textures");return false;
    }
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
    s->feed_shop.poultry=&s->poultry;
    hmbtn_house_interaction_init(&s->house);
    hmbtn_farm_clock_init(&s->clock);
    hmbtn_new_game_init(&s->dialogue);
    hmbtn_farm_field_init(&s->field,title_layout0_cells,1125);
    hmbtn_farm_action_init(&s->farm_action);
    hmbtn_farm_menu_init(&s->menu);
    s->menu.poultry=&s->poultry;
    s->menu_ui=hmbtn_farm_menu_ui_create();
    if(!s->menu_ui) goto allocation;
    /* Texture uploads preserve the rest of PS1 VRAM. In particular, seasonal
     * packs can leave padding from earlier sections outside their rectangles. */
    s->pixels = malloc(FRAME_BYTES); s->atlas = calloc(1,sizeof(*s->atlas));
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
    if (event!=12 && event!=172 && event!=299 && (event < 116 || event > 121)) { snprintf(error,error_size,"Event %u is not integrated",event); return false; }
    const uint32_t start = event==172 ? 0x417800 : event==299 ? 0x6D2000 : event==12 ? 0x36000 : offsets[event-116];
    const size_t bytes = event==172 ? 0x6000 : event==299 ? 0x800 : event==12 ? 0xb800 : offsets[event-115]-start;
    free(s->event_block); s->event_block = malloc(bytes);
    if (!s->event_block) { snprintf(error,error_size,"Cannot allocate bounded opening event"); return false; }
    HmbtnEventBlock block;
    if (!hmbtn_resource_read(s->data_root,"A_FILE/12_event_s.bin",start,s->event_block,bytes,error,error_size)
            || !hmbtn_event_block_open(&block,s->event_block,bytes,error,error_size)) return false;
    memset(s->mayor_atlas,0,sizeof(*s->mayor_atlas));
    memset(s->actor_animation,0,sizeof(s->actor_animation));
    memset(s->intro.childhood,0,sizeof(s->intro.childhood));
    memset(&s->intro.mayor,0,sizeof(s->intro.mayor));
    memset(&s->intro.ellen,0,sizeof(s->intro.ellen));memset(&s->intro.zack,0,sizeof(s->intro.zack));
    memset(&s->intro.carter,0,sizeof(s->intro.carter));memset(&s->intro.barley,0,sizeof(s->intro.barley));
    memset(&s->intro.rick,0,sizeof(s->intro.rick));
    memset(&s->event_player_animation,0,sizeof(s->event_player_animation));
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
        } else if(event==12 && flags==0x401) {
            if(!hmbtn_animation_open(&s->event_player_animation,entry,size,error,error_size)) return false;
        } else { snprintf(error,error_size,"Unsupported opening actor flags 0x%04x",flags); return false; }
    }
    const uint64_t expected = event==299 ? 0 : event==172 ? (UINT64_C(1)<<10)|(UINT64_C(1)<<15)|(UINT64_C(1)<<19)
        : event==12 ? (UINT64_C(1)<<17)|(UINT64_C(1)<<23)|(UINT64_C(1)<<26)
        : event == 116 || event == 121 ? UINT64_C(1)<<17
        : event == 117 ? 14u : event == 118 ? 2u : event == 120 ? 30u
        : (UINT64_C(1)<<27)|(UINT64_C(1)<<33);
    if (!script || actor_mask != expected || banks != (event==299 ? 0u : 1u)) {
        snprintf(error,error_size,"Opening event is missing its expected actors, textures or script"); return false;
    }
    if (event == 116) {
        if (!hmbtn_farm_intro_open(&s->intro,script,script_size,error,error_size)) return false;
    } else if (!hmbtn_event_script_open(&s->intro.script,script,script_size,error,error_size)) return false;
    memset(s->intro.slots,0,sizeof(s->intro.slots));
    s->intro.event = event; s->intro.event_pending = false; s->intro.queued_event = 0;
    s->intro.ground=&s->scene.terrain;
    s->loaded_cell = UINT32_MAX;
    s->intro.cell_actor = 0; s->intro.camera_actor = 0; s->intro.camera_gliding = false;
    s->intro.message = 0; s->intro.message_done = false; s->loaded_message = 0;
    s->intro.text_bank=UINT32_MAX;s->intro.zone_exited=false;s->intro.advanced_minutes=0;
    s->intro.finish_requested=false;s->intro.action_ready=s->intro.action_active=false;
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
        s->clock=*clock;
    }
    HmbtnHouseExit arrival={0};bool found=false;
    unsigned facing=0;
    /* Use a real incoming passage, rather than a guessed point in the map.
     * The house's exit is first so region 0 starts beside the front door. */
    const unsigned sources[]={80,0,24,28,32,36,40,44,48,52,56,60,64,68,72,76,
        128,132,136,140,144,148,152,156,160,164,168,172,176,180,184,188,192,196,200,204,208,212};
    for(unsigned n=0;n<sizeof(sources)/sizeof(sources[0]) && !found;++n) {
        uint8_t zones[16+50*44];
        if(!hmbtn_resource_read(root,"A_FILE/00_gf.bin",scene_sections[sources[n]+s->clock.season][0],
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
    s->intro.area=area;s->first_visit_started=true;
    memcpy(s->intro.player.position,arrival.entry,sizeof(arrival.entry));
    HmbtnGroundHit ground;
    if(hmbtn_ground_find(&s->scene.terrain,s->intro.player.position,0xC080,200,&ground))
        s->intro.player.position[1]=ground.height;
    s->intro.player.facing=facing;s->intro.player.sprite=(HmbtnAnimationPlayer){0,(uint16_t)facing,0};
    memcpy(s->intro.camera,s->intro.player.position,sizeof(s->intro.camera));
    hmbtn_world_camera(s->section,s->scene.terrain.size+0x2000,s->intro.camera);
    s->centre_x=s->intro.camera[0];s->centre_z=s->intro.camera[2];
    printf("Region test entry: area %u; season %u, day %u at %02u:%02u; fresh session, Thomas visit skipped.\n",
        area,s->clock.season,s->clock.day+1,s->clock.hour,s->clock.minute);
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

static void sync_clock(HmbtnScenePreview *s)
{
    s->house.year=s->clock.year;s->house.season=s->clock.season;s->house.day=s->clock.day;
    s->house.weekday=s->clock.weekday;s->house.hour=s->clock.hour;s->house.minute=s->clock.minute;
}
static unsigned world_music_volume(const HmbtnScenePreview *s)
{
    return hmbtn_world_music_volume(s->section,16,s->clock.season,s->clock.hour);
}
static bool farm_song(HmbtnScenePreview *s,unsigned song,char *error,size_t error_size)
{
    if(s->audio_song==song && s->completion_reported) return true;
    hmbtn_title_audio_close(&s->audio);
    if(!hmbtn_title_audio_open_song(&s->audio,s->data_root,song,error,error_size)) return false;
    uint8_t *fx=malloc(227328);
    if(!fx) { snprintf(error,error_size,"Cannot allocate farm effects bank");return false; }
    const bool ok=hmbtn_resource_read(s->data_root,"A_FILE/09_sound.stm",24576,fx,227328,error,error_size)
        && hmbtn_sound_bank(s->audio.sound,2,fx,10240,fx+10240,217088,error,error_size);
    free(fx);if(!ok) return false;
    s->watering_voice=0;s->audio_song=song;s->audio.xa=&s->xa;hmbtn_sound_start(s->audio.sound);
    hmbtn_sound_stereo(s->audio.sound,s->house.options.stereo);return true;
}
static void world_clock_events(void *context,const HmbtnFarmClock *clock,unsigned changes)
{
    HmbtnScenePreview *s=context;
    hmbtn_farm_shipping_clock(&s->shipping,&s->menu.stats,clock,changes);
    if(s->village_ready) hmbtn_village_clock(&s->village,clock,changes,s->house.weather,false);
    if(changes&HMBTN_CLOCK_HOUR) hmbtn_poultry_hour(&s->poultry,clock,s->house.weather);
    if(changes&HMBTN_CLOCK_DAY) {
        hmbtn_poultry_new_day(&s->poultry,&s->feed,clock);
        hmbtn_poultry_carry_sync(&s->poultry,&s->house);
        hmbtn_farm_feed_new_day(&s->feed);hmbtn_farm_gather_new_day(&s->gather);
    }
}
static HmbtnEventResult animal_command(void *context,const HmbtnEventCommand *c,char *error,size_t size)
{
    HmbtnScenePreview *s=context;
    if(c->type==0x51 && c->command==14 && s->animal_event==172) {
        if(c->operand==0 && c->x>=0 && c->x<=2 && c->y>=0 && c->y<=1)
            return c->x==2 && hmbtn_poultry_dead(&s->poultry,c->y==0) ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
        if(c->operand==1 && !c->x && !c->y) {
            s->carry.discarded+=hmbtn_poultry_clear_dead(&s->poultry);
            if(s->carry.discarded>99) s->carry.discarded=99;
            return HMBTN_EVENT_DONE;
        }
    }
    if(c->type==0x51 && c->command==13 && s->animal_event==299 && !c->y) {
        if(c->operand==0 && !c->x) return HMBTN_EVENT_DONE;
        if(c->operand==1 && c->x>=0 && c->x<=3)
            return c->x>=2 && hmbtn_poultry_hatch_ready(&s->poultry,(unsigned)c->x-2) ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
        if(c->operand==2 && c->x>=2 && c->x<=3) {
            HmbtnHen *hen=hmbtn_poultry_hatch_ready(&s->poultry,(unsigned)c->x-2);
            if(!hen) { snprintf(error,size,"Chick naming lost its reserved slot");return HMBTN_EVENT_ERROR; }
            if(!s->animal_name_active) {
                hmbtn_name_editor_begin(&s->animal_name);s->animal_name.no_cancel=true;s->animal_name_active=true;
                return HMBTN_EVENT_WAIT;
            }
            if(!s->animal_name.finished || s->animal_name.message.phase!=HMBTN_MESSAGE_CLOSED) return HMBTN_EVENT_WAIT;
            memcpy(hen->name,s->animal_name.name,sizeof(hen->name));s->animal_name_active=false;return HMBTN_EVENT_DONE;
        }
        if(c->operand==3 && !c->x) { hmbtn_poultry_name_end(&s->poultry);return HMBTN_EVENT_DONE; }
    }
    if(c->type==0x5B && s->animal_event==172 && c->command==32 && c->operand==1 && c->x==-20 && c->y==-1000) {
        /* event_cmd_apply_var_table: bachelorettes use affection, others friendship. */
        for(unsigned i=1;i<HMBTN_VILLAGERS;++i) if(i!=27 && i!=33) {
            HmbtnVillager *v=&s->village.people[i];
            if(i==3 || i==7 || i==9 || i==14 || i==20) v->affection=v->affection>1000 ? v->affection-1000 : 0;
            else v->friendship=v->friendship>20 ? v->friendship-20 : 0;
        }
        for(unsigned i=0;i<7;++i) s->menu.stats.helper_affection[i]=s->menu.stats.helper_affection[i]>20 ? s->menu.stats.helper_affection[i]-20 : 0;
        return HMBTN_EVENT_DONE;
    }
    snprintf(error,size,"Unsupported animal event command %u/%d/%d",c->type,c->command,c->operand);
    return HMBTN_EVENT_ERROR;
}
static bool animal_event_begin(HmbtnScenePreview *s,unsigned event,char *error,size_t size)
{
    if(!load_event(s,event,error,size)) return false;
    s->animal_event=event;s->control.enabled=false;s->control.mode=0;
    s->intro.game_command=animal_command;s->intro.game_context=s;s->intro.event_active=true;
    s->intro.player.pose=false;s->intro.player.sprite=(HmbtnAnimationPlayer){0,(uint16_t)s->intro.player.facing,0};
    memset(s->intro.player.velocity,0,sizeof(s->intro.player.velocity));
    s->intro.fade_level=0;s->intro.fade_mode=0;s->intro.fade_length=0;s->intro.music_mode=0;
    return true;
}
static bool village_conversation(HmbtnScenePreview *s,uint32_t pressed,uint32_t held,char *error,size_t size)
{
    HmbtnVillageTalk *t=&s->talk;
    if(t->pending && t->loaded && !t->message_done) {
        HmbtnMessage *m=&s->dialogue.message;
        if(m->choices && m->phase==HMBTN_MESSAGE_WAIT_END && (pressed&(HMBTN_PAD_UP|HMBTN_PAD_DOWN))) {
            s->dialogue.choice=(s->dialogue.choice+((pressed&HMBTN_PAD_UP) ? m->choices-1 : 1))%m->choices;
            hmbtn_sound_effect(s->audio.sound,5);
        }
        const int sound=hmbtn_message_tick(m,pressed,held);
        if(sound>=0) hmbtn_sound_effect(s->audio.sound,(unsigned)sound);
        if(m->phase==HMBTN_MESSAGE_CLOSED) {
            t->message_done=true;t->result=m->choices ? s->dialogue.choice+1 : 0;
        }
    }
    if(!hmbtn_village_talk_tick(t,error,size)) return false;
    if(t->pending && !t->loaded && t->active) {
        uint8_t record[HMBTN_TEXT_RECORD_BYTES];
        const HmbtnTextResourceId id={HMBTN_TEXT_RECORD,t->bank,t->request};
        if(!hmbtn_text_resource_read(s->data_root,id,record,sizeof(record),error,size)
                || !hmbtn_feed_shop_expand(record,sizeof(record),&s->dialogue)
                || !hmbtn_ui_message_record(s->ui,record,sizeof(record),&s->dialogue.message,error,size)) return false;
        s->dialogue.step=100;s->dialogue.choice=0;
        s->dialogue.message.suppress_cursor=s->dialogue.message.choices!=0;t->loaded=true;
    }
    if(t->script.boundary) {
        fprintf(stderr,"NPC %u conversation stopped at unported command %u, step %u.\n",
            t->target,t->boundary_type,t->script.pc);
    }
    s->control.mode=0;
    s->intro.player.sprite=(HmbtnAnimationPlayer){0,(uint16_t)s->intro.player.facing,0};
    return true;
}
static bool feed_shop_message(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnFeedShop *shop=&s->feed_shop;
    if(shop->phase==HMBTN_SHOP_NAME) {
        if(!hmbtn_new_game_ui_load(s->ui,s->data_root,&shop->editor,error,size)) return false;
        if(shop->sound>=0) hmbtn_sound_effect(s->audio.sound,(unsigned)shop->sound);
        s->dialogue.message=(HmbtnMessage){0};return true;
    }
    if(shop->phase!=HMBTN_SHOP_OFF && shop->request!=shop->loaded) {
        uint8_t record[HMBTN_TEXT_RECORD_BYTES];
        const HmbtnTextResourceId id={HMBTN_TEXT_RECORD,2,shop->request};
        if(!hmbtn_text_resource_read(s->data_root,id,record,sizeof(record),error,size)) return false;
        if(!hmbtn_feed_shop_expand(record,sizeof(record),&s->dialogue)) {
            snprintf(error,size,"Cannot expand feed shop names");return false;
        }
        if(!hmbtn_ui_message_record(s->ui,record,sizeof(record),&shop->message,error,size)) return false;
        shop->message.suppress_cursor=shop->phase==HMBTN_SHOP_SELECT || shop->phase==HMBTN_SHOP_COUNT
            || shop->phase==HMBTN_SHOP_SELL_LIST || shop->phase==HMBTN_SHOP_SELL_CONFIRM;
        shop->loaded=shop->request;
    }
    if(shop->sound>=0) hmbtn_sound_effect(s->audio.sound,(unsigned)shop->sound);
    s->dialogue.message=shop->message;
    s->intro.player.sprite=(HmbtnAnimationPlayer){0,(uint16_t)s->intro.player.facing,0};
    s->control.mode=0;return true;
}
static bool morning(HmbtnScenePreview *s,bool slept,char *error,size_t error_size)
{
    const unsigned bedtime=s->clock.hour*60+s->clock.minute;
    if(slept) hmbtn_farm_clock_advance_notify(&s->clock,
        hmbtn_farm_clock_sleep_minutes(&s->clock,s->farm_action.fatigue),world_clock_events,s);
    sync_clock(s);
    if(slept) {
        const unsigned hours=(1440+s->clock.hour*60+s->clock.minute-bedtime)/60;
        const unsigned gain=hours*7+50,loss=hours*2+5;
        s->farm_action.stamina=s->farm_action.stamina+gain>100 ? 100 : s->farm_action.stamina+gain;
        s->farm_action.fatigue=s->farm_action.fatigue>loss ? s->farm_action.fatigue-loss : 0;
    } else {
        s->farm_action.fatigue+=10;
        if(s->farm_action.fatigue>99) s->farm_action.fatigue=99;
    }
    /* Field reset is separate from the ordered feed/egg observer above. */
    hmbtn_farm_field_new_day(&s->field,s->clock.season);
    const bool wanted_save=s->house.save_requested;
    memset(&s->sleep,0,sizeof(s->sleep));s->house.mode=HMBTN_HOUSE_FREE;s->house.save_requested=false;
    memset(&s->house.message,0,sizeof(s->house.message));s->house.request=s->house.loaded=0;
    memset(&s->dialogue.message,0,sizeof(s->dialogue.message));
    s->intro.action_active=false;s->intro.player.pose=false;s->intro.player.visible=true;s->intro.player.facing=0;
    s->intro.player.sprite=(HmbtnAnimationPlayer){0};
    s->intro.player.position[0]=-90;s->intro.player.position[1]=0;s->intro.player.position[2]=541;
    memset(s->intro.player.velocity,0,sizeof(s->intro.player.velocity));
    s->control=(HmbtnHouseControl){0};s->control.enabled=true;
    s->farm_action.phase=HMBTN_FARM_ACTION_FREE;s->farm_action.bank=-1;
    const unsigned discarded=s->carry.discarded;
    memset(&s->carry,0,sizeof(s->carry));s->carry.discarded=discarded;
    memset(s->field_effects,0,sizeof(s->field_effects));
    s->travel_tick=s->door_tick=s->overnight_tick=0;s->morning_fade=45;
    if(!load_area(s,s->data_root,80,error,error_size)) return false;
    s->intro.area=80;s->intro.camera_actor=0;s->intro.camera_gliding=false;
    memcpy(s->intro.camera,s->intro.player.position,sizeof(s->intro.camera));
    s->house.tv_dirty=true;s->tv_frame=UINT32_MAX;memset(s->tv_sectors,0,sizeof(s->tv_sectors));
    if(!farm_song(s,s->clock.season+1,error,error_size)) return false;
    hmbtn_sound_start(s->audio.sound);s->intro.stop_music=false;s->intro.music_volume=0;
    printf("Morning: year %u, season %u, day %u at %02u:%02u; session state retained.%s\n",
        s->clock.year+1,s->clock.season,s->clock.day+1,s->clock.hour,s->clock.minute,
        wanted_save ? " Save writing is not implemented; no save was written." : "");
    return true;
}
static bool opening_tick(HmbtnScenePreview *s, uint32_t held, char *error, size_t error_size)
{
    /* The source's global fade keeps running after FARM_REQUEST_FINISH. */
    if(s->completion_reported && s->intro.script.finished && s->intro.fade_mode==1 && s->intro.fade_level) --s->intro.fade_level;
    if(s->morning_fade) {
        --s->morning_fade;s->intro.music_volume=world_music_volume(s)*(45-s->morning_fade)/45;
        goto opening_done;
    }
    if(s->overnight_tick) {
        if(++s->overnight_tick>=45 && !morning(s,false,error,error_size)) return false;
        goto opening_done;
    }
    if(s->completion_reported && s->control.enabled && s->area==80 && !s->animal_event
            && !s->intro.fade_level
            && !s->travel_tick && !s->door_tick && !s->sleep.active && !s->control.exit_pending
            && s->house.mode==HMBTN_HOUSE_FREE && s->menu.mode==HMBTN_MENU_NONE && !s->carry.phase && !s->quick_ticks) {
        unsigned event=hmbtn_poultry_dead(&s->poultry,false) || hmbtn_poultry_dead(&s->poultry,true) ? 172
            : hmbtn_poultry_hatch_ready(&s->poultry,0) || hmbtn_poultry_hatch_ready(&s->poultry,1) ? 299 : 0;
        if(event && !animal_event_begin(s,event,error,error_size)) return false;
    }
    if(s->animal_name_active) {
        hmbtn_new_game_tick(&s->animal_name,s->pending_pressed,held);
        if(!hmbtn_new_game_ui_load(s->ui,s->data_root,&s->animal_name,error,error_size)) return false;
        if(s->animal_name.sound>=0) hmbtn_sound_effect(s->audio.sound,(unsigned)s->animal_name.sound);
    }
    /* The tour first leaves through the farm's real north exit. Keep its
     * script cursor suspended while the fade and resource load complete. */
    if(s->first_exit_tick) {
        const unsigned tick=++s->first_exit_tick;
        s->intro.fade_length=24;s->intro.fade_mode=0;
        s->intro.fade_level=tick;
        if(tick==24) {
            if(!load_area(s,s->data_root,s->first_exit.target,error,error_size)) return false;
            s->intro.area=s->area;
            memcpy(s->intro.player.position,s->first_exit.entry,sizeof(s->intro.player.position));
            memcpy(s->intro.camera,s->intro.player.position,sizeof(s->intro.camera));
            memset(s->intro.mayor.velocity,0,sizeof(s->intro.mayor.velocity));
            s->intro.mayor.visible=false;
            /* The next script command supplies the fade in. */
            s->first_exit_tick=0;s->intro.zone_exited=true;
        }
        goto opening_done;
    }
    if (!s->animal_name_active && s->intro.message && s->loaded_message == s->intro.message) {
        if(s->first_visit_active && s->dialogue.message.choices && s->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END) {
            if(s->pending_pressed&(HMBTN_PAD_UP|HMBTN_PAD_DOWN)) {
                s->dialogue.choice^=1u;hmbtn_sound_effect(s->audio.sound,5);
            }
        }
        const int sound = hmbtn_message_tick(&s->dialogue.message,s->pending_pressed,held);
        if (sound >= 0) hmbtn_sound_effect(s->audio.sound,(unsigned)sound);
        if(!hmbtn_ui_message_portrait(s->ui,&s->dialogue.message,error,error_size)) return false;
        s->intro.text_bank=s->dialogue.message.bank;
        if (s->dialogue.message.phase == HMBTN_MESSAGE_CLOSED) {
            s->intro.message_done = true;
            if(s->first_visit_active && s->dialogue.message.choices) s->intro.slots[0]=(int32_t)s->dialogue.choice+1;
        }
    }
    hmbtn_xa_stream_tick(&s->xa); s->intro.xa_active = s->xa.active;
    const unsigned previous = s->intro.script.pc;
    if (!hmbtn_farm_intro_tick(&s->intro,error,error_size)) return false;
    if(s->first_visit_active && s->area==0 && !s->intro.zone_exited
            && s->intro.player.velocity[2]>0 && hmbtn_house_exit_at(s->section,
                s->scene.terrain.size+0x2000,s->intro.player.position,&s->first_exit)) {
        s->first_exit_tick=1;s->intro.fade_mode=0;s->intro.fade_length=24;s->intro.fade_level=1;
        memset(s->intro.player.velocity,0,sizeof(s->intro.player.velocity));
    }
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
        if(!s->action_block) s->action_block = malloc(0x3800);
        if(!s->action_atlas) s->action_atlas = malloc(sizeof(*s->action_atlas));
        if (!s->action_block || !s->action_atlas) {
            snprintf(error,error_size,"Cannot allocate the player's action bank"); return false;
        }
        const unsigned at=s->animal_event==172 ? 0x3CC6800 : 0x3CE2800,bank=s->animal_event==172 ? 508 : 352;
        if (!hmbtn_resource_read(s->data_root,"A_FILE/00_gf.bin",at,s->action_block,0x3800,error,error_size)
                || !hmbtn_animation_open(&s->action_animation,s->action_block,bank,error,error_size)
                || !hmbtn_bank_decode(s->action_block+bank,0x3800-bank,s->action_atlas,error,error_size)) return false;
        s->intro.action_pending = false; s->intro.action_ready = true;
    }
    if (s->intro.song_pending) {
        hmbtn_title_audio_close(&s->audio);
        const unsigned song=s->animal_event==172 ? 10 : 34;
        if (!hmbtn_title_audio_open_song(&s->audio,s->data_root,song,error,error_size)) return false;
        s->audio_song = s->intro.music_mode = song; s->audio.xa = &s->xa; s->intro.song_pending = false;
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
    if(s->animal_event && s->intro.script.finished && s->intro.finish_requested) {
        s->animal_event=0;s->intro.game_command=NULL;s->intro.game_context=NULL;
        s->intro.carter.visible=s->intro.barley.visible=s->intro.rick.visible=false;
        s->intro.action_active=false;s->intro.action_ready=false;s->intro.player.pose=false;
        s->intro.player.sprite=(HmbtnAnimationPlayer){0,(uint16_t)s->intro.player.facing,0};
        s->control.enabled=true;s->control.exit_pending=false;
        hmbtn_farm_clock_advance_notify(&s->clock,s->intro.advanced_minutes,world_clock_events,s);sync_clock(s);
        s->intro.advanced_minutes=0;hmbtn_daylight_select(&s->daylight,&s->clock);
        s->menu.stats.animals[0]=hmbtn_poultry_count(&s->poultry);
        if(!farm_song(s,hmbtn_world_song(s->section,16,s->clock.season,s->clock.hour),error,error_size)) return false;
        s->intro.stop_music=false;s->intro.music_volume=world_music_volume(s);s->house_music_ticks=100;
    }
    if(s->first_visit_active && s->intro.script.finished && s->intro.finish_requested) {
        s->first_visit_active=false;s->control.enabled=true;s->control.exit_pending=false;
        s->intro.mayor.visible=s->intro.ellen.visible=s->intro.zack.visible=false;
        s->intro.player.sprite=(HmbtnAnimationPlayer){0,(uint16_t)s->intro.player.facing,0};
        s->intro.player_marker=false;
        hmbtn_farm_clock_advance_notify(&s->clock,s->intro.advanced_minutes,world_clock_events,s);sync_clock(s);
        hmbtn_daylight_select(&s->daylight,&s->clock);
        s->intro.music_volume=hmbtn_farm_music_volume(s->section,16);
        s->house_music_ticks=100;
        puts("First morning complete: Thomas/Zack encounter; farm control restored.");
    }
    if (s->intro.script.finished && s->intro.finish_requested && !s->completion_reported) {
        /* farm_track_id: sunny spring, sceneId 999 -> song 1. The farm's
         * effects VAB is independent of both common UI effects and music. */
        if(!farm_song(s,1,error,error_size)) return false;
        s->house_ui=hmbtn_house_ui_create();
        if(!s->house_ui) { snprintf(error,error_size,"Cannot allocate house interface"); return false; }
        s->audio_song=1; s->audio.xa=&s->xa; hmbtn_sound_start(s->audio.sound);
        s->intro.stop_music=false; s->intro.music_volume=0;
        s->control.enabled=true;
        s->schedule_block=malloc(0xA800);
        if(!s->schedule_block || !hmbtn_resource_read(s->data_root,"A_FILE/00_gf.bin",0x74800,
                s->schedule_block,0xA800,error,error_size)) return false;
        hmbtn_village_init(&s->village,s->schedule_block,0xA800);s->village_ready=true;
        s->intro.action_active=false; s->intro.player.pose=false;
        puts("Opening complete: farmhouse interior (area 80). Movement, house/farm interactions, clock/calendar and day/night enabled. Save/load and remaining world systems are pending.");
        fflush(stdout); s->completion_reported = true;
    }
    if(s->control.enabled) {
        if(s->talk.active) {
            if(!village_conversation(s,s->pending_pressed,held,error,error_size)) return false;
            goto house_done;
        }
        if(s->feed_shop.phase!=HMBTN_SHOP_OFF) {
            hmbtn_feed_shop_tick(&s->feed_shop,&s->house,&s->menu.stats,s->pending_pressed,held);
            if(s->feed_shop.phase==HMBTN_SHOP_OFF) {
                HmbtnIntroActor *merchant=&s->village.people[s->feed_shop.merchant].actor;
                merchant->facing=s->feed_shop.saved_facing;
                merchant->sprite=(HmbtnAnimationPlayer){0,(uint16_t)merchant->facing,0};
            }
            if(!feed_shop_message(s,error,error_size)) return false;
            goto house_done;
        }
        if(!s->travel_tick && !s->door_tick && s->house.mode==HMBTN_HOUSE_FREE) {
            const bool opened=!s->carry.phase && !s->feed.phase && !s->poultry.carry_phase && s->farm_action.phase==HMBTN_FARM_ACTION_FREE
                && hmbtn_farm_menu_open(&s->menu,s->pending_pressed);
            if(s->menu.mode!=HMBTN_MENU_NONE) {
                if(opened) { hmbtn_sound_release_voice(s->audio.sound,s->watering_voice);s->watering_voice=0; }
                else hmbtn_farm_menu_tick(&s->menu,&s->house,s->pending_pressed,held);
                if(s->menu.sound>=0) hmbtn_sound_effect(s->audio.sound,(unsigned)s->menu.sound);
                if(s->menu.request && s->menu.loaded!=s->menu.request) {
                    uint8_t record[8192];
                    if(!hmbtn_resource_read(s->data_root,"A_FILE/05_messege3.bin",s->menu.request*8192u,record,sizeof(record),error,error_size)) return false;
                    HmbtnHouseInteraction context=s->house;
                    const HmbtnStoredTool *selected=hmbtn_inventory_slot(&s->house,s->menu.cursor);
                    context.cursor=0;if(selected) context.equipped=*selected;
                    if(!hmbtn_house_expand_reading(&context,record,sizeof(record),s->dialogue.dog_name,s->dialogue.farm_name)
                            || !hmbtn_ui_message_record(s->ui,record,sizeof(record),&s->menu.message,error,error_size)) return false;
                    s->menu.message.suppress_cursor=!s->menu.detail;s->menu.loaded=s->menu.request;
                }
                s->dialogue.message=s->menu.message;
                goto house_done;
            }
        }
        if(!s->travel_tick && !s->door_tick && !s->sleep.active && !s->morning_fade) {
            hmbtn_farm_carry_flight(&s->carry,s->area==0 ? &s->field : NULL,&s->scene.terrain,&s->shipping,&s->menu.stats);
            if(s->carry.sound>=0) {
                const HmbtnFarmSound *fx=hmbtn_farm_sound((unsigned)s->carry.sound);
                if(!hmbtn_sound_key_on(s->audio.sound,2,fx->program,fx->tone,fx->note,fx->volume)) return false;
            }
            if(s->carry.changed && !hmbtn_farm_field_retexture(&s->field,
                    s->section+(s->scene.terrain.data-s->section),s->scene.terrain.size)) return false;
        }
        s->menu.stats.discarded=s->carry.discarded;
        if(!s->carry.phase && !s->house.held_item.id && !s->quick_ticks && !s->travel_tick && !s->door_tick
                && !s->control.jumping && !s->control.exit_pending && s->house.mode==HMBTN_HOUSE_FREE
                && s->farm_action.phase==HMBTN_FARM_ACTION_FREE
                && (s->pending_pressed&HMBTN_PAD_CROSS)) {
            bool taken=(s->area==112 || s->area==0) ? hmbtn_poultry_take_egg(&s->poultry,s->area,&s->house,&s->carry,&s->intro.player)
                : hmbtn_farm_gather_take(&s->gather,hmbtn_read_le16(s->section+2),s->clock.season,&s->house,&s->carry,&s->intro.player);
            if(taken) hmbtn_sound_effect(s->audio.sound,7);
        }
        if(s->poultry.carry_phase || (!s->carry.phase && !s->feed.phase && !s->quick_ticks && !s->travel_tick && !s->door_tick
                && !s->control.jumping && !s->control.exit_pending && s->house.mode==HMBTN_HOUSE_FREE
                && s->farm_action.phase==HMBTN_FARM_ACTION_FREE && (s->pending_pressed&HMBTN_PAD_CROSS))) {
            HmbtnHouseExit door;unsigned model;
            const bool at_door=!s->poultry.carry_phase && hmbtn_world_door_at(s->section,0x2000,&s->intro.player,&door,&model);
            if(!at_door && hmbtn_poultry_carry_tick(&s->poultry,&s->house,&s->intro.player,s->area,
                    &s->scene.terrain,s->area==0 ? &s->field : NULL,s->pending_pressed)) {
                if(s->poultry.sound>=0) hmbtn_sound_effect(s->audio.sound,(unsigned)s->poultry.sound);
                s->control.carrying=s->house.held_item.id!=0;
                s->control.carrying_big=hmbtn_poultry_held_slot(&s->poultry)<HMBTN_HENS;
                s->control.mode=0;memcpy(s->intro.camera,s->intro.player.position,sizeof(s->intro.camera));
                goto house_done;
            }
        }
        if(s->carry.phase || (s->area==0 && !s->quick_ticks && !s->travel_tick && !s->door_tick
                && !s->control.jumping && s->house.mode==HMBTN_HOUSE_FREE
                && s->farm_action.phase==HMBTN_FARM_ACTION_FREE && (s->pending_pressed&HMBTN_PAD_CROSS))) {
            if(hmbtn_farm_carry_tick(&s->carry,&s->house,s->area==0 ? &s->field : NULL,
                    &s->intro.player,s->pending_pressed)) {
                if(s->carry.sound>=0) {
                    const HmbtnFarmSound *fx=hmbtn_farm_sound((unsigned)s->carry.sound);
                    if(!hmbtn_sound_key_on(s->audio.sound,2,fx->program,fx->tone,fx->note,fx->volume)) return false;
                }
                if(s->carry.changed && !hmbtn_farm_field_retexture(&s->field,
                        s->section+(s->scene.terrain.data-s->section),s->scene.terrain.size)) return false;
                s->control.mode=0;
                memcpy(s->intro.camera,s->intro.player.position,sizeof(s->intro.camera));
                goto house_done;
            }
        }
        if(s->quick_ticks) {
            if(!--s->quick_ticks) {
                s->quick_id=0;s->intro.player.sprite=(HmbtnAnimationPlayer){0,(uint16_t)s->intro.player.facing,0};
            }
            goto house_done;
        }
        if(hmbtn_poultry_held_slot(&s->poultry)==HMBTN_HENS && !s->control.jumping && !s->control.exit_pending && s->house.mode==HMBTN_HOUSE_FREE
                && s->farm_action.phase==HMBTN_FARM_ACTION_FREE) {
            const bool tool=(s->pending_pressed&HMBTN_PAD_R1)!=0 || ((held&HMBTN_PAD_R1) && (s->pending_pressed&(HMBTN_PAD_LEFT|HMBTN_PAD_RIGHT))),
                item=(s->pending_pressed&HMBTN_PAD_R2)!=0 || ((held&HMBTN_PAD_R2) && (s->pending_pressed&(HMBTN_PAD_LEFT|HMBTN_PAD_RIGHT)));
            const bool storing=item && !tool && s->house.held_item.id!=0;
            if((tool || item) && hmbtn_inventory_cycle(&s->house,tool,(s->pending_pressed&HMBTN_PAD_LEFT)!=0)) {
                s->quick_tool=tool;s->quick_id=tool ? s->house.equipped.id : s->house.held_item.id;s->quick_ticks=10;
                s->control.mode=0;
                s->intro.player.sprite=(HmbtnAnimationPlayer){(uint16_t)(tool ? (s->quick_id ? 71u : 0u) : (storing ? 29u : 11u)+s->intro.player.facing),0,0};s->intro.player.animation_done=false;
                if(tool && !s->quick_id) { s->quick_ticks=1;s->intro.player.sprite.frame=(uint16_t)s->intro.player.facing; }
                const HmbtnFarmSound *fx=hmbtn_farm_sound(7);
                if(!hmbtn_sound_key_on(s->audio.sound,2,fx->program,fx->tone,fx->note,fx->volume)) return false;
                goto house_done;
            }
        }
        if(s->area==0) hmbtn_field_effect_tick(s->field_effects,s->field_art);
        if(s->door_tick) {
            /* doors.c: facing determines the swing sign, acceleration 8 and
             * limit 0x420. Walking timing remains a limited adaptation. */
            if(s->door_angle>-0x420) {
                s->intro.player.sprite=(HmbtnAnimationPlayer){(uint16_t)(s->control.carrying_big ? 41 : 0),(uint16_t)s->intro.player.facing,0};
                s->door_speed+=8;s->door_angle-=s->door_speed;
                if(s->door_angle<-0x420) s->door_angle=-0x420;
                const int turn=(s->intro.player.facing==1 || s->intro.player.facing==2) ? s->door_angle : -s->door_angle;
                s->scene.angle[s->door_model][1]=(uint16_t)(s->door_base_angle+turn);
                if(s->door_twin>=0) s->scene.angle[s->door_twin][1]=(uint16_t)(s->door_twin_base-turn);
            } else {
                ++s->door_tick;
                const unsigned face=s->intro.player.facing;
                s->intro.player.position[face&1u ? 0 : 2]+=face<2 ? -41 : 41;
                memcpy(s->intro.camera,s->intro.player.position,sizeof(s->intro.camera));
                const unsigned animation=face+(s->control.carrying_big ? 42u : 1u);
                if(s->intro.player.sprite.animation!=animation) s->intro.player.sprite=(HmbtnAnimationPlayer){(uint16_t)animation,0,0};
                if(s->door_tick>=11) { s->door_tick=0;s->travel_tick=1; }
            }
            goto house_done;
        }
        if(s->travel_tick) {
            if(++s->travel_tick==11) {
                if(!load_area(s,s->data_root,s->travel_exit.target,error,error_size)) return false;
                s->intro.area=s->area;
                memcpy(s->intro.player.position,s->travel_exit.entry,sizeof(s->intro.player.position));
                HmbtnGroundHit ground;
                if(hmbtn_ground_find(&s->scene.terrain,s->intro.player.position,0xC080,200,&ground))
                    s->intro.player.position[1]=ground.height;
                memcpy(s->intro.camera,s->intro.player.position,sizeof(s->intro.camera));
                s->control.mode=s->control.blocked_frames=0;
                s->intro.player.facing=s->area==80 ? 3u : s->intro.player.facing;
                s->intro.player.sprite=(HmbtnAnimationPlayer){(uint16_t)(s->control.carrying_big ? 41 : 0),(uint16_t)s->intro.player.facing,0};
                s->house.tv_dirty=s->area==80;
                memset(s->tv_sectors,0,sizeof(s->tv_sectors));
                printf("Native area transition: area %u, entry (%d,%d,%d). Session inventory and field retained.\n",s->area,
                    (int)s->intro.player.position[0],(int)s->intro.player.position[1],(int)s->intro.player.position[2]);fflush(stdout);
            }
            if(s->travel_tick>=21) {
                s->travel_tick=0;s->control.exit_pending=false;
                if(s->area==0 && !s->first_visit_started) {
                    s->first_visit_started=s->first_visit_active=true;s->control.enabled=false;
                    if(!load_event(s,12,error,error_size)) return false;
                    s->dialogue.step=100;
                    s->intro.finish_requested=false;s->intro.event_active=true;
                    s->intro.visit_approach=true;
                    s->intro.mayor.position[0]=-3888;s->intro.mayor.position[1]=0;s->intro.mayor.position[2]=7895;
                    s->intro.mayor.visible=true;s->intro.mayor.sprite=(HmbtnAnimationPlayer){1,0,0};
                }
            }
            goto house_done;
        }
        if((s->area==0 || s->house.equipped.id==0x1E || s->farm_action.phase==HMBTN_FARM_MEDICINE)
                && !s->feed.phase && !s->house.held_item.id && !s->control.exit_pending && !s->control.jumping && s->house.mode==HMBTN_HOUSE_FREE
                && (!(s->pending_pressed&HMBTN_PAD_CROSS) || s->farm_action.phase!=HMBTN_FARM_ACTION_FREE)) {
            const bool medicine=s->farm_action.phase==HMBTN_FARM_MEDICINE || s->house.equipped.id==0x1E;
            const bool acting=medicine ? hmbtn_poultry_medicine_tick(&s->poultry,s->area,
                &s->farm_action,&s->intro.player,&s->house,s->pending_pressed)
                : hmbtn_farm_action_tick(&s->farm_action,&s->intro.player,&s->house,
                    &s->field,&s->scene.terrain,s->pending_pressed,held);
            if(s->farm_action.bank_pending) {
                const unsigned row=(unsigned)s->farm_action.bank;
                const unsigned aligned=row==23 ? 1960 : row==22 ? 3012 : 380; /* player_anim_banks[23/22/3]. */
                if(!s->action_block) s->action_block=malloc(0x3800);
                if(!s->action_atlas) s->action_atlas=malloc(sizeof(*s->action_atlas));
                if(!s->action_block || !s->action_atlas) {
                    snprintf(error,error_size,"Cannot allocate farm action bank");return false;
                }
                if(!hmbtn_resource_read(s->data_root,"A_FILE/00_gf.bin",0x3cb8800+row*0x3800,
                        s->action_block,0x3800,error,error_size)
                        || !hmbtn_animation_open(&s->action_animation,s->action_block,aligned,error,error_size)
                        || !hmbtn_bank_decode(s->action_block+aligned,0x3800-aligned,s->action_atlas,error,error_size)) return false;
                s->intro.action_active=true;s->farm_action.bank_pending=false;
            }
            if(s->farm_action.finished_bank) s->intro.action_active=false;
            if(s->farm_action.sound>=0) {
                const HmbtnFarmSound *fx=hmbtn_farm_sound((unsigned)s->farm_action.sound);
                const uint64_t voice=fx ? hmbtn_sound_effect_start(s->audio.sound,fx->bank==1 ? 2u : 0u,
                        fx->program,fx->tone,fx->note,fx->volume) : 0;
                if(!voice) {
                    snprintf(error,error_size,"Original farm action tone unavailable");return false;
                }
                if(s->farm_action.kind==4 && s->farm_action.phase==HMBTN_FARM_TOOL_SWING) s->watering_voice=voice;
            }
            if(s->farm_action.release_sound) {
                hmbtn_sound_release_voice(s->audio.sound,s->watering_voice);s->watering_voice=0;
            }
            if(s->farm_action.broken_item) hmbtn_field_effect_submit(s->field_effects,
                s->farm_action.broken_item,s->farm_action.effect_column,s->farm_action.effect_row);
            if(s->area==0 && s->farm_action.changed && !hmbtn_farm_field_retexture(&s->field,
                    s->section+(s->scene.terrain.data-s->section),s->scene.terrain.size)) {
                snprintf(error,error_size,"Cannot update field soil textures");return false;
            }
            if(acting) {
                s->control.mode=0;
                memcpy(s->intro.camera,s->intro.player.position,sizeof(s->intro.camera));
                goto house_done;
            }
        }
        bool opened=false;
        if(s->house.mode==HMBTN_HOUSE_FREE && !s->control.exit_pending && !s->control.jumping
                && (s->pending_pressed&HMBTN_PAD_CROSS) && hmbtn_feed_shop_open(&s->feed_shop,
                    &s->village,&s->intro.player,&s->clock,s->area,s->house.held_item.id)) {
            HmbtnIntroActor *merchant=&s->village.people[s->feed_shop.merchant].actor;
            merchant->facing=(s->intro.player.facing+2)&3u;
            merchant->sprite=(HmbtnAnimationPlayer){0,(uint16_t)merchant->facing,0};
            if(!feed_shop_message(s,error,error_size)) return false;
            goto house_done;
        }
        if(s->house.mode==HMBTN_HOUSE_FREE && !s->control.exit_pending && !s->control.jumping
                && !s->house.held_item.id && (s->pending_pressed&HMBTN_PAD_CROSS)) {
            const unsigned id=hmbtn_village_target(&s->village,&s->intro.player);
            if(id) {
                uint32_t offset;size_t length;uint8_t block[8192];
                if(!hmbtn_village_talk_range(id,&offset,&length)
                        || !hmbtn_resource_read(s->data_root,"A_FILE/12_event_s.bin",offset,block,length,error,error_size)
                        || !hmbtn_village_talk_open(&s->talk,id,block,length,&s->village,&s->intro.player,
                            &s->clock,&s->dialogue,&s->house,error,error_size)
                        || !village_conversation(s,0,0,error,error_size)) return false;
                hmbtn_sound_effect(s->audio.sound,3);goto house_done;
            }
        }
        if(s->area==112 && !s->control.exit_pending && !s->control.jumping && s->house.mode==HMBTN_HOUSE_FREE
                && !s->quick_ticks && hmbtn_poultry_incubator_tick(&s->poultry,&s->feed,&s->house,
                    &s->intro.player,&s->clock,s->section,s->scene.terrain.size+0x2000,s->pending_pressed)) {
            if(s->feed.sound>=0) hmbtn_sound_effect(s->audio.sound,(unsigned)s->feed.sound);
            if(s->feed.request) {
                s->house.mode=HMBTN_HOUSE_BOOK;s->house.message_bank=2;s->house.request=s->feed.request;
                s->house.loaded=0;s->house.closing=s->house.detail=false;s->house.sound=3;opened=true;
            } else { s->control.mode=0;goto house_done; }
        }
        if(s->feed.phase || ((s->area==96 || s->area==112) && !s->control.exit_pending
                && !s->control.jumping && s->house.mode==HMBTN_HOUSE_FREE && !s->quick_ticks)) {
            if(hmbtn_farm_feed_tick(&s->feed,&s->house,&s->intro.player,s->section,
                    s->scene.terrain.size+0x2000,s->pending_pressed)) {
                if(s->feed.sound>=0) hmbtn_sound_effect(s->audio.sound,(unsigned)s->feed.sound);
                if(s->feed.request) {
                    s->house.mode=HMBTN_HOUSE_BOOK;s->house.message_bank=2;s->house.request=s->feed.request;
                    s->house.loaded=0;s->house.closing=s->house.detail=false;s->house.sound=3;opened=true;
                } else { s->control.mode=0;goto house_done; }
            }
        }
        if(s->area!=0 && s->house.held_item.id && !s->control.exit_pending && !s->control.jumping
                && s->house.mode==HMBTN_HOUSE_FREE && !s->quick_ticks
                && hmbtn_farm_carry_tick(&s->carry,&s->house,NULL,&s->intro.player,s->pending_pressed)) {
            s->control.mode=0;goto house_done;
        }
        if(s->house.mode==HMBTN_HOUSE_FREE && !s->control.exit_pending && !s->control.jumping && (s->pending_pressed&HMBTN_PAD_CROSS)
                && hmbtn_world_door_at(s->section,s->scene.terrain.size+0x2000,
                    &s->intro.player,&s->travel_exit,&s->door_model)) {
            const unsigned event=hmbtn_read_le16(s->section+16+s->travel_exit.zone*44+10);
            const int refusal=hmbtn_village_door(&s->village,event,&s->clock);
            if(refusal>=0) {
                s->house.mode=HMBTN_HOUSE_BOOK;s->house.message_bank=2;s->house.request=(unsigned)refusal;
                s->house.loaded=0;s->house.closing=false;s->house.sound=3;
                s->house.detail=false;opened=true;
            } else {
            s->control.exit_pending=true;s->door_tick=1;s->door_speed=s->door_angle=0;
            s->door_base_angle=s->scene.angle[s->door_model][1];s->door_twin=-1;
            if(s->scene.flags[s->door_model]&0x400u) {
                const unsigned twin=hmbtn_read_le16(s->section+0x948+s->door_model*44+8);
                if(twin>=HMBTN_SCENE_MODELS) { snprintf(error,error_size,"Invalid twin door model");return false; }
                s->door_twin=(int)twin;s->door_twin_base=s->scene.angle[twin][1];
            }
            if(!hmbtn_sound_key_on(s->audio.sound,2,1,12,60,127)) {
                snprintf(error,error_size,"Original door opening tone unavailable");return false;
            }
            s->intro.player.sprite=(HmbtnAnimationPlayer){(uint16_t)(s->control.carrying_big ? 41 : 0),(uint16_t)s->intro.player.facing,0};
            if(hmbtn_read_le16(s->section+16+s->travel_exit.zone*44)==0x1000) { s->door_tick=0;s->travel_tick=1; }
            goto house_done;
            }
        }
        if(!opened && !s->control.exit_pending && !s->control.jumping
                && s->house.mode==HMBTN_HOUSE_FREE && (s->pending_pressed&HMBTN_PAD_CROSS))
            opened=hmbtn_house_interact(&s->house,s->section,s->scene.terrain.size+0x2000,&s->intro.player);
        if(s->house.mode!=HMBTN_HOUSE_FREE) {
            if(!opened) hmbtn_house_interaction_tick(&s->house,s->pending_pressed,held);
            if(s->house.sound>=0) hmbtn_sound_effect(s->audio.sound,(unsigned)s->house.sound);
            if(s->house.request && s->house.loaded!=s->house.request) {
                if(s->house.message_bank==2) {
                    uint8_t record[8192];
                    if(!hmbtn_resource_read(s->data_root,"A_FILE/04_messege2.bin",s->house.request*8192u,
                            record,sizeof(record),error,error_size)) return false;
                    if(!hmbtn_house_expand_reading(&s->house,record,sizeof(record),s->dialogue.dog_name,s->dialogue.farm_name)) {
                        snprintf(error,error_size,"Cannot expand original reading message 0x%x",s->house.request);
                        return false;
                    }
                    if(!hmbtn_ui_message_record(s->ui,record,sizeof(record),&s->house.message,error,error_size)) return false;
                } else if(s->house.request==21) {
                    /* text_find_staged_value: count of the selected watering can. */
                    uint8_t record[8192];
                    if(!hmbtn_resource_read(s->data_root,"A_FILE/05_messege3.bin",21*8192u,
                            record,sizeof(record),error,error_size)) return false;
                    if(!hmbtn_house_expand_reading(&s->house,record,sizeof(record),s->dialogue.dog_name,s->dialogue.farm_name)) {
                        snprintf(error,error_size,"Cannot expand watering can count");return false;
                    }
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
            s->control.npc_blocked=hmbtn_village_blocked;s->control.collision_context=&s->village;
            s->control.carrying=s->house.held_item.id!=0;
            s->control.carrying_big=hmbtn_poultry_held_slot(&s->poultry)<HMBTN_HENS;
            hmbtn_farm_control_tick(&s->control,&s->intro.player,&s->scene.terrain,s->area==0 ? &s->field : NULL,
                hmbtn_house_world_buttons(&s->house.options,held));
            if(s->control.jump_started && !hmbtn_sound_key_on(s->audio.sound,2,0,0,60,127)) {
                snprintf(error,error_size,"Original jump tone unavailable"); return false;
            }
            hmbtn_house_options_idle(&s->house.options,s->control.mode!=0,s->intro.player.facing);
            if(s->control.mode) { s->farm_action.strikes=0;s->farm_action.target=HMBTN_FIELD_CELLS; }
            if(s->control.mode && !s->control.exit_pending && !s->control.jumping && !s->control.landed) {
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
                hmbtn_poultry_release(&s->poultry,&s->house,&s->intro.player,s->area,&s->scene.terrain);
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
            if(s->sleep.boundary && !morning(s,true,error,error_size)) return false;
        }
        if(s->area==80) s->scene.angle[0][2]=(uint16_t)s->house.pan;
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
        if(s->house.mode==HMBTN_HOUSE_FREE && !s->control.exit_pending && s->village_ready) {
            if(hmbtn_world_gate_at(s->section,s->scene.terrain.size+0x2000,s->intro.player.position,&exit)) {
                const unsigned event=hmbtn_read_le16(s->section+16+exit.zone*44+10);
                const int refusal=hmbtn_village_door(&s->village,event,&s->clock);
                if(refusal<0) {
                    s->travel_exit=exit;s->travel_tick=1;s->control.exit_pending=true;
                } else if(s->refused_zone!=(int)exit.zone) {
                    s->refused_zone=(int)exit.zone;
                    s->house.mode=HMBTN_HOUSE_BOOK;s->house.message_bank=2;s->house.request=(unsigned)refusal;
                    s->house.loaded=0;s->house.closing=s->house.detail=false;
                    /* The common loader opens this on the next tick. */
                    memset(&s->house.message,0,sizeof(s->house.message));s->house.message.phase=HMBTN_MESSAGE_OPENING;
                }
            } else s->refused_zone=-1;
        }
        if(s->house.mode==HMBTN_HOUSE_FREE && !s->control.exit_pending && hmbtn_house_exit_at(s->section,
                s->scene.terrain.size+0x2000,s->intro.player.position,&exit)) {
            unsigned logical,resource;
            if(hmbtn_world_area(exit.target,s->clock.season,&logical,&resource)) {
                s->control.exit_pending=true;s->control.exit_zone=exit.zone;
                s->intro.player.sprite=(HmbtnAnimationPlayer){0,(uint16_t)s->intro.player.facing,0};
                s->travel_exit=exit;s->travel_tick=1;
            }
        }
        memcpy(s->intro.camera,s->sleep.active ? s->sleep.camera : s->intro.player.position,sizeof(s->intro.camera));
    }
house_done:
    if(s->completion_reported) {
        s->village.player=&s->intro.player;s->village.player_running=s->control.mode==4;
        const bool held_clock=!s->control.enabled || s->travel_tick || s->door_tick || s->control.exit_pending
            || s->sleep.active || s->morning_fade || s->intro.message || s->first_visit_active
            || s->menu.mode!=HMBTN_MENU_NONE || s->house.mode!=HMBTN_HOUSE_FREE || s->feed_shop.phase!=HMBTN_SHOP_OFF
            || s->talk.active;
        const unsigned changes=hmbtn_farm_clock_tick(&s->clock,held_clock
            || hmbtn_farm_clock_place_held(s->section,16));
        world_clock_events(s,&s->clock,changes);
        s->menu.stats.animals[0]=hmbtn_poultry_count(&s->poultry);
        if(!held_clock) {
            hmbtn_poultry_tick(&s->poultry,s->area,&s->scene.terrain,s->area==0 ? &s->field : NULL,&s->intro.player,s->clock.hour);
            for(unsigned i=0;i<HMBTN_HENS;++i) if(s->poultry.hens[i].occupied && !s->poultry.hens[i].dead
                    && !s->poultry.hens[i].held && s->poultry.hens[i].area==s->area
                    && s->poultry.hens[i].stage!=HMBTN_HEN_EGG
                    && !hmbtn_animation_tick(s->poultry.hens[i].stage==HMBTN_HEN_CHICK ? &s->poultry_art.chick : &s->poultry_art.adult,
                        &s->poultry.hens[i].actor.sprite,error,error_size)) return false;
        }
        if(s->village_ready) {
            if(!hmbtn_village_sync(&s->village,&s->clock,error,error_size)
                    || !hmbtn_village_art_update(&s->village_art,s->data_root,hmbtn_read_le16(s->section+12),&s->clock,error,error_size)) return false;
            hmbtn_village_tick(&s->village,s->section,s->scene.terrain.size+0x2000,&s->scene.terrain,held_clock);
            if(s->talk.active && s->talk.released) s->village.people[s->talk.target].actor.visible=false;
            /* villager_set_action: Ellen remains seated; Gotz works on farm row 4. */
            for(unsigned id=1;id<37;++id) if(s->village.people[id].actor.visible) {
                HmbtnVillager *p=&s->village.people[id];
                const unsigned special=id==23 ? 1 : id==24 && s->area==0 && p->row==4 ? p->actor.facing+5 : 0;
                if(special && p->actor.sprite.animation!=special)
                    p->actor.sprite=(HmbtnAnimationPlayer){(uint16_t)special,0,0};
            }
            if(!held_clock) for(unsigned id=1;id<37;++id) if(s->village.people[id].actor.visible) {
                const HmbtnImageAtlas *atlas;
                const HmbtnAnimation *ani=hmbtn_village_art_actor(&s->village_art,id,&atlas);
                if(ani && !hmbtn_animation_tick(ani,&s->village.people[id].actor.sprite,error,error_size)) return false;
            }
        }
        sync_clock(s);
        if(changes&HMBTN_CLOCK_DAY) s->overnight_tick=1;
        if(changes&HMBTN_CLOCK_HOUR) {
            s->farm_action.fatigue+=hmbtn_farm_clock_fatigue(s->clock.hour);
            if(s->farm_action.fatigue>100) s->farm_action.fatigue=100;
            if(s->clock.hour==6 && s->clock.minute==0 && s->farm_action.fatigue==100) s->farm_action.fatigue=99;
        }
        hmbtn_daylight_tick(&s->daylight,&s->clock,held_clock || (hmbtn_read_le16(s->section+8)&0x1800u));
        if(!s->first_visit_active && !s->animal_event && !s->sleep.active) {
            const unsigned song=hmbtn_world_song(s->section,16,s->clock.season,s->clock.hour);
            if(!farm_song(s,song,error,error_size)) return false;
        }
    }
    if(s->control.enabled && !s->sleep.active && !s->morning_fade && !s->overnight_tick) {
        if(s->house_music_ticks<100) ++s->house_music_ticks;
        s->intro.music_volume=s->house_music_ticks*world_music_volume(s)/100;
    }
    if (s->intro.message && s->loaded_message != s->intro.message) {
        if(s->first_visit_active || s->animal_event) {
            uint8_t record[8192];uint16_t words[1024];unsigned count=0;
            if(!hmbtn_resource_read(s->data_root,"A_FILE/03_messege1.bin",s->intro.message*8192u,
                    record,sizeof(record),error,error_size)) return false;
            if(s->animal_event==172 && !hmbtn_poultry_expand_dead(&s->poultry,record,sizeof(record))) {
                snprintf(error,error_size,"Funeral names exceed message capacity");return false;
            }
            for(unsigned i=0;i<1024 && count<1024;++i) {
                uint16_t c=hmbtn_read_le16(record+0x1800+i*2);
                const uint16_t *name=c==0x400a ? s->dialogue.player_name : c==0x400c ? s->dialogue.dog_name : c==0x400d ? s->dialogue.farm_name : NULL;
                if(name) { for(unsigned j=0;j<8 && name[j]!=0xffff && count<1023;++j) words[count++]=name[j]; }
                else words[count++]=c;
                if(c==0xffff) break;
            }
            if(!count || words[count-1]!=0xffff) { snprintf(error,error_size,"First morning message expansion overflow");return false; }
            for(unsigned i=0;i<count;++i) hmbtn_write_le16(record+0x1800+i*2,words[i]);
            if(!hmbtn_ui_message_record(s->ui,record,sizeof(record),&s->dialogue.message,error,error_size)) return false;
        } else if (!hmbtn_new_game_ui_message(s->ui,s->data_root,"A_FILE/03_messege1.bin",
                s->intro.message,&s->dialogue.message,error,error_size)) return false;
        s->loaded_message = s->intro.message;
        if(s->first_visit_active) { s->dialogue.choice=0;s->dialogue.message.suppress_cursor=s->dialogue.message.choices!=0; }
        printf("Opening message: 0x%03x\n",s->loaded_message); fflush(stdout);
    }
    if (!s->intro.message) {
        s->loaded_message = 0;
        if (s->menu.mode==HMBTN_MENU_NONE && s->farm_action.phase!=HMBTN_FARM_TOOL_CHARGE && (!s->sleep.active || s->sleep.phase<4) && !actor_animation_tick(s->intro.action_active ? &s->action_animation : s->first_visit_active ? &s->event_player_animation : &s->player_animation,
                &s->intro.player,error,error_size)) return false;
        if (s->intro.action_active) s->intro.action_finished = s->intro.player.animation_done;
        if(s->menu.mode==HMBTN_MENU_NONE) for (unsigned id = 1; id < 35; ++id) {
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
opening_done:
    if(s->control.enabled && !s->sleep.active && !s->door_tick)
        hmbtn_world_camera(s->section,s->scene.terrain.size+0x2000,s->intro.camera);
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
        if (s->menu.mode==HMBTN_MENU_NONE && (!s->opening || (!s->intro.message && !s->intro.script.boundary)))
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
        if(s->control.enabled) for(unsigned i=125;i<=172;++i) {
            const unsigned item=hmbtn_farm_gather_item(&s->gather,i,hmbtn_read_le16(s->section+2),s->clock.season);
            const int art=hmbtn_estate_item_art(item);
            const HmbtnAnimation *ani=art>=0 ? hmbtn_estate_art_sprite(&s->estate_art,(unsigned)art) : NULL;
            if(!item || !ani) continue;
            actors[count]=(HmbtnWorldSprite){ani,s->estate_art.atlas,{0},{0},0};
            memcpy(actors[count++].position,hmbtn_farm_gather_record(i)->position,sizeof(actors[0].position));
        }
        for(unsigned i=0;i<HMBTN_HENS;++i) {
            const HmbtnHen *hen=&s->poultry.hens[i];
            if(hen->occupied && !hen->dead && !hen->held && hen->area==s->area && hen->stage!=HMBTN_HEN_EGG) {
                actors[count]=(HmbtnWorldSprite){hen->stage==HMBTN_HEN_CHICK ? &s->poultry_art.chick : &s->poultry_art.adult,
                    s->poultry_art.atlas,hen->actor.sprite,{0},0};
                memcpy(actors[count++].position,hen->actor.position,sizeof(actors[0].position));
                if(hen->praise && hmbtn_new_game_ui_cue(s->ui,&actors[count],hen->actor.position,37,hen->praise,-20)) ++count;
            }
            if(hen->occupied && hen->area==s->area && hen->stage==HMBTN_HEN_EGG) {
                const HmbtnAnimation *ani=hmbtn_estate_art_sprite(&s->estate_art,hen->golden_egg ? 6 : 5);
                if(ani) {
                    actors[count]=(HmbtnWorldSprite){ani,s->estate_art.atlas,{0},{0},0};
                    memcpy(actors[count++].position,hen->actor.position,sizeof(actors[0].position));
                }
            }
            const HmbtnCoopEgg *egg=&s->poultry.eggs[i];
            /* estate_code_lookup: the outdoor egg sprites are seasonal. */
            static const unsigned farm_egg[4]={0x16,0x1B,0x1B,0x0C};
            const int art=s->area==0 ? (int)(farm_egg[s->clock.season]+(egg->item==0xE8)) : hmbtn_estate_item_art(egg->item);
            const HmbtnAnimation *ani=art>=0 ? hmbtn_estate_art_sprite(&s->estate_art,(unsigned)art) : NULL;
            if(egg->item && egg->area==s->area && ani) {
                actors[count]=(HmbtnWorldSprite){ani,s->estate_art.atlas,{0},{0},0};
                memcpy(actors[count++].position,egg->position,sizeof(actors[0].position));
            }
        }
        if(s->feed_block && (s->area==96 || s->area==112)) {
            const bool barn=s->area==96;const bool *bins=barn ? s->feed.barn : s->feed.coop;
            const unsigned capacity=barn ? 22 : 10;
            for(unsigned i=0;i<capacity;++i) if(bins[i]) {
                actors[count]=(HmbtnWorldSprite){&s->feed_art[barn ? 0 : 1],s->feed_atlas,{0},{0},0};
                hmbtn_farm_feed_position(barn,i,actors[count++].position);
            }
        }
        if(s->control.enabled && s->village_ready) for(unsigned id=1;id<37;++id) {
            const HmbtnIntroActor *a=&s->village.people[id].actor;const HmbtnImageAtlas *atlas;
            const HmbtnAnimation *ani=hmbtn_village_art_actor(&s->village_art,id,&atlas);
            if(!a->visible || !ani) continue;
            actors[count]=(HmbtnWorldSprite){ani,atlas,a->sprite,{0},0};
            memcpy(actors[count++].position,a->position,sizeof(a->position));
        }
        if (s->opening && s->intro.player.visible) {
            actors[count] = (HmbtnWorldSprite){s->intro.action_active ? &s->action_animation : s->first_visit_active ? &s->event_player_animation : &s->player_animation,
                s->intro.action_active ? s->action_atlas : s->first_visit_active ? s->mayor_atlas : s->player_atlas,s->intro.player.sprite,{0},0};
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
            switch (s->control.enabled || s->first_visit_started ? s->field.cells[row*45+col].value : layout[row*45+col]) {
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
        if(s->area==0) for(unsigned i=0;i<HMBTN_FIELD_EFFECTS;++i) if(s->field_effects[i].active) {
            const HmbtnFieldEffect *e=&s->field_effects[i];
            if(count>=512) { snprintf(error,error_size,"Field effect capacity exceeded");return false; }
            actors[count]=(HmbtnWorldSprite){&s->field_art[e->art],s->field_atlas,{1,(uint16_t)(e->tick/3),0},{0},0};
            memcpy(actors[count++].position,e->position,sizeof(e->position));
        }
        const unsigned model_count = hmbtn_scene_data_instances(&s->scene,instances);
        if (!hmbtn_model_render_instances(s->geometry,instances,model_count,s->atlas,&camera,actors,count,
                s->pixels,FRAME_BYTES,NULL,error,error_size)) return false;
        if((s->quick_ticks && s->quick_id) || s->house.held_item.id || s->farm_action.phase==HMBTN_FARM_MEDICINE) {
            int x,y;const int32_t point[3]={s->intro.player.position[0]+150,s->intro.player.position[1]-100,s->intro.player.position[2]-150};
            int offset_x=-8,offset_y=-10;
            if(s->control.carrying_big && s->control.mode!=4) {
                static const int offset[4][2]={{-8,-10},{-6,-18},{6,-18},{8,-10}};
                offset_x=offset[s->intro.player.facing&3][0];offset_y=offset[s->intro.player.facing&3][1];
            }
            else if(s->house.held_item.id && !(s->quick_ticks && s->quick_tool))
                hmbtn_farm_carried_offset(s->intro.player.facing,s->control.mode==4,&offset_x,&offset_y);
            if(hmbtn_model_project_point(&camera,point,&x,&y)
                    && !hmbtn_house_ui_picture(s->house_ui,s->data_root,s->pixels,
                        s->quick_ticks && s->quick_id ? s->quick_id
                            : s->farm_action.phase==HMBTN_FARM_MEDICINE ? s->house.equipped.id : s->house.held_item.id,
                        x+offset_x,y+offset_y,error,error_size)) return false;
        }
        if(s->intro.player_marker) {
            int x,y;const int32_t point[3]={s->intro.player.position[0]+150,s->intro.player.position[1]-100,s->intro.player.position[2]-150};
            if(hmbtn_model_project_point(&camera,point,&x,&y)
                    && !hmbtn_ui_icon(s->ui,s->pixels,14,(s->ticks/2)&1u,x,y-50,error,error_size)) return false;
        }
        for(unsigned i=0;i<3;++i) if(s->carry.thrown[i].item.id && s->carry.thrown[i].area==s->area) {
            const HmbtnThrownItem *t=&s->carry.thrown[i];int x,y;
            const int32_t point[3]={t->position[0]+150,t->position[1]-100,t->position[2]-150};
            if(hmbtn_model_project_point(&camera,point,&x,&y)
                    && !hmbtn_house_ui_picture(s->house_ui,s->data_root,s->pixels,t->item.id,x-8,y-10,error,error_size)) return false;
        }
        }
        if (s->opening) {
            if (s->intro.cell_actor) {
                const HmbtnIntroActor *cell = hmbtn_farm_intro_actor(&s->intro,(int32_t)s->intro.cell_actor);
                if (!cell || s->intro.cell_actor > 4) { snprintf(error,error_size,"Invalid childhood illustration actor"); return false; }
                for (size_t i = 0; i < FRAME_BYTES; i += 4) memset(s->pixels+i,0,3);
                if (!hmbtn_animation_draw(&s->actor_animation[s->intro.cell_actor],s->mayor_atlas,
                        &cell->sprite,80,60,s->pixels,320,240,FRAME_BYTES,error,error_size)) return false;
            }
            if(s->completion_reported && !(hmbtn_read_le16(s->section+8)&0x1800u))
                hmbtn_daylight_apply(&s->daylight,s->pixels,FRAME_BYTES);
            const unsigned brightness = s->morning_fade ? 255*(45-s->morning_fade)/45 : s->overnight_tick ? 255*(45-s->overnight_tick)/45 : s->travel_tick ? (s->travel_tick<=11 ? 255*(11-s->travel_tick)/10 : 255*(s->travel_tick-11)/10) : s->sleep.active ? 255*(30-s->sleep.fade)/30 : hmbtn_farm_intro_brightness(&s->intro);
            for (size_t i = 0; i < FRAME_BYTES; ++i)
                if (i%4 != 3) s->pixels[i] = (uint8_t)((s->pixels[i]*brightness+(s->intro.fade_white ? 255*(255-brightness) : 0))/255);
            if(s->control.enabled && !s->travel_tick && !s->morning_fade && !s->overnight_tick && !hmbtn_house_ui_draw(s->house_ui,s->ui,&s->house,
                    s->data_root,s->ticks,s->pixels,error,error_size)) return false;
            if(s->menu.mode==HMBTN_MENU_RUCKSACK && !hmbtn_farm_menu_ui_draw(s->menu_ui,s->house_ui,s->ui,&s->menu,&s->house,&s->farm_action,
                    &s->dialogue,s->data_root,s->ticks,s->pixels,error,error_size)) return false;
            if(s->feed_shop.phase==HMBTN_SHOP_SELL_LIST || s->feed_shop.phase==HMBTN_SHOP_SELL_CONFIRM)
                for(size_t i=0;i<FRAME_BYTES;++i) if(i%4!=3) s->pixels[i]/=2;
            if(!hmbtn_feed_shop_ui_draw(&s->feed_shop,&s->menu.stats,&s->dialogue,s->ui,s->pixels,s->ticks,error,error_size)) return false;
            if (!hmbtn_new_game_ui_draw(s->ui,s->animal_name_active ? &s->animal_name : s->feed_shop.phase==HMBTN_SHOP_NAME ? &s->feed_shop.editor : &s->dialogue,
                    s->ticks,s->pixels,error,error_size)) return false;
            if(s->menu.mode==HMBTN_MENU_STATUS && !hmbtn_farm_menu_ui_draw(s->menu_ui,s->house_ui,s->ui,&s->menu,&s->house,&s->farm_action,
                    &s->dialogue,s->data_root,s->ticks,s->pixels,error,error_size)) return false;
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
