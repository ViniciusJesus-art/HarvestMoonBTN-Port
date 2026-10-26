#include "services.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool matches(HmbtnServiceRequest a,HmbtnServiceRequest b)
{
    return a.kind==b.kind && a.id==b.id && a.bank==b.bank && a.actor==b.actor && a.cursor==b.cursor;
}
static bool valid(HmbtnServiceChannel c,HmbtnServiceKind k)
{
    switch(c) {
    case HMBTN_SERVICE_RESOURCE:return k>=HMBTN_SERVICE_EVENT_DATA && k<=HMBTN_SERVICE_TEXT_DATA;
    case HMBTN_SERVICE_MESSAGE:return k==HMBTN_SERVICE_SHOW_MESSAGE;
    case HMBTN_SERVICE_ANIMATION:return k==HMBTN_SERVICE_WAIT_ANIMATION;
    case HMBTN_SERVICE_ACTION:return k==HMBTN_SERVICE_WAIT_ACTION;
    case HMBTN_SERVICE_AUDIO:return k>=HMBTN_SERVICE_LOAD_SONG && k<=HMBTN_SERVICE_WAIT_XA;
    default:return false;
    }
}
static HmbtnServiceResult *pending(HmbtnServices *s,uint64_t ticket)
{
    if(s && ticket) for(unsigned i=0;i<HMBTN_SERVICE_CHANNELS;++i)
        if(s->channels[i].ticket==ticket && s->channels[i].status==HMBTN_SERVICE_PENDING) return &s->channels[i];
    return NULL;
}
uint64_t hmbtn_service_begin(HmbtnServices *s,HmbtnServiceChannel c,HmbtnServiceRequest request)
{
    if(!s || !valid(c,request.kind)) return 0;
    HmbtnServiceResult *r=&s->channels[c];
    if(r->status!=HMBTN_SERVICE_IDLE) return matches(r->request,request) ? r->ticket : 0;
    if(s->sequence==UINT64_MAX) return 0; /* Never reuse an identity. */
    *r=(HmbtnServiceResult){.ticket=++s->sequence,.request=request,.status=HMBTN_SERVICE_PENDING};
    return r->ticket;
}
const HmbtnServiceResult *hmbtn_service_poll(const HmbtnServices *s,HmbtnServiceChannel c)
{
    return s && c>=0 && c<HMBTN_SERVICE_CHANNELS ? &s->channels[c] : NULL;
}
const HmbtnServiceResult *hmbtn_service_claim(HmbtnServices *s,HmbtnServiceChannel c)
{
    if(!s || c<0 || c>=HMBTN_SERVICE_CHANNELS) return NULL;
    HmbtnServiceResult *r=&s->channels[c];
    if(r->status!=HMBTN_SERVICE_PENDING || r->claimed) return NULL;
    r->claimed=true;return r;
}
bool hmbtn_service_complete(HmbtnServices *s,uint64_t ticket,uint32_t value)
{
    HmbtnServiceResult *r=pending(s,ticket);
    if(!r || !r->claimed || r->request.kind<=HMBTN_SERVICE_TEXT_DATA) return false;
    r->value=value;r->status=HMBTN_SERVICE_DONE;return true;
}
bool hmbtn_service_fail(HmbtnServices *s,uint64_t ticket,const char *error)
{
    HmbtnServiceResult *r=pending(s,ticket);if(!r) return false;
    snprintf(r->error,sizeof(r->error),"%s",error ? error : "Service failed");
    r->status=HMBTN_SERVICE_FAILED;return true;
}
bool hmbtn_service_complete_data(HmbtnServices *s,uint64_t ticket,const uint8_t *data,size_t bytes,uint32_t value)
{
    HmbtnServiceResult *r=pending(s,ticket);
    if(!r || !r->claimed || r->request.kind>HMBTN_SERVICE_TEXT_DATA) return false;
    if(!data || !bytes || bytes>HMBTN_SERVICE_MAX_BYTES) return hmbtn_service_fail(s,ticket,"Invalid service resource buffer");
    uint8_t *copy=malloc(bytes);
    if(!copy) return hmbtn_service_fail(s,ticket,"Cannot allocate service resource result");
    memcpy(copy,data,bytes);r->data=copy;r->bytes=bytes;r->value=value;r->status=HMBTN_SERVICE_DONE;return true;
}
bool hmbtn_service_cancel(HmbtnServices *s,uint64_t ticket)
{
    HmbtnServiceResult *r=pending(s,ticket);if(!r) return false;
    r->status=HMBTN_SERVICE_CANCELLED;return true;
}
bool hmbtn_service_ack(HmbtnServices *s,uint64_t ticket)
{
    if(!s || !ticket) return false;
    for(unsigned i=0;i<HMBTN_SERVICE_CHANNELS;++i) {
        HmbtnServiceResult *r=&s->channels[i];
        if(r->ticket!=ticket || r->status==HMBTN_SERVICE_PENDING || r->status==HMBTN_SERVICE_IDLE) continue;
        free(r->data);*r=(HmbtnServiceResult){0};return true;
    }
    return false;
}
void hmbtn_services_rebind(HmbtnServices *s)
{
    if(!s) return;
    for(unsigned i=0;i<HMBTN_SERVICE_CHANNELS;++i) {
        HmbtnServiceResult *r=&s->channels[i];
        if(r->status!=HMBTN_SERVICE_PENDING) continue;
        if(s->sequence==UINT64_MAX) { hmbtn_service_fail(s,r->ticket,"Service identity exhausted");continue; }
        r->ticket=++s->sequence;r->claimed=false;
    }
}
void hmbtn_services_clear(HmbtnServices *s)
{
    if(!s) return;
    for(unsigned i=0;i<HMBTN_SERVICE_CHANNELS;++i) {
        free(s->channels[i].data);s->channels[i]=(HmbtnServiceResult){0};
    }
}
