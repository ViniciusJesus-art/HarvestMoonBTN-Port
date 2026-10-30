/* Original guards, prices, deadlines and effects; all tiers/kinds/directions. */
#include "save_fixture.h"
#include "tool_upgrade.h"
#include "inventory.h"
#include "mine.h"
#include <stdio.h>
#include <stdlib.h>
static void input(HmbtnSession *s,uint32_t pad)
{
    HmbtnFeedShop *shop=&s->feed_shop;shop->loaded=shop->request;shop->message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_feed_shop_tick(shop,&s->house,&s->game.economy,&s->menu.stats,pad,pad);
}
static void until(HmbtnSession *s,HmbtnFeedShopPhase phase)
{
    for(unsigned i=0;i<30 && (s->feed_shop.phase!=phase || s->feed_shop.closing);++i) {
        s->feed_shop.loaded=s->feed_shop.request;s->feed_shop.message.phase=HMBTN_MESSAGE_CLOSED;
        hmbtn_feed_shop_tick(&s->feed_shop,&s->house,&s->game.economy,&s->menu.stats,0,0);
    }
    assert(s->feed_shop.phase==phase && !s->feed_shop.closing);
}
static HmbtnSession *reload(HmbtnSession *s)
{
    char error[256];uint8_t *data=NULL;size_t bytes=0;
    assert(hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    HmbtnSession *next=hmbtn_save_decode(data,bytes,error,sizeof(error));assert(next);
    assert(next->feed_shop.action==&next->farm_action && next->feed_shop.makers==&next->makers);
    free(data);hmbtn_session_destroy(s);
    static const uint8_t schedules[1]={0};assert(hmbtn_session_bind_schedules(next,schedules,1));return next;
}
static void swing(HmbtnSession *s,unsigned kind,unsigned rank,unsigned charge,unsigned face,unsigned stock)
{
    hmbtn_farm_action_init(&s->farm_action);s->game.weather.today=0;s->game.clock.hour=12;
    s->game.inventory.equipped=(HmbtnStoredTool){(uint16_t)(1+kind*5+rank),(uint16_t)rank,(uint16_t)stock};
    HmbtnIntroActor *p=&s->intro.player;p->facing=face;p->animation_done=false;
    assert(hmbtn_farm_field_position(&s->field,s->field.indoor ? 0 : 10,s->field.indoor ? 3 : 10,p->position));
    assert(hmbtn_farm_action_tick(&s->farm_action,p,&s->house,&s->field,NULL,HMBTN_PAD_SQUARE,HMBTN_PAD_SQUARE));
    assert(s->farm_action.phase==HMBTN_FARM_TOOL_CHARGE);
    for(unsigned i=1;i<charge*20;++i) assert(hmbtn_farm_action_tick(&s->farm_action,p,&s->house,&s->field,NULL,0,HMBTN_PAD_SQUARE));
    assert(hmbtn_farm_action_tick(&s->farm_action,p,&s->house,&s->field,NULL,0,0));
    assert(s->farm_action.phase==HMBTN_FARM_TOOL_SWING && p->sprite.animation==72+kind*20+face*5+charge);
    p->animation_done=true;
    for(unsigned i=0;i<40 && s->farm_action.phase!=HMBTN_FARM_ACTION_FREE;++i)
        assert(hmbtn_farm_action_tick(&s->farm_action,p,&s->house,&s->field,NULL,0,0));
    assert(s->farm_action.phase==HMBTN_FARM_ACTION_FREE && s->farm_action.stamina==99-charge);
}
int main(void)
{
    HmbtnSession *s=save_fixture(0);memset(&s->game.inventory,0,sizeof(s->game.inventory));s->game.inventory.rucksack_level=2;
    memset(&s->poultry,0,sizeof(s->poultry));s->poultry.upgraded=true;s->construction.house_level=s->construction.barn_level=1;s->makers.owned=7;
    static const unsigned ores[]={0,0x7c,0x7b,0x7a,0x60},prices[]={0,1000,2000,3000,5000},xp[]={0,6000,18000,36000,65535};
    const HmbtnStoredTool sentinel={28,3,17};s->game.inventory.toolbox[63]=sentinel;
    assert(hmbtn_tool_upgrade_reason(&s->game.inventory,s->farm_action.experience)==1);
    for(unsigned kind=0;kind<5;++kind) for(unsigned tier=1;tier<5;++tier) {
        s->game.economy.gold=10000;s->farm_action.experience[kind]=xp[tier]-1;
        s->game.inventory.equipped=(HmbtnStoredTool){(uint16_t)(1+kind*5+tier-1),(uint16_t)(tier-1),77};
        s->game.inventory.held_item=(HmbtnStoredTool){(uint16_t)ores[tier],0,1};
        assert(hmbtn_tool_upgrade_price(tier)==prices[tier] && hmbtn_tool_upgrade_experience(tier)==xp[tier]);
        assert(hmbtn_tool_upgrade_reason(&s->game.inventory,s->farm_action.experience)==5);
        assert(!hmbtn_tool_upgrade_order(&s->makers,&s->game.inventory,&s->game.economy,s->farm_action.experience));
        s->farm_action.experience[kind]=xp[tier];
        assert(hmbtn_blacksmith_upgrade_open(&s->feed_shop,&s->game.clock,&s->game.inventory));
        assert(s->feed_shop.request==0x99+tier && s->feed_shop.phase==HMBTN_SHOP_MAKER_CONFIRM);
        input(s,HMBTN_PAD_CIRCLE);until(s,HMBTN_SHOP_OFF);assert(!s->makers.pending && s->game.economy.gold==10000);
        assert(hmbtn_blacksmith_upgrade_open(&s->feed_shop,&s->game.clock,&s->game.inventory));
        input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_MAKER_AGAIN);
        input(s,HMBTN_PAD_DOWN);input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_OFF);assert(!s->makers.pending);
        s->game.economy.gold=prices[tier]-1;
        assert(hmbtn_blacksmith_upgrade_open(&s->feed_shop,&s->game.clock,&s->game.inventory));
        input(s,HMBTN_PAD_CROSS);assert(s->feed_shop.next_request==0x4f);until(s,HMBTN_SHOP_OFF);
        s->game.economy.gold=10000;
        assert(hmbtn_blacksmith_upgrade_open(&s->feed_shop,&s->game.clock,&s->game.inventory));
        input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_MAKER_AGAIN);
        input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_OFF);s->feed_shop.departure=false;
        const unsigned item=1+kind*5+tier;
        assert(s->makers.pending==item && s->makers.mornings_left==3 && !s->game.inventory.equipped.id && !s->game.inventory.held_item.id);
        assert(s->game.economy.gold==10000-prices[tier] && !hmbtn_tool_upgrade_order(&s->makers,&s->game.inventory,&s->game.economy,s->farm_action.experience));
        assert(hmbtn_makers_valid(&s->makers,&s->construction,&s->poultry));s=reload(s);
        for(unsigned day=0;day<3;++day) {
            assert(!hmbtn_makers_deliver(&s->makers));hmbtn_session_advance_minutes(s,1440);
            hmbtn_session_clock_events(s,&s->game.clock,HMBTN_CLOCK_DAY);
            assert(s->makers.mornings_left==2-day);
        }
        assert(hmbtn_makers_deliver(&s->makers) && !hmbtn_makers_deliver(&s->makers) && s->makers.owned==7);
        assert(!memcmp(&s->game.inventory.toolbox[63],&sentinel,sizeof(sentinel)) && s->farm_action.experience[kind]==xp[tier]);
    }
    s->game.inventory.equipped=(HmbtnStoredTool){5,4,1};assert(hmbtn_tool_upgrade_reason(&s->game.inventory,s->farm_action.experience)==2);
    s->game.inventory.equipped=(HmbtnStoredTool){4,3,1};s->game.inventory.held_item=(HmbtnStoredTool){0x7b,0,1};assert(hmbtn_tool_upgrade_reason(&s->game.inventory,s->farm_action.experience)==3);
    s->game.inventory.held_item.id=0x7a;assert(hmbtn_tool_upgrade_reason(&s->game.inventory,s->farm_action.experience)==4);
    s->game.inventory.equipped=(HmbtnStoredTool){1,0,1};s->game.inventory.held_item.id=0x60;s->game.economy.gold=10000;
    assert(hmbtn_tool_upgrade_order(&s->makers,&s->game.inventory,&s->game.economy,s->farm_action.experience) && s->makers.pending==5);
    s->makers.pending=s->makers.mornings_left=0;
    static const unsigned counts[5][5]={{1,3,6,9,25},{1,2,3,4,6},{1,1,1,1,1},{1,1,1,1,1},{1,3,6,9,15}};
    for(unsigned kind=0;kind<5;++kind) for(unsigned tier=0;tier<5;++tier) for(unsigned face=0;face<4;++face) {
        hmbtn_farm_field_init(&s->field,NULL,0);
        const unsigned initial=kind==0 ? 0x40 : kind==4 ? 0x168 : kind==2 ? 0x3e : kind==3 ? 0x3a : 0x167;
        for(unsigned i=0;i<HMBTN_FIELD_CELLS;++i) s->field.cells[i]=(HmbtnFarmTile){(uint16_t)initial,(uint16_t)(kind==4 ? 0x168 : 0x167),0,0,0};
        swing(s,kind,tier,tier,face,100);
        unsigned changed=0;for(unsigned i=0;i<HMBTN_FIELD_CELLS;++i) changed+=s->field.cells[i].value!=initial;
        if(changed!=counts[kind][tier] || s->farm_action.experience[kind]!=150) fprintf(stderr,"kind=%u tier=%u face=%u changed=%u XP=%u\n",kind,tier,face,changed,s->farm_action.experience[kind]);
        assert(changed==counts[kind][tier] && s->farm_action.experience[kind]==150);
        assert(hmbtn_tool_targets(kind,tier)==counts[kind][tier]);
        if(kind==4) assert(s->game.inventory.equipped.count==100-counts[kind][tier]);
        if(kind==2 || kind==3) assert(s->farm_action.strikes==(1u<<tier));
    }
    /* Owning mythril does not imply a full-power swing on a tap. */
    hmbtn_farm_field_init(&s->field,NULL,0);swing(s,1,4,0,3,100);
    unsigned tilled=0;for(unsigned i=0;i<HMBTN_FIELD_CELLS;++i) tilled+=s->field.cells[i].value==0x168;
    assert(tilled==1 && s->game.inventory.equipped.modifier==4);
    hmbtn_farm_field_init(&s->field,NULL,0);s->field.cells[10*45+11].value=0x3a;
    swing(s,1,4,4,3,100);assert(s->field.cells[10*45+12].value==0x167); /* Blocked line stops. */
    hmbtn_farm_field_init(&s->field,NULL,0);
    for(unsigned i=0;i<HMBTN_FIELD_CELLS;++i) s->field.cells[i]=(HmbtnFarmTile){0x168,0x168,0,0,0};
    swing(s,4,4,4,3,2);unsigned watered=0;
    for(unsigned i=0;i<HMBTN_FIELD_CELLS;++i) watered+=s->field.cells[i].value==0x169;
    assert(watered==2 && !s->game.inventory.equipped.count);
    hmbtn_farm_field_init(&s->field,NULL,0);s->field.season=3;swing(s,1,4,4,3,100);
    assert(s->field.cells[10*45+11].value==0x167 && s->farm_action.experience[1]==40);
    hmbtn_farm_field_init_greenhouse(&s->field);s->field.season=3;swing(s,1,4,4,3,100);
    unsigned inside=0;for(unsigned i=0;i<49;++i) inside+=s->field.cells[i].value==0x168;
    assert(inside==6);
    hmbtn_farm_field_init(&s->field,NULL,0);
    for(unsigned i=0;i<HMBTN_FIELD_CELLS;++i) s->field.cells[i]=(HmbtnFarmTile){0x36,0x167,(uint8_t)hmbtn_farm_crop(0x36)->grown,0,0};
    s->game.inventory.supplies[0]=10;swing(s,0,4,4,3,100);assert(s->game.inventory.supplies[0]==35);
    HmbtnMine mine={0};hmbtn_mine_enter(&mine,245);
    for(unsigned i=0;i<100;++i) mine.active[i]=true;
    hmbtn_farm_action_init(&s->farm_action);s->game.inventory.equipped=(HmbtnStoredTool){10,4,1};
    HmbtnIntroActor *p=&s->intro.player;p->position[0]=-200;p->position[2]=200;p->facing=3;
    assert(hmbtn_farm_action_mine_tick(&s->farm_action,p,&s->house,NULL,NULL,HMBTN_PAD_SQUARE,HMBTN_PAD_SQUARE,&mine,s->village.flags));
    for(unsigned i=0;i<80;++i) assert(hmbtn_farm_action_mine_tick(&s->farm_action,p,&s->house,NULL,NULL,0,HMBTN_PAD_SQUARE,&mine,s->village.flags));
    assert(hmbtn_farm_action_mine_tick(&s->farm_action,p,&s->house,NULL,NULL,0,0,&mine,s->village.flags));p->animation_done=true;
    assert(hmbtn_farm_action_mine_tick(&s->farm_action,p,&s->house,NULL,NULL,0,0,&mine,s->village.flags));
    unsigned dug=0;for(unsigned i=0;i<100;++i) dug+=mine.cells[0][i]!=0;
    assert(dug==1 && s->farm_action.stamina==95);
    hmbtn_farm_action_init(&s->farm_action);
    assert(hmbtn_farm_action_tick(&s->farm_action,p,&s->house,&s->field,NULL,HMBTN_PAD_SQUARE,HMBTN_PAD_SQUARE));
    for(unsigned i=0;i<141;++i) assert(hmbtn_farm_action_tick(&s->farm_action,p,&s->house,&s->field,NULL,0,HMBTN_PAD_SQUARE));
    assert(s->farm_action.phase==HMBTN_FARM_OVERCHARGE && !s->farm_action.experience[1] && s->farm_action.stamina==100);
    hmbtn_farm_action_init(&s->farm_action);s->game.inventory.equipped=(HmbtnStoredTool){5,4,1};
    assert(hmbtn_farm_action_tick(&s->farm_action,p,&s->house,NULL,NULL,HMBTN_PAD_SQUARE,HMBTN_PAD_SQUARE));
    for(unsigned i=0;i<80;++i) assert(hmbtn_farm_action_tick(&s->farm_action,p,&s->house,NULL,NULL,0,HMBTN_PAD_SQUARE));
    assert(hmbtn_farm_action_tick(&s->farm_action,p,&s->house,NULL,NULL,0,0));p->animation_done=true;
    assert(hmbtn_farm_action_tick(&s->farm_action,p,&s->house,NULL,NULL,0,0));
    assert(s->farm_action.phase==HMBTN_FARM_ACTION_FREE && s->farm_action.experience[0]==40 && s->farm_action.stamina==95);
    hmbtn_session_destroy(s);puts("Tool orders, guards, skips, deadlines/resume and all tier/kind/facing effects passed");return 0;
}
