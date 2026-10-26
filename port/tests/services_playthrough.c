/* Real native readers, ANI/UI/audio backends and an externally delayed request. */
#include "scene_fixture.h"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Services integration line %d: %s; %s\n",__LINE__,#x,error);goto fail; } } while(0)
static bool step(HmbtnScenePreview *s,char *error,size_t size)
{
    return hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){0,0},error,size);
}
int main(int argc,char **argv)
{
    if(argc!=2) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};uint8_t *bytes=NULL;
    CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,0,2,2,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(s);
    CHECK(step(s,error,sizeof(error)));
    /* An external provider claims the event request and completes it later.
     * The real coordinator must wait instead of loading or advancing twice. */
    HmbtnServices *services=&hmbtn_scene_preview_session(s)->services;
    uint64_t ticket=hmbtn_service_begin(services,HMBTN_SERVICE_RESOURCE,(HmbtnServiceRequest){HMBTN_SERVICE_EVENT_DATA,117,0,0,0});
    CHECK(ticket && hmbtn_service_claim(services,HMBTN_SERVICE_RESOURCE));
    hmbtn_scene_preview_session(s)->intro.event_pending=true;hmbtn_scene_preview_session(s)->intro.queued_event=117;
    const unsigned pc=hmbtn_scene_preview_session(s)->intro.script.pc,event=hmbtn_scene_preview_session(s)->intro.event;
    for(unsigned i=0;i<5;++i) CHECK(step(s,error,sizeof(error)) && hmbtn_scene_preview_session(s)->intro.event==event && hmbtn_scene_preview_session(s)->intro.script.pc==pc);
    HmbtnEventResourceRange range;CHECK(hmbtn_event_resource_locate(HMBTN_EVENT_RESOURCE_SCRIPT,117,&range));
    bytes=malloc(range.bytes);CHECK(bytes);
    CHECK(hmbtn_event_resource_read(argv[1],HMBTN_EVENT_RESOURCE_SCRIPT,117,bytes,range.bytes,error,sizeof(error)));
    CHECK(hmbtn_service_complete_data(services,ticket,bytes,range.bytes,0));
    memset(bytes,0,range.bytes);free(bytes);bytes=NULL;
    CHECK(step(s,error,sizeof(error)) && hmbtn_scene_preview_session(s)->intro.event==117 && !hmbtn_scene_preview_session(s)->intro.event_pending);
    CHECK(hmbtn_scene_preview_session(s)->intro.script.data==hmbtn_scene_preview_session(s)->event_code && !hmbtn_service_complete(services,ticket,0));
    /* Logical resource results match the established action/text readers. */
    uint8_t action[HMBTN_ACTION_RESOURCE_BYTES],direct[HMBTN_ACTION_RESOURCE_BYTES];uint32_t ani=0;HmbtnActionResourceRange ar;
    CHECK(test_read(s,HMBTN_SERVICE_ACTION_DATA,12,0,action,sizeof(action),&ani,error,sizeof(error)));
    CHECK(hmbtn_action_resource_read(argv[1],12,direct,sizeof(direct),&ar,error,sizeof(error)));
    CHECK(ani==ar.animation_bytes && !memcmp(action,direct,sizeof(action)));
    uint8_t record[HMBTN_TEXT_RECORD_BYTES],expected[HMBTN_TEXT_RECORD_BYTES];
    CHECK(test_read(s,HMBTN_SERVICE_TEXT_DATA,2041,1,record,sizeof(record),NULL,error,sizeof(error)));
    CHECK(hmbtn_text_resource_read(argv[1],(HmbtnTextResourceId){HMBTN_TEXT_RECORD,1,2041},expected,sizeof(expected),error,sizeof(error)));
    CHECK(!memcmp(record,expected,sizeof(record)));
    /* Polling completion does not reopen the audio backend or restart XA. */
    const HmbtnServiceResult *r=test_audio(s,HMBTN_SERVICE_LOAD_SONG,34,error,sizeof(error));CHECK(r && r->status==HMBTN_SERVICE_DONE);
    ticket=r->ticket;HmbtnSound *sound=(*hmbtn_scene_preview_resources(s).audio).sound;CHECK(sound);
    for(unsigned i=0;i<5;++i) CHECK(test_audio(s,HMBTN_SERVICE_LOAD_SONG,34,error,sizeof(error))->ticket==ticket && (*hmbtn_scene_preview_resources(s).audio).sound==sound);
    CHECK(hmbtn_service_ack(services,ticket));
    r=test_audio(s,HMBTN_SERVICE_START_XA,4,error,sizeof(error));CHECK(r && r->status==HMBTN_SERVICE_DONE);ticket=r->ticket;
    for(unsigned i=0;i<7;++i) hmbtn_xa_stream_tick(&(*hmbtn_scene_preview_resources(s).xa));
    const unsigned countdown=(*hmbtn_scene_preview_resources(s).xa).countdown,volume=(*hmbtn_scene_preview_resources(s).xa).volume;const uint32_t sector=(*hmbtn_scene_preview_resources(s).xa).sector;
    for(unsigned i=0;i<5;++i) CHECK(test_audio(s,HMBTN_SERVICE_START_XA,4,error,sizeof(error))->ticket==ticket);
    CHECK((*hmbtn_scene_preview_resources(s).xa).countdown==countdown && (*hmbtn_scene_preview_resources(s).xa).volume==volume && (*hmbtn_scene_preview_resources(s).xa).sector==sector);
    CHECK(hmbtn_service_ack(services,ticket));
    r=test_audio(s,HMBTN_SERVICE_WAIT_XA,4,error,sizeof(error));CHECK(r && r->status==HMBTN_SERVICE_PENDING);ticket=r->ticket;
    for(unsigned i=0;i<1000 && r->status==HMBTN_SERVICE_PENDING;++i) {
        hmbtn_xa_stream_tick(&(*hmbtn_scene_preview_resources(s).xa));hmbtn_audio_service(argv[1],&(*hmbtn_scene_preview_resources(s).audio),&(*hmbtn_scene_preview_resources(s).xa),services);
    }
    CHECK(r->status==HMBTN_SERVICE_DONE && !(*hmbtn_scene_preview_resources(s).xa).active && !hmbtn_service_complete(services,ticket,0));
    CHECK(hmbtn_service_ack(services,ticket));
    CHECK(!test_audio(s,HMBTN_SERVICE_LOAD_SONG,999,error,sizeof(error)));
    r=hmbtn_service_poll(services,HMBTN_SERVICE_AUDIO);CHECK(r->status==HMBTN_SERVICE_FAILED);
    CHECK(hmbtn_service_ack(services,r->ticket));error[0]=0;
    /* A failed read publishes no partial buffer, and polling is terminal. */
    ticket=hmbtn_service_begin(services,HMBTN_SERVICE_RESOURCE,(HmbtnServiceRequest){HMBTN_SERVICE_TEXT_DATA,0,1,0,0});CHECK(ticket);
    hmbtn_resource_service("/nonexistent-hmbtn-service-fixture",services);
    r=hmbtn_service_poll(services,HMBTN_SERVICE_RESOURCE);
    CHECK(r->status==HMBTN_SERVICE_FAILED && !r->data && !r->bytes);
    hmbtn_resource_service(argv[1],services);CHECK(r->status==HMBTN_SERVICE_FAILED);
    CHECK(hmbtn_scene_preview_session(s)->game.economy.gold==500 && !hmbtn_scene_preview_session(s)->game.inventory.held_item.id);
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Delayed real event, bounded text/action readers, audio/XA claim-once completion and terminal read/audio errors passed.");return 0;
fail:
    free(bytes);hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}
