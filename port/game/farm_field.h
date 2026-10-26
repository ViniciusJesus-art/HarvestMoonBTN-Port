#ifndef HMBTN_FARM_FIELD_H
#define HMBTN_FARM_FIELD_H
#include "ground.h"
enum { HMBTN_FIELD_COLS=45, HMBTN_FIELD_ROWS=25, HMBTN_FIELD_CELLS=1125 };
typedef struct HmbtnFarmTile {
    uint16_t value, ground;
    uint8_t kind;
    uint16_t growth_flags;
    unsigned watered_days; /* Original FarmGrowth.days, independent of binary layout. */
} HmbtnFarmTile;
typedef struct HmbtnFarmField {
    HmbtnFarmTile cells[HMBTN_FIELD_CELLS];
    size_t quad[HMBTN_FIELD_CELLS]; /* Offsets into the current terrain, never pointers. */
    bool active[HMBTN_FIELD_CELLS];
    unsigned season;
} HmbtnFarmField;
typedef struct HmbtnFarmItem {
    uint16_t flags;
    unsigned category;
    int crop, rules[5], picture, buy_price, sell_price;
    int stamina, fatigue, plant_animation, half_width, half_depth;
    uint32_t class_flags;
} HmbtnFarmItem;
/* Definitions are immutable catalog data; cells only hold instance state. */
typedef struct HmbtnFarmCrop { unsigned season,grown; int regrow; unsigned days[4]; int harvest; } HmbtnFarmCrop;
const HmbtnFarmCrop *hmbtn_farm_crop(unsigned seed);
bool hmbtn_farm_crop_supported(unsigned seed);
bool hmbtn_farm_field_plant(HmbtnFarmField *field,int column,int row,unsigned seed);
unsigned hmbtn_farm_field_harvest_item(const HmbtnFarmField *field,unsigned index);
unsigned hmbtn_farm_field_harvest(HmbtnFarmField *field,unsigned index);
/* Original nine-cell seed grid, rotated by facing. */
bool hmbtn_farm_seed_cell(unsigned seed,unsigned part,unsigned facing,int *dx,int *dz);

typedef struct HmbtnFarmTileRule { unsigned power, reach, status, usable; } HmbtnFarmTileRule;
typedef struct HmbtnFarmSound { unsigned bank, unused1, program, tone, note, unused5, volume; } HmbtnFarmSound;
void hmbtn_farm_field_init(HmbtnFarmField *field, const uint16_t *layout, size_t count);
void hmbtn_farm_field_new_day(HmbtnFarmField *field,unsigned season);
bool hmbtn_farm_field_bind(HmbtnFarmField *field, const HmbtnModel *terrain);
const HmbtnFarmItem *hmbtn_farm_item(unsigned id);
const HmbtnFarmSound *hmbtn_farm_sound(unsigned id);
const HmbtnFarmTileRule *hmbtn_farm_tile_rule(const HmbtnFarmTile *tile);
bool hmbtn_farm_world_cell(const int32_t position[3], int *column, int *row);
/* 0 clear; 1 blocks and may be hopped; 2 blocks and cannot be hopped. */
unsigned hmbtn_farm_field_probe(const HmbtnFarmField *field, const int32_t position[3]);
bool hmbtn_farm_field_can_hop(const HmbtnFarmField *field, const int32_t position[3], unsigned facing);
bool hmbtn_farm_settle(const HmbtnFarmField *field, const HmbtnModel *terrain,
    int32_t position[3], uint16_t mask, unsigned tolerance);
bool hmbtn_farm_field_retexture(const HmbtnFarmField *field, uint8_t *terrain, size_t size);
/* field_gate_open / weather.c: field_reset_cells; same eligibility as manual watering. */
bool hmbtn_farm_field_water(HmbtnFarmField *field,unsigned cell);
void hmbtn_farm_field_rain(HmbtnFarmField *field);
bool hmbtn_farm_field_tool(HmbtnFarmField *field, int column, int row, unsigned kind,
    unsigned power, unsigned reach, unsigned supplies[4]);
#endif
