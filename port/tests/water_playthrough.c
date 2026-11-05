#include "poultry_helpers.h"
static uint64_t texture_hash(HmbtnScenePreview *s)
{
    const uint8_t *section=hmbtn_scene_preview_resources(s).section;const HmbtnImageAtlas *a=hmbtn_scene_preview_resources(s).atlas;
    uint64_t hash=1469598103934665603ULL;
    for(unsigned i=0;i<8;++i) {
        const uint8_t *p=section+0x8a8+i*18;if(hmbtn_read_le16(p)==65535) continue;
        unsigned x=hmbtn_read_le16(p+6),y=hmbtn_read_le16(p+8),w=hmbtn_read_le16(p+14),h=hmbtn_read_le16(p+16);
        for(unsigned r=0;r<h;++r) for(unsigned c=0;c<w;++c) { hash^=a->words[(y+r)*1024+x+c];hash*=1099511628211ULL; }
    }
    return hash;
}
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) return 2;
    char error[512]={0};HmbtnHost host={0};HmbtnScenePreview *s=NULL;CHECK(hmbtn_host_open(&host));
    const unsigned maps[]={0,56,60,68,72};
    for(unsigned n=0;n<5;++n) {
        HmbtnFarmClock date={0,0,4,4,12,0,0};s=hmbtn_scene_area_create_at(host.renderer,argv[1],maps[n],&date,error,sizeof(error));CHECK(s);
        HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.weather.today=0;
        for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
        if(maps[n]==56) {
            /* Look across the sea, not at the beach entrance stairs. */
            const int32_t ocean[3]={-2000,960,-7000};
            memcpy(g->intro.player.position,ocean,sizeof(ocean));
            memcpy(g->intro.camera,ocean,sizeof(ocean));
        }
        const uint64_t first=texture_hash(s);bool changed=false;
        char name[64];snprintf(name,sizeof(name),"water-%u",maps[n]);
        CHECK(capture(s,&host,argc==3 ? argv[2] : NULL,name,error,sizeof(error)));
        if(maps[n]==56) {
            /* The lower-left sea sample exposes the original blue clear
             * color directly. Black clear made the translucent sea dark. */
            SDL_Surface *surface=SDL_RenderReadPixels(host.renderer,NULL);CHECK(surface);
            uint8_t red,green,blue,alpha;
            bool read=SDL_ReadSurfacePixel(surface,10*surface->w/320,230*surface->h/240,&red,&green,&blue,&alpha);
            SDL_DestroySurface(surface);
            CHECK(read && !red && green>=99 && green<=165 && blue>=127 && blue<=213 && alpha==255);
        }
        for(unsigned i=0;i<48;++i) { CHECK(step(s,0,error,sizeof(error)));if(texture_hash(s)!=first) changed=true; }
        CHECK(changed);
        /* Draw does not animate textures; the same logical tick is stable. */
        const uint64_t stable=texture_hash(s);CHECK(capture(s,&host,NULL,"draw-only",error,sizeof(error)) && texture_hash(s)==stable);
        g->menu.mode=HMBTN_MENU_STATUS;
        for(unsigned i=0;i<12;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(texture_hash(s)==stable);g->menu.mode=HMBTN_MENU_NONE;
        fprintf(stderr,"Animated water map %u passed\n",maps[n]);hmbtn_scene_preview_destroy(s);s=NULL;
    }
    puts("Installed water: farm stream/pond, sea, mountain lake, river and waterfall/spring changed; draw stable and menu paused");
    hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}
