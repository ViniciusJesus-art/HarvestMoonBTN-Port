/* Original request 288 -> delivery 289 with installed BINs and current routes.
 * Dates, inventory and player placement are fixtures; quest flags come from scripts. */
#include "scene_preview.c"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Ingredients line %d scenario %u: %s; %s (event %u step %u)\n",__LINE__,scenario,#x,error,s ? s->intro.event : 0,s ? s->intro.script.pc : 0);goto fail; } } while(0)
static bool step(HmbtnScenePreview *s,uint32_t pressed,char *error,size_t size)
{
    s->pending_pressed=pressed;hmbtn_scene_data_tick(&s->scene);
    const bool ok=opening_tick(s,HMBTN_PAD_CROSS,error,size);s->pending_pressed=0;return ok;
}
static bool travel(HmbtnScenePreview *s,unsigned area,char *error,size_t size)
{
    if(!load_area(s,s->data_root,area,error,size)) return false;
    s->intro.area=area;memset(s->intro.player.position,0,sizeof(s->intro.player.position));
    return step(s,0,error,size) && step(s,0,error,size);
}
static bool approach(HmbtnScenePreview *s,unsigned id,char *error,size_t size)
{
    const HmbtnIntroActor *actor=id==17 && s->visit.active ? &s->intro.mayor : s->session->village.event_actors[id] ? s->session->village.event_actors[id] : &s->session->village.people[id].actor;
    if(!actor->visible) return false;
    memcpy(s->intro.player.position,actor->position,sizeof(actor->position));
    s->intro.player.position[2]-=300;s->intro.player.facing=2;s->control.mode=0;
    return hmbtn_village_target(&s->session->village,&s->intro.player)==id && step(s,HMBTN_PAD_CROSS,error,size);
}
static bool original_message(HmbtnScenePreview *s,char *error,size_t size)
{
    uint8_t record[8192];HmbtnMessage expected;
    const HmbtnTextResourceId id={HMBTN_TEXT_RECORD,s->intro.message_bank,s->intro.message};
    return hmbtn_text_resource_read(s->data_root,id,record,sizeof(record),error,size)
        && hmbtn_feed_shop_expand(record,sizeof(record),hmbtn_game_registration_const(&s->session->game))
        && hmbtn_message_open(&expected,record,sizeof(record),error,size)
        && expected.count==s->dialogue.message.count
        && !memcmp(expected.words,s->dialogue.message.words,expected.count*sizeof(*expected.words));
}
static bool play(HmbtnScenePreview *s,SDL_Renderer *renderer,bool refuse,unsigned until,bool messages[2042],char *error,size_t size)
{
    bool selected=false,spoke=false;unsigned last=UINT32_MAX;
    for(unsigned frame=0;frame<6000 && s->visit.active;++frame) {
        if(s->intro.script.boundary) return false;
        if(s->intro.message && s->loaded_message==s->intro.message && last!=s->intro.message) {
            if(s->intro.message>=2042 || !original_message(s,error,size)
                    || !hmbtn_scene_preview_draw(s,renderer,0,0,0,error,size)) return false;
            messages[s->intro.message]=true;last=s->intro.message;
        }
        if(!s->intro.message) last=UINT32_MAX;
        if(until && s->intro.script.pc==until && s->visit.free && !s->intro.message && !s->visit.talk) return true;
        uint32_t pressed=s->intro.message ? HMBTN_PAD_CROSS : 0;
        if(s->dialogue.message.choices && s->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END && !selected) {
            selected=true;if(refuse) pressed=HMBTN_PAD_DOWN;
        }
        if(s->visit.event==288 && s->control.enabled && !s->intro.message
                && s->intro.mayor.position[2]<=3959 && !spoke) {
            if(!approach(s,17,error,size)) return false;spoke=true;continue;
        }
        const HmbtnFarmClock before=s->session->game.clock;
        if(!step(s,pressed,error,size)) return false;
        if(s->intro.message && memcmp(&before,&s->session->game.clock,sizeof(before))) return false;
    }
    return !until && !s->visit.active && s->control.enabled;
}
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};unsigned scenario=0;
    CHECK(hmbtn_host_open(&host));
    static const unsigned requested[9]={151,65,71,67,77,290,139,162,68};
    static const unsigned prompt[9]={927,875,899,903,907,911,915,919,923};
    static const unsigned receipt[9]={928,876,900,904,908,912,916,920,924};
    static const unsigned reminder[9]={929,877,901,905,909,913,917,921,925};
    for(scenario=0;scenario<20;++scenario) {
        const bool extra_only=scenario>=18,refuse=scenario>=9 && !extra_only;
        const unsigned year=extra_only ? scenario-14 : scenario%9,cycle=(year+1)%9;
        HmbtnFarmClock clock={year,0,14,0,6,0,0};bool messages[2042]={0};
        s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(s);
        CHECK(step(s,0,error,sizeof(error)) && step(s,0,error,sizeof(error)) && s->visit.event==288);
        CHECK(play(s,host.renderer,false,0,messages,error,sizeof(error)) && s->session->village.flags[217]);
        CHECK(travel(s,176,error,sizeof(error)));
        CHECK(s->session->village.people[17].actor.visible);
        for(unsigned id=1;id<HMBTN_VILLAGERS;++id) { s->session->village.people[id].friendship=100;s->session->village.people[id].affection=5000; }
        s->session->village.people[1].friendship=250;s->session->village.people[3].affection=65000;
        s->menu.stats.helper_affection[0]=250;s->menu.stats.helper_affection[1]=100;
        s->house.inventory->rucksack_level=2;memset(s->house.inventory->items,0,sizeof(s->house.inventory->items));
        s->house.inventory->held_item=(HmbtnStoredTool){(uint16_t)(extra_only ? year==4 ? 232 : 227 : requested[cycle]),0,7};
        s->house.inventory->items[0]=s->house.inventory->held_item;
        CHECK(!hmbtn_farm_event_trigger(&s->triggers,&s->session->village,&s->session->game.clock,0,s->previous_place,3,17,s->house.inventory));
        s->house.inventory->items[1]=s->house.inventory->held_item;s->house.inventory->items[2]=s->house.inventory->held_item;
        if(cycle==5 && !extra_only) { s->house.inventory->items[0].id=291;s->house.inventory->items[1].id=293;s->house.inventory->items[2].id=293; }
        if(cycle==6 && !extra_only) { s->house.inventory->items[0].id=140;s->house.inventory->items[1].id=141;s->house.inventory->items[2].id=141; }
        /* The gate also counts golden eggs and gold milk, whereas the script
         * removes only the four normal egg / three normal milk variants. */
        s->house.inventory->items[3]=(HmbtnStoredTool){(uint16_t)(cycle==5 ? 232 : cycle==6 ? 227 : 161),0,1};
        const HmbtnInventory before=*s->house.inventory;
        CHECK(hmbtn_farm_event_trigger(&s->triggers,&s->session->village,&s->session->game.clock,0,s->previous_place,3,17,s->house.inventory)==289);
        HmbtnFarmClock late=s->session->game.clock;late.day=21;
        CHECK(!hmbtn_farm_event_trigger(&s->triggers,&s->session->village,&late,0,s->previous_place,3,17,s->house.inventory));
        s->session->village.days.count0[26]=7;
        CHECK(!hmbtn_farm_event_trigger(&s->triggers,&s->session->village,&s->session->game.clock,0,s->previous_place,3,17,s->house.inventory));
        s->session->village.days.count0[26]=0;
        CHECK(approach(s,17,error,sizeof(error)) && s->visit.active && s->visit.event==289);
        CHECK(play(s,host.renderer,refuse,86,messages,error,sizeof(error)) && messages[prompt[cycle]]);
        CHECK(s->control.enabled && s->visit.free && s->session->village.flags[217]==refuse && !s->session->village.flags[540]);
        if(argc==3 && (scenario==0 || scenario==4)) {
            CHECK(hmbtn_scene_preview_draw(s,host.renderer,0,0,0,error,sizeof(error)));
            SDL_Surface *pixels=SDL_RenderReadPixels(host.renderer,NULL);CHECK(pixels);
            char path[1024];snprintf(path,sizeof(path),"%s/ingredients-%u.bmp",argv[2],scenario);
            const bool saved=SDL_SaveBMP(pixels,path);SDL_DestroySurface(pixels);CHECK(saved);
        }
        if(refuse) {
            CHECK(messages[878] && !memcmp(&before.held_item,&s->house.inventory->held_item,sizeof(before.held_item))
                && !memcmp(before.items,s->house.inventory->items,sizeof(before.items)));
            CHECK(s->session->village.people[17].friendship==100 && s->menu.stats.helper_affection[1]==100);
        } else {
            CHECK(messages[receipt[cycle]]);
            if(extra_only) CHECK(!memcmp(&before.held_item,&s->house.inventory->held_item,sizeof(before.held_item))
                && !memcmp(before.items,s->house.inventory->items,sizeof(before.items)));
            else CHECK(!s->house.inventory->held_item.id && !s->house.inventory->items[0].id && !s->house.inventory->items[1].id);
            CHECK(!memcmp(&before.items[2],&s->house.inventory->items[2],sizeof(before.items[2]))
                && !memcmp(before.items+3,s->house.inventory->items+3,3*sizeof(before.items[0])));
            CHECK(s->session->village.people[17].friendship==120 && s->session->village.people[1].friendship==255
                && s->session->village.people[3].affection==65535 && s->session->village.people[27].friendship==100
                && s->menu.stats.helper_affection[0]==255 && s->menu.stats.helper_affection[1]==120);
        }
        CHECK(approach(s,17,error,sizeof(error)) && play(s,host.renderer,false,86,messages,error,sizeof(error)));
        CHECK(messages[refuse ? 879 : reminder[cycle]]);
        CHECK(s->session->village.people[17].friendship==(refuse ? 100u : 120u));
        /* Other residents in Thomas's house retain their original bank-0 replies. */
        for(unsigned id=1;id<HMBTN_VILLAGERS;++id) if(id!=17 && s->session->village.event_actors[id] && s->session->village.event_actors[id]->visible) {
            CHECK(approach(s,id,error,sizeof(error)) && play(s,host.renderer,false,86,messages,error,sizeof(error)));
            CHECK(messages[id==15 ? 923 : id==28 ? 1411 : 1436]);
        }
        CHECK(travel(s,32,error,sizeof(error)) && play(s,host.renderer,false,0,messages,error,sizeof(error)));
        CHECK(!s->session->village.event_actor && !s->intro.game_command && !s->intro.mayor.visible);
        for(unsigned id=1;id<HMBTN_VILLAGERS;++id) CHECK(!s->session->village.event_actors[id]);
        CHECK(travel(s,176,error,sizeof(error)));
        CHECK(hmbtn_farm_event_trigger(&s->triggers,&s->session->village,&s->session->game.clock,0,s->previous_place,3,17,s->house.inventory)==(refuse ? 289u : 0u));
        hmbtn_scene_preview_destroy(s);s=NULL;
    }
    hmbtn_host_close(&host);puts("Original ingredient delivery: nine cycles, yes/no, record removal, affinity, reminders and bank-0 residents passed.");return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}
