#ifndef HMBTN_GODDESS_H
#define HMBTN_GODDESS_H
#include "event_script.h"
struct HmbtnSession;
typedef struct HmbtnGoddess { unsigned cue,cue_ticks;bool cue_active; } HmbtnGoddess;
bool hmbtn_goddess_event(unsigned);
bool hmbtn_goddess_bind(struct HmbtnSession *,unsigned,char *,size_t);
HmbtnEventResult hmbtn_goddess_command(struct HmbtnSession *,const HmbtnEventCommand *,char *,size_t);
#endif
