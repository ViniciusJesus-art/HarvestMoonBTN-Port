#include "event_script.h"
#include "farm_intro.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Line %d: %s (%s)\n",__LINE__,#x,error); return 1; } } while (0)
static void step(uint8_t *p,unsigned type,unsigned yes,unsigned no,unsigned stop,
    int a,int b,int32_t x,int32_t y)
{
    memset(p,0,16); p[0]=(uint8_t)type; p[1]=(uint8_t)yes; p[2]=(uint8_t)no; p[3]=(uint8_t)stop;
    hmbtn_write_le16(p+4,(uint16_t)a); hmbtn_write_le16(p+6,(uint16_t)b);
    hmbtn_write_le32(p+8,(uint32_t)x); hmbtn_write_le32(p+12,(uint32_t)y);
}
static HmbtnEventResult handler(void *context,const HmbtnEventCommand *c,char *error,size_t size)
{
    (void)error; (void)size;
    unsigned *counter=context;
    if (c->type==0) { ++*counter; return HMBTN_EVENT_DONE; }
    if (c->type==1) return *counter==3 ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
    return HMBTN_EVENT_BOUNDARY;
}
int main(void)
{
    char error[256]={0}; uint8_t bytes[144]; unsigned counter=0;
    HmbtnEventScript s;
    step(bytes,0,255,255,0,-1,-2,INT32_MIN,INT32_MAX);
    step(bytes+16,1,2,0,1,0,0,0,0);
    step(bytes+32,255,0,0,0,0,0,0,0);
    CHECK(hmbtn_event_script_open(&s,bytes,48,error,sizeof(error)));
    for (unsigned i=0;i<3;++i) CHECK(hmbtn_event_tick(&s,handler,&counter,error,sizeof(error)));
    CHECK(counter==3 && s.pc==2 && !s.finished);
    CHECK(hmbtn_event_tick(&s,handler,&counter,error,sizeof(error)) && s.finished);
    CHECK(hmbtn_event_tick(&s,handler,&counter,error,sizeof(error)) && counter==3);
    bytes[1]=3;
    CHECK(!hmbtn_event_script_open(&s,bytes,48,error,sizeof(error)));
    bytes[1]=0; /* Non-yielding self-loop is bounded. */
    CHECK(hmbtn_event_script_open(&s,bytes,48,error,sizeof(error)));
    CHECK(!hmbtn_event_tick(&s,handler,&counter,error,sizeof(error)));
    CHECK(strstr(error,"256 instructions")!=NULL);
    bytes[0]=2;
    CHECK(hmbtn_event_script_open(&s,bytes,48,error,sizeof(error)));
    CHECK(hmbtn_event_tick(&s,handler,&counter,error,sizeof(error)) && s.boundary && s.pc==0);
    CHECK(!hmbtn_event_script_open(&s,bytes,47,error,sizeof(error)));
    CHECK(!hmbtn_event_script_open(&s,bytes,32,error,sizeof(error)));

    uint8_t block[84]={0}; HmbtnEventBlock b; const uint8_t *data; size_t size; uint16_t flags;
    hmbtn_write_le16(block,0x8080); hmbtn_write_le16(block+2,3);
    hmbtn_write_le16(block+4,0x2011); hmbtn_write_le16(block+6,0x8000); hmbtn_write_le16(block+8,0x4000);
    hmbtn_write_le32(block+56,2); hmbtn_write_le32(block+60,4); hmbtn_write_le32(block+64,16);
    CHECK(hmbtn_event_block_open(&b,block,sizeof(block),error,sizeof(error)));
    CHECK(b.payload==68 && b.script==2 && b.count==3);
    CHECK(hmbtn_event_block_entry(&b,1,&data,&size,&flags) && data==block+70 && size==2 && flags==0x8000);
    CHECK(!hmbtn_event_block_entry(&b,3,&data,&size,&flags));
    for (size_t cut=0;cut<sizeof(block);++cut) CHECK(!hmbtn_event_block_open(&b,block,cut,error,sizeof(error)));
    hmbtn_write_le32(block+60,1);
    CHECK(!hmbtn_event_block_open(&b,block,sizeof(block),error,sizeof(error)));
    hmbtn_write_le32(block+60,4); hmbtn_write_le16(block+4,0x4000);
    CHECK(!hmbtn_event_block_open(&b,block,sizeof(block),error,sizeof(error)));

    /* Scripted walk, original half-duration wait, pause on a real message request,
     * then stop without pretending to have changed scenes. Synthetic bytecode. */
    step(bytes,0x3D,255,255,0,0,0,-3888,5000);
    step(bytes+16,0x3E,3,2,0,0,0,4918,0);
    step(bytes+32,0xF,1,1,1,0,0,4,0);
    step(bytes+48,0x40,255,255,0,1,0,0,0);
    step(bytes+64,0x15,255,255,0,0,0,0,0);
    step(bytes+80,3,6,5,1,4,0,0,0);
    step(bytes+96,2,7,6,1,1,123,-1,0);
    step(bytes+112,8,255,7,1,230,1,0,0);
    step(bytes+128,255,0,0,0,0,0,0,0);
    HmbtnFarmIntro intro;
    CHECK(hmbtn_farm_intro_open(&intro,bytes,sizeof(bytes),error,sizeof(error)));
    CHECK(hmbtn_farm_intro_brightness(&intro)==0);
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.player.position[2]==4959);
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.player.position[2]==4918);
    for (unsigned i=0;i<4;++i) CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)));
    CHECK(intro.script.pc==6 && intro.slots[0]==4 && intro.player.facing==1);
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.message==123);
    for (unsigned i=0;i<8;++i) CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)));
    CHECK(intro.script.pc==6 && intro.player.position[2]==4918 && intro.slots[0]==4);
    intro.message_done=true;
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.message==0 && intro.script.pc==7);
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && !intro.script.boundary && intro.area_pending && intro.target_area==230);
    CHECK(!intro.script.finished && intro.script.pc==7);
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.script.pc==7);
    intro.area = 230; intro.area_pending = false; /* Host completed the real resource load. */
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.script.pc==8);
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.script.finished);
    step(bytes,4,255,255,0,17,0,0,32760);
    step(bytes+16,1,2,1,1,17,2,32767,3);
    step(bytes+32,255,0,0,0,0,0,0,0);
    CHECK(hmbtn_farm_intro_open(&intro,bytes,48,error,sizeof(error)));
    CHECK(!hmbtn_farm_intro_tick(&intro,error,sizeof(error)));
    CHECK(strstr(error,"world bounds")!=NULL);
    /* Childhood uses several actors, a faster walk and camera follow. */
    step(bytes,4,255,255,0,2,0,-3200,5000);
    step(bytes+16,0x33,255,255,0,2,0,0,0);
    step(bytes+32,1,3,2,1,2,0,4952,1);
    step(bytes+48,6,255,255,0,2,1,0,0);
    step(bytes+64,0x32,255,255,1,118,0,0,0);
    step(bytes+80,255,0,0,0,0,0,0,0);
    CHECK(hmbtn_farm_intro_open(&intro,bytes,96,error,sizeof(error)));
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.childhood[1].position[2]==4976);
    CHECK(intro.camera_actor==2 && intro.camera[2]==4976);
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.childhood[1].position[2]==4952);
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.script.pc==3);
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.event_pending && intro.queued_event==118);
    CHECK(!intro.childhood[1].visible && intro.script.pc==4 && !intro.script.finished);
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.script.pc==4);
    /* Action records name a player mode/animation, not an ANI frame. */
    step(bytes,0x3F,255,255,1,0,71,2,11);
    step(bytes+16,255,0,0,0,0,0,0,0);
    CHECK(hmbtn_farm_intro_open(&intro,bytes,32,error,sizeof(error)));
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.action_pending && intro.script.pc==0);
    intro.action_ready=true; intro.action_pending=false;
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.action_active && intro.script.pc==1);
    CHECK(intro.player.sprite.animation==2 && intro.player.sprite.frame==0);
    intro.action_finished=true;
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && !intro.player.pose && !intro.action_active);
    /* Song cues wait for actual host playback; the camera keeps its manual
     * centre during the original 177 interpolation steps. */
    step(bytes,0x17,255,255,1,4,0,1,0);
    step(bytes+16,0x17,2,1,1,4,0,0,0);
    step(bytes+32,0x6D,3,2,1,7,8,0,0);
    step(bytes+48,255,0,0,0,0,0,0,0);
    CHECK(hmbtn_farm_intro_open(&intro,bytes,64,error,sizeof(error)));
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.xa_pending && intro.script.pc==0);
    intro.xa_pending=false; intro.xa_started=intro.xa_active=true;
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.script.pc==1);
    for(unsigned i=0;i<3;++i) CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.script.pc==1);
    intro.xa_active=false;
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.script.pc==2);
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.slots[7]==1);
    for(unsigned i=0;i<177;++i) CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)));
    CHECK(intro.slots[7]==2 && intro.slots[8]==177 && intro.camera_gliding);
    CHECK(intro.camera_height==-820 && intro.camera_distance==-10094 && intro.camera[1]==-995);
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.script.pc==3);
    /* Actor 33 and its marker are distinct from 27; the mode's real song must
     * be loaded before selecting it. */
    step(bytes,4,255,255,0,33,170,-10751,4317);
    step(bytes+16,0x59,255,255,0,33,0,0,0);
    step(bytes+32,0x18,255,2,1,34,0,0,0);
    step(bytes+48,0x19,255,255,1,0,0,0,0);
    step(bytes+64,255,0,0,0,0,0,0,0);
    CHECK(hmbtn_farm_intro_open(&intro,bytes,80,error,sizeof(error)));
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.song_pending && intro.script.pc==2);
    CHECK(intro.girl.marker==1 && intro.girl.position[1]==170 && intro.boy.position[1]==0);
    intro.music_mode=34; intro.song_pending=false;
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.script.pc==3);
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.start_music);
    /* House loading holds the cursor before repositioning; the fade completes
     * before the original finish request. Finishing does not enable gameplay. */
    step(bytes,8,255,0,0,-2,0,0,0);
    step(bytes+16,0x3D,255,255,0,2,0,0,0);
    step(bytes+32,0x40,255,255,0,0,0,0,0);
    step(bytes+48,0x1B,255,255,1,1,10,1,0);
    step(bytes+64,0x1B,255,4,1,1,0,0,0);
    step(bytes+80,0x25,255,5,1,0,0,0,0);
    step(bytes+96,255,0,0,0,0,0,0,0);
    CHECK(hmbtn_farm_intro_open(&intro,bytes,112,error,sizeof(error)));
    intro.event=121; intro.event_active=true;
    intro.player.position[0]=-556; intro.player.position[2]=-3024;
    for(unsigned i=0;i<3;++i) {
        CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.area_pending);
        CHECK(intro.target_area==80 && intro.script.pc==0 && !intro.finish_requested);
        CHECK(intro.player.position[0]==-556 && intro.player.position[2]==-3024);
    }
    intro.area=80; intro.area_pending=false;
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.script.pc==4);
    CHECK(intro.player.position[0]==-90 && intro.player.position[1]==0 && intro.player.position[2]==541);
    CHECK(intro.camera[0]==-90 && intro.camera[2]==541 && intro.player.facing==0);
    for(unsigned i=0;i<9;++i) {
        CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && !intro.finish_requested);
    }
    CHECK(hmbtn_farm_intro_brightness(&intro)==255);
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.finish_requested && !intro.event_active);
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.script.finished && !intro.script.boundary);
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.player.position[0]==-90);
    /* A house spawn elsewhere must fail instead of teleporting to guessed data. */
    CHECK(hmbtn_farm_intro_open(&intro,bytes+16,96,error,sizeof(error)));
    CHECK(!hmbtn_farm_intro_tick(&intro,error,sizeof(error)));
    /* Calls and runtime returns leave the immutable bytecode untouched. */
    step(bytes,0x4F,3,255,0,5,0,0,0);
    step(bytes+16,0x15,2,255,1,6,0,77,0);
    step(bytes+32,255,0,0,0,0,0,0,0);
    step(bytes+48,0x15,255,255,0,7,0,88,0);
    step(bytes+64,0x50,4,4,0,5,0,0,0);
    uint8_t immutable[80];memcpy(immutable,bytes,sizeof(immutable));
    CHECK(hmbtn_farm_intro_open(&intro,bytes,80,error,sizeof(error)));
    intro.event=12;
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)));
    CHECK(intro.script.pc==2 && intro.slots[5]==1 && intro.slots[6]==77 && intro.slots[7]==88);
    CHECK(memcmp(bytes,immutable,sizeof(immutable))==0);
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.script.finished);
    CHECK(hmbtn_farm_intro_open(&intro,bytes,80,error,sizeof(error)));
    intro.script.pc=4;intro.slots[5]=80;
    CHECK(!hmbtn_farm_intro_tick(&intro,error,sizeof(error)));
    /* Wait on a bank halfway through one conversation, without reopening it. */
    step(bytes,2,255,255,0,1,70,-1,0);
    step(bytes+16,0x3B,255,1,1,105,0,0,0);
    step(bytes+32,2,255,2,1,1,70,-1,0);
    step(bytes+48,255,0,0,0,0,0,0,0);
    CHECK(hmbtn_farm_intro_open(&intro,bytes,64,error,sizeof(error)));
    intro.event=12;intro.text_bank=142;
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.message==70 && intro.script.pc==1);
    for(unsigned i=0;i<5;++i) CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.script.pc==1);
    intro.text_bank=105;
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.script.pc==2 && intro.message==70);
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.script.pc==2);
    intro.message_done=true;
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.script.pc==3 && !intro.message);
    /* Acknowledge targets only after the host loads the actual scene. */
    const unsigned tour_areas[]={0,32,36,40,48,52};
    for(unsigned i=0;i<sizeof(tour_areas)/sizeof(tour_areas[0]);++i) {
        step(bytes,8,255,0,0,(int)tour_areas[i],0,0,0);
        step(bytes+16,255,0,0,0,0,0,0,0);
        CHECK(hmbtn_farm_intro_open(&intro,bytes,32,error,sizeof(error)));
        intro.event=12;intro.area=80;
        CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.area_pending && intro.script.pc==0);
        CHECK(intro.target_area==tour_areas[i] && !intro.script.boundary);
        intro.area=tour_areas[i];intro.area_pending=false;
        CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.script.finished);
    }
    /* Zack's cut saves/restores (y,x,z); Thomas stops by his previous speed. */
    step(bytes,0x49,255,255,0,41,1,2,3);
    step(bytes+16,0x3D,255,255,0,0,192,-5023,1450);
    step(bytes+32,0x3D,255,255,0,1,1,2,3);
    step(bytes+48,0x53,255,255,0,17,0,0,2);
    step(bytes+64,0x67,255,255,0,41,0,0,26);
    step(bytes+80,255,0,0,0,0,0,0,0);
    CHECK(hmbtn_farm_intro_open(&intro,bytes,96,error,sizeof(error)));
    intro.event=12;intro.player.position[0]=-4257;intro.player.position[1]=10;intro.player.position[2]=3959;
    intro.mayor.velocity[2]=40;intro.zack.position[0]=-3888;intro.zack.position[2]=7895;
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)) && intro.script.finished);
    CHECK(intro.slots[1]==10 && intro.slots[2]==-4257 && intro.slots[3]==3959);
    CHECK(intro.player.position[0]==-4257 && intro.player.position[1]==10 && intro.player.position[2]==3959);
    CHECK(intro.player.facing==2 && intro.mayor.sprite.frame==2 && !intro.mayor.velocity[2]);
    /* The last step from the house points north; standing must face Thomas. */
    CHECK(hmbtn_farm_intro_open(&intro,bytes,96,error,sizeof(error)));
    intro.visit_approach=true;
    intro.player.position[0]=-4257;intro.player.position[2]=4000;intro.player.facing=0;
    intro.mayor.position[0]=-3888;intro.mayor.position[2]=3959;
    CHECK(hmbtn_farm_intro_tick(&intro,error,sizeof(error)));
    CHECK(!intro.visit_approach && intro.player.position[2]==3959 && intro.player.facing==3);
    CHECK(intro.player.sprite.animation==0 && intro.player.sprite.frame==3);
    puts("Bounded events, native movement, camera, resource handshakes and farmhouse handoff passed.");
    return 0;
}
