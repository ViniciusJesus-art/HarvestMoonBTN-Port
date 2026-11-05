#ifndef HMBTN_KAPPA_H
#define HMBTN_KAPPA_H
#include "event_script.h"
struct HmbtnSession;
bool hmbtn_kappa_bind(struct HmbtnSession *,unsigned,char *,size_t);
HmbtnEventResult hmbtn_kappa_command(struct HmbtnSession *,const HmbtnEventCommand *,char *,size_t);
#endif
