#include "save_fixture.h"
#include "kappa.h"
#include "item_reaction.h"
#include "psx_data.h"
#include <stdio.h>
#include <stdlib.h>
int main(void)
{
    char error[256]={0};HmbtnSession *s=save_fixture(1);
    const unsigned hours[]={0,5,6,21,22,23};
    for(unsigned half=0;half<2;++half) for(unsigned weather=0;weather<5;++weather)
        for(unsigned t=0;t<6;++t) for(unsigned empty=0;empty<2;++empty) for(unsigned extra=0;extra<4;++extra) {
            s->game.clock.hour=hours[t];s->game.weather.today=weather;s->farm_action.stamina=empty ? 0 : 20;s->farm_action.half_fatigue=half!=0;
            const unsigned original=extra+(weather!=0)+(hours[t]<6 ? 2u : hours[t]>=22 ? 1u : 0u)+(empty ? 2u : 0u);
            assert(hmbtn_farm_fatigue_cost(&s->farm_action,&s->house,extra)==(half ? original/2 : original));
        }
    /* The receipt completes once, restores the prior hand and does not grant energy capacity. */
    s->farm_action.half_fatigue=false;s->farm_action.stamina=30;s->farm_action.max_stamina=100;s->farm_action.fatigue=20;
    const HmbtnStoredTool hand=s->game.inventory.held_item;
    assert(hmbtn_event_item_begin(&s->event_item,&s->intro.player,&s->game.inventory,294,true));
    assert(!s->farm_action.half_fatigue && !hmbtn_save_available(s,error,sizeof(error)));
    s->intro.player.animation_done=true;hmbtn_event_item_tick(&s->event_item,&s->intro.player,&s->game.inventory,&s->farm_action);
    assert(s->farm_action.half_fatigue && s->farm_action.stamina==30 && s->farm_action.max_stamina==100 && s->farm_action.fatigue==19);
    assert(!memcmp(&hand,&s->game.inventory.held_item,sizeof(hand)));
    hmbtn_event_item_tick(&s->event_item,&s->intro.player,&s->game.inventory,&s->farm_action);assert(s->farm_action.fatigue==19);
    /* Actual fishing/medicine callers use the shared fatigue rule, too. */
    s->game.clock.hour=23;s->game.weather.today=1;s->farm_action.stamina=s->farm_action.fatigue=0;
    HmbtnFishing fish={.phase=HMBTN_FISHING_CAST};s->intro.player.animation_done=true;
    assert(hmbtn_fishing_tick(&fish,&s->intro.player,&s->house,&s->farm_action,NULL,&s->game.clock,13,s->village.flags,0,0));
    assert(s->farm_action.fatigue==2 && !s->farm_action.stamina);
    s->farm_action.phase=HMBTN_FARM_MEDICINE;s->farm_action.kind=30;s->farm_action.target=0;s->farm_action.fatigue=0;
    s->game.inventory.equipped=(HmbtnStoredTool){30,0,1};s->intro.player.animation_done=true;
    assert(hmbtn_poultry_medicine_tick(&s->poultry,112,&s->farm_action,&s->intro.player,&s->house,0));
    assert(s->farm_action.fatigue==2 && !s->game.inventory.equipped.id);
    /* Trigger metadata, not a parallel cucumber whitelist or quest counter. */
    uint8_t table[38];memset(table,255,sizeof(table));hmbtn_write_le16(table,(63u<<10)|197);table[12]=0xf7;
    hmbtn_write_le32(table+8,0x3ffffd72);hmbtn_write_le32(table+24,0x13ff);hmbtn_write_le32(table+28,0x7ffcb800);table[34]=4;table[35]=39;
    HmbtnFarmTriggers triggers;assert(hmbtn_farm_triggers_open(&triggers,table,sizeof(table),error,sizeof(error)));
    assert(hmbtn_item_villager_reaction(67,39)==0 && hmbtn_item_villager_reaction(65,39)!=0);
    s->game.clock.hour=11;
    for(unsigned w=0;w<5;++w) assert(hmbtn_farm_event_trigger(&triggers,&s->village,&s->game.clock,w,0,4,67,&s->game.inventory)==197);
    s->game.clock.hour=10;assert(!hmbtn_farm_event_trigger(&triggers,&s->village,&s->game.clock,0,0,4,67,&s->game.inventory));
    s->game.clock.hour=11;assert(!hmbtn_farm_event_trigger(&triggers,&s->village,&s->game.clock,0,0,4,65,&s->game.inventory));
    s->visit.event=197;HmbtnEventCommand command={.type=75,.operand=39,.x=-1};
    s->visit.gift=65;assert(hmbtn_kappa_command(s,&command,error,sizeof(error))==HMBTN_EVENT_WAIT && !s->visit.gift);
    s->visit.gift=67;assert(hmbtn_kappa_command(s,&command,error,sizeof(error))==HMBTN_EVENT_DONE && !s->visit.gift);
    assert(hmbtn_kappa_command(s,&command,error,sizeof(error))==HMBTN_EVENT_WAIT);
    s->village.flags[0xcc]=true;assert(!hmbtn_farm_event_trigger(&triggers,&s->village,&s->game.clock,0,0,4,67,&s->game.inventory));
    s->visit=(HmbtnFarmVisit){0};s->game.clock.hour=12;s->game.weather.today=0;
    uint8_t *data=NULL;size_t size=0;assert(hmbtn_save_encode(s,&data,&size,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(data,size,error,sizeof(error));assert(loaded && loaded->village.flags[0xcc] && loaded->farm_action.half_fatigue);
    assert(hmbtn_session_morning(loaded,true) && loaded->farm_action.half_fatigue);
    free(data);hmbtn_session_destroy(loaded);hmbtn_session_destroy(s);
    puts("Kappa: original gift/weather/hour/flag conditions, consumed action, receipt, fatigue rounding/tools/fishing/medicine and persistent reward passed");return 0;
}
