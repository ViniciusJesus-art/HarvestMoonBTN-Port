/* shop_result_update: original short Yes/No panel at (32,32). The message
 * itself is rendered by the common dialogue UI, using mine_messages. */
#include "mine_ui.h"
#include "window_frame.h"
#include "feed_shop_labels.inc"
bool hmbtn_mine_ui_draw(const HmbtnMine *m,HmbtnNewGameUI *ui,uint8_t *pixels,char *error,size_t size)
{
    if(!m->prompt || m->message.phase!=HMBTN_MESSAGE_WAIT_END) return true;
    hmbtn_window_frame_draw(pixels,320,240,32,32,44,36);
    return hmbtn_ui_text(ui,pixels,shop_yes,sizeof(shop_yes)/2,42,36,0,3,error,size)
        && hmbtn_ui_text(ui,pixels,shop_no,sizeof(shop_no)/2,42,50,0,3,error,size)
        && hmbtn_ui_icon(ui,pixels,11,1,34,42+(int)m->answer*14,error,size);
}
