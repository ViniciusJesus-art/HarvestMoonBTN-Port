/* Native field queries adapted from farm_probe.c and field/field.c. */
#include "farm_field.h"
#include "psx_data.h"
#include "psx_random.h"
#include "inventory.h"
#include <string.h>
typedef struct HmbtnFarmChoice { int32_t x_min,z_min,x_max,z_max; unsigned location; } HmbtnFarmChoice;
typedef struct HmbtnFarmCellUV { uint8_t uv[8]; uint16_t clut; } HmbtnFarmCellUV;
typedef struct HmbtnGreenhouseUV { uint8_t uv[4][2];uint16_t clut; } HmbtnGreenhouseUV;
#include "farm_tables.inc"
enum { GRASS=0x36,SOIL=0x167,TILLED=0x168,WATERED=0x169,SOWN=0x16a,WATERED_SOWN=0x16b };
const HmbtnFarmItem *hmbtn_farm_item(unsigned id) { return id<380 ? &farm_items[id] : NULL; }
const HmbtnFarmSound *hmbtn_farm_sound(unsigned id) { return id<78 ? &farm_sounds[id] : NULL; }
unsigned hmbtn_farm_field_columns(const HmbtnFarmField *f) { return f && f->indoor ? HMBTN_GREENHOUSE_COLS : HMBTN_FIELD_COLS; }
unsigned hmbtn_farm_field_rows(const HmbtnFarmField *f) { return f && f->indoor ? HMBTN_GREENHOUSE_ROWS : HMBTN_FIELD_ROWS; }
unsigned hmbtn_farm_field_cells(const HmbtnFarmField *f) { return hmbtn_farm_field_columns(f)*hmbtn_farm_field_rows(f); }
void hmbtn_farm_field_clear(HmbtnFarmField *f)
{
    if(f) for(unsigned i=0;i<hmbtn_farm_field_cells(f);++i) f->cells[i]=(HmbtnFarmTile){.value=SOIL,.ground=SOIL};
}
const HmbtnFarmTileRule *hmbtn_farm_tile_rule(const HmbtnFarmTile *t)
{
    const HmbtnFarmItem *item=t ? hmbtn_farm_item(t->value) : NULL;
    if(!item || t->kind>=5 || item->rules[t->kind]<0 || item->rules[t->kind]>=17) return NULL;
    return &tile_rules[item->rules[t->kind]];
}
void hmbtn_farm_field_init(HmbtnFarmField *f,const uint16_t *layout,size_t count)
{
    memset(f,0,sizeof(*f));
    f->plant_random=1;
    for(unsigned i=0;i<HMBTN_FIELD_CELLS;++i) {
        HmbtnFarmTile *t=&f->cells[i]; t->ground=SOIL;
        t->value=layout && i<count ? layout[i] : SOIL;
        const HmbtnFarmItem *item=hmbtn_farm_item(t->value);
        if(t->value>=34 && t->value<49 && item && item->crop>=0 && item->crop<22) {
            t->ground=TILLED; t->kind=(uint8_t)crop_table[item->crop].grown;
        }
        f->quad[i]=SIZE_MAX; f->active[i]=true;
    }
}
void hmbtn_farm_field_init_greenhouse(HmbtnFarmField *f)
{
    memset(f,0,sizeof(*f));f->indoor=true;f->plant_random=1;
    hmbtn_farm_field_clear(f);
    for(unsigned i=0;i<HMBTN_FIELD_CELLS;++i) { f->quad[i]=SIZE_MAX;f->active[i]=i<HMBTN_GREENHOUSE_CELLS; }
}
bool hmbtn_farm_field_position(const HmbtnFarmField *f,unsigned c,unsigned r,int32_t pos[3])
{
    if(!f || !pos || c>=hmbtn_farm_field_columns(f) || r>=hmbtn_farm_field_rows(f)) return false;
    pos[0]=(int32_t)c*400+(f->indoor ? -1420 : -4200);pos[1]=0;
    pos[2]=(f->indoor ? 1020 : 1400)-(int32_t)r*400;return true;
}
bool hmbtn_farm_world_cell(const int32_t pos[3],int *column,int *row)
{
    if(!pos || !column || !row || pos[0]<-32768 || pos[0]>32767 || pos[2]<-32768 || pos[2]>32767) return false;
    /* Original truncates both intermediate coordinates to signed halfwords. */
    unsigned x=((unsigned)pos[0]+4400u)&65535u, z=(1600u-(unsigned)pos[2])&65535u;
    const int dx=x<32768 ? (int)x : (int)x-65536, dz=z<32768 ? (int)z : (int)z-65536;
    *column=dx/399; *row=dz/399;
    if(dx<0) --*column;
    if(dz<0) --*row;
    return dx>=0 && dz>=0 && *column<HMBTN_FIELD_COLS && *row<HMBTN_FIELD_ROWS;
}
bool hmbtn_farm_field_world_cell(const HmbtnFarmField *f,const int32_t pos[3],int *column,int *row)
{
    if(!f || !f->indoor) return hmbtn_farm_world_cell(pos,column,row);
    if(!pos || !column || !row || pos[0]<-32768 || pos[0]>32767 || pos[2]<-32768 || pos[2]>32767) return false;
    /* greenhouse_world_to_cell: preserve signed intermediate coordinates. */
    const unsigned x=((unsigned)pos[0]+1620u)&65535u,z=(1220u-(unsigned)pos[2])&65535u;
    const int dx=x<32768 ? (int)x : (int)x-65536,dz=z<32768 ? (int)z : (int)z-65536;
    *column=dx/400;*row=dz/400;
    if(dx<0) --*column;
    if(dz<0) --*row;
    return dx>=0 && dz>=0 && *column<HMBTN_GREENHOUSE_COLS && *row<HMBTN_GREENHOUSE_ROWS;
}
/* Adapted from src/overlay/11_GF_FARM/field/field.c: field_event_dispatch,
 * field_daily_tick, field_tile_dry, field_tile_item_stage; reference d3c0f38.
 * Explicit native state, no packed FarmGrowth alias or shared frame RNG.
 * Both field layouts share crop rules; outdoor seasonal behavior remains
 * unchanged. Random weeds remain absent. Other catalog definitions remain available but are not claimed
 * supported. The growing loop uses definition lengths, not a new catalog cap. */
const HmbtnFarmCrop *hmbtn_farm_crop(unsigned seed)
{
    const HmbtnFarmItem *item=hmbtn_farm_item(seed);
    return item && item->category==2 && item->crop>=0
        && (unsigned)item->crop<sizeof(crop_table)/sizeof(*crop_table) ? &crop_table[item->crop] : NULL;
}
/* Flower definitions/daily/harvest are already shared with food crops.
 * Source: farm_tables.c CROP_PINK_CAT / CROP_TOY_FLOWER, field.c.
 * Their zero-price products remain flowers, not saleable vegetables.
 * Orangecup (reference 6fa97ab: CROP_ORANGE_CUP, field_event_dispatch,
 * shop_seed_menu_setup_d) needs only this support filter: do not impose a
 * greenhouse purchase/planting lock absent from those original functions. */
bool hmbtn_farm_crop_supported(unsigned seed)
{ return (seed>=0x22 && seed<=GRASS) || seed==0x178 || seed==0x179; }
bool hmbtn_farm_field_has_grown(const HmbtnFarmField *f,unsigned seed)
{
    /* farm_event_available(7), field_find_free_tile(flags=2): value and
     * grown stage match; ground is ignored. Existence needs no random start. */
    const HmbtnFarmCrop *crop=hmbtn_farm_crop(seed);
    if(!f || !crop) return false;
    for(unsigned i=0;i<hmbtn_farm_field_cells(f);++i)
        if(f->cells[i].value==seed && f->cells[i].kind==crop->grown) return true;
    return false;
}
static void dry(HmbtnFarmTile *t)
{
    if(t->ground==WATERED) { t->ground=TILLED;if(t->value==WATERED) t->value=TILLED; }
    else if(t->ground==WATERED_SOWN) t->ground=SOWN;
}
bool hmbtn_farm_field_plant(HmbtnFarmField *f,int c,int r,unsigned seed)
{
    if(!f || c<0 || c>=(int)hmbtn_farm_field_columns(f) || r<0 || r>=(int)hmbtn_farm_field_rows(f)
            || !hmbtn_farm_crop_supported(seed) || (!f->indoor && f->season==3)) return false;
    HmbtnFarmTile *t=&f->cells[r*hmbtn_farm_field_columns(f)+c];
    if(!f->active[r*hmbtn_farm_field_columns(f)+c] || (t->value!=TILLED && t->value!=WATERED)) return false;
    /* field_event_dispatch: one roll per accepted cell, after eligibility.
     * Store the original variant item ID; growth/draw/load never reroll it.
     * Reuse rand_range's arithmetic on a native stream independent of frames. */
    if(seed==0x33) seed=hmbtn_psx_random_range(&f->plant_random,25)==0 ? 0x179 : 0x178;
    t->ground=t->ground==TILLED ? SOWN : WATERED_SOWN;t->value=(uint16_t)seed;
    t->kind=0;t->watered_days=0;t->growth_flags&=0x7fff;return true;
}
void hmbtn_farm_field_new_day(HmbtnFarmField *f,unsigned season)
{
    if(!f || season>3) return;
    f->season=season;
    for(unsigned i=0;i<hmbtn_farm_field_cells(f);++i) {
        HmbtnFarmTile *t=&f->cells[i];
        const HmbtnFarmCrop *crop=hmbtn_farm_crop(t->value);
        if(crop && hmbtn_farm_crop_supported(t->value)) {
            const bool grass=t->value==GRASS;
            /* field_daily_tick resets established grass every winter morning,
             * then runs the normal ANY-season growth step (days becomes 1). */
            if(!f->indoor && grass && season==3 && t->kind) {
                t->kind=1;t->watered_days=0;t->growth_flags=0;
            }
            if(!f->indoor && crop->season!=season && crop->season!=4) {
                /* Original leaves stage 1 dead stalks in the following season;
                 * winter and unrelated seasons clear the seed tile entirely. */
                if(season!=3 && t->kind && crop->season+1==season) t->kind=1;
                else { t->value=t->ground=TILLED;t->kind=0;t->growth_flags=0;t->watered_days=0; }
                t->watered_days=0;dry(t);continue;
            }
            if((t->ground==WATERED || t->ground==WATERED_SOWN || grass) && t->kind!=crop->grown
                    && t->kind<sizeof(crop->days)/sizeof(*crop->days)) {
                if(t->growth_flags&0x8000u) t->growth_flags&=0x7fffu;
                else ++t->watered_days;
                if(t->watered_days>=crop->days[t->kind]) {
                    t->ground=TILLED;t->watered_days=0;++t->kind;
                }
            }
        }
        dry(t);
        if(!f->indoor && season==3 && t->value==0x3a) { *t=(HmbtnFarmTile){.value=SOIL,.ground=SOIL}; }
    }
}
unsigned hmbtn_farm_field_harvest_item(const HmbtnFarmField *f,unsigned i)
{
    if(!f || i>=hmbtn_farm_field_cells(f) || !f->active[i]) return 0;
    const HmbtnFarmTile *t=&f->cells[i];const HmbtnFarmCrop *crop=hmbtn_farm_crop(t->value);
    if(!hmbtn_farm_crop_supported(t->value) || !crop || (!f->indoor && crop->season!=f->season)
            || t->kind!=crop->grown || crop->harvest<=0) return 0;
    return (unsigned)crop->harvest;
}
unsigned hmbtn_farm_field_harvest(HmbtnFarmField *f,unsigned i)
{
    const unsigned item=hmbtn_farm_field_harvest_item(f,i);
    if(!item) return 0;
    HmbtnFarmTile *t=&f->cells[i];const HmbtnFarmCrop *crop=hmbtn_farm_crop(t->value);
    if(crop->regrow<0) { t->value=t->ground;t->kind=0;t->growth_flags=0;t->watered_days=0; }
    else { t->kind=(uint8_t)crop->regrow;t->growth_flags|=0x8000u; }
    t->watered_days=0;return item;
}
/* picture_cells.c:picture_grid_find_cell on farm_tables.c picture grid 25.
 * The nine offsets are extracted, not an assumed 3x3 pattern. */
#include "seed_grid.inc"
bool hmbtn_farm_seed_cell(unsigned seed,unsigned part,unsigned facing,int *dx,int *dz)
{
    const HmbtnFarmItem *item=hmbtn_farm_item(seed);
    if(!hmbtn_farm_crop_supported(seed) || !item || item->picture!=25 || part>=9 || facing>3 || !dx || !dz) return false;
    int x=seed_grid[part][0],z=seed_grid[part][1];
    /* Original orientations: transpose, mirror, transpose+flip, copy. */
    if(facing==0) { const int tmp=x;x=z;z=tmp; }
    else if(facing==1) x=-x;
    else if(facing==2) { const int tmp=x;x=z;z=-tmp; }
    *dx=x;*dz=z;return true;
}
bool hmbtn_farm_field_bind(HmbtnFarmField *f,const HmbtnModel *m)
{
    if(!f || !m || !m->data) return false;
    for(unsigned i=0;i<HMBTN_FIELD_CELLS;++i) { f->quad[i]=SIZE_MAX;f->active[i]=false; }
    for(unsigned i=0;i<m->object_count;++i) {
        HmbtnModelObject o; if(!hmbtn_model_object(m,i,&o)) return false;
        size_t at=12+(size_t)o.primitives;
        for(unsigned j=0;j<o.primitive_count;++j) {
            HmbtnModelPrimitive p; if(!hmbtn_model_primitive(m,at,&p)) return false;
            if(p.count==4 && (p.flags&8)) {
                int32_t centre[3]={0};
                for(unsigned k=0;k<4;++k) {
                    int32_t v[3];hmbtn_model_vertex(m,&o,p.vertex[k],v);
                    for(unsigned a=0;a<3;++a) centre[a]+=v[a];
                }
                for(unsigned a=0;a<3;++a) centre[a]/=4;
                int c,r;
                if(hmbtn_farm_field_world_cell(f,centre,&c,&r)) {
                    const unsigned cell=(unsigned)(r*hmbtn_farm_field_columns(f)+c);
                    if(f->active[cell]) return false;
                    f->quad[cell]=at;f->active[cell]=true;
                }
            }
            at+=p.bytes;
        }
    }
    return true;
}
unsigned hmbtn_farm_field_probe(const HmbtnFarmField *f,const int32_t pos[3])
{
    int c,r;
    if(!f || !hmbtn_farm_field_world_cell(f,pos,&c,&r)) return 0;
    const unsigned i=(unsigned)(r*hmbtn_farm_field_columns(f)+c);
    if(!f->active[i] || f->cells[i].value==SOIL) return 0;
    const HmbtnFarmTileRule *rule=hmbtn_farm_tile_rule(&f->cells[i]);
    return rule && rule->status>0 && rule->status<=2 ? rule->status : 0;
}
bool hmbtn_farm_field_can_hop(const HmbtnFarmField *f,const int32_t pos[3],unsigned facing)
{
    static const int step[4][2]={{0,-400},{-400,0},{0,400},{400,0}};
    if(!f || !pos || facing>3) return false;
    int32_t first[3]={pos[0]+step[facing][0],pos[1],pos[2]+step[facing][1]},
        second[3]={pos[0]+step[facing][0]*2,pos[1],pos[2]+step[facing][1]*2};
    int c,r;
    if(!hmbtn_farm_field_world_cell(f,first,&c,&r) || (c==(int)hmbtn_farm_field_columns(f)-1 && facing==3)) return false;
    /* farm_tile_state: only soil/no rule beyond the obstacle permits the hop. */
    if(hmbtn_farm_field_probe(f,first)!=1) return false;
    if(!hmbtn_farm_field_world_cell(f,second,&c,&r)) return true;
    const unsigned i=(unsigned)(r*hmbtn_farm_field_columns(f)+c);
    const HmbtnFarmTileRule *rule=hmbtn_farm_tile_rule(&f->cells[i]);
    return !f->active[i] || f->cells[i].value==SOIL || !rule || rule->status==0;
}
static bool choice_inside(const HmbtnFarmChoice *table,size_t count,const int32_t pos[3])
{
    for(size_t i=0;i<count;++i) {
        const HmbtnFarmChoice *c=&table[i];
        if(c->location==1 && c->x_min<pos[0] && pos[0]<c->x_max && c->z_min<pos[2] && pos[2]<c->z_max) return true;
    }
    return false;
}
bool hmbtn_farm_settle(const HmbtnFarmField *f,const HmbtnModel *m,int32_t pos[3],uint16_t mask,unsigned tolerance)
{
    if(!pos) return false;
    if(f && !f->indoor && ((mask==0xc080 && choice_inside(farm_choices_default,sizeof(farm_choices_default)/sizeof(*farm_choices_default),pos))
            || ((mask==0xc080 || mask==0xe080) && choice_inside(farm_choices_flagged,sizeof(farm_choices_flagged)/sizeof(*farm_choices_flagged),pos)))) {
        if(pos[1]<0) return false;
        pos[1]=0;return true;
    }
    HmbtnGroundHit hit;
    if(!hmbtn_ground_find(m,pos,mask,tolerance,&hit)) return false;
    pos[1]=hit.height;return true;
}
/* field_reshape_grown, merge=0: the original nine-neighbour masks select eight
 * terrain shapes. Derive this visual data from the field instead of keeping a
 * second, manually synchronized shape cache in growth_flags. */
static unsigned grass_shape(const HmbtnFarmField *f,unsigned cell)
{
    const unsigned cols=hmbtn_farm_field_columns(f);
    const int c=(int)(cell%cols),r=(int)(cell/cols);
    unsigned mask=0,bit=0;
    for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx,++bit) {
        const int x=c+dx,y=r+dy;
        if(x<0 || x>=(int)hmbtn_farm_field_columns(f) || y<0 || y>=(int)hmbtn_farm_field_rows(f)) continue;
        const unsigned i=(unsigned)(y*(int)cols+x);
        if(f->active[i] && f->cells[i].value==GRASS && f->cells[i].kind) mask|=1u<<bit;
    }
    if((mask&0x1b8)==0x1b8) return 6;
    if((mask&0xdb)==0xdb && (mask&0x24)!=0x24 && (mask&0x120)!=0x120) return 2;
    if((mask&0x1bb)==0x1bb || (mask&0xfe)==0xfe) return 6;
    if((mask&0x3f)==0x3f && (mask&0xc0)!=0xc0 && (mask&0x180)!=0x180) return 4;
    if((mask&0x1b6)==0x1b6 && (mask&0x9)!=0x9 && (mask&0x48)!=0x48) return 6;
    if((mask&0x1b)==0x1b && (mask&0xc0)!=0xc0 && (mask&0x24)!=0x24) return 3;
    if((mask&0xd8)==0xd8 && (mask&0x3)!=0x3 && (mask&0x120)!=0x120) return 1;
    if((mask&0x36)==0x36 && (mask&0x9)!=0x9 && (mask&0x180)!=0x180) return 5;
    if((mask&0x1b0)==0x1b0 && (mask&0x48)!=0x48 && (mask&0x6)!=0x6) return 7;
    return 0;
}
bool hmbtn_farm_field_retexture(const HmbtnFarmField *f,uint8_t *data,size_t size)
{
    if(!f || !data) return false;
    for(unsigned i=0;i<hmbtn_farm_field_cells(f);++i) {
        if(!f->active[i] || f->quad[i]==SIZE_MAX) continue;
        const size_t at=f->quad[i];
        if(at>size || size-at<16) return false;
        uint8_t *p=data+at; const unsigned key=hmbtn_read_le16(p+2)&0xfdffu;
        if(key!=0x2d01 && key!=0x3d01) return false;
        unsigned look=f->cells[i].ground-SOIL;
        if(look>4) look=0;
        if(f->cells[i].value==GRASS && f->cells[i].kind>0 && f->cells[i].kind<=3)
            look=5+(f->cells[i].kind-1)*8+grass_shape(f,i);
        if(f->indoor) {
            /* greenhouse_update_cells/retexture_quad: fixed palette/page,
             * only row zero mirrors the original rectangle corners. */
            const HmbtnGreenhouseUV *uv=&greenhouse_uvs[look];
            for(unsigned k=0;k<4;++k) {
                const unsigned src=i/hmbtn_farm_field_columns(f)==0 ? 3-k : k;
                p[4+k*4]=uv->uv[src][0];p[5+k*4]=uv->uv[src][1];
            }
            hmbtn_write_le16(p+6,uv->clut);hmbtn_write_le16(p+10,look>=5 ? 29 : 9);continue;
        }
        if(f->season==3) look=29;
        const HmbtnFarmCellUV *uv=&cell_looks[look];
        const unsigned mirror=column_mirror_table[i%45];
        for(unsigned k=0;k<4;++k) {
            const unsigned src=mirror ? k : 3-k;
            p[4+k*4]=uv->uv[src*2];p[5+k*4]=uv->uv[src*2+1];
        }
        const bool grass=look>=5 && look!=29;
        const unsigned clut=grass && f->season==2 ? (uv->clut&0xffc0u)|0x35u : uv->clut;
        hmbtn_write_le16(p+6,(uint16_t)clut);hmbtn_write_le16(p+10,f->season==3 ? 7 : grass ? 29 : 8);
    }
    return true;
}
bool hmbtn_farm_field_water(HmbtnFarmField *f,unsigned i)
{
    if(!f || i>=hmbtn_farm_field_cells(f) || !f->active[i] || (!f->indoor && f->season==3)) return false;
    HmbtnFarmTile *t=&f->cells[i];
    if(t->value==GRASS && t->kind) return false;
    if(t->ground!=TILLED && t->ground!=SOWN) return false;
    t->ground=(uint16_t)(t->ground==TILLED ? WATERED : WATERED_SOWN);
    if(t->value==TILLED) t->value=WATERED;
    return true;
}
void hmbtn_farm_field_rain(HmbtnFarmField *f)
{
    if(f && !f->indoor) for(unsigned i=0;i<hmbtn_farm_field_cells(f);++i) hmbtn_farm_field_water(f,i);
}
static void reset(HmbtnFarmTile *t) { t->ground=t->value=SOIL;t->kind=0;t->growth_flags=0;t->watered_days=0; }
bool hmbtn_farm_field_tool(HmbtnFarmField *f,int c,int r,unsigned kind,unsigned power,unsigned reach,unsigned supplies[4])
{
    if(!f || c<0 || c>=(int)hmbtn_farm_field_columns(f) || r<0 || r>=(int)hmbtn_farm_field_rows(f) || kind>4) return false;
    const unsigned i=(unsigned)(r*(int)hmbtn_farm_field_columns(f)+c);if(!f->active[i]) return false;
    HmbtnFarmTile *t=&f->cells[i];const HmbtnFarmTileRule *rule=hmbtn_farm_tile_rule(t);
    if(kind==4) return hmbtn_farm_field_water(f,i);
    if(!rule) return false;
    if(kind==1) {
        if(!(rule->usable&0x800)) return false;
        t->ground=t->value=(uint16_t)(t->ground==WATERED_SOWN || t->value==WATERED ? WATERED : TILLED);
        t->kind=0;t->watered_days=0;t->growth_flags&=0x7fff;return true;
    }
    if(kind==0) {
        if(!(rule->usable&0x8000)) return false;
        if(t->value==GRASS) {
            /* field_tile_harvest -> estate_fodder_add / field_grown_cell_request.
             * A cut still regrows when stock is full; never yield a held item. */
            if(!supplies) return false;
            if(supplies[0]<999) ++supplies[0];
            t->kind=(uint8_t)hmbtn_farm_crop(GRASS)->regrow;
            t->watered_days=0;t->growth_flags|=0x8000u;return true;
        }
        t->value=t->ground;t->kind=0;t->growth_flags=0;t->watered_days=0;return true;
    }
    if(!(rule->usable&(kind==2 ? 0x4000 : 0x2000)) || power<rule->power || reach<rule->reach) return false;
    const unsigned value=t->value;
    const bool large=(value>=0x3b && value<=0x3d) || (value>=0x16c && value<=0x174);
    if(large) {
        /* The four encoded corners of one 2x2 obstacle. */
        unsigned corner=value>=0x16c ? (value-0x16c)%3+1 : 0;
        const int left=c-((corner==1 || corner==2) ? 1 : 0), top=r-(corner>=2 ? 1 : 0);
        if(left<0 || left+1>=(int)hmbtn_farm_field_columns(f) || top<0 || top+1>=(int)hmbtn_farm_field_rows(f)) return false;
        for(unsigned y=0;y<2;++y) for(unsigned x=0;x<2;++x) reset(&f->cells[(top+(int)y)*(int)hmbtn_farm_field_columns(f)+left+(int)x]);
    } else reset(t);
    if(kind==2 && supplies) {
        const unsigned add=large ? 6u : 1u;
        const unsigned limit=hmbtn_inventory_lumber_limit();
        supplies[1]=supplies[1]+add>limit ? limit : supplies[1]+add;
    }
    return true;
}
