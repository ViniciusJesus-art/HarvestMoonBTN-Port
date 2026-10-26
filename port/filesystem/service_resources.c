#include "service_resources.h"
#include "event_resources.h"
#include "action_resources.h"
#include "text_resources.h"
#include <stdlib.h>
void hmbtn_resource_service(const char *root,HmbtnServices *services)
{
    const HmbtnServiceResult *r=hmbtn_service_claim(services,HMBTN_SERVICE_RESOURCE);
    if(!r) return;
    const uint64_t ticket=r->ticket;const HmbtnServiceRequest q=r->request;
    HmbtnEventResourceRange event={0};HmbtnActionResourceRange action={0};HmbtnTextResourceRange text={0};
    size_t bytes=0;uint32_t value=0;
    switch(q.kind) {
    case HMBTN_SERVICE_EVENT_DATA:case HMBTN_SERVICE_TRIGGER_DATA:
        if(hmbtn_event_resource_locate(q.kind==HMBTN_SERVICE_EVENT_DATA ? HMBTN_EVENT_RESOURCE_SCRIPT : HMBTN_EVENT_RESOURCE_TRIGGERS,q.id,&event)) bytes=event.bytes;
        break;
    case HMBTN_SERVICE_ACTION_DATA:
        if(hmbtn_action_resource_locate(q.id,&action)) { bytes=action.bytes;value=(uint32_t)action.animation_bytes; }
        break;
    case HMBTN_SERVICE_TEXT_DATA:
        if(hmbtn_text_resource_locate((HmbtnTextResourceId){HMBTN_TEXT_RECORD,q.bank,q.id},&text)) bytes=text.size;
        break;
    default:break;
    }
    if(!bytes || bytes>HMBTN_SERVICE_MAX_BYTES) { hmbtn_service_fail(services,ticket,"Invalid logical resource request");return; }
    uint8_t *data=malloc(bytes);char error[192]={0};bool ok=false;
    if(!data) { hmbtn_service_fail(services,ticket,"Cannot allocate host resource buffer");return; }
    switch(q.kind) {
    case HMBTN_SERVICE_EVENT_DATA:case HMBTN_SERVICE_TRIGGER_DATA:
        ok=hmbtn_event_resource_read(root,q.kind==HMBTN_SERVICE_EVENT_DATA ? HMBTN_EVENT_RESOURCE_SCRIPT : HMBTN_EVENT_RESOURCE_TRIGGERS,q.id,data,bytes,error,sizeof(error));break;
    case HMBTN_SERVICE_ACTION_DATA:ok=hmbtn_action_resource_read(root,q.id,data,bytes,&action,error,sizeof(error));break;
    case HMBTN_SERVICE_TEXT_DATA:ok=hmbtn_text_resource_read(root,(HmbtnTextResourceId){HMBTN_TEXT_RECORD,q.bank,q.id},data,bytes,error,sizeof(error));break;
    default:break;
    }
    if(ok) hmbtn_service_complete_data(services,ticket,data,bytes,value);
    else hmbtn_service_fail(services,ticket,error);
    free(data); /* Services copied successful bytes; failures expose no partial data. */
}
