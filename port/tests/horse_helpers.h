/* Shared driver for installed Barley acquisition scripts, no manufactured horse. */
static bool encounter(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,unsigned event,bool decline,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);bool seen=false,selected=false,named=false,cancel=false;
    for(unsigned t=0;t<9000;++t) {
        if(g->visit.active) { REQUIRE(g->visit.event==event);seen=true; }
        uint32_t pad=g->intro.message ? HMBTN_PAD_CROSS : 0;
        if(g->animal_name_active) {
            HmbtnNewGame *e=&g->animal_name;pad=0;named=true;
            if(e->message.phase==HMBTN_MESSAGE_WAIT_END && !e->queued) {
                if(!cancel && !e->length) { pad=HMBTN_PAD_CIRCLE;cancel=true; }
                else if(!e->length) pad=HMBTN_PAD_CROSS;
                else pad=e->confirming ? HMBTN_PAD_CROSS : HMBTN_PAD_START;
            }
        } else if(g->intro.message && g->dialogue.message.choices && g->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END) {
            if(!selected) {
                REQUIRE(capture(s,host,shots,event==1 ? "barley-ranch-offer" : "barley-farm-offer",error,size));
                selected=true;if(decline) pad=HMBTN_PAD_DOWN;
            }
        }
        if(seen && selected && ((event==1 && g->visit.free && g->intro.script.pc>=36) || (!g->visit.active && g->control.enabled))) {
            REQUIRE(g->horse.owned==!decline && named==!decline);return true;
        }
        REQUIRE(step(s,pad,error,size) && !g->intro.script.boundary);
        if(pad==HMBTN_PAD_CIRCLE) REQUIRE(!g->animal_name.finished);
    }
    fprintf(stderr,"Horse event stuck event=%u pc=%u msg=%u name=%d free=%d\n",g->visit.event,g->intro.script.pc,g->intro.message,g->animal_name_active,g->visit.free);
    REQUIRE(false);return false;
}
