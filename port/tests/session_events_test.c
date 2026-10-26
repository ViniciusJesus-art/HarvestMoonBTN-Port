#include "session.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Session events line %d: %s; %s\n",__LINE__,#x,error);goto fail; } } while(0)
static void instruction(uint8_t *p,unsigned type,unsigned yes,unsigned no,unsigned flags,int a,int b,int x,int y)
{
    memset(p,0,16);p[0]=(uint8_t)type;p[1]=(uint8_t)yes;p[2]=(uint8_t)no;p[3]=(uint8_t)flags;
    hmbtn_write_le16(p+4,(uint16_t)a);hmbtn_write_le16(p+6,(uint16_t)b);
    hmbtn_write_le32(p+8,(uint32_t)x);hmbtn_write_le32(p+12,(uint32_t)y);
}
int main(void)
{
    HmbtnSession *s=hmbtn_session_create(NULL,0),*other=hmbtn_session_create(NULL,0);
    char error[256]={0};uint8_t schedules[64]={0},code[48],section[0x2000]={0};
    CHECK(s && other);
    CHECK(s->house.inventory==&s->game.inventory && s->village.player==&s->intro.player);
    CHECK(hmbtn_session_bind_schedules(s,schedules,sizeof(schedules)));
    /* Wait at the same instruction across ticks; source bytes can disappear. */
    instruction(code,3,1,0,1,4,0,0,0);
    instruction(code+16,4,255,255,1,17,0,900,500);
    instruction(code+32,255,0,0,0,0,0,0,0);
    CHECK(hmbtn_session_open_event(s,288,code,sizeof(code),error,sizeof(error)));
    CHECK(hmbtn_session_open_event(other,288,code,sizeof(code),error,sizeof(error)));
    memset(code,0,sizeof(code));
    CHECK(s->intro.script.data==s->event_code && other->intro.script.data==other->event_code);
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && s->intro.script.pc==0 && s->intro.slots[0]==1);
    CHECK(!other->intro.slots[0]);
    const uint8_t *owned=s->intro.script.data;
    CHECK(!hmbtn_session_open_event(s,116,code,sizeof(code),error,sizeof(error)));
    CHECK(s->intro.script.data==owned && s->intro.event==288 && s->intro.slots[0]==1);
    CHECK(!hmbtn_session_open_event(s,116,s->event_code,sizeof(s->event_code)+16,error,sizeof(error)));
    error[0]=0;
    HmbtnVillager *resident=&s->village.people[17];
    resident->actor.position[0]=123;resident->actor.position[2]=100;
    resident->actor.visible=true;resident->placed=true;resident->location=9;
    resident->schedule[0][0]=360;s->village.minute=360;
    const unsigned friendship=resident->friendship;
    HmbtnIntroActor *actor=hmbtn_session_bind_event_actor(s,17,HMBTN_SESSION_ACTOR_PRIMARY,true);
    CHECK(actor==&s->intro.mayor && actor->position[0]==123);
    CHECK(hmbtn_village_actor(&s->village,17)==actor);
    resident->blocked_by_player=true;resident->delay=7;
    hmbtn_village_clock(&s->village,&s->game.clock,HMBTN_CLOCK_MINUTE,0,false);
    CHECK(resident->delay==7); /* A dormant route cannot accumulate collision delay. */
    resident->delay=0;
    actor->position[0]=900;actor->position[2]=500;
    CHECK(hmbtn_session_bind_event_actor(s,17,HMBTN_SESSION_ACTOR_PRIMARY,true)==actor && actor->position[0]==900);
    CHECK(!hmbtn_session_bind_event_actor(s,17,HMBTN_SESSION_ACTOR_VISIT,true));
    CHECK(!hmbtn_session_bind_event_actor(s,1,HMBTN_SESSION_ACTOR_INTRO,true));
    CHECK(!hmbtn_session_bind_event_actor(s,27,HMBTN_SESSION_ACTOR_INTRO,true));
    CHECK(!hmbtn_session_bind_event_actor(s,33,HMBTN_SESSION_ACTOR_INTRO,true));
    CHECK(!hmbtn_session_bind_event_actor(s,41,HMBTN_SESSION_ACTOR_VISIT,true));
    CHECK(!hmbtn_session_bind_event_actor(NULL,17,HMBTN_SESSION_ACTOR_PRIMARY,true));
    CHECK(hmbtn_session_bind_event_actor(s,9,HMBTN_SESSION_ACTOR_INTRO,true)==&s->intro.elli);
    CHECK(hmbtn_session_bind_event_actor(s,27,HMBTN_SESSION_ACTOR_VISIT,true)==&s->visit.actors[27]);
    HmbtnIntroActor player={.position={900,0,800},.facing=0};
    CHECK(hmbtn_village_target(&s->village,&player)==17);
    const int32_t next[3]={900,0,650};
    CHECK(hmbtn_village_blocked(&s->village,player.position,next));
    hmbtn_write_le16(section+2,9);
    /* A matching authored conversation zone must use the same primary actor. */
    uint8_t *zone=section+16;
    hmbtn_write_le16(zone,0x4000);hmbtn_write_le16(zone+2,36);hmbtn_write_le16(zone+4,17);
    hmbtn_write_le16(zone+8,5);
    hmbtn_write_le16(zone+14,600);hmbtn_write_le16(zone+16,0);hmbtn_write_le16(zone+18,300);
    hmbtn_write_le16(zone+20,1200);hmbtn_write_le16(zone+22,0);hmbtn_write_le16(zone+24,900);
    hmbtn_write_le16(zone+26,900);hmbtn_write_le16(zone+30,500);
    CHECK(hmbtn_village_zone_target(&s->village,section,sizeof(section),&player)==17);
    /* Override ownership suspends the route, even when exploration is free. */
    hmbtn_write_le16(section+0x1000,5);hmbtn_write_le16(section+0x1006,10);
    hmbtn_write_le16(section+0x100a,20);
    hmbtn_village_tick(&s->village,section,sizeof(section),NULL,false);
    CHECK(resident->actor.position[0]==123 && actor->position[0]==900);
    hmbtn_session_unbind_resources(s);
    CHECK(!s->village_ready && s->village.player==&s->intro.player && hmbtn_village_actor(&s->village,17)==actor);
    CHECK(s->intro.script.pc==0 && s->intro.slots[0]==1 && s->intro.script.data==owned);
    CHECK(hmbtn_session_bind_schedules(s,schedules,sizeof(schedules)));
    for(unsigned i=0;i<3;++i) CHECK(hmbtn_session_event_tick(s,error,sizeof(error)));
    CHECK(s->intro.script.pc==1 && s->intro.slots[0]==4);
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && s->intro.script.pc==2);
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && s->intro.script.finished);
    s->visit.active=true;s->visit.event=130;
    s->village.flags[3]=s->village.flags[0xB0]=true;
    s->intro.elli.visible=s->intro.ellen.visible=s->intro.stu.visible=true;
    hmbtn_session_finish_visit(s);
    CHECK(!s->visit.active && !s->village.event_actor && !s->village.event_actors[9] && !s->village.event_actors[27]);
    CHECK(!s->village.flags[3] && !s->village.flags[0xB0] && !s->intro.mayor.visible && !s->intro.elli.visible);
    CHECK(!resident->placed && resident->actor.position[0]==123 && resident->friendship==friendship);
    CHECK(hmbtn_village_actor(&s->village,17)==&resident->actor);
    hmbtn_village_tick(&s->village,section,sizeof(section),NULL,false);
    CHECK(resident->placed && resident->actor.visible && resident->actor.position[0]==10 && resident->actor.position[2]==20);
    CHECK(!other->intro.script.finished && !other->intro.slots[0]);
    hmbtn_session_destroy(other);other=NULL;
    CHECK(resident->friendship==friendship && s->intro.script.finished);
    /* Named tour/funeral actors share live NPC identities; childhood IDs do not. */
    instruction(code,255,0,0,0,0,0,0,0);
    CHECK(hmbtn_session_open_event(s,12,code,16,error,sizeof(error)));
    CHECK(hmbtn_village_actor(&s->village,17)==&s->intro.mayor && hmbtn_village_actor(&s->village,23)==&s->intro.ellen
        && hmbtn_village_actor(&s->village,26)==&s->intro.zack);
    CHECK(hmbtn_session_open_event(s,172,code,16,error,sizeof(error)));
    CHECK(!hmbtn_village_event_actor(&s->village,17) && hmbtn_village_actor(&s->village,10)==&s->intro.carter
        && hmbtn_village_actor(&s->village,15)==&s->intro.barley && hmbtn_village_actor(&s->village,19)==&s->intro.rick);
    CHECK(hmbtn_session_open_event(s,117,code,16,error,sizeof(error)));
    CHECK(!hmbtn_village_event_actor(&s->village,10) && !hmbtn_village_event_actor(&s->village,1));
    hmbtn_session_destroy(s);
    puts("Owned bytecode/cursors, independent events, actor authority, resource detachment and route resumption passed.");return 0;
fail:
    hmbtn_session_destroy(other);hmbtn_session_destroy(s);return 1;
}
