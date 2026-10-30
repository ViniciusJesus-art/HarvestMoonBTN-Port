/* field/mine.c: dig/probe/layer tables, exit guards and return. The native
 * matrix uses row-major indexes consistently; no packed PS1 pointers/layout. */
#include "mine.h"
#include "psx_data.h"
#include "psx_random.h"
#include <string.h>
typedef struct MineTexture { uint8_t uv[8];uint16_t clut; } MineTexture;
#include "mine_tables.inc"
unsigned hmbtn_mine_kind(unsigned area)
{ return area>=234 && area<=243 ? 1u : area>=245 && area<=254 ? 2u : 0u; }
void hmbtn_mine_enter(HmbtnMine *m,unsigned area)
{
    if(area==244 && m->kind==1 && m->layer==9) return;
    const unsigned kind=hmbtn_mine_kind(area),base=kind==1 ? 234u : 245u;
    if(!kind || m->kind!=kind || area==base) {
        const uint32_t random=m->random;memset(m,0,sizeof(*m));m->random=random;
        for(unsigned i=0;i<HMBTN_MINE_LAYERS;++i) m->exit_cell[i]=HMBTN_MINE_CELLS;
    }
    m->kind=kind;m->layer=kind ? area-base : 0;m->exit_chance=10;
}
bool hmbtn_mine_cell(const HmbtnMine *m,const int32_t p[3],unsigned *cell)
{
    if(!m || !m->kind || !p || !cell || p[0]<-2000 || p[0]>=2000 || p[2]>2000 || p[2]<=-2000) return false;
    *cell=(unsigned)(2000-p[2])/400*10+(unsigned)(p[0]+2000)/400;
    return *cell<HMBTN_MINE_CELLS && m->active[*cell];
}
bool hmbtn_mine_bind(HmbtnMine *m,HmbtnSceneData *scene)
{
    if(!m || !m->kind || !scene) return false;
    memset(m->active,0,sizeof(m->active));
    for(unsigned row=0;row<10;++row) for(unsigned col=0;col<10;++col) {
        const int32_t p[3]={-1800+(int)col*400,0,1800-(int)row*400};HmbtnGroundHit hit;
        if(hmbtn_ground_find(&scene->terrain,p,0xC000,64,&hit) && (hit.flags&8u)) {
            const unsigned cell=row*10+col;m->active[cell]=true;m->quad[cell]=hit.primitive;
        }
    }
    if(m->layer && m->exit_cell[m->layer-1]<100) {
        const unsigned cell=m->exit_cell[m->layer-1];
        scene->flags[0]&=(uint16_t)~0x1000u;
        scene->position[0][0]=(int32_t)(cell%10)*400-2000;scene->position[0][1]=0;
        scene->position[0][2]=1800-(int32_t)(cell/10)*400;
    }
    unsigned count=0;for(unsigned i=0;i<100;++i) count+=m->active[i];return count!=0;
}
bool hmbtn_mine_retexture(const HmbtnMine *m,uint8_t *terrain,size_t size)
{
    if(!m || !m->kind || !terrain || m->layer>=10) return false;
    for(unsigned i=0;i<100;++i) if(m->active[i] && m->cells[m->layer][i]) {
        const size_t off=m->quad[i];if(off>size || size-off<28) return false;
        uint8_t *p=terrain+off;const unsigned type=p[3];
        if(!(p[0]&8) || ((type&0xfdu)!=0x2d && (type&0xfdu)!=0x3d)) return false;
        const MineTexture *uv=&mine_cell_textures[m->cells[m->layer][i]];
        for(unsigned k=0;k<4;++k) { p[4+k*4]=uv->uv[k*2];p[5+k*4]=uv->uv[k*2+1]; }
        hmbtn_write_le16(p+6,uv->clut);
    }
    return true;
}
static unsigned roll(HmbtnMine *m,bool *flags)
{
    const int16_t *weights=m->kind==1 ? mine_layer_weights_mine[m->layer] : mine_layer_weights[m->layer];
    const int16_t *events=m->kind==1 ? mine_layer_events_mine : mine_layer_events;
    const unsigned chance=hmbtn_psx_random_range(&m->random,100);unsigned total=0;
    for(unsigned i=0;i<8;++i) {
        total+=(unsigned)weights[i];if(chance>=total) continue;
        const unsigned item=(unsigned)events[i];
        const unsigned flag=item==0x67 ? (m->kind==1 ? 0x62u : 0x61u) : item==0x63 && m->kind==1 ? 0x60u : 0u;
        if(flag) { if(flags[flag]) return 0;flags[flag]=true; }
        return item;
    }
    return 0;
}
unsigned hmbtn_mine_dig(HmbtnMine *m,const HmbtnIntroActor *player,bool *flags,bool *changed)
{
    *changed=false;if(!m || !m->kind || !player || !flags || player->facing>3 || m->layer>=10) return 0;
    static const int step[4][2]={{0,-400},{-400,0},{0,400},{400,0}};
    const int32_t p[3]={player->position[0]+step[player->facing][0],player->position[1],player->position[2]+step[player->facing][1]};
    unsigned cell;if(!hmbtn_mine_cell(m,p,&cell) || m->cells[m->layer][cell]) return 0;
    const unsigned col=cell%10,row=cell/10;
    if(m->layer && m->exit_cell[m->layer-1]==cell) return 0;
    bool clear=col==0 || col==9 || row==0 || row==9;
    if(m->layer && m->exit_cell[m->layer-1]<100) {
        const unsigned above=m->exit_cell[m->layer-1],x=above%10,y=above/10;
        if((int)col>=(int)x-2 && col<=x+2 && (int)row>=(int)y-2 && row<=y+2) clear=true;
    }
    *changed=true;m->cells[m->layer][cell]=1;
    if(clear) return 0;
    /* The original consumes this roll even after an exit was found. Last
     * layer is explicitly marked as having an exit, so never descends to 10. */
    if(hmbtn_psx_random_range(&m->random,100)<m->exit_chance && m->layer<9 && m->exit_cell[m->layer]==100) {
        m->cells[m->layer][cell]=2;m->exit_cell[m->layer]=cell;return 0;
    }
    const unsigned item=roll(m,flags);m->exit_chance+=item ? 2u : 1u;return item;
}
unsigned hmbtn_mine_check(const HmbtnMine *m,const HmbtnIntroActor *p)
{
    unsigned cell;if(!hmbtn_mine_cell(m,p->position,&cell)) return 0;
    if(m->layer && m->exit_cell[m->layer-1]==cell) return 2;
    return m->layer<9 && m->cells[m->layer][cell]==2 ? 1u : 0u;
}
bool hmbtn_mine_prompt(HmbtnMine *m,unsigned prompt,char *error,size_t size)
{
    uint8_t record[8192]={0};unsigned n=0;
    for(unsigned i=prompt==2 ? 0x34/2 : 0;i<68;++i) {
        const uint16_t word=mine_messages[i];hmbtn_write_le16(record+0x1800+n++*2,word);if(word==0xffff) break;
    }
    if(!hmbtn_message_open(&m->message,record,sizeof(record),error,size)) return false;
    m->prompt=prompt;m->answer=0;m->message.suppress_cursor=true;return true;
}
unsigned hmbtn_mine_transition(HmbtnMine *m,int32_t entry[3])
{
    if(m->prompt==1 && m->layer<9 && m->exit_cell[m->layer]<100) {
        const unsigned cell=m->exit_cell[m->layer++];m->exit_chance=10;
        entry[0]=(int32_t)(cell%10)*400-1800;entry[1]=0;entry[2]=2000-(int32_t)(cell/10+2)*400;
        m->prompt=0;return (m->kind==1 ? 234u : 245u)+m->layer;
    }
    /* world/scene.c's original return positions, not an invented spawn. */
    entry[0]=m->kind==1 ? -10911 : -11453;entry[1]=m->kind==1 ? -1685 : -1892;entry[2]=m->kind==1 ? 9242 : 9706;
    m->prompt=0;return m->kind==1 ? 68u : 72u;
}
bool hmbtn_mine_valid(const HmbtnMine *m,unsigned area)
{
    if(!m || (m->kind!=hmbtn_mine_kind(area) && !(area==244 && m->kind==1 && m->layer==9)) || m->layer>=10 || m->exit_chance>210) return false;
    if(m->kind && area!=244 && area!=(m->kind==1 ? 234u : 245u)+m->layer) return false;
    for(unsigned layer=0;layer<10;++layer) {
        unsigned exits=0;
        for(unsigned i=0;i<100;++i) { if(m->cells[layer][i]>2) return false;exits+=m->cells[layer][i]==2; }
        if(exits>1 || m->exit_cell[layer]>100 || (exits ? m->exit_cell[layer]>=100 || m->cells[layer][m->exit_cell[layer]]!=2 : m->exit_cell[layer]!=100)) return false;
    }
    return m->exit_cell[9]==100;
}
