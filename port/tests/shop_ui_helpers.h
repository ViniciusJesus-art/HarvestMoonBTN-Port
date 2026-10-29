#ifndef HMBTN_TEST_SHOP_UI_HELPERS_H
#define HMBTN_TEST_SHOP_UI_HELPERS_H
/* Check actual shop pixels against text_draw_cursor's original +6 Y offset.
 * Horizontal anchors are the native layouts' existing insets. */
static bool shop_cursor_pixels(HmbtnScenePreview *s,int x,int row_y,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);HmbtnNewGameUI *ui=hmbtn_scene_preview_resources(s).ui;
    uint8_t *actual=calloc(1,FRAME_BYTES),*cursor=calloc(1,FRAME_BYTES);
    if(!actual || !cursor) { free(actual);free(cursor);return false; }
    bool ok=hmbtn_feed_shop_ui_draw(&g->feed_shop,&g->game.economy,&g->game.registration,ui,actual,g->runtime.ticks,error,size)
        && hmbtn_ui_icon(ui,cursor,11,1,x,row_y+6,error,size);
    unsigned pixels=0;
    if(ok) for(unsigned i=0;i<FRAME_BYTES;i+=4) if(cursor[i+3]) {
        ++pixels;if(memcmp(actual+i,cursor+i,4)) ok=false;
    }
    free(actual);free(cursor);
    REQUIRE(ok && pixels>0);return true;
}
#endif
