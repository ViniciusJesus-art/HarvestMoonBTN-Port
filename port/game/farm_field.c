/* Native field queries adapted from farm_probe.c and field/field.c. */
#include "farm_field.h"
#include "psx_data.h"
#include <string.h>
typedef struct HmbtnFarmChoice { int32_t x_min,z_min,x_max,z_max; unsigned location; } HmbtnFarmChoice;
typedef struct HmbtnFarmCrop { unsigned season,grown; int regrow; unsigned days[4]; int harvest; } HmbtnFarmCrop;
typedef struct HmbtnFarmCellUV { uint8_t uv[8]; uint16_t clut; } HmbtnFarmCellUV;
#include "farm_tables.inc"
enum { SOIL=0x167,TILLED=0x168,WATERED=0x169,SOWN=0x16a,WATERED_SOWN=0x16b };
const HmbtnFarmItem *hmbtn_farm_item(unsigned id) { return id<380 ? &farm_items[id] : NULL; }
const HmbtnFarmSound *hmbtn_farm_sound(unsigned id) { return id<78 ? &farm_sounds[id] : NULL; }
const HmbtnFarmTileRule *hmbtn_farm_tile_rule(const HmbtnFarmTile *t)
{
    const HmbtnFarmItem *item=t ? hmbtn_farm_item(t->value) : NULL;
    if(!item || t->kind>=5 || item->rules[t->kind]<0 || item->rules[t->kind]>=17) return NULL;
    return &tile_rules[item->rules[t->kind]];
}
void hmbtn_farm_field_init(HmbtnFarmField *f,const uint16_t *layout,size_t count)
{
    memset(f,0,sizeof(*f));
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
void hmbtn_farm_field_new_day(HmbtnFarmField *f,unsigned season)
{
    f->season=season;
    for(unsigned i=0;i<HMBTN_FIELD_CELLS;++i) {
        HmbtnFarmTile *t=&f->cells[i];
        if(t->ground==WATERED) {
            t->ground=TILLED;if(t->value==WATERED) t->value=TILLED;
        } else if(t->ground==WATERED_SOWN) t->ground=SOWN;
        if(season==3 && t->value==0x3a) { t->value=SOIL;t->ground=SOIL;t->kind=0;t->growth_flags=0; }
    }
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
                if(hmbtn_farm_world_cell(centre,&c,&r)) {
                    const unsigned cell=(unsigned)(r*HMBTN_FIELD_COLS+c);
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
    if(!f || !hmbtn_farm_world_cell(pos,&c,&r)) return 0;
    const unsigned i=(unsigned)(r*HMBTN_FIELD_COLS+c);
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
    if(!hmbtn_farm_world_cell(first,&c,&r) || (c==44 && facing==3)) return false;
    /* farm_tile_state: only soil/no rule beyond the obstacle permits the hop. */
    if(hmbtn_farm_field_probe(f,first)!=1) return false;
    if(!hmbtn_farm_world_cell(second,&c,&r)) return true;
    const unsigned i=(unsigned)(r*HMBTN_FIELD_COLS+c);
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
    if(f && ((mask==0xc080 && choice_inside(farm_choices_default,sizeof(farm_choices_default)/sizeof(*farm_choices_default),pos))
            || ((mask==0xc080 || mask==0xe080) && choice_inside(farm_choices_flagged,sizeof(farm_choices_flagged)/sizeof(*farm_choices_flagged),pos)))) {
        if(pos[1]<0) return false;
        pos[1]=0;return true;
    }
    HmbtnGroundHit hit;
    if(!hmbtn_ground_find(m,pos,mask,tolerance,&hit)) return false;
    pos[1]=hit.height;return true;
}
bool hmbtn_farm_field_retexture(const HmbtnFarmField *f,uint8_t *data,size_t size)
{
    if(!f || !data) return false;
    for(unsigned i=0;i<HMBTN_FIELD_CELLS;++i) {
        if(!f->active[i] || f->quad[i]==SIZE_MAX) continue;
        const size_t at=f->quad[i];
        if(at>size || size-at<16) return false;
        uint8_t *p=data+at; const unsigned key=hmbtn_read_le16(p+2)&0xfdffu;
        if(key!=0x2d01 && key!=0x3d01) return false;
        unsigned look=f->cells[i].ground-SOIL;
        if(look>4) look=0;
        if(f->season==3) look=29;
        const HmbtnFarmCellUV *uv=&cell_looks[look];
        const unsigned mirror=column_mirror_table[i%45];
        for(unsigned k=0;k<4;++k) {
            const unsigned src=mirror ? k : 3-k;
            p[4+k*4]=uv->uv[src*2];p[5+k*4]=uv->uv[src*2+1];
        }
        hmbtn_write_le16(p+6,uv->clut);hmbtn_write_le16(p+10,f->season==3 ? 7 : 8);
    }
    return true;
}
static void reset(HmbtnFarmTile *t) { t->ground=t->value=SOIL;t->kind=0;t->growth_flags=0; }
bool hmbtn_farm_field_tool(HmbtnFarmField *f,int c,int r,unsigned kind,unsigned power,unsigned reach,unsigned supplies[4])
{
    if(!f || c<0 || c>=45 || r<0 || r>=25 || kind>4) return false;
    const unsigned i=(unsigned)(r*45+c);if(!f->active[i]) return false;
    HmbtnFarmTile *t=&f->cells[i];const HmbtnFarmTileRule *rule=hmbtn_farm_tile_rule(t);
    if(kind==4) {
        if(t->ground!=TILLED && t->ground!=SOWN) return false;
        t->ground=(uint16_t)(t->ground==TILLED ? WATERED : WATERED_SOWN);
        if(t->value==TILLED) t->value=WATERED;
        return true;
    }
    if(!rule) return false;
    if(kind==1) {
        if(!(rule->usable&0x800)) return false;
        t->ground=t->value=(uint16_t)(t->ground==WATERED_SOWN || t->value==WATERED ? WATERED : TILLED);
        t->kind=0;t->growth_flags&=0x7fff;return true;
    }
    if(kind==0) {
        if(!(rule->usable&0x8000)) return false;
        t->value=t->ground;t->kind=0;t->growth_flags=0;return true;
    }
    if(!(rule->usable&(kind==2 ? 0x4000 : 0x2000)) || power<rule->power || reach<rule->reach) return false;
    const unsigned value=t->value;
    const bool large=(value>=0x3b && value<=0x3d) || (value>=0x16c && value<=0x174);
    if(large) {
        /* The four encoded corners of one 2x2 obstacle. */
        unsigned corner=value>=0x16c ? (value-0x16c)%3+1 : 0;
        const int left=c-((corner==1 || corner==2) ? 1 : 0), top=r-(corner>=2 ? 1 : 0);
        if(left<0 || left+1>=45 || top<0 || top+1>=25) return false;
        for(unsigned y=0;y<2;++y) for(unsigned x=0;x<2;++x) reset(&f->cells[(top+(int)y)*45+left+(int)x]);
    } else reset(t);
    if(kind==2 && supplies) {
        const unsigned add=large ? 6u : 1u;
        supplies[1]=supplies[1]+add>9999 ? 9999 : supplies[1]+add;
    }
    return true;
}
