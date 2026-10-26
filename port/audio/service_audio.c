#include "service_audio.h"
void hmbtn_audio_service(const char *root,HmbtnTitleAudio *audio,HmbtnXAStream *xa,HmbtnServices *services)
{
    const HmbtnServiceResult *r=hmbtn_service_poll(services,HMBTN_SERVICE_AUDIO);
    if(!r || r->status!=HMBTN_SERVICE_PENDING) return;
    /* WAIT_XA claims once but observes completion on later ticks. */
    const bool claimed=hmbtn_service_claim(services,HMBTN_SERVICE_AUDIO)!=NULL;
    if(claimed && r->request.kind==HMBTN_SERVICE_WAIT_XA) audio->service_wait_ticket=r->ticket;
    if(!claimed && (r->request.kind!=HMBTN_SERVICE_WAIT_XA || audio->service_wait_ticket!=r->ticket)) return;
    const uint64_t ticket=r->ticket;char error[192]={0};bool ok=false;
    switch(r->request.kind) {
    case HMBTN_SERVICE_LOAD_SONG:
        hmbtn_title_audio_close(audio);
        ok=hmbtn_title_audio_open_song(audio,root,r->request.id,error,sizeof(error));
        if(ok) audio->xa=xa;
        break;
    case HMBTN_SERVICE_START_SONG:
        if(audio->sound) { hmbtn_sound_start(audio->sound);ok=true; }
        break;
    case HMBTN_SERVICE_START_XA:ok=hmbtn_xa_stream_start(xa,root,r->request.id,error,sizeof(error));break;
    case HMBTN_SERVICE_WAIT_XA:
        if(xa->active) return;
        ok=true;break;
    default:break;
    }
    if(ok) hmbtn_service_complete(services,ticket,0);
    else hmbtn_service_fail(services,ticket,error[0] ? error : "Invalid audio service request");
}
