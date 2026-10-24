/* Original 128..133, seasonal tables, routes, ANI, pictures and exact installed text.
 * Only dates/affinity and travel positions are fixtures; no quest items/flags are granted. */
#include "scene_preview.c"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Pie line %d scenario %u: %s; %s (event %u step %u)\n",__LINE__,scenario,#x,error,s ? s->intro.event : 0,s ? s->intro.script.pc : 0);goto fail; } } while(0)
static int half(const uint8_t *p) { const unsigned n=hmbtn_read_le16(p);return n<32768 ? (int)n : (int)n-65536; }
static bool step(HmbtnScenePreview *s,uint32_t pressed,char *error,size_t size)
{
    s->pending_pressed=pressed;hmbtn_scene_data_tick(&s->scene);
    const bool ok=opening_tick(s,HMBTN_PAD_CROSS,error,size);s->pending_pressed=0;return ok;
}
static bool travel(HmbtnScenePreview *s,unsigned area,char *error,size_t size)
{
    if(!load_area(s,s->data_root,area,error,size)) return false;
    s->intro.area=area;s->intro.player.position[0]=0;s->intro.player.position[1]=0;s->intro.player.position[2]=0;
    for(unsigned i=0;i<2;++i) if(!step(s,0,error,size)) return false;
    return true;
}
static bool capture(HmbtnScenePreview *s,HmbtnHost *host,const char *dir,const char *name,char *error,size_t size)
{
    if(!hmbtn_scene_preview_draw(s,host->renderer,0,0,0,error,size)) return false;
    if(!dir) return true;
    SDL_Surface *pixels=SDL_RenderReadPixels(host->renderer,NULL);if(!pixels) return false;
    char path[1024];snprintf(path,sizeof(path),"%s/%s.bmp",dir,name);
    const bool saved=SDL_SaveBMP(pixels,path);SDL_DestroySurface(pixels);return saved;
}
static bool play(HmbtnScenePreview *s,HmbtnHost *host,unsigned event,bool second,bool timeout,
    bool messages[2046],const char *dir,char *error,size_t size)
{
    bool talked=false,selected=false,coordinated=false;unsigned last=UINT32_MAX;
    for(unsigned frame=0;frame<8000 && (s->visit.active || s->event_item.phase!=HMBTN_EVENT_ITEM_IDLE);++frame) {
        if(s->intro.script.boundary) return false;
        uint32_t pressed=s->intro.message ? HMBTN_PAD_CROSS : 0;
        if(s->intro.message && s->loaded_message==s->intro.message && last!=s->intro.message) {
            const unsigned message=s->intro.message;if(message>=2046) return false;
            uint8_t record[8192];HmbtnMessage expected;
            const HmbtnTextResourceId id={HMBTN_TEXT_RECORD,1,message};
            if(!hmbtn_text_resource_read(s->data_root,id,record,sizeof(record),error,size)
                    || !hmbtn_feed_shop_expand(record,sizeof(record),&s->dialogue)
                    || !hmbtn_message_open(&expected,record,sizeof(record),error,size)
                    || expected.count!=s->dialogue.message.count
                    || memcmp(expected.words,s->dialogue.message.words,expected.count*sizeof(*expected.words))) return false;
            messages[message]=true;last=message;
        }
        if(!s->intro.message) last=UINT32_MAX;
        if(s->dialogue.message.choices && s->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END && !selected) {
            selected=true;if(second) pressed=HMBTN_PAD_DOWN;
        }
        if(event==130 && s->control.enabled && s->visit.free && !s->intro.message && s->intro.script.pc>=28) {
            if(!travel(s,32,error,size)) return false;
        } else if(event!=129 && event!=130 && s->control.enabled && !s->intro.message
                && s->intro.mayor.position[2]<=3959 && !talked && !timeout) {
            memcpy(s->intro.player.position,s->intro.mayor.position,sizeof(s->intro.player.position));
            s->intro.player.position[2]-=300;s->intro.player.facing=2;s->control.mode=0;
            if(hmbtn_village_target(&s->village,&s->intro.player)!=17) return false;
            pressed=HMBTN_PAD_CROSS;talked=true;
        }
        if(event==130 && s->intro.message==394 && s->intro.text_bank==145) {
            if(s->intro.script.pc>=16) coordinated=true;
            if((s->dialogue.message.phase==HMBTN_MESSAGE_WAIT_PAGE || s->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END)
                    && !capture(s,host,dir,"elli-visit",error,size)) return false;
        }
        if(s->event_item.phase==HMBTN_EVENT_ITEM_EAT
                && !capture(s,host,dir,"thomas-cake",error,size)) return false;
        if(event==129 && s->event_item.phase==HMBTN_EVENT_ITEM_SHOW
                && !capture(s,host,dir,"doug-pie",error,size)) return false;
        if(!step(s,pressed,error,size)) return false;
    }
    return !s->visit.active && s->event_item.phase==HMBTN_EVENT_ITEM_IDLE && s->control.enabled
        && (event!=130 || !second || coordinated);
}
static bool counter(HmbtnScenePreview *s,char *error,size_t size)
{
    for(unsigned i=0;i<50;++i) {
        const uint8_t *p=s->section+16+i*44;
        if(hmbtn_read_le16(p+2)!=36 || hmbtn_read_le16(p+4)!=6) continue;
        const unsigned facing=hmbtn_read_le16(p+8);if(facing>3) continue;
        for(unsigned axis=0;axis<3;++axis) s->intro.player.position[axis]=(half(p+14+axis*2)+half(p+20+axis*2))/2;
        s->intro.player.position[facing&1u ? 0 : 2]+=facing<2 ? 224 : -224;s->intro.player.facing=facing;
        return hmbtn_village_zone_target(&s->village,s->section,s->scene.terrain.size+0x2000,&s->intro.player)==6
            && step(s,HMBTN_PAD_CROSS,error,size);
    }
    return false;
}
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};unsigned scenario=0;
    CHECK(hmbtn_host_open(&host));
    for(scenario=0;scenario<5;++scenario) {
        HmbtnFarmClock clock={0,1,5,0,8,0,0};bool messages[2046]={0};
        s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(s);
        CHECK(step(s,0,error,sizeof(error)));s->village.people[17].friendship=100;
        CHECK(step(s,0,error,sizeof(error)) && s->visit.event==128);
        CHECK(play(s,&host,128,false,false,messages,NULL,error,sizeof(error)) && s->village.flags[203]);
        if(scenario!=2) {
            CHECK(travel(s,204,error,sizeof(error)));
            CHECK(counter(s,error,sizeof(error)) && s->visit.active && s->visit.event==129);
            CHECK(play(s,&host,129,false,false,messages,argc==3 && !scenario ? argv[2] : NULL,error,sizeof(error)));
            CHECK(messages[390] && s->house.held_item.id==204 && s->village.flags[380]);
            CHECK(s->village.people[6].friendship==70);
            HmbtnHouseInteraction empty=s->house;empty.held_item=(HmbtnStoredTool){0};
            CHECK(!hmbtn_farm_event_trigger(&s->triggers,&s->village,&s->clock,0,s->previous_place,10,6,&empty));
        }
        if(scenario<2) {
            CHECK(travel(s,172,error,sizeof(error)));
            const HmbtnIntroActor *ellen=&s->village.people[23].actor;CHECK(ellen->visible);
            memcpy(s->intro.player.position,ellen->position,sizeof(ellen->position));
            s->intro.player.position[2]-=300;s->intro.player.facing=2;
            CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && s->visit.active && s->visit.event==130);
            CHECK(play(s,&host,130,scenario==1,false,messages,argc==3 && scenario==1 ? argv[2] : NULL,error,sizeof(error)));
            CHECK(messages[391] && s->village.flags[381] && s->menu.stats.gold==800 && s->menu.stats.ledger[0][0]==300);
            CHECK(s->village.people[23].friendship==70 && s->house.held_item.id==(scenario==1 ? 204u : 0u));
            if(scenario==1) CHECK(messages[393] && messages[394]);
        }
        CHECK(travel(s,80,error,sizeof(error)));
        hmbtn_farm_clock_advance_notify(&s->clock,hmbtn_farm_clock_sleep_minutes(&s->clock,0),world_clock_events,s);
        sync_clock(s);CHECK(s->village.days.count0[13]==1);
        unsigned before[37];for(unsigned i=0;i<37;++i) before[i]=s->village.people[i].friendship;
        if(scenario>=3) {
            CHECK(travel(s,172,error,sizeof(error)));
            CHECK(s->house.held_item.id==204
                && !hmbtn_farm_event_trigger(&s->triggers,&s->village,&s->clock,0,s->previous_place,3,23,&s->house));
        }
        s->farm_action.fatigue=20;const HmbtnStoredTool held=s->house.held_item;
        const unsigned expected=scenario<2 ? 131u : scenario==2 ? 133u : 132u;
        CHECK(travel(s,0,error,sizeof(error)) && s->visit.active && s->visit.event==expected);
        CHECK(play(s,&host,expected,false,scenario==4,messages,argc==3 && scenario==1 ? argv[2] : NULL,error,sizeof(error)));
        CHECK(s->village.flags[251+expected] && !memcmp(&held,&s->house.held_item,sizeof(held)));
        CHECK(s->farm_action.fatigue==(scenario<2 ? 16u : 20u));
        if(scenario<2) CHECK(messages[395] && messages[396] && messages[397]);
        else {
            CHECK(s->village.people[17].friendship==before[17]-30 && s->village.people[23].friendship==before[23]-20);
            CHECK(s->village.people[6].friendship==before[6]-(scenario==2 ? 0u : 30u));
            CHECK(scenario==4 ? !messages[398] : messages[scenario==2 ? 399 : 398]);
        }
        for(unsigned i=0;i<20;++i) CHECK(step(s,0,error,sizeof(error)) && !s->visit.active);
        CHECK(!s->village.event_actor && !s->village.event_actors[9] && !s->village.event_actors[23] && !s->village.event_actors[34]);
        hmbtn_scene_preview_destroy(s);s=NULL;
    }
    hmbtn_host_close(&host);puts("Original pie quest: Doug counter, both Ellen choices, Elli sequence, 300G, cake and failure visits passed.");return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}
