#include "services.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Services line %d: %s\n",__LINE__,#x);goto fail; } } while(0)
int main(void)
{
    HmbtnServices s={0},other={0};uint8_t source[16]={1,2,3};
    HmbtnServiceRequest resource={HMBTN_SERVICE_EVENT_DATA,116,0,0,0};
    uint64_t ticket=hmbtn_service_begin(&s,HMBTN_SERVICE_RESOURCE,resource);CHECK(ticket);
    CHECK(hmbtn_service_begin(&s,HMBTN_SERVICE_RESOURCE,resource)==ticket);
    resource.id=117;CHECK(!hmbtn_service_begin(&s,HMBTN_SERVICE_RESOURCE,resource));resource.id=116;
    CHECK(!hmbtn_service_complete_data(&s,ticket,source,sizeof(source),0)); /* Must be claimed. */
    CHECK(hmbtn_service_claim(&s,HMBTN_SERVICE_RESOURCE));
    for(unsigned i=0;i<10;++i) {
        CHECK(!hmbtn_service_claim(&s,HMBTN_SERVICE_RESOURCE));
        CHECK(hmbtn_service_poll(&s,HMBTN_SERVICE_RESOURCE)->status==HMBTN_SERVICE_PENDING);
        CHECK(!hmbtn_service_ack(&s,ticket));
    }
    CHECK(!hmbtn_service_complete(&s,ticket,0)); /* Resource cannot publish void success. */
    CHECK(hmbtn_service_complete_data(&s,ticket,source,sizeof(source),7));
    memset(source,0,sizeof(source));
    const HmbtnServiceResult *r=hmbtn_service_poll(&s,HMBTN_SERVICE_RESOURCE);
    CHECK(r->status==HMBTN_SERVICE_DONE && r->bytes==16 && r->data[0]==1 && r->value==7);
    CHECK(!hmbtn_service_complete_data(&s,ticket,source,sizeof(source),0));
    CHECK(!hmbtn_service_fail(&s,ticket,"late failure"));
    hmbtn_services_rebind(&s);CHECK(r->ticket==ticket && r->data[0]==1);
    CHECK(hmbtn_service_ack(&s,ticket) && !hmbtn_service_ack(&s,ticket));
    CHECK(!hmbtn_service_complete_data(&s,ticket,source,sizeof(source),0));
    const HmbtnServiceRequest requests[]={
        {HMBTN_SERVICE_SHOW_MESSAGE,7,1,0,12},
        {HMBTN_SERVICE_WAIT_ANIMATION,2,0,17,13},
        {HMBTN_SERVICE_WAIT_ACTION,0,0,41,14},
        {HMBTN_SERVICE_START_XA,4,0,0,15}};
    const HmbtnServiceChannel channels[]={HMBTN_SERVICE_MESSAGE,HMBTN_SERVICE_ANIMATION,HMBTN_SERVICE_ACTION,HMBTN_SERVICE_AUDIO};
    for(unsigned i=0;i<4;++i) {
        ticket=hmbtn_service_begin(&s,channels[i],requests[i]);CHECK(ticket);
        CHECK(hmbtn_service_claim(&s,channels[i]));
        CHECK(hmbtn_service_complete(&s,ticket,2));
        CHECK(!hmbtn_service_complete(&s,ticket,3));
        CHECK(hmbtn_service_poll(&s,channels[i])->value==2);
    }
    hmbtn_services_clear(&s);
    for(unsigned i=0;i<4;++i) {
        ticket=hmbtn_service_begin(&s,channels[i],requests[i]);CHECK(ticket);
        CHECK(hmbtn_service_claim(&s,channels[i]));
        CHECK(hmbtn_service_fail(&s,ticket,"injected provider failure"));
        CHECK(hmbtn_service_poll(&s,channels[i])->status==HMBTN_SERVICE_FAILED);
        CHECK(!hmbtn_service_complete(&s,ticket,0));
        CHECK(hmbtn_service_ack(&s,ticket));
    }
    CHECK(hmbtn_service_begin(&other,HMBTN_SERVICE_RESOURCE,resource));
    CHECK(hmbtn_service_poll(&other,HMBTN_SERVICE_RESOURCE)->status==HMBTN_SERVICE_PENDING);
    hmbtn_services_clear(&s);
    CHECK(hmbtn_service_poll(&other,HMBTN_SERVICE_RESOURCE)->status==HMBTN_SERVICE_PENDING);
    ticket=hmbtn_service_begin(&s,HMBTN_SERVICE_RESOURCE,resource);CHECK(ticket);
    CHECK(hmbtn_service_claim(&s,HMBTN_SERVICE_RESOURCE));
    hmbtn_services_rebind(&s);
    const uint64_t replacement=hmbtn_service_poll(&s,HMBTN_SERVICE_RESOURCE)->ticket;
    CHECK(replacement!=ticket && !hmbtn_service_complete_data(&s,ticket,source,sizeof(source),0));
    CHECK(hmbtn_service_claim(&s,HMBTN_SERVICE_RESOURCE));
    CHECK(hmbtn_service_cancel(&s,replacement));
    CHECK(!hmbtn_service_complete_data(&s,replacement,source,sizeof(source),0));
    CHECK(hmbtn_service_ack(&s,replacement));
    ticket=hmbtn_service_begin(&s,HMBTN_SERVICE_RESOURCE,resource);CHECK(ticket);
    CHECK(hmbtn_service_claim(&s,HMBTN_SERVICE_RESOURCE));
    CHECK(hmbtn_service_complete_data(&s,ticket,source,HMBTN_SERVICE_MAX_BYTES+1u,0));
    CHECK(hmbtn_service_poll(&s,HMBTN_SERVICE_RESOURCE)->status==HMBTN_SERVICE_FAILED);
    CHECK(!hmbtn_service_poll(&s,HMBTN_SERVICE_RESOURCE)->data);
    CHECK(hmbtn_service_ack(&s,ticket));
    CHECK(!hmbtn_service_begin(&s,HMBTN_SERVICE_AUDIO,resource));
    CHECK(!hmbtn_service_begin(NULL,HMBTN_SERVICE_RESOURCE,resource));
    CHECK(!hmbtn_service_poll(&s,(HmbtnServiceChannel)-1));
    CHECK(!hmbtn_service_claim(&s,HMBTN_SERVICE_CHANNELS));
    s.sequence=UINT64_MAX;CHECK(!hmbtn_service_begin(&s,HMBTN_SERVICE_RESOURCE,resource));
    hmbtn_services_clear(&other);hmbtn_services_clear(&s);hmbtn_services_clear(NULL);
    puts("Service identities, claim-once polling, immutable responses, owned buffers, errors, cancellation and rebind passed.");return 0;
fail:
    hmbtn_services_clear(&other);hmbtn_services_clear(&s);return 1;
}
