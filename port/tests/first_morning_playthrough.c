/* Optional integration check with the user's extracted data. Including the
 * coordinator gives the driver state assertions without a runtime debug API. */
#include "scene_preview.c"
#include "sdl_host.h"

#define CHECK(x) do { if(!(x)) { fprintf(stderr,"First morning line %d: %s; %s (area %u, step %u)\n",__LINE__,#x,error,s ? s->area : 0,s ? s->intro.script.pc : 0);goto fail; } } while(0)
static bool tick(HmbtnScenePreview *s,uint32_t pressed,char *error,size_t size)
{
    hmbtn_scene_data_tick(&s->scene);s->pending_pressed=pressed;
    const bool ok=opening_tick(s,HMBTN_PAD_CROSS,error,size);
    s->pending_pressed=0;return ok;
}
static bool play(HmbtnHost *host,const char *root,const char *captures,bool accept,char *error,size_t error_size)
{
    HmbtnScenePreview *s=hmbtn_scene_house_create(host->renderer,root,error,error_size);
    CHECK(s);
    HmbtnRegistration registration;hmbtn_registration_init(&registration);
    for(unsigned i=0;i<8;++i) registration.player_name[i]=registration.farm_name[i]=registration.dog_name[i]=(uint16_t)(0x20+i);
    hmbtn_scene_intro_registration(s,&registration);
    for(unsigned i=0;i<110;++i) CHECK(tick(s,0,error,error_size));
    /* Use the actual exit trigger and preserve session state across the tour. */
    s->house.inventory->bag[0]=(HmbtnStoredTool){21,0,73};
    s->session->field.cells[400].value=0;s->session->field.cells[400].ground=1;
    HmbtnFarmTile field[HMBTN_FIELD_CELLS];memcpy(field,s->session->field.cells,sizeof(field));
    s->intro.player.position[0]=1680;s->intro.player.position[2]=-800;
    for(unsigned i=0;i<23;++i) CHECK(tick(s,0,error,error_size));
    CHECK(s->first_visit_started && s->first_visit_active && !s->control.enabled);
    bool areas[233]={0},messages[81]={0},banks[512]={0},marker=false,selected=false;
    unsigned approach_ticks=0;
    bool mayor_stairs=false,player_stairs=false;
    unsigned rendered_area=UINT32_MAX,rendered_bank=UINT32_MAX;
    bool captured[81]={0};
    bool approach_captured=false,stairs_captured=false;
    for(unsigned i=0;i<20000 && s->first_visit_active;++i) {
        CHECK(!s->intro.script.boundary && !s->control.enabled);
        if(s->intro.visit_approach) { ++approach_ticks;CHECK(!s->intro.message && s->intro.mayor.sprite.animation==1); }
        if(s->intro.message==64) {
            CHECK(s->intro.player.facing==3 && s->intro.player.sprite.animation==0 && s->intro.player.sprite.frame==3);
        }
        if(s->area==32) {
            mayor_stairs |= s->intro.mayor.position[1]>-800 && s->intro.mayor.position[1]<0;
            player_stairs |= s->intro.player.position[1]>-800 && s->intro.player.position[1]<0;
        }
        if(s->area<233) areas[s->area]=true;
        if(s->intro.message<81) messages[s->intro.message]=true;
        if(s->intro.text_bank<512) banks[s->intro.text_bank]=true;
        marker |= s->intro.player_marker;
        const bool waiting=s->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END
            || s->dialogue.message.phase==HMBTN_MESSAGE_WAIT_PAGE;
        uint32_t pressed=waiting ? HMBTN_PAD_CROSS : 0;
        if(s->intro.message==64 && s->dialogue.message.choices && !selected) {
            selected=true;
            if(!accept) pressed=HMBTN_PAD_DOWN;
        }
        CHECK(tick(s,pressed,error,error_size));
        if(s->first_exit_tick==0 && s->intro.zone_exited && s->intro.script.pc==16)
            CHECK(hmbtn_farm_intro_brightness(&s->intro)==0);
        if(s->intro.message) for(unsigned w=0;w<s->dialogue.message.count;++w)
            CHECK(s->dialogue.message.words[w]!=0x400a && s->dialogue.message.words[w]!=0x400c && s->dialogue.message.words[w]!=0x400d);
        /* Render every new scene and every portrait switch, even with dummy
         * video, to catch missing ANI/textures and malformed geometry. */
        const bool capture_approach=captures && accept && !approach_captured && s->intro.visit_approach && s->intro.mayor.position[2]<5400;
        const bool capture_stairs=captures && accept && !stairs_captured && s->area==32
            && s->intro.player.position[1]>-600 && s->intro.player.position[1]<-200;
        const bool capture=capture_approach || capture_stairs || (captures && accept && s->intro.message<81 && !captured[s->intro.message]
            && s->intro.message && (s->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END
                || s->dialogue.message.phase==HMBTN_MESSAGE_WAIT_PAGE));
        if(s->area!=rendered_area || s->intro.text_bank!=rendered_bank || i%120==0 || capture) {
            CHECK(hmbtn_scene_preview_draw(s,host->renderer,0,0,0,error,error_size));
            rendered_area=s->area;rendered_bank=s->intro.text_bank;
            if(capture) {
                char path[1024];
                CHECK(snprintf(path,sizeof(path),"%s/tour-%u-area-%u.bmp",captures,s->intro.message,s->area)<(int)sizeof(path));
                if(capture_approach) { snprintf(path,sizeof(path),"%s/thomas-approach.bmp",captures);approach_captured=true; }
                if(capture_stairs) { snprintf(path,sizeof(path),"%s/tour-stairs.bmp",captures);stairs_captured=true; }
                SDL_Surface *surface=SDL_RenderReadPixels(host->renderer,NULL);CHECK(surface);
                const bool saved=SDL_SaveBMP(surface,path);SDL_DestroySurface(surface);CHECK(saved);
                captured[s->intro.message]=true;
            }
            CHECK(SDL_RenderPresent(host->renderer));
        }
    }
    CHECK(!s->first_visit_active && s->control.enabled && s->area==0);
    CHECK(s->intro.script.finished && !s->intro.script.boundary && s->intro.finish_requested);
    CHECK(messages[64] && messages[80] && marker && banks[159]);
    CHECK(approach_ticks>100);
    CHECK(!s->intro.mayor.visible && !s->intro.ellen.visible && !s->intro.zack.visible && !s->intro.player_marker);
    CHECK(s->house.inventory->bag[0].id==21 && s->house.inventory->bag[0].count==73);
    CHECK(memcmp(field,s->session->field.cells,sizeof(field))==0);
    CHECK(memcmp(registration.player_name,s->session->game.registration.player_name,sizeof(registration.player_name))==0);
    CHECK(s->intro.player.position[0]==-4257 && s->intro.player.position[2]==3959);
    if(accept) {
        const unsigned visited[]={0,32,36,40,48,52};
        for(unsigned i=0;i<sizeof(visited)/sizeof(visited[0]);++i) CHECK(areas[visited[i]]);
        for(unsigned i=65;i<=77;++i) CHECK(messages[i]);
        CHECK(!messages[78] && !messages[79] && s->house.clock->hour==8 && banks[105] && banks[142]);
        CHECK(mayor_stairs && player_stairs);
    } else {
        CHECK(messages[78] && messages[79] && !messages[65] && !areas[32] && s->house.clock->hour==6);
    }
    /* Re-entering the farm cannot start the consumed encounter again. */
    s->travel_exit=(HmbtnHouseExit){0};s->travel_exit.target=80;
    s->travel_exit.entry[0]=-90;s->travel_exit.entry[2]=541;s->travel_tick=10;
    for(unsigned i=0;i<12;++i) CHECK(tick(s,0,error,error_size));
    CHECK(s->area==80);
    s->intro.player.position[0]=1680;s->intro.player.position[2]=-800;
    for(unsigned i=0;i<23;++i) CHECK(tick(s,0,error,error_size));
    CHECK(s->area==0 && !s->first_visit_active && s->control.enabled);
    CHECK(memcmp(field,s->session->field.cells,sizeof(field))==0);
    hmbtn_scene_preview_destroy(s);return true;
fail:
    hmbtn_scene_preview_destroy(s);return false;
}
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) { fputs("Usage: first_morning_playthrough extracted-data-directory [capture-directory]\n",stderr);return 2; }
    HmbtnHost host={0};char error[512]={0};
    if(!hmbtn_host_open(&host)) { fprintf(stderr,"%s\n",SDL_GetError());return 1; }
    const bool ok=play(&host,argv[1],argc==3 ? argv[2] : NULL,true,error,sizeof(error))
        && play(&host,argv[1],NULL,false,error,sizeof(error));
    hmbtn_host_close(&host);
    if(ok) puts("Accepted/refused tour, original scenes/dialogues/portraits, Zack, camera, field/inventory and repeat-entry checks passed.");
    return ok ? 0 : 1;
}
