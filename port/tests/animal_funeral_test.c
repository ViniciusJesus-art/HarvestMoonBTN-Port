/* farm_kind_selected, text_list_* and farm_clear_flagged_items vectors. */
#include "save_fixture.h"
#include "psx_data.h"
#include <stdio.h>
#include <stdlib.h>
static const uint16_t name[9]={0x10,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff};
static HmbtnSession *roundtrip(HmbtnSession *s)
{
    uint8_t *data=NULL;size_t bytes=0;char error[256]={0};
    assert(hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(data,bytes,error,sizeof(error));assert(loaded);free(data);
    static const uint8_t schedules[1]={0};assert(hmbtn_session_bind_schedules(loaded,schedules,sizeof(schedules)));
    hmbtn_session_destroy(s);return loaded;
}
int main(void)
{
    HmbtnSession *s=save_fixture(80);HmbtnLivestock *p=&s->livestock;
    memset(&s->poultry,0,sizeof(s->poultry));s->poultry.upgraded=true;
    s->construction.house_level=1;s->construction.barn_level=1;
    assert(!hmbtn_livestock_dead(NULL,false,false) && !hmbtn_livestock_clear_dead(NULL));
    for(unsigned i=0;i<6;++i) {
        assert(hmbtn_livestock_buy(p,i&1 ? 0x165 : 0x161,name,&s->game.clock));
        p->animals[i].growth=28;p->animals[i].name[0]=(uint16_t)(0x20+i);
    }
    assert(hmbtn_livestock_breed(p,4,false) && hmbtn_livestock_breed(p,5,true));
    p->animals[4].pregnancy=0;p->births[0].ready=true;
    for(unsigned i=0;i<4;++i) { p->animals[i].dead=true;p->animals[i].died_of_age=i<2;p->animals[i].actor.visible=false; }
    p->animals[5].dead=true;p->animals[5].actor.visible=false;
    for(unsigned i=0;i<2;++i) {
        assert(hmbtn_poultry_buy(&s->poultry,name,&s->game.clock));
        s->poultry.hens[i].dead=true;s->poultry.hens[i].died_of_age=i==0;s->poultry.hens[i].name[0]=(uint16_t)(0x30+i);
    }
    for(unsigned sheep=0;sheep<2;++sheep) for(unsigned age=0;age<2;++age) assert(hmbtn_livestock_dead(p,sheep,age));
    const unsigned expected[6][2]={{0x20,0},{0x21,0},{0x30,0},{0x22,0},{0x23,0x25},{0x31,0}};
    uint8_t text[8192];HmbtnMessage m;char error[256]={0};
    for(unsigned selector=0;selector<6;++selector) {
        memset(text,0,sizeof(text));hmbtn_write_le16(text+0x1800,(uint16_t)(0x401b+selector));hmbtn_write_le16(text+0x1802,0xffff);
        assert(hmbtn_animal_death_expand(&s->poultry,p,text,sizeof(text)) && hmbtn_message_open(&m,text,sizeof(text),error,sizeof(error)));
        unsigned found=0;
        for(unsigned i=0;i<m.count;++i) if(m.words[i]>=0x20 && m.words[i]<=0x31) {
            assert(found<2 && m.words[i]==expected[selector][found]);++found;
        }
        assert(found==(selector==4 ? 2u : 1u));
    }
    assert(!hmbtn_animal_death_expand(&s->poultry,p,text,32));
    s=roundtrip(s);p=&s->livestock;
    assert(p->animals[5].dead && p->births[1].active && p->animals[7].reserved);
    const HmbtnFarmAnimal survivor=p->animals[4],reservation=p->animals[6];
    const HmbtnLivestockBirth pending=p->births[0];const HmbtnFarmFeed feed=s->feed;
    assert(hmbtn_livestock_clear_dead(p)==5 && hmbtn_livestock_clear_dead(p)==0);
    assert(!p->animals[7].reserved && !p->births[1].active && !p->animals[5].occupied);
    assert(!memcmp(&survivor,&p->animals[4],sizeof(survivor)) && !memcmp(&reservation,&p->animals[6],sizeof(reservation))
        && !memcmp(&pending,&p->births[0],sizeof(pending)) && !memcmp(&feed,&s->feed,sizeof(feed)));
    assert(hmbtn_livestock_valid_births(p) && hmbtn_poultry_dead(&s->poultry,true) && hmbtn_poultry_dead(&s->poultry,false));
    assert(hmbtn_poultry_clear_dead(&s->poultry)==2);s=roundtrip(s);p=&s->livestock;
    assert(p->births[0].ready && p->animals[6].reserved && !p->animals[6].occupied);
    /* An already born child survives loss of its mother. Only the mother counts. */
    hmbtn_livestock_birth_begin(p,&s->game.clock);memcpy(p->animals[6].name,name,sizeof(name));p->animals[4].dead=true;
    assert(hmbtn_livestock_clear_dead(p)==1 && p->animals[6].occupied && !p->animals[6].reserved
        && hmbtn_livestock_kind(&p->animals[6])==0x160 && hmbtn_livestock_valid_births(p));
    s=roundtrip(s);p=&s->livestock;assert(hmbtn_livestock_free(p)==0);
    assert(hmbtn_livestock_buy(p,0x165,name,&s->game.clock));
    assert(hmbtn_livestock_count(p,true)==1 && hmbtn_livestock_count(p,false)==1);
    hmbtn_session_destroy(s);
    /* Full herd list: all eight glyphs of twenty names, pages accepted by the
     * shared renderer, with no truncation or insertion codes left behind. */
    HmbtnLivestock herd={0};for(unsigned i=0;i<20;++i) {
        herd.animals[i].occupied=herd.animals[i].dead=herd.animals[i].died_of_age=true;
        for(unsigned j=0;j<8;++j) herd.animals[i].name[j]=(uint16_t)i;
        herd.animals[i].name[8]=0xffff;
    }
    memset(text,0,sizeof(text));hmbtn_write_le16(text+0x1800,0x401b);hmbtn_write_le16(text+0x1802,0xffff);
    assert(hmbtn_animal_death_expand(NULL,&herd,text,sizeof(text)) && hmbtn_message_open(&m,text,sizeof(text),error,sizeof(error)));
    unsigned glyphs=0,lines=0,pages=0;
    for(unsigned i=0;i<m.count;++i) { glyphs+=m.words[i]<20;lines+=m.words[i]==0x51;pages+=m.words[i]==0xffc; }
    assert(glyphs==160 && lines==20 && pages>=4);
    /* Original overnight causes reach a pending funeral before any removal. */
    HmbtnLivestock deaths={0};HmbtnFarmFeed food={0};const HmbtnFarmClock date={.day=4};
    for(unsigned i=0;i<3;++i) {
        assert(hmbtn_livestock_buy(&deaths,i==1 ? 0x165 : 0x161,name,&date));
        deaths.animals[i].growth=28;food.barn[i]=true;
    }
    deaths.animals[0].age=10;deaths.animals[1].age=5;deaths.animals[2].days_sick=31;
    hmbtn_livestock_new_day(&deaths,&food,&date);
    assert(hmbtn_livestock_dead(&deaths,false,true) && hmbtn_livestock_dead(&deaths,true,true)
        && hmbtn_livestock_dead(&deaths,false,false) && !hmbtn_livestock_dead(&deaths,true,false));
    assert(deaths.animals[0].occupied && deaths.animals[2].occupied && !deaths.animals[2].actor.visible);
    puts("Animal funerals: species/causes, complete paged names, source overnight deaths, exact removal/reservations, born-child survival and state roundtrip passed.");return 0;
}
