#include "session.h"
#include <stdlib.h>
#include <string.h>

HmbtnSession *hmbtn_session_create(const uint16_t *layout,size_t count)
{
    HmbtnSession *s=calloc(1,sizeof(*s));
    if(!s) return NULL;
    hmbtn_game_state_init(&s->game);
    hmbtn_farm_field_init(&s->field,layout,count);
    /* Preserve zeroed poultry and the pre-handoff village. The village's
     * existing defaults are installed only when its schedules arrive. */
    return s;
}
bool hmbtn_session_bind_schedules(HmbtnSession *s,const uint8_t *data,size_t size)
{
    if(!s || !data || !size) return false;
    if(!s->village_initialized) {
        hmbtn_village_init(&s->village,data,size);s->village_initialized=true;
    } else {
        s->village.schedule_data=data;s->village.schedule_size=size;
    }
    s->village_ready=true;
    return true;
}
void hmbtn_session_unbind_resources(HmbtnSession *s)
{
    if(!s) return;
    s->village.schedule_data=NULL;s->village.schedule_size=0;s->village_ready=false;
    s->village.player=NULL;s->village.event_actor=NULL;s->village.event_actor_id=0;
    memset(s->village.event_actors,0,sizeof(s->village.event_actors));
}
void hmbtn_session_destroy(HmbtnSession *s)
{
    if(!s) return;
    hmbtn_session_unbind_resources(s);
    free(s);
}
