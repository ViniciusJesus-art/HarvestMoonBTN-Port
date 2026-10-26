/* Original 128..133, seasonal tables, routes, ANI, pictures and exact installed text.
 * Only dates/affinity and travel positions are fixtures; no quest items/flags are granted. */
#include "scene_fixture.h"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Pie line %d scenario %u: %s; %s (event %u step %u)\n",__LINE__,scenario,#x,error,s ? hmbtn_scene_preview_session(s)->intro.event : 0,s ? hmbtn_scene_preview_session(s)->intro.script.pc : 0);goto fail; } } while(0)
static int half(const uint8_t *p) { const unsigned n=hmbtn_read_le16(p);return n<32768 ? (int)n : (int)n-65536; }
static bool step(HmbtnScenePreview *s,uint32_t pressed,char *error,size_t size)
{
    return hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){pressed,HMBTN_PAD_CROSS},error,size);
}
static bool travel(HmbtnScenePreview *s,unsigned area,char *error,size_t size)
{
    if(!hmbtn_scene_preview_load_area(s,area,error,size)) return false;
    hmbtn_scene_preview_session(s)->intro.area=area;hmbtn_scene_preview_session(s)->intro.player.position[0]=0;hmbtn_scene_preview_session(s)->intro.player.position[1]=0;hmbtn_scene_preview_session(s)->intro.player.position[2]=0;
    for(unsigned i=0;i<2;++i) if(!step(s,0,error,size)) return false;
    return true;
}
static bool capture(HmbtnScenePreview *s,HmbtnHost *host,const char *dir,const char *name,char *error,size_t size)
{
    if(!hmbtn_scene_preview_draw(s,host->renderer,error,size)) return false;
    if(!dir) return true;
    SDL_Surface *pixels=SDL_RenderReadPixels(host->renderer,NULL);if(!pixels) return false;
    char path[1024];snprintf(path,sizeof(path),"%s/%s.bmp",dir,name);
    const bool saved=SDL_SaveBMP(pixels,path);SDL_DestroySurface(pixels);return saved;
}
static bool play(HmbtnScenePreview *s,HmbtnHost *host,unsigned event,bool second,bool timeout,
    bool messages[2046],const char *dir,char *error,size_t size)
{
    bool talked=false,selected=false,coordinated=false;unsigned last=UINT32_MAX;
    for(unsigned frame=0;frame<8000 && (hmbtn_scene_preview_session(s)->visit.active || hmbtn_scene_preview_session(s)->event_item.phase!=HMBTN_EVENT_ITEM_IDLE);++frame) {
        if(hmbtn_scene_preview_session(s)->intro.script.boundary) return false;
        uint32_t pressed=hmbtn_scene_preview_session(s)->intro.message ? HMBTN_PAD_CROSS : 0;
        if(hmbtn_scene_preview_session(s)->intro.message && hmbtn_scene_preview_session(s)->runtime.loaded_message==hmbtn_scene_preview_session(s)->intro.message && last!=hmbtn_scene_preview_session(s)->intro.message) {
            const unsigned message=hmbtn_scene_preview_session(s)->intro.message;if(message>=2046) return false;
            uint8_t record[8192];HmbtnMessage expected;
            const HmbtnTextResourceId id={HMBTN_TEXT_RECORD,1,message};
            if(!hmbtn_text_resource_read(hmbtn_scene_preview_resources(s).root,id,record,sizeof(record),error,size)
                    || !hmbtn_feed_shop_expand(record,sizeof(record),hmbtn_game_registration_const(&hmbtn_scene_preview_session(s)->game))
                    || !hmbtn_message_open(&expected,record,sizeof(record),error,size)
                    || expected.count!=hmbtn_scene_preview_session(s)->dialogue.message.count
                    || memcmp(expected.words,hmbtn_scene_preview_session(s)->dialogue.message.words,expected.count*sizeof(*expected.words))) return false;
            messages[message]=true;last=message;
        }
        if(!hmbtn_scene_preview_session(s)->intro.message) last=UINT32_MAX;
        if(hmbtn_scene_preview_session(s)->dialogue.message.choices && hmbtn_scene_preview_session(s)->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END && !selected) {
            selected=true;if(second) pressed=HMBTN_PAD_DOWN;
        }
        if(event==130 && hmbtn_scene_preview_session(s)->control.enabled && hmbtn_scene_preview_session(s)->visit.free && !hmbtn_scene_preview_session(s)->intro.message && hmbtn_scene_preview_session(s)->intro.script.pc>=28) {
            if(!travel(s,32,error,size)) return false;
        } else if(event!=129 && event!=130 && hmbtn_scene_preview_session(s)->control.enabled && !hmbtn_scene_preview_session(s)->intro.message
                && hmbtn_scene_preview_session(s)->intro.mayor.position[2]<=3959 && !talked && !timeout) {
            memcpy(hmbtn_scene_preview_session(s)->intro.player.position,hmbtn_scene_preview_session(s)->intro.mayor.position,sizeof(hmbtn_scene_preview_session(s)->intro.player.position));
            hmbtn_scene_preview_session(s)->intro.player.position[2]-=300;hmbtn_scene_preview_session(s)->intro.player.facing=2;hmbtn_scene_preview_session(s)->control.mode=0;
            if(hmbtn_village_target(&hmbtn_scene_preview_session(s)->village,&hmbtn_scene_preview_session(s)->intro.player)!=17) return false;
            pressed=HMBTN_PAD_CROSS;talked=true;
        }
        if(event==130 && hmbtn_scene_preview_session(s)->intro.message==394 && hmbtn_scene_preview_session(s)->intro.text_bank==145) {
            if(hmbtn_scene_preview_session(s)->intro.script.pc>=16) coordinated=true;
            if((hmbtn_scene_preview_session(s)->dialogue.message.phase==HMBTN_MESSAGE_WAIT_PAGE || hmbtn_scene_preview_session(s)->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END)
                    && !capture(s,host,dir,"elli-visit",error,size)) return false;
        }
        if(hmbtn_scene_preview_session(s)->event_item.phase==HMBTN_EVENT_ITEM_EAT
                && !capture(s,host,dir,"thomas-cake",error,size)) return false;
        if(event==129 && hmbtn_scene_preview_session(s)->event_item.phase==HMBTN_EVENT_ITEM_SHOW
                && !capture(s,host,dir,"doug-pie",error,size)) return false;
        if(!step(s,pressed,error,size)) return false;
    }
    return !hmbtn_scene_preview_session(s)->visit.active && hmbtn_scene_preview_session(s)->event_item.phase==HMBTN_EVENT_ITEM_IDLE && hmbtn_scene_preview_session(s)->control.enabled
        && (event!=130 || !second || coordinated);
}
static bool counter(HmbtnScenePreview *s,char *error,size_t size)
{
    for(unsigned i=0;i<50;++i) {
        const uint8_t *p=hmbtn_scene_preview_resources(s).section+16+i*44;
        if(hmbtn_read_le16(p+2)!=36 || hmbtn_read_le16(p+4)!=6) continue;
        const unsigned facing=hmbtn_read_le16(p+8);if(facing>3) continue;
        for(unsigned axis=0;axis<3;++axis) hmbtn_scene_preview_session(s)->intro.player.position[axis]=(half(p+14+axis*2)+half(p+20+axis*2))/2;
        hmbtn_scene_preview_session(s)->intro.player.position[facing&1u ? 0 : 2]+=facing<2 ? 224 : -224;hmbtn_scene_preview_session(s)->intro.player.facing=facing;
        return hmbtn_village_zone_target(&hmbtn_scene_preview_session(s)->village,hmbtn_scene_preview_resources(s).section,(*hmbtn_scene_preview_resources(s).scene).terrain.size+0x2000,&hmbtn_scene_preview_session(s)->intro.player)==6
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
        CHECK(step(s,0,error,sizeof(error)));hmbtn_scene_preview_session(s)->village.people[17].friendship=100;
        CHECK(step(s,0,error,sizeof(error)) && hmbtn_scene_preview_session(s)->visit.event==128);
        CHECK(play(s,&host,128,false,false,messages,NULL,error,sizeof(error)) && hmbtn_scene_preview_session(s)->village.flags[203]);
        if(scenario!=2) {
            CHECK(travel(s,204,error,sizeof(error)));
            CHECK(counter(s,error,sizeof(error)) && hmbtn_scene_preview_session(s)->visit.active && hmbtn_scene_preview_session(s)->visit.event==129);
            CHECK(play(s,&host,129,false,false,messages,argc==3 && !scenario ? argv[2] : NULL,error,sizeof(error)));
            CHECK(messages[390] && hmbtn_scene_preview_session(s)->house.inventory->held_item.id==204 && hmbtn_scene_preview_session(s)->village.flags[380]);
            CHECK(hmbtn_scene_preview_session(s)->village.people[6].friendship==70);
            HmbtnInventory empty=*hmbtn_scene_preview_session(s)->house.inventory;empty.held_item=(HmbtnStoredTool){0};
            CHECK(!hmbtn_farm_event_trigger(&hmbtn_scene_preview_session(s)->triggers,&hmbtn_scene_preview_session(s)->village,&hmbtn_scene_preview_session(s)->game.clock,0,hmbtn_scene_preview_session(s)->previous_place,10,6,&empty));
        }
        if(scenario<2) {
            CHECK(travel(s,172,error,sizeof(error)));
            const HmbtnIntroActor *ellen=&hmbtn_scene_preview_session(s)->village.people[23].actor;CHECK(ellen->visible);
            memcpy(hmbtn_scene_preview_session(s)->intro.player.position,ellen->position,sizeof(ellen->position));
            hmbtn_scene_preview_session(s)->intro.player.position[2]-=300;hmbtn_scene_preview_session(s)->intro.player.facing=2;
            CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && hmbtn_scene_preview_session(s)->visit.active && hmbtn_scene_preview_session(s)->visit.event==130);
            CHECK(play(s,&host,130,scenario==1,false,messages,argc==3 && scenario==1 ? argv[2] : NULL,error,sizeof(error)));
            CHECK(messages[391] && hmbtn_scene_preview_session(s)->village.flags[381] && hmbtn_scene_preview_session(s)->game.economy.gold==800 && hmbtn_scene_preview_session(s)->game.economy.ledger[0][0]==300);
            CHECK(hmbtn_scene_preview_session(s)->village.people[23].friendship==70 && hmbtn_scene_preview_session(s)->house.inventory->held_item.id==(scenario==1 ? 204u : 0u));
            if(scenario==1) CHECK(messages[393] && messages[394]);
        }
        CHECK(travel(s,80,error,sizeof(error)));
        hmbtn_farm_clock_advance_notify(&hmbtn_scene_preview_session(s)->game.clock,hmbtn_farm_clock_sleep_minutes(&hmbtn_scene_preview_session(s)->game.clock,0),hmbtn_session_clock_events,hmbtn_scene_preview_session(s));
        CHECK(hmbtn_scene_preview_session(s)->village.days.count0[13]==1);
        unsigned before[37];for(unsigned i=0;i<37;++i) before[i]=hmbtn_scene_preview_session(s)->village.people[i].friendship;
        if(scenario>=3) {
            CHECK(travel(s,172,error,sizeof(error)));
            CHECK(hmbtn_scene_preview_session(s)->house.inventory->held_item.id==204
                && !hmbtn_farm_event_trigger(&hmbtn_scene_preview_session(s)->triggers,&hmbtn_scene_preview_session(s)->village,&hmbtn_scene_preview_session(s)->game.clock,0,hmbtn_scene_preview_session(s)->previous_place,3,23,hmbtn_scene_preview_session(s)->house.inventory));
        }
        hmbtn_scene_preview_session(s)->farm_action.fatigue=20;const HmbtnStoredTool held=hmbtn_scene_preview_session(s)->house.inventory->held_item;
        const unsigned expected=scenario<2 ? 131u : scenario==2 ? 133u : 132u;
        CHECK(travel(s,0,error,sizeof(error)) && hmbtn_scene_preview_session(s)->visit.active && hmbtn_scene_preview_session(s)->visit.event==expected);
        CHECK(play(s,&host,expected,false,scenario==4,messages,argc==3 && scenario==1 ? argv[2] : NULL,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_session(s)->village.flags[251+expected] && !memcmp(&held,&hmbtn_scene_preview_session(s)->house.inventory->held_item,sizeof(held)));
        CHECK(hmbtn_scene_preview_session(s)->farm_action.fatigue==(scenario<2 ? 16u : 20u));
        if(scenario<2) CHECK(messages[395] && messages[396] && messages[397]);
        else {
            CHECK(hmbtn_scene_preview_session(s)->village.people[17].friendship==before[17]-30 && hmbtn_scene_preview_session(s)->village.people[23].friendship==before[23]-20);
            CHECK(hmbtn_scene_preview_session(s)->village.people[6].friendship==before[6]-(scenario==2 ? 0u : 30u));
            CHECK(scenario==4 ? !messages[398] : messages[scenario==2 ? 399 : 398]);
        }
        for(unsigned i=0;i<20;++i) CHECK(step(s,0,error,sizeof(error)) && !hmbtn_scene_preview_session(s)->visit.active);
        CHECK(!hmbtn_scene_preview_session(s)->village.event_actor && !hmbtn_scene_preview_session(s)->village.event_actors[9] && !hmbtn_scene_preview_session(s)->village.event_actors[23] && !hmbtn_scene_preview_session(s)->village.event_actors[34]);
        hmbtn_scene_preview_destroy(s);s=NULL;
    }
    hmbtn_host_close(&host);puts("Original pie quest: Doug counter, both Ellen choices, Elli sequence, 300G, cake and failure visits passed.");return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}
