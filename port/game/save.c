/* PF-05. Persistence selection follows saveload.c: save_pack/unpack_farm_state
 * and clock_events.c: farm_overnight_finish. Native field encoding replaces
 * memory-card images, copied addresses, icon blocks and PS1 checksums. */
#include "save.h"
#include "world_map.h"
#include "economy.h"
#include "inventory.h"
#include "cooking.h"
#include "tv_shopping.h"
#include "psx_data.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

enum { HEADER=24,CHUNKS=7,CATALOG_BTN=1 };
static const uint8_t magic[8]={'H','M','B','T','N','S','V',0};
typedef struct Codec { uint8_t *out;const uint8_t *in;size_t pos,end;bool read,ok; } Codec;
static uint32_t read32(Codec *c)
{
    if(!c->ok || c->pos>c->end || c->end-c->pos<4) { c->ok=false;return 0; }
    const uint32_t v=hmbtn_read_le32(c->in+c->pos);c->pos+=4;return v;
}
static void write32(Codec *c,uint32_t v)
{
    if(!c->ok || c->pos>c->end || c->end-c->pos<4) { c->ok=false;return; }
    if(c->out) hmbtn_write_le32(c->out+c->pos,v);
    c->pos+=4;
}
static int32_t signed32(uint32_t v) { return v<=INT32_MAX ? (int32_t)v : -1-(int32_t)(UINT32_MAX-v); }
#define U(x) do { if(c->read) d->x=read32(c);else write32(c,s->x); } while(0)
#define S(x) do { if(c->read) d->x=signed32(read32(c));else write32(c,(uint32_t)s->x); } while(0)
#define B(x) do { if(c->read) { uint32_t v=read32(c);if(v>1)c->ok=false;d->x=v!=0; } else write32(c,s->x ? 1u : 0u); } while(0)
#define W(x) do { if(c->read) { uint32_t v=read32(c);if(v>UINT16_MAX)c->ok=false;d->x=(uint16_t)v; } else write32(c,s->x); } while(0)
#define Y(x) do { if(c->read) { uint32_t v=read32(c);if(v>UINT8_MAX)c->ok=false;d->x=(uint8_t)v; } else write32(c,s->x); } while(0)
#define COUNT(n) do { if(c->read) {if(read32(c)!=(n))c->ok=false;}else write32(c,(n)); } while(0)
#define CHILD(fn,x) fn(c,c->read ? &d->x : NULL,c->read ? NULL : &s->x)
static void actor(Codec *c,HmbtnIntroActor *d,const HmbtnIntroActor *s)
{
    for(unsigned i=0;i<3;++i) { S(position[i]);S(velocity[i]); }
    W(sprite.animation);W(sprite.frame);W(sprite.ticks);U(facing);
    B(visible);B(pose);B(animation_done);U(marker);
}
static void tool(Codec *c,HmbtnStoredTool *d,const HmbtnStoredTool *s) { W(id);W(modifier);W(count); }
static void game(Codec *c,HmbtnSession *d,const HmbtnSession *s)
{
    COUNT(CATALOG_BTN);CHILD(tool,game.inventory.equipped);CHILD(tool,game.inventory.held_item);
    COUNT(8);for(unsigned i=0;i<8;++i) { CHILD(tool,game.inventory.bag[i]);CHILD(tool,game.inventory.items[i]); }
    COUNT(64);for(unsigned i=0;i<64;++i) {
        CHILD(tool,game.inventory.toolbox[i]);CHILD(tool,game.inventory.icebox[i]);CHILD(tool,game.inventory.cabinet[i]);
    }
    U(game.cooking.utensils);COUNT(HMBTN_RECIPE_RECORDS);
    for(unsigned i=0;i<HMBTN_RECIPE_RECORDS;++i) {
        W(game.cooking.records[i].id);W(game.cooking.records[i].quality);W(game.cooking.records[i].options);
        for(unsigned j=0;j<8;++j) W(game.cooking.records[i].values[j]);
    }
    W(game.shopping.date);for(unsigned i=0;i<9;++i) Y(game.shopping.offers[i]);
    U(game.inventory.rucksack_level);for(unsigned i=0;i<4;++i) U(game.inventory.supplies[i]);
    U(game.economy.gold);U(game.economy.gold_limit);
    COUNT(25);for(unsigned i=0;i<25;++i) U(game.economy.shipments[i]);
    for(unsigned i=0;i<4;++i) for(unsigned j=0;j<2;++j) U(game.economy.ledger[i][j]);
    U(game.economy.shipping.pending_value);U(game.economy.shipping.fish_shipped);
    B(game.economy.shipping.settled);B(game.economy.shipping.item_shipped);B(game.economy.shipping.honey_bonus);
    U(game.clock.year);U(game.clock.season);U(game.clock.day);U(game.clock.weekday);
    U(game.clock.hour);U(game.clock.minute);U(game.clock.frames);
    COUNT(HMBTN_NAME_CHARS+1);for(unsigned i=0;i<=HMBTN_NAME_CHARS;++i) {
        W(game.registration.player_name[i]);W(game.registration.farm_name[i]);W(game.registration.dog_name[i]);
    }
    U(game.registration.birthday_season);U(game.registration.birthday_day);
}
static void field_grid(Codec *c,HmbtnFarmField *d,const HmbtnFarmField *s)
{
    const HmbtnFarmField *grid=c->read ? d : s;
    const unsigned count=hmbtn_farm_field_cells(grid);
    COUNT(hmbtn_farm_field_columns(grid));COUNT(hmbtn_farm_field_rows(grid));COUNT(count);
    U(season);U(plant_random);
    for(unsigned i=0;i<count;++i) {
        W(cells[i].value);W(cells[i].ground);Y(cells[i].kind);
        W(cells[i].growth_flags);U(cells[i].watered_days);
    }
}
static void field(Codec *c,HmbtnSession *d,const HmbtnSession *s)
{
    CHILD(field_grid,field);CHILD(field_grid,greenhouse);
    U(mine.kind);U(mine.layer);U(mine.exit_chance);U(mine.random);
    for(unsigned layer=0;layer<HMBTN_MINE_LAYERS;++layer) {
        U(mine.exit_cell[layer]);
        for(unsigned i=0;i<HMBTN_MINE_CELLS;++i) Y(mine.cells[layer][i]);
    }
    COUNT(22);for(unsigned i=0;i<22;++i) B(feed.barn[i]);
    COUNT(10);for(unsigned i=0;i<10;++i) B(feed.coop[i]);
    COUNT(HMBTN_GATHER_RECORDS);for(unsigned i=0;i<HMBTN_GATHER_RECORDS;++i) B(gather.taken[i]);
}
static void village(Codec *c,HmbtnSession *d,const HmbtnSession *s)
{
    U(village.random);U(village.date_key);U(village.minute);
    U(village.ann_birthday_season);U(village.ann_birthday_day);
    B(village.invitation_available);B(village.dog_held);B(village.player_running);
    COUNT(HMBTN_VILLAGE_FLAGS);for(unsigned i=0;i<HMBTN_VILLAGE_FLAGS;++i) B(village.flags[i]);
    COUNT(128);for(unsigned i=0;i<128;++i) { Y(village.days.count0[i]);Y(village.days.values[i]); }
    COUNT(8);for(unsigned i=0;i<8;++i) Y(village.days.count1[i]);
    COUNT(HMBTN_VILLAGERS);for(unsigned i=0;i<HMBTN_VILLAGERS;++i) {
        COUNT(i);CHILD(actor,village.people[i].actor);
        U(village.people[i].friendship);U(village.people[i].affection);U(village.people[i].location);
        U(village.people[i].step);U(village.people[i].row);U(village.people[i].column);
        U(village.people[i].wait);U(village.people[i].delay);W(village.people[i].talk_flags);
        B(village.people[i].placed);B(village.people[i].standing);
        B(village.people[i].cell_entered);B(village.people[i].blocked_by_player);
        COUNT(30);for(unsigned j=0;j<30;++j) for(unsigned k=0;k<4;++k) W(village.people[i].schedule[j][k]);
    }
}
static void hen(Codec *c,HmbtnHen *d,const HmbtnHen *s)
{
    B(occupied);B(fed_yesterday);B(prize);B(dead);B(held);B(fed_today);
    B(naming_pending);B(died_of_age);B(golden_egg);
    if(c->read) d->stage=(HmbtnHenStage)read32(c);else write32(c,(uint32_t)s->stage);
    U(growth);U(incubator);U(affection);U(age);U(season);U(day);U(days_sick);U(timer);U(praise);U(area);U(grazing);
    for(unsigned i=0;i<=HMBTN_NAME_CHARS;++i) W(name[i]);
    CHILD(actor,actor);
}
static void poultry(Codec *c,HmbtnSession *d,const HmbtnSession *s)
{
    U(poultry.random);B(poultry.upgraded);COUNT(HMBTN_HENS);
    for(unsigned i=0;i<HMBTN_HENS;++i) {
        COUNT(i);CHILD(hen,poultry.hens[i]);U(poultry.eggs[i].item);U(poultry.eggs[i].area);
        for(unsigned j=0;j<3;++j) S(poultry.eggs[i].position[j]);
    }
}
static void animal(Codec *c,HmbtnFarmAnimal *d,const HmbtnFarmAnimal *s)
{
    B(occupied);B(sheep);B(dead);B(died_of_age);B(fed_today);B(fed_yesterday);B(talked_today);
    B(brushed_today);B(milked_today);B(prize_winner);B(reserved);U(pregnancy);
    U(growth);U(affection);U(age);U(season);U(day);U(days_sick);U(days_since_shearing);
    U(area);U(timer);U(state);U(praise);U(graze_count);
    for(unsigned i=0;i<3;++i) S(destination[i]);
    for(unsigned i=0;i<=HMBTN_NAME_CHARS;++i) W(name[i]);
    CHILD(actor,actor);
}
static void livestock(Codec *c,HmbtnSession *d,const HmbtnSession *s)
{
    for(unsigned i=0;i<2;++i) {
        B(livestock.births[i].active);B(livestock.births[i].ready);U(livestock.births[i].mother);U(livestock.births[i].child);
    }
    U(livestock.random);COUNT(HMBTN_LIVESTOCK);
    for(unsigned i=0;i<HMBTN_LIVESTOCK;++i) { COUNT(i);CHILD(animal,livestock.animals[i]); }
}
static void player(Codec *c,HmbtnSession *d,const HmbtnSession *s)
{
    U(area);U(previous_place);B(trigger_entry);B(first_visit_started);S(refused_zone);
    CHILD(actor,intro.player);
    for(unsigned i=0;i<3;++i) S(intro.camera[i]);
    S(intro.camera_height);S(intro.camera_distance);
    U(control.mode);U(control.blocked_frames);B(control.carrying);B(control.carrying_big);
    U(farm_action.stamina);U(farm_action.max_stamina);U(farm_action.fatigue);U(farm_action.strikes);U(farm_action.target);
    for(unsigned i=0;i<5;++i) U(farm_action.experience[i]);
    U(carry.discarded);B(carry.small_fish_placed);
    U(fishing.random);U(fishing.streak);U(fishing.catch_flags);B(fishing.berry_eaten);
    U(menu.stats.animals[0]);
    U(menu.stats.fish);U(menu.stats.discarded);U(menu.stats.power_berries);
    U(menu.stats.dog_affection);
    for(unsigned i=0;i<7;++i) U(menu.stats.helper_affection[i]);
    U(quick_id);B(quick_tool);
}
static void auxiliary(Codec *c,HmbtnSession *d,const HmbtnSession *s)
{
    U(game.weather.today);U(game.weather.tomorrow);U(house.steps);U(house.event_count1);
    U(house.options.layout);B(house.options.vibration);B(house.options.stereo);
    B(house.options.clock);B(house.options.rotated);U(house.options.idle_frames);
    if(c->read) { uint64_t lo=read32(c);d->daily_date=lo|((uint64_t)read32(c)<<32); }
    else { write32(c,(uint32_t)s->daily_date);write32(c,(uint32_t)(s->daily_date>>32)); }
    U(runtime.house_music_ticks);U(runtime.ticks);
    for(unsigned i=0;i<3;++i) { S(daylight.rgb[i]);S(daylight.step[i]); }
    U(daylight.period);U(daylight.taken);U(daylight.total);
    U(game.weather.random);B(game.progress.moondrop_seeds);
    U(construction.mornings_left);U(construction.house_level);U(construction.barn_level);U(construction.pending);
    B(construction.greenhouse);U(construction.damage_random);
    U(makers.owned);U(makers.pending);U(makers.mornings_left);
    U(game.inventory.rucksack_mornings_left);
    B(game.basket.owned);B(game.basket.placed);U(game.basket.area);U(game.basket.count);U(game.basket.value);
    for(unsigned i=0;i<3;++i) S(game.basket.position[i]);
    for(unsigned i=0;i<25;++i) U(game.basket.shipments[i]);
    U(house_lumber.deliveries);
}
#undef U
#undef S
#undef B
#undef W
#undef Y
#undef CHILD
#undef COUNT
static const struct { uint32_t id;void (*visit)(Codec *,HmbtnSession *,const HmbtnSession *); } chunks[]={
    {0x454d4147,game},{0x444c4946,field},{0x4c4c4956,village},
    {0x4c554f50,poultry},{0x4b54534c,livestock},{0x59414c50,player},{0x49585541,auxiliary}
};
static uint32_t checksum(const uint8_t *data,size_t bytes)
{
    uint32_t crc=UINT32_MAX;
    for(size_t i=0;i<bytes;++i) {
        crc^=data[i];for(unsigned j=0;j<8;++j) crc=(crc>>1)^(UINT32_C(0xedb88320)&(0u-(crc&1u)));
    }
    return ~crc;
}
bool hmbtn_save_available(const HmbtnSession *s,char *error,size_t size)
{
    const char *why=NULL;
    if(!s || !s->completion_reported || !s->village_initialized || !s->village_ready) why="Opening or resource handoff has not completed";
    else if(s->active_frame) why="Save must run between game ticks";
    else if(s->mine.prompt || s->mine.back_ticks || s->mine.reward || s->mine.found) why="A mine transition or reward is active";
    else if(s->first_visit_active || s->visit.active || s->talk.active || s->animal_event || s->animal_name_active
        || !s->intro.script.finished || s->intro.message || s->intro.area_pending || s->intro.event_pending) why="An event or conversation is active";
    else if(!s->control.enabled || s->control.exit_pending || s->control.jumping || s->travel_tick || s->door_tick
        || s->first_exit_tick || s->sleep.active || s->overnight_tick || s->morning_fade || s->morning_prepared || s->intro.fade_level) why="A transition, sleep or fade is active";
    else if(s->house.mode!=HMBTN_HOUSE_FREE || s->menu.mode!=HMBTN_MENU_NONE || s->feed_shop.phase!=HMBTN_SHOP_OFF || s->feed_shop.departure
        || s->house.message.phase!=HMBTN_MESSAGE_CLOSED || s->dialogue.message.phase!=HMBTN_MESSAGE_CLOSED) why="A menu or message is open";
    else if(s->farm_action.phase!=HMBTN_FARM_ACTION_FREE || s->fishing.phase!=HMBTN_FISHING_FREE
        || s->carry.phase || s->feed.phase || s->poultry.carry_phase || s->house_lumber.phase || s->quick_ticks
        || s->event_item.phase!=HMBTN_EVENT_ITEM_IDLE || s->basket_action.phase) why="An action is active";
    if(!why) for(unsigned i=0;i<3;++i) if(s->carry.thrown[i].item.id) why="A thrown item is in flight";
    if(!why && hmbtn_poultry_held_slot(&s->poultry)<HMBTN_HENS) why="Put down the animal before saving";
    if(!why) for(unsigned i=0;i<HMBTN_VILLAGERS;++i) if(hmbtn_village_event_actor(&s->village,i)) why="An event actor is bound";
    if(!why) for(unsigned i=0;i<HMBTN_SERVICE_CHANNELS;++i) {
        HmbtnServiceStatus status=s->services.channels[i].status;
        if(status==HMBTN_SERVICE_PENDING || status==HMBTN_SERVICE_FAILED || status==HMBTN_SERVICE_CANCELLED) why="A service has not completed successfully";
    }
    if(why) { snprintf(error,size,"Cannot save: %s",why);return false; }
    return true;
}
static bool valid_name(const uint16_t *s)
{
    bool end=false;
    for(unsigned i=0;i<=HMBTN_NAME_CHARS;++i) {
        if(s[i]==0xffff) end=true;
        else if(s[i]>0xfff || end) return false;
    }
    return end;
}
static bool valid_actor(const HmbtnIntroActor *a)
{
    if(a->facing>3 || a->marker>2) return false;
    for(unsigned i=0;i<3;++i) if(a->position[i]<-1000000 || a->position[i]>1000000 || a->velocity[i]<-10000 || a->velocity[i]>10000) return false;
    /* Bounds prevent malformed animation offsets; resource-specific ANI checks
     * occur before the candidate scene replaces the live scene. */
    return a->sprite.animation<1024 && a->sprite.frame<4096 && a->sprite.ticks<32768;
}
static bool valid_tool(const HmbtnStoredTool *t) { return !t->id || hmbtn_farm_item(t->id)!=NULL; }
static bool valid(const HmbtnSession *s,char *error,size_t size)
{
    const HmbtnFarmClock *t=&s->game.clock;unsigned logical,resource;
    bool ok=t->year<=1000000 && t->season<4 && t->day<30 && t->weekday<7 && t->hour<24 && t->minute<60 && t->frames<15
        && hmbtn_world_area(s->area,t->season,&logical,&resource) && logical==s->area
        && s->daily_date==hmbtn_session_date(t) && s->game.inventory.rucksack_level<=2
        && s->game.inventory.rucksack_mornings_left<=10
        && (s->game.inventory.rucksack_level || !s->game.inventory.rucksack_mornings_left)
        && s->game.registration.birthday_season<4 && s->game.registration.birthday_day<30
        && valid_name(s->game.registration.player_name) && valid_name(s->game.registration.farm_name) && valid_name(s->game.registration.dog_name)
        && valid_actor(&s->intro.player) && (s->control.mode==0 || s->control.mode==2 || s->control.mode==4)
        && s->farm_action.max_stamina<=1000000 && s->farm_action.stamina<=s->farm_action.max_stamina && s->farm_action.fatigue<=100
        && !s->field.indoor && s->greenhouse.indoor && s->field.season<4 && s->greenhouse.season<4
        && (s->area!=124 || s->construction.greenhouse)
        && s->village.ann_birthday_season<4 && s->village.ann_birthday_day<30
        && s->game.weather.today<HMBTN_WEATHER_KINDS && s->game.weather.tomorrow<HMBTN_WEATHER_KINDS && s->house.options.layout<3
        && s->runtime.house_music_ticks<=100 && s->house.options.idle_frames<=16
        && s->farm_action.target<=HMBTN_FIELD_CELLS && s->refused_zone>=-1 && s->refused_zone<50
        && s->daylight.period<4 && s->daylight.total<=256 && s->daylight.taken<=s->daylight.total
        && s->game.economy.gold<=hmbtn_economy_gold_limit(&s->game.economy)
        && s->construction.mornings_left<=hmbtn_construction_mornings()
        && s->construction.house_level<=2 && (!s->construction.house_level || s->poultry.upgraded)
        && s->construction.barn_level<=1 && (!s->construction.barn_level || s->construction.house_level>=1)
        && (s->construction.house_level<2 || s->construction.barn_level==1)
        && (!s->construction.greenhouse || s->construction.house_level==2)
        && (s->construction.mornings_left ?
            (s->construction.pending==HMBTN_BUILD_COOP && !s->poultry.upgraded)
            || (s->construction.pending==HMBTN_BUILD_HOUSE && s->poultry.upgraded && !s->construction.house_level)
            || (s->construction.pending==HMBTN_BUILD_BARN && s->construction.house_level==1 && !s->construction.barn_level)
            || (s->construction.pending==HMBTN_BUILD_HOUSE2 && s->construction.house_level==1 && s->construction.barn_level==1)
            || (s->construction.pending==HMBTN_BUILD_GREENHOUSE && s->construction.house_level==2 && !s->construction.greenhouse)
            : s->construction.pending==HMBTN_BUILD_NONE)
        && s->game.inventory.held_item.id!=0x109 && s->game.inventory.held_item.id!=0x10A;
    ok=ok && s->game.cooking.utensils<=255;
    ok=ok && hmbtn_tv_shopping_valid(&s->game.shopping);
    for(unsigned i=0;i<64;++i) {
        const HmbtnStoredTool *ice=&s->game.inventory.icebox[i],*cab=&s->game.inventory.cabinet[i];
        ok=ok && hmbtn_inventory_storage_accepts(2,ice->id) && hmbtn_inventory_storage_accepts(1,cab->id)
            && (!ice->id || (ice->count>0 && ice->count<=99)) && (!cab->id || (cab->count>0 && cab->count<=99));
    }
    for(unsigned i=0;i<HMBTN_RECIPE_RECORDS;++i) {
        const HmbtnRecipeRecord *r=&s->game.cooking.records[i];
        if(!r->id) continue;
        HmbtnRecipeRecord expected;
        ok=ok && hmbtn_cooking_evaluate(r->options,r->values,&expected) && r->id==expected.id && r->quality==expected.quality;
        for(unsigned j=0;j<8;++j) ok=ok && (!r->values[j] || hmbtn_farm_item(r->values[j]));
        for(unsigned j=0;j<i;++j) ok=ok && r->id!=s->game.cooking.records[j].id;
    }
    for(unsigned i=0;i<3;++i) ok=ok && s->intro.camera[i]>=-1000000 && s->intro.camera[i]<=1000000
        && s->daylight.rgb[i]>=0 && s->daylight.rgb[i]<=31*32768 && s->daylight.step[i]>=-32768 && s->daylight.step[i]<=32768;
    ok=ok && s->intro.camera_height>=-100000 && s->intro.camera_height<=100000
        && s->intro.camera_distance>=-100000 && s->intro.camera_distance<=100000;
    ok=ok && valid_tool(&s->game.inventory.equipped) && valid_tool(&s->game.inventory.held_item);
    for(unsigned i=0;i<8;++i) ok=ok && valid_tool(&s->game.inventory.bag[i]) && valid_tool(&s->game.inventory.items[i]);
    for(unsigned i=0;i<64;++i) ok=ok && valid_tool(&s->game.inventory.toolbox[i]);
    for(unsigned i=0;i<HMBTN_VILLAGERS;++i) {
        const HmbtnVillager *p=&s->village.people[i];
        ok=ok && p->step<30 && p->column<256 && p->row<256 && p->friendship<=255 && p->affection<=65535 && valid_actor(&p->actor);
    }
    const HmbtnFarmField *fields[2]={&s->field,&s->greenhouse};
    for(unsigned f=0;f<2;++f) for(unsigned i=0;i<hmbtn_farm_field_cells(fields[f]);++i) {
        const HmbtnFarmTile *p=&fields[f]->cells[i];
        ok=ok && p->kind<16 && p->watered_days<=1000000 && hmbtn_farm_item(p->value)
            && p->ground>=0x167 && p->ground<=0x16b;
        if(f==1 && !s->construction.greenhouse) ok=ok && p->value==0x167 && p->ground==0x167 && !p->kind && !p->watered_days && !p->growth_flags;
    }
    for(unsigned i=0;i<HMBTN_HENS;++i) {
        const HmbtnHen *h=&s->poultry.hens[i];const HmbtnCoopEgg *e=&s->poultry.eggs[i];
        ok=ok && (unsigned)h->stage<=HMBTN_HEN_CHICK && !h->held && h->incubator<3
            && (!h->occupied || (h->season<4 && h->day<30 && h->affection<=255 && (h->area==0 || h->area==112)
                && valid_name(h->name) && valid_actor(&h->actor)))
            && (!e->item || (hmbtn_farm_item(e->item) && (e->area==0 || e->area==112)));
    }
    ok=ok && hmbtn_makers_valid(&s->makers,&s->construction,&s->poultry);
    ok=ok && hmbtn_basket_valid(&s->game.basket,&s->game.inventory);
    ok=ok && hmbtn_mine_valid(&s->mine,s->area);
    ok=ok && hmbtn_livestock_valid_births(&s->livestock);
    for(unsigned i=0;i<HMBTN_LIVESTOCK;++i) {
        const HmbtnFarmAnimal *a=&s->livestock.animals[i];
        ok=ok && a->pregnancy<21 && (!a->pregnancy || hmbtn_livestock_kind(a)==(a->sheep ? 0x165u : 0x162u));
        if(!a->occupied) continue;
        ok=ok && i<hmbtn_livestock_capacity(&s->livestock) && a->growth<=31 && a->affection<=255
            && a->age<=1000000 && a->season<4 && a->day<30 && a->days_sick<=31
            && a->days_since_shearing<=7 && (a->area==96 || a->area==0) && a->graze_count<=65535 && a->timer<=300 && a->state<=5 && a->praise<=45
            && (!a->died_of_age || a->dead) && valid_name(a->name) && valid_actor(&a->actor);
        for(unsigned j=0;j<3;++j) ok=ok && a->destination[j]>=-1000000 && a->destination[j]<=1000000;
    }
    if(!ok) snprintf(error,size,"Save contains invalid or unsupported gameplay values");
    return ok;
}
bool hmbtn_save_encode(const HmbtnSession *s,uint8_t **data,size_t *bytes,char *error,size_t size)
{
    if(!data || !bytes) { snprintf(error,size,"Missing save output");return false; }
    *data=NULL;*bytes=0;
    if(!hmbtn_save_available(s,error,size) || !valid(s,error,size)) return false;
    Codec c={.pos=HEADER,.end=HMBTN_SAVE_MAX_BYTES,.ok=true};
    for(unsigned pass=0;pass<2;++pass) {
        for(unsigned i=0;i<CHUNKS;++i) {
            write32(&c,chunks[i].id);size_t length_at=c.pos;write32(&c,0);size_t start=c.pos;
            chunks[i].visit(&c,NULL,s);
            if(c.out && c.ok) hmbtn_write_le32(c.out+length_at,(uint32_t)(c.pos-start));
        }
        if(!c.ok) { free(c.out);snprintf(error,size,"Save exceeds native format bounds");return false; }
        if(pass==0) {
            *bytes=c.pos;c.out=malloc(*bytes);
            if(!c.out) { *bytes=0;snprintf(error,size,"Cannot allocate save buffer");return false; }
            c.pos=HEADER;c.end=*bytes;
        }
    }
    memcpy(c.out,magic,8);hmbtn_write_le32(c.out+8,HMBTN_SAVE_VERSION);
    hmbtn_write_le32(c.out+12,(uint32_t)(*bytes-HEADER));hmbtn_write_le32(c.out+16,CHUNKS);
    hmbtn_write_le32(c.out+20,checksum(c.out+HEADER,*bytes-HEADER));*data=c.out;return true;
}
HmbtnSession *hmbtn_save_decode(const uint8_t *data,size_t bytes,char *error,size_t size)
{
    if(!data || bytes<HEADER || bytes>HMBTN_SAVE_MAX_BYTES || memcmp(data,magic,8)) { snprintf(error,size,"Invalid native save header");return NULL; }
    if(hmbtn_read_le32(data+8)!=HMBTN_SAVE_VERSION) { snprintf(error,size,"Unsupported native save version %u (expected %u); start a new game",hmbtn_read_le32(data+8),HMBTN_SAVE_VERSION);return NULL; }
    if(hmbtn_read_le32(data+12)!=bytes-HEADER || hmbtn_read_le32(data+16)!=CHUNKS
        || hmbtn_read_le32(data+20)!=checksum(data+HEADER,bytes-HEADER)) { snprintf(error,size,"Save length or checksum mismatch");return NULL; }
    HmbtnSession *s=hmbtn_session_create(NULL,0);
    if(!s) { snprintf(error,size,"Cannot allocate loaded session");return NULL; }
    Codec c={.in=data,.pos=HEADER,.end=bytes,.read=true,.ok=true};unsigned seen=0;
    for(unsigned i=0;i<CHUNKS && c.ok;++i) {
        uint32_t id=read32(&c),length=read32(&c);unsigned k=0;
        for(;k<CHUNKS && chunks[k].id!=id;++k) {}
        if(!c.ok || k==CHUNKS || (seen&(1u<<k)) || length>c.end-c.pos) { c.ok=false;break; }
        Codec part=c;part.end=c.pos+length;chunks[k].visit(&part,s,NULL);
        if(!part.ok || part.pos!=part.end) { c.ok=false;break; }
        c.pos=part.end;seen|=1u<<k;
    }
    if(!c.ok || c.pos!=bytes || seen!=((1u<<CHUNKS)-1)) {
        snprintf(error,size,"Invalid, missing or duplicate save section");hmbtn_session_destroy(s);return NULL;
    }
    if(!valid(s,error,size)) { hmbtn_session_destroy(s);return NULL; }
    /* Resource-detached candidate. Constructors already bound all domain views
     * to THIS object. Never memcpy/move HmbtnSession or initialize village again. */
    s->village_initialized=true;s->completion_reported=true;s->control.enabled=true;
    s->intro.area=s->area;s->carry.area=s->area;s->intro.script.finished=true;s->intro.finish_requested=true;
    s->house.tv_dirty=true;s->house.sound=-1;s->carry.sound=s->feed.sound=s->poultry.sound=-1;
    s->farm_action.bank=-1;s->runtime.loaded_cell=UINT32_MAX;s->runtime.audio_song=UINT32_MAX;
    return s;
}
