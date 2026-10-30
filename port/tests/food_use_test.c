/* Original flags/callback arguments, direct-use removal and event restoration. */
#include "save_fixture.h"
#include "inventory.h"
#include <stdio.h>
#include <stdlib.h>
static void finish(HmbtnSession *s)
{
    s->intro.player.animation_done=true;
    hmbtn_event_item_tick(&s->event_item,&s->intro.player,&s->game.inventory,&s->farm_action);
    assert(!s->event_item.phase && !s->event_item.shown && !s->event_item.clock && !s->intro.player.pose);
}
int main(void)
{
    HmbtnSession *s=save_fixture(1);HmbtnInventory *h=&s->game.inventory;HmbtnFarmAction *a=&s->farm_action;
    s->game.clock.hour=12;s->game.clock.minute=0;s->daily_date=hmbtn_session_date(&s->game.clock);
    const unsigned refused[]={0,34,100,165,166,167,164,0x67,380};
    for(unsigned i=0;i<sizeof(refused)/sizeof(*refused);++i) {
        h->held_item=(HmbtnStoredTool){(uint16_t)refused[i],154,1};const HmbtnInventory before=*h;const HmbtnFarmAction status=*a;
        assert(!hmbtn_event_item_use(&s->event_item,&s->intro.player,h,&s->game.clock));
        assert(!memcmp(h,&before,sizeof(before)) && !memcmp(a,&status,sizeof(status)));
    }
    const struct { unsigned id,quality;int stamina,fatigue,happiness;bool drink; } cases[]={
        {153,0,2,-1,0,false},{154,0,2,-1,0,false},{88,0,10,-1,0,false},
        {178,30,30,-3,0,false},{178,128,-128,-3,0,false},{178,255,-1,-3,0,false},
        {178,256,0,-3,0,false},{180,20,20,-4,0,true},{159,0,0,-5,1,true},
        {226,1,1,0,2,false},{226,246,-10,0,2,false},{91,0,-10,5,0,false}
    };
    for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);++i) for(unsigned facing=0;facing<4;++facing) {
        a->stamina=50;a->max_stamina=100;a->fatigue=20;a->happiness=10;
        h->held_item=(HmbtnStoredTool){(uint16_t)cases[i].id,(uint16_t)cases[i].quality,5};
        s->intro.player.facing=facing;s->intro.player.animation_done=false;const HmbtnInventory before=*h;
        assert(hmbtn_event_item_use(&s->event_item,&s->intro.player,h,&s->game.clock));
        assert(hmbtn_event_item_consuming(&s->event_item) && s->event_item.sound==(cases[i].drink ? 75 : 74));
        assert(s->intro.player.sprite.animation==(cases[i].drink ? 0 : 5) && !memcmp(h,&before,sizeof(before)));
        assert(!hmbtn_event_item_use(&s->event_item,&s->intro.player,h,&s->game.clock));
        for(unsigned n=0;n<3;++n) { hmbtn_event_item_tick(&s->event_item,&s->intro.player,h,a);assert(a->stamina==50 && a->fatigue==20 && a->happiness==10 && h->held_item.count==5); }
        finish(s);const unsigned expected=cases[i].stamina<-50 ? 0 : (unsigned)(50+cases[i].stamina);
        assert(a->stamina==expected && a->fatigue==(unsigned)(20+cases[i].fatigue) && a->happiness==(unsigned)(10+cases[i].happiness) && !h->held_item.id);
        const HmbtnFarmAction after=*a;finish(s);assert(!memcmp(a,&after,sizeof(after)) && !memcmp(h->items,before.items,sizeof(h->items)));
    }
    /* Ordinary bounds and the original 06:00 fatigue exception. */
    a->stamina=99;a->fatigue=0;h->held_item=(HmbtnStoredTool){154,0,1};
    assert(hmbtn_event_item_use(&s->event_item,&s->intro.player,h,&s->game.clock));finish(s);assert(a->stamina==100 && !a->fatigue);
    a->stamina=3;a->fatigue=99;s->game.clock.hour=6;h->held_item=(HmbtnStoredTool){91,0,1};
    assert(hmbtn_event_item_use(&s->event_item,&s->intro.player,h,&s->game.clock));finish(s);assert(!a->stamina && a->fatigue==99);
    s->game.clock.minute=1;h->held_item=(HmbtnStoredTool){91,0,1};
    assert(hmbtn_event_item_use(&s->event_item,&s->intro.player,h,&s->game.clock));finish(s);assert(a->fatigue==100);
    a->happiness=65534;h->held_item=(HmbtnStoredTool){226,1,1};
    assert(hmbtn_event_item_use(&s->event_item,&s->intro.player,h,&s->game.clock));finish(s);assert(a->happiness==65535);
    /* A changed hand must not grant health or destroy its replacement. */
    h->held_item=(HmbtnStoredTool){153,0,1};const unsigned stamina=a->stamina;
    assert(hmbtn_event_item_use(&s->event_item,&s->intro.player,h,&s->game.clock));h->held_item=(HmbtnStoredTool){164,154,1};finish(s);
    assert(h->held_item.id==164 && h->held_item.modifier==154 && a->stamina==stamina);
    /* Existing scripted cake and berry use restores the item set aside. */
    const HmbtnStoredTool gift=h->held_item;assert(hmbtn_event_item_begin(&s->event_item,&s->intro.player,h,225,true));finish(s);
    assert(!memcmp(&h->held_item,&gift,sizeof(gift)));
    s->daily_date=hmbtn_session_date(&s->game.clock);char error[256];uint8_t *data=NULL;size_t bytes=0;
    assert(hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(data,bytes,error,sizeof(error));assert(loaded && loaded->farm_action.happiness==65535 && loaded->farm_action.stamina==a->stamina && loaded->farm_action.fatigue==a->fatigue);
    hmbtn_session_destroy(loaded);free(data);hmbtn_session_destroy(s);
    puts("Food use: flags, direct food/drink ANI/sound, signed quality, deferred unique record/effects, bounds, stale hand, events and status resume passed");return 0;
}
