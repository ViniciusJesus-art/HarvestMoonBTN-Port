#include "exploration.h"
#include "psx_data.h"
#include "pad_buttons.h"
#include "inventory_checks.h"
#include "economy_checks.h"
#include "calendar_checks.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Exploration line %d: %s; %s\n",__LINE__,#x,error);goto fail; } } while(0)
typedef struct Fixture {
    uint8_t section[0x2000],floor[88],ani[1724];
    HmbtnModel ground;HmbtnAnimation animation;
    unsigned effects,loads;
} Fixture;
static bool prepare(Fixture *f,unsigned flags,char *error,size_t size)
{
    memset(f,0,sizeof(*f));
    hmbtn_write_le16(f->section+2,0); /* Farm: time advances. */
    uint8_t *b=f->floor;
    hmbtn_write_le32(b,0x42);hmbtn_write_le32(b+8,1);
    hmbtn_write_le32(b+12,28);hmbtn_write_le32(b+16,4);
    hmbtn_write_le32(b+28,60);hmbtn_write_le32(b+32,1);
    for(unsigned i=0;i<4;++i) {
        hmbtn_write_le16(b+40+i*8,(uint16_t)(i&1 ? 15000 : -10000));
        hmbtn_write_le16(b+44+i*8,(uint16_t)(i<2 ? -10000 : 5000));
        hmbtn_write_le16(b+80+i*2,(uint16_t)i);
    }
    hmbtn_write_le16(b+72,(uint16_t)flags);hmbtn_write_le16(b+74,0x2901);
    /* Valid ANI, 256 ranges and 64 frames. Short loop controls report an
     * actual ENDED/JUMPED signal; tests never force animation_done. */
    b=f->ani;hmbtn_write_le32(b,0x0e);
    const uint16_t counts[]={1,1,1,256,64};const uint32_t offsets[]={36,44,48,60,1084};
    for(unsigned i=0;i<5;++i) { hmbtn_write_le16(b+4+i*2,counts[i]);hmbtn_write_le32(b+16+i*4,offsets[i]); }
    for(unsigned i=0;i<256;++i) hmbtn_write_le16(b+60+i*4,64);
    for(unsigned i=0;i<64;++i) { b[1084+i*10+6]=1;b[1084+i*10+7]=0x80; }
    return hmbtn_model_open(&f->ground,f->floor,sizeof(f->floor),error,size)
        && hmbtn_animation_open(&f->animation,f->ani,sizeof(f->ani),error,size);
}
static const HmbtnAnimation *animation(void *context,HmbtnExplorationAnimation kind,unsigned id)
{ (void)kind;(void)id;return &((Fixture *)context)->animation; }
static bool action(void *context,unsigned row,char *error,size_t size)
{ (void)row;(void)error;(void)size;++((Fixture *)context)->loads;return true; }
static bool effect(void *context,const HmbtnExplorationEffect *event,char *error,size_t size)
{ (void)event;(void)error;(void)size;++((Fixture *)context)->effects;return true; }
static bool text(void *context,unsigned bank,unsigned id,uint8_t *bytes,size_t count,char *error,size_t size)
{
    (void)context;(void)bank;(void)id;(void)error;(void)size;
    if(count!=8192) return false;
    memset(bytes,0,count);hmbtn_write_le32(bytes,UINT32_MAX);hmbtn_write_le16(bytes+0x1800,0xffff);return true;
}
static HmbtnExplorationFrame frame(Fixture *f,bool presentation)
{
    return (HmbtnExplorationFrame){.section=f->section,.section_size=sizeof(f->section),.ground=&f->ground,
        .context=f,.animation=animation,.action_bank=action,.text=text,.effect=presentation ? effect : NULL};
}
static HmbtnSession *session(unsigned area)
{
    HmbtnSession *s=hmbtn_session_create(NULL,0);if(!s) return NULL;
    s->completion_reported=true;s->control.enabled=true;s->first_visit_started=true;
    s->intro.script.finished=true;s->intro.player.visible=true;s->area=s->carry.area=area;
    s->game.clock=(HmbtnFarmClock){0,0,2,2,12,0,0};s->intro.area=area;
    s->intro.player.position[0]=-600;s->intro.player.position[2]=-2600;s->intro.player.facing=3;
    s->intro.player.sprite.frame=3;s->refused_zone=-1;
    return s;
}
static bool tick(HmbtnSession *s,HmbtnExplorationFrame *f,uint32_t pressed,uint32_t held,char *error,size_t size)
{ return hmbtn_session_exploration_tick(s,f,(HmbtnLogicalInput){pressed,held},error,size); }
static bool same(const HmbtnSession *a,const HmbtnSession *b)
{
    if(!inventory_equal(&a->game.inventory,&b->game.inventory) || !economy_equal(&a->game.economy,&b->game.economy)
            || !clock_equal(&a->game.clock,&b->game.clock) || !registration_equal(&a->game.registration,&b->game.registration)) return false;
    for(unsigned axis=0;axis<3;++axis) if(a->intro.player.position[axis]!=b->intro.player.position[axis]) return false;
    return a->intro.player.facing==b->intro.player.facing && a->intro.player.sprite.animation==b->intro.player.sprite.animation
        && a->intro.player.sprite.frame==b->intro.player.sprite.frame && a->intro.player.sprite.ticks==b->intro.player.sprite.ticks
        && a->farm_action.phase==b->farm_action.phase && a->farm_action.stamina==b->farm_action.stamina
        && a->carry.phase==b->carry.phase && a->carry.discarded==b->carry.discarded && a->menu.mode==b->menu.mode;
}
int main(void)
{
    char error[512]={0};Fixture fixture;HmbtnSession *s=NULL,*other=NULL;
    CHECK(prepare(&fixture,0,error,sizeof(error)));
    HmbtnExplorationFrame f=frame(&fixture,false),visible=frame(&fixture,true);
    s=session(0);other=session(0);CHECK(s && other);
    /* Same logical input, with presentation effects entirely absent/present. */
    for(unsigned t=0;t<60;++t) {
        uint32_t held=t<20 ? HMBTN_PAD_RIGHT : t<40 ? HMBTN_PAD_UP : HMBTN_PAD_LEFT;
        CHECK(tick(s,&f,0,held,error,sizeof(error)) && tick(other,&visible,0,held,error,sizeof(error)));
        CHECK(same(s,other));
    }
    CHECK(fixture.effects && s->intro.player.position[0]!=-600 && s->game.clock.minute==4);
    CHECK(s->house.inventory==&s->game.inventory && s->menu.economy==&s->game.economy);
    /* The shared NPC probe blocks walking; running retains its original bypass. */
    s->control.mode=0;HmbtnIntroActor *npc=&s->village.people[18].actor;
    npc->visible=true;npc->position[0]=s->intro.player.position[0]+300;npc->position[1]=s->intro.player.position[1];npc->position[2]=s->intro.player.position[2];
    for(unsigned t=0;t<12;++t) CHECK(tick(s,&f,0,HMBTN_PAD_RIGHT,error,sizeof(error)));
    CHECK(s->intro.player.position[0]<npc->position[0]-190);
    for(unsigned t=0;t<12;++t) CHECK(tick(s,&f,0,HMBTN_PAD_RIGHT|HMBTN_PAD_CIRCLE,error,sizeof(error)));
    CHECK(s->intro.player.position[0]>npc->position[0]+200);npc->visible=false;
    /* Pause freezes calendar and player, including the partially elapsed minute. */
    const HmbtnFarmClock before=s->game.clock;int32_t x=s->intro.player.position[0],z=s->intro.player.position[2];
    CHECK(tick(s,&f,HMBTN_PAD_START,0,error,sizeof(error)) && s->menu.mode==HMBTN_MENU_STATUS);
    for(unsigned t=0;t<45;++t) CHECK(tick(s,&f,0,HMBTN_PAD_RIGHT,error,sizeof(error)) && clock_equal(&s->game.clock,&before)
        && s->intro.player.position[0]==x && s->intro.player.position[2]==z);
    CHECK(tick(s,&f,HMBTN_PAD_START,0,error,sizeof(error)));
    for(unsigned t=0;t<40 && s->menu.mode!=HMBTN_MENU_NONE;++t) CHECK(tick(s,&f,0,0,error,sizeof(error)));
    CHECK(s->menu.mode==HMBTN_MENU_NONE);
    /* Tool change and complete hoe swing consume their real ANI completion. */
    s->intro.player.position[0]=-600;s->intro.player.position[2]=-2600;s->intro.player.facing=3;s->control.mode=0;
    s->game.inventory.equipped=(HmbtnStoredTool){6,0,0};
    const unsigned cell=10*45+10,stamina=s->farm_action.stamina;
    CHECK(tick(s,&f,HMBTN_PAD_SQUARE,HMBTN_PAD_SQUARE,error,sizeof(error)) && s->farm_action.phase==HMBTN_FARM_TOOL_CHARGE);
    CHECK(tick(s,&f,0,0,error,sizeof(error)) && s->farm_action.phase==HMBTN_FARM_TOOL_SWING);
    for(unsigned t=0;t<10 && s->farm_action.phase!=HMBTN_FARM_ACTION_FREE;++t) CHECK(tick(s,&f,0,0,error,sizeof(error)));
    CHECK(s->field.cells[cell].value==0x168 && s->farm_action.stamina==stamina-1);
    CHECK(tick(s,&f,0,0,error,sizeof(error)) && s->farm_action.stamina==stamina-1);
    s->game.inventory.equipped=(HmbtnStoredTool){21,0,4};
    CHECK(tick(s,&f,HMBTN_PAD_SQUARE,HMBTN_PAD_SQUARE,error,sizeof(error)));
    for(unsigned t=0;t<12 && s->farm_action.phase!=HMBTN_FARM_ACTION_FREE;++t) CHECK(tick(s,&f,0,0,error,sizeof(error)));
    CHECK(s->field.cells[cell].value==0x169 && s->game.inventory.equipped.count==3);
    /* Pick up and throw a loose item; flight and settlement also need no draw. */
    s->field.cells[cell].value=0x3a;s->game.inventory.equipped.id=0;
    CHECK(tick(s,&f,HMBTN_PAD_CROSS,0,error,sizeof(error)) && s->game.inventory.held_item.id==0x3a);
    for(unsigned t=0;t<10 && s->carry.phase;++t) CHECK(tick(s,&f,0,0,error,sizeof(error)));
    CHECK(!s->carry.phase && s->field.cells[cell].value==0x169);
    CHECK(tick(s,&f,HMBTN_PAD_CROSS,0,error,sizeof(error)));
    for(unsigned t=0;t<80;++t) CHECK(tick(s,&f,0,0,error,sizeof(error)));
    CHECK(!s->game.inventory.held_item.id && !s->carry.phase && !s->carry.thrown[0].item.id);
    /* An original outdoor gather record is reserved and collected once. */
    hmbtn_session_destroy(s);s=session(24);CHECK(s);
    unsigned gather=HMBTN_GATHER_RECORDS,id=0;
    for(unsigned i=125;i<=172;++i) {
        const HmbtnGatherRecord *r=hmbtn_farm_gather_record(i);
        id=hmbtn_farm_gather_item(&s->gather,i,r->location,s->game.clock.season);
        if(id) { gather=i;break; }
    }
    CHECK(gather<HMBTN_GATHER_RECORDS);
    const HmbtnGatherRecord *record=hmbtn_farm_gather_record(gather);
    hmbtn_write_le16(fixture.section+2,(uint16_t)record->location);
    for(unsigned k=0;k<3;++k) s->intro.player.position[k]=record->position[k];
    s->intro.player.position[0]-=300;s->intro.player.facing=3;
    CHECK(tick(s,&f,HMBTN_PAD_CROSS,0,error,sizeof(error)) && s->game.inventory.held_item.id==id && s->gather.taken[gather]);
    for(unsigned t=0;t<10 && s->carry.phase;++t) CHECK(tick(s,&f,0,0,error,sizeof(error)));
    CHECK(!s->carry.phase && !hmbtn_farm_gather_item(&s->gather,gather,record->location,s->game.clock.season));
    /* The same tick completes coop stock take/feed and animal reservations. */
    hmbtn_session_destroy(s);s=session(112);CHECK(s);
    memset(fixture.section,0,sizeof(fixture.section));uint8_t *zone=fixture.section+16;
    hmbtn_write_le16(zone,0x4000);hmbtn_write_le16(zone+2,26);hmbtn_write_le16(zone+8,3);
    hmbtn_write_le16(zone+14,(uint16_t)-1000);hmbtn_write_le16(zone+18,(uint16_t)-3000);
    hmbtn_write_le16(zone+20,1000);hmbtn_write_le16(zone+24,(uint16_t)-2000);
    s->game.inventory.supplies[2]=2;
    CHECK(tick(s,&f,HMBTN_PAD_CROSS,0,error,sizeof(error)) && s->game.inventory.supplies[2]==1 && s->game.inventory.held_item.id==0x69);
    for(unsigned t=0;t<10 && s->feed.phase;++t) CHECK(tick(s,&f,0,0,error,sizeof(error)));
    hmbtn_write_le16(zone+2,40);hmbtn_write_le16(zone+8,3);
    CHECK(tick(s,&f,HMBTN_PAD_CROSS,0,error,sizeof(error)) && s->feed.coop[0]);
    for(unsigned t=0;t<10 && s->feed.phase;++t) CHECK(tick(s,&f,0,0,error,sizeof(error)));
    CHECK(!s->game.inventory.held_item.id && s->game.inventory.supplies[2]==1);
    memset(fixture.section,0,sizeof(fixture.section));
    const uint16_t name[9]={1,2,0xffff};CHECK(hmbtn_poultry_buy(&s->poultry,name,&s->game.clock));
    HmbtnHen *hen=&s->poultry.hens[0];hen->actor.position[0]=-300;hen->actor.position[1]=0;hen->actor.position[2]=-2600;
    s->intro.player.facing=3;s->intro.player.position[0]=-600;s->intro.player.position[2]=-2600;
    CHECK(tick(s,&f,HMBTN_PAD_CROSS,0,error,sizeof(error)) && hen->held && s->poultry.carry_phase);
    for(unsigned t=0;t<12 && s->poultry.carry_phase;++t) CHECK(tick(s,&f,0,0,error,sizeof(error)));
    CHECK(!s->poultry.carry_phase && hmbtn_poultry_held_slot(&s->poultry)==0 && s->game.inventory.held_item.id==0x109);
    CHECK(tick(s,&f,HMBTN_PAD_CROSS,0,error,sizeof(error)));
    for(unsigned t=0;t<12 && s->poultry.carry_phase;++t) CHECK(tick(s,&f,0,0,error,sizeof(error)));
    CHECK(!hen->held && !s->game.inventory.held_item.id && hmbtn_poultry_count(&s->poultry)==1);
    /* Fishing's existing action-bank and flight state complete without video. */
    hmbtn_session_destroy(s);s=session(56);CHECK(s);
    CHECK(prepare(&fixture,2,error,sizeof(error)));f=frame(&fixture,false);hmbtn_write_le16(fixture.section+2,44);
    s->intro.player.position[0]=s->intro.player.position[2]=0;s->intro.player.facing=3;
    s->game.inventory.equipped.id=0x38;
    CHECK(tick(s,&f,HMBTN_PAD_SQUARE,HMBTN_PAD_SQUARE,error,sizeof(error)) && fixture.loads==1);
    for(unsigned t=0;t<300 && s->fishing.phase==HMBTN_FISHING_CAST;++t) CHECK(tick(s,&f,0,HMBTN_PAD_SQUARE,error,sizeof(error)));
    CHECK(s->fishing.phase==HMBTN_FISHING_WAIT);
    CHECK(tick(s,&f,0,0,error,sizeof(error)));
    for(unsigned t=0;t<20 && s->fishing.phase!=HMBTN_FISHING_FREE;++t) CHECK(tick(s,&f,0,0,error,sizeof(error)));
    CHECK(s->fishing.phase==HMBTN_FISHING_FREE && fixture.loads==1);
    /* 17:58 shipping settlement is notified once by the domain observer. */
    s->game.clock=(HmbtnFarmClock){0,0,2,2,17,57,14};
    CHECK(hmbtn_farm_shipping_add(&s->game.economy,0xa3));const unsigned gold=s->game.economy.gold;
    const unsigned value=s->game.economy.shipping.pending_value;
    CHECK(tick(s,&f,0,0,error,sizeof(error)) && s->game.clock.hour==17 && s->game.clock.minute==58 && s->game.economy.gold==gold+value);
    CHECK(tick(s,&f,0,0,error,sizeof(error)) && s->game.economy.gold==gold+value);
    CHECK(!hmbtn_session_exploration_tick(NULL,&f,(HmbtnLogicalInput){0},error,sizeof(error)));
    const HmbtnFarmClock guard=s->game.clock;s->travel_tick=1;
    CHECK(!tick(s,&f,0,HMBTN_PAD_RIGHT,error,sizeof(error)) && clock_equal(&guard,&s->game.clock));
    s->travel_tick=0;
    HmbtnExplorationFrame missing=f;missing.animation=NULL;
    CHECK(!tick(s,&missing,0,0,error,sizeof(error)));
    hmbtn_session_destroy(other);hmbtn_session_destroy(s);
    puts("Headless exploration: movement, pause/calendar, tools/ANI, carry/flight, feed, poultry, fishing and shipping passed.");return 0;
fail:
    hmbtn_session_destroy(other);hmbtn_session_destroy(s);return 1;
}
