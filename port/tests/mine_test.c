#include "save_fixture.h"
#include "psx_random.h"
#include "world_map.h"
#include <stdio.h>
#include <stdlib.h>
static uint32_t rolls(unsigned first,unsigned second)
{
    for(uint32_t seed=0;seed<1000000;++seed) {
        uint32_t next=seed;
        if(hmbtn_psx_random_range(&next,100)==first && hmbtn_psx_random_range(&next,100)==second) return seed;
    }
    assert(!"missing deterministic PS1 roll pair");return 0;
}
static void aim(HmbtnIntroActor *p,unsigned cell)
{
    p->facing=3;p->position[0]=(int32_t)(cell%10)*400-2200;p->position[1]=0;p->position[2]=1800-(int32_t)(cell/10)*400;
}
int main(void)
{
    HmbtnSession *s=save_fixture(0);HmbtnMine *m=&s->mine;bool changed;unsigned logical,resource;
    for(unsigned area=234;area<=254;++area) for(unsigned season=0;season<4;++season) {
        assert(hmbtn_world_area(area,season,&logical,&resource) && logical==area && resource==area);
    }
    hmbtn_mine_enter(m,234);memset(m->active,1,sizeof(m->active));s->area=s->intro.area=s->carry.area=234;
    unsigned cell;assert(!hmbtn_mine_cell(m,(int32_t[]){-2001,0,1800},&cell));
    assert(!hmbtn_mine_cell(m,(int32_t[]){0,0,-2000},&cell));
    assert(hmbtn_mine_cell(m,(int32_t[]){-2000,0,2000},&cell) && cell==0);
    aim(&s->intro.player,0);uint32_t before=m->random;
    assert(!hmbtn_mine_dig(m,&s->intro.player,s->village.flags,&changed) && changed && m->random==before);
    assert(!hmbtn_mine_dig(m,&s->intro.player,s->village.flags,&changed) && !changed);
    aim(&s->intro.player,44);m->random=rolls(0,0);
    assert(!hmbtn_mine_dig(m,&s->intro.player,s->village.flags,&changed) && changed && m->cells[0][44]==2 && m->exit_cell[0]==44);
    s->intro.player.position[0]+=400;assert(hmbtn_mine_check(m,&s->intro.player)==1);
    char error[256]={0};assert(hmbtn_mine_prompt(m,1,error,sizeof(error)) && m->message.words[0]==0x13);
    assert(!hmbtn_save_available(s,error,sizeof(error)));int32_t entry[3];assert(hmbtn_mine_transition(m,entry)==235);
    assert(m->layer==1 && entry[0]==-200 && entry[2]==-400);
    s->area=s->intro.area=s->carry.area=235;
    aim(&s->intro.player,44);before=m->random;
    assert(!hmbtn_mine_dig(m,&s->intro.player,s->village.flags,&changed) && !changed && before==m->random);
    aim(&s->intro.player,55);assert(!hmbtn_mine_dig(m,&s->intro.player,s->village.flags,&changed) && changed && before==m->random);
    /* BIN's place 0x37 uses the rare-mineral table; the other mine uses the
     * base table. Test exact item thresholds, not a statistical approximation. */
    m->layer=4;s->area=s->intro.area=s->carry.area=238;m->exit_cell[3]=11;m->cells[3][11]=2;
    const unsigned values[]={0,0x138,0x66,0x60,0x61,0x62,0x67};
    const unsigned chances[]={0,25,40,60,75,85,95};
    for(unsigned i=0;i<7;++i) {
        aim(&s->intro.player,88);m->cells[4][88]=0;m->exit_chance=10;m->random=rolls(99,chances[i]);
        assert(hmbtn_mine_dig(m,&s->intro.player,s->village.flags,&changed)==values[i] && changed && m->cells[4][88]==1);
        assert(m->exit_chance==(values[i] ? 12u : 11u));
    }
    assert(s->village.flags[0x62]);m->cells[4][88]=0;m->random=rolls(99,95);
    assert(!hmbtn_mine_dig(m,&s->intro.player,s->village.flags,&changed));
    m->layer=8;s->area=s->intro.area=s->carry.area=242;m->random=rolls(99,50);
    assert(hmbtn_mine_dig(m,&s->intro.player,s->village.flags,&changed)==0x63 && s->village.flags[0x60]);
    m->cells[8][88]=0;m->random=rolls(99,50);assert(!hmbtn_mine_dig(m,&s->intro.player,s->village.flags,&changed));
    m->layer=9;s->area=s->intro.area=s->carry.area=243;m->exit_chance=200;m->random=rolls(0,99);
    assert(!hmbtn_mine_dig(m,&s->intro.player,s->village.flags,&changed) && m->exit_cell[9]==100);
    /* Synthetic intermediate layers need no stair unless one was really dug. */
    uint8_t *data=NULL;size_t bytes=0;assert(hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(data,bytes,error,sizeof(error));assert(loaded);free(data);
    assert(loaded->mine.layer==9 && loaded->mine.random==m->random && !memcmp(loaded->mine.cells,m->cells,sizeof(m->cells)));
    hmbtn_session_destroy(loaded);
    hmbtn_mine_enter(m,244);assert(m->kind==1 && m->layer==9);
    hmbtn_mine_enter(m,245);memset(m->active,1,sizeof(m->active));
    aim(&s->intro.player,44);m->random=rolls(99,90);
    assert(hmbtn_mine_dig(m,&s->intro.player,s->village.flags,&changed)==0x7c);
    /* A regular tool swing commits once and uses the existing status/XP. */
    s->game.inventory.held_item=(HmbtnStoredTool){0};s->game.inventory.equipped=(HmbtnStoredTool){6,0,1};
    s->farm_action.phase=HMBTN_FARM_TOOL_SWING;s->farm_action.kind=1;s->farm_action.charge_frames=1;
    s->intro.player.animation_done=true;m->cells[0][44]=0;m->random=rolls(99,90);
    const unsigned stamina=s->farm_action.stamina;
    assert(hmbtn_farm_action_mine_tick(&s->farm_action,&s->intro.player,&s->house,NULL,NULL,0,0,m,s->village.flags));
    assert(m->found==0x7c && s->farm_action.stamina==stamina-1 && s->farm_action.phase==HMBTN_FARM_ACTION_FREE);
    assert(hmbtn_event_item_show(&s->event_item,&s->intro.player,s->house.inventory,m->found));
    s->intro.player.animation_done=true;hmbtn_event_item_tick(&s->event_item,&s->intro.player,s->house.inventory,&s->farm_action);
    assert(s->game.inventory.held_item.id==0x7c);
    hmbtn_mine_enter(m,68);assert(!m->kind && !m->layer && m->exit_cell[0]==100);
    hmbtn_session_destroy(s);
    puts("Mine: original resource IDs, edges/return radius, exact PS1 roll thresholds, unique finds, final-layer guard, shared tool/reward/status, state roundtrip and reset passed.");return 0;
}
