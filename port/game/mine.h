#ifndef HMBTN_MINE_H
#define HMBTN_MINE_H
#include "new_game.h"
#include "ground.h"
#include "farm_intro.h"
#include "scene_decode.h"
enum { HMBTN_MINE_LAYERS=10,HMBTN_MINE_CELLS=100 };
typedef struct HmbtnMine {
    unsigned kind,layer,exit_chance,exit_cell[HMBTN_MINE_LAYERS];
    uint8_t cells[HMBTN_MINE_LAYERS][HMBTN_MINE_CELLS];
    uint32_t random;
    /* Resource bindings and modal presentation are transient, never pointers on disc. */
    size_t quad[HMBTN_MINE_CELLS];bool active[HMBTN_MINE_CELLS];
    unsigned found,prompt,answer,back_ticks,reward,reward_phase;
    HmbtnMessage message;
} HmbtnMine;
unsigned hmbtn_mine_kind(unsigned area);
void hmbtn_mine_enter(HmbtnMine *,unsigned area);
bool hmbtn_mine_bind(HmbtnMine *,HmbtnSceneData *);
bool hmbtn_mine_cell(const HmbtnMine *,const int32_t position[3],unsigned *cell);
bool hmbtn_mine_retexture(const HmbtnMine *,uint8_t *terrain,size_t);
/* Returns the found item, not another inventory or deferred loot collection. */
unsigned hmbtn_mine_dig(HmbtnMine *,const HmbtnIntroActor *,bool *flags,bool *changed);
unsigned hmbtn_mine_check(const HmbtnMine *,const HmbtnIntroActor *); /* 1 descend, 2 leave */
bool hmbtn_mine_prompt(HmbtnMine *,unsigned prompt,char *,size_t);
/* Commits a confirmed transition; caller owns the existing area/fade handoff. */
unsigned hmbtn_mine_transition(HmbtnMine *,int32_t entry[3]);
bool hmbtn_mine_valid(const HmbtnMine *,unsigned area);
#endif
