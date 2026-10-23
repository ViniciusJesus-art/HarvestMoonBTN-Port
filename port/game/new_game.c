/* Host adaptation of title.c states 11..28, menu_window.c and name_entry.c.
 * Resource text is decoded as LE halfwords. No serialized pointers are used. */
#include "new_game.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
#define FAIL(msg) do { snprintf(error,error_size,"%s",msg); return false; } while (0)

void hmbtn_new_game_init(HmbtnNewGame *g)
{
    memset(g, 0, sizeof(*g));
    g->step = 11; g->sound = -1;
    for (unsigned i = 0; i <= HMBTN_NAME_CHARS; ++i)
        g->name[i] = g->player_name[i] = g->farm_name[i] = g->dog_name[i] = 0xffff;
}
bool hmbtn_message_open(HmbtnMessage *m, const uint8_t *record, size_t size,
    char *error, size_t error_size)
{
    if (!record || size != 8192) FAIL("Invalid message record size");
    memset(m, 0, sizeof(*m));
    m->face_bank=m->bank=hmbtn_read_le32(record);
    unsigned lines = 0;
    bool page_leading = false;
    for (unsigned i = 0; i < HMBTN_MESSAGE_WORDS; ++i) {
        const uint16_t c = hmbtn_read_le16(record + 0x1800 + i * 2);
        if(c==0xff1) m->scroll_hold=true;
        m->words[m->count++] = c;
        if (c == 0xffff) { m->phase = HMBTN_MESSAGE_OPENING; return true; }
        if (c == 0xffc || c == 0xffb) { lines = 0; page_leading = true; }
        if (c == 0xfffe) {
            /* WAIT_PAGE consumes the first line break after FFB. It is a
             * separator before the new page, not an extra displayed line. */
            if (!page_leading && ++lines >= 4 && !m->scroll_hold) FAIL("Scrolling text beyond four lines is not ported in this slice");
            page_leading = false;
        }
        if (c < 256 || c == 0xffd) page_leading = false;
        if (c < 256 || c == 0xfffe || c == 0xffd || c == 0xffc || c == 0xffb
                || c == 0xffa || c == 0xff1 || c == 0xff2 || c == 0xff3 || c == 0xff7 || c == 0xff4 || c == 0xff6 || c == 0xff5
                || (c >= 0xe25 && c <= 0xe2f)
                || (c >= 0xe38 && c <= 0xfef)) continue;
        FAIL("Unsupported message command in new-game slice");
    }
    FAIL("Message terminator missing");
}
/* text.c's fixed/no-scroll page and choice-list path presents a complete page,
 * without typing or blips. Used by editor help and confirmation prompts. */
static void fixed_page(HmbtnMessage *m)
{
    if (m->words[m->page] == 0xffa) {
        m->choices = 2; m->cursor = m->page+1;
        if (m->words[m->cursor] == 0xfffe) ++m->cursor;
        m->page = m->cursor;
    }
    while (m->cursor < m->count) {
        const uint16_t c = m->words[m->cursor++];
        if (c == 0xff4) m->face = true;
        if (c == 0xff6) m->face = false;
        if (c >= 0xe38 && c <= 0xfef) m->face_bank=0xfefu-c;
        if (c == 0xff7) m->bank=m->face_bank;
        if (c == 0xffff) { m->phase = HMBTN_MESSAGE_WAIT_END; break; }
        if (c == 0xffc) { m->phase = HMBTN_MESSAGE_WAIT_PAGE; break; }
    }
}
bool hmbtn_new_game_name_editor(const HmbtnNewGame *g)
{
    return g->step == 15 || g->step == 21 || g->step == 24;
}
int hmbtn_message_tick(HmbtnMessage *m, uint32_t pressed, uint32_t held)
{
    int sound = -1;
    switch (m->phase) {
    case HMBTN_MESSAGE_SCROLLING:
        m->scroll_y+=(held&(HMBTN_PAD_CROSS|HMBTN_PAD_CIRCLE)) ? 2u : 1u;
        if(m->scroll_y>=14) {
            while(m->page<m->cursor && m->words[m->page++]!=0xfffe) {}
            m->scroll_y=0; if(m->lines) --m->lines;
            m->phase=HMBTN_MESSAGE_PRINTING;
        }
        break;
    case HMBTN_MESSAGE_OPENING:
        if (++m->box_frame >= 18) {
            m->phase = HMBTN_MESSAGE_PRINTING; m->timer = 0;
            if (m->words[0] == 0xff3 || m->words[0] == 0xffa) {
                m->no_scroll = true; fixed_page(m);
            }
        }
        break;
    case HMBTN_MESSAGE_CLOSING:
        if (++m->box_frame >= 12) m->phase = HMBTN_MESSAGE_CLOSED;
        break;
    case HMBTN_MESSAGE_PRINTING: {
        if (m->wait_frames) { --m->wait_frames; break; }
        /* Default text speed is 3 VBlanks; VSync(2) crosses it every 2 ticks.
         * Held Cross/Circle uses interval zero: still one entry per game frame. */
        const unsigned interval = held & (HMBTN_PAD_CROSS | HMBTN_PAD_CIRCLE) ? 1 : 2;
        if (++m->timer < interval) break;
        m->timer = 0;
        if (m->cursor >= m->count) break;
        const uint16_t code = m->words[m->cursor++];
        if (code == 0xffff) m->phase = HMBTN_MESSAGE_WAIT_END;
        else if (code == 0xffc) m->phase = HMBTN_MESSAGE_WAIT_PAGE;
        else if (code == 0xff4) m->face = true;
        else if (code == 0xff6) m->face = false;
        else if (code >= 0xe38 && code <= 0xfef) m->face_bank=0xfefu-code;
        else if (code == 0xff7) m->bank=m->face_bank;
        else if (code == 0xff3) m->no_scroll = true;
        else if (code == 0xffa) { m->no_scroll=true;m->page=m->cursor-1;fixed_page(m); }
        else if(code==0xfffe && m->scroll_hold && ++m->lines>=4) m->phase=HMBTN_MESSAGE_SCROLLING;
        else if (code >= 0xe25 && code <= 0xe2f) m->wait_frames = (0xe2f-code+1)/2;
        /* The source alternates its text blip per typing timer. */
        m->blip = !m->blip;
        if (!m->blip) sound = 4;
        break;
    }
    case HMBTN_MESSAGE_WAIT_PAGE:
        if (pressed & (HMBTN_PAD_CROSS | HMBTN_PAD_CIRCLE)) {
            if (m->cursor < m->count && m->words[m->cursor] == 0xffb) ++m->cursor;
            if (m->cursor < m->count && m->words[m->cursor] == 0xfffe) ++m->cursor;
            m->page = m->cursor; m->phase = HMBTN_MESSAGE_PRINTING;
            if (m->no_scroll) fixed_page(m);
        }
        break;
    case HMBTN_MESSAGE_WAIT_END:
        if (pressed & (HMBTN_PAD_CROSS | HMBTN_PAD_CIRCLE)) {
            m->phase = HMBTN_MESSAGE_CLOSING; m->box_frame = 0;
        }
        break;
    default: break;
    }
    return sound;
}
static void request(HmbtnNewGame *g, unsigned id)
{
    g->request = id;
}
static void queue(HmbtnNewGame *g, unsigned id)
{
    g->queued = id;
    g->message.phase = HMBTN_MESSAGE_CLOSING; g->message.box_frame = 0;
}
static unsigned move_cell(unsigned cell, uint32_t button, unsigned grid)
{
    static const int row_last[2][6] = {{13,13,12,10,9,13},{13,13,7,0,0,13}};
    static const int col_last[2][14] = {{5,4,4,4,4,5,4,4,4,4,3,2,2,5},{2,2,2,2,2,2,2,2,1,1,0,0,0,5}};
    int col = (int)(cell % 14), row = (int)(cell / 14);
    const bool horizontal = button == HMBTN_PAD_LEFT || button == HMBTN_PAD_RIGHT;
    const bool forward = button == HMBTN_PAD_DOWN || button == HMBTN_PAD_RIGHT;
    if (!horizontal && button != HMBTN_PAD_UP && button != HMBTN_PAD_DOWN) return cell;
    int *axis = horizontal ? &col : &row;
    const int last = horizontal ? row_last[grid][row] : col_last[grid][col];
    *axis += forward ? 1 : -1;
    if (*axis < 0) *axis = last;
    if (*axis > last) *axis = 0;
    if (!horizontal) {
        if (col == 13 && row >= 2 && row < 5) row = forward ? 5 : 1;
    } else if (grid == 0 && row == 5) {
        if (col >= 1 && col < 5) col = forward ? 5 : 0;
        if (col >= 6 && col < 13) col = forward ? 13 : 5;
    } else if (grid == 1) {
        if (row < 2 && col >= 10 && col < 13) col = forward ? 13 : 9;
        if (row == 5) col = 13;
    }
    return (unsigned)(col + row * 14);
}
static uint16_t key_glyph(unsigned cell, unsigned grid)
{
    static const uint16_t symbols[28] = {0x63,0x62,0x64,0x6c,0x8d,0x83,0x84,0x85,0x82,0x86,
        0x68,0x69,0x8e,0x6a,0x6b,0x6d,0x6e,0x70,0x6f,0x77,0x76,0x74,0x71,0x66,0x67,0x80,0x81,0x8b};
    const unsigned row = cell / 14, col = cell % 14;
    if (grid) return symbols[row * 10 + col];
    return (uint16_t)(col < 5 ? row * 5 + col : col < 10 ? 0x20 + row * 5 + col - 5 : 0x40 + row * 3 + col - 10);
}

static void start_name(HmbtnNewGame *g)
{
    for (unsigned i=0;i<=HMBTN_NAME_CHARS;++i) g->name[i]=0xffff;
    g->length=g->cell=g->grid=g->choice=0;g->dirty=g->confirming=false;
    request(g,0x3a8);
}
static void name_tick(HmbtnNewGame *g,uint32_t pressed,uint32_t triggered)
{
    if (g->message.phase != HMBTN_MESSAGE_WAIT_END || g->queued) return;
    if (g->confirming) {
        if (pressed == HMBTN_PAD_CROSS && g->choice == 0) {
            uint16_t *dest=g->step==15?g->player_name:g->step==21?g->farm_name:g->dog_name;
            memcpy(dest,g->name,sizeof(g->name));++g->step;g->sound=0;
            g->message.phase=HMBTN_MESSAGE_CLOSING;g->message.box_frame=0;
        } else if (pressed==HMBTN_PAD_CROSS || pressed==HMBTN_PAD_CIRCLE) {
            g->confirming=false;g->sound=1;queue(g,0x3a8);
        } else if (triggered & (HMBTN_PAD_UP|HMBTN_PAD_DOWN)) {g->choice^=1;g->sound=5;}
    } else if (pressed==HMBTN_PAD_START || (pressed==HMBTN_PAD_CROSS && g->cell==83)) {
        if (!g->length || !g->dirty) g->sound=2;
        else {g->confirming=true;g->choice=0;g->sound=0;queue(g,0x3a6);}
    } else if (pressed==HMBTN_PAD_CIRCLE) {
        if (g->length) {g->name[--g->length]=0xffff;g->sound=1;}
        else if (g->step==15) g->sound=2;
        else {
            g->step=g->step==21?16:19;g->sound=1;
            g->message.phase=HMBTN_MESSAGE_CLOSING;g->message.box_frame=0;
        }
    } else if (pressed==HMBTN_PAD_CROSS) {
        g->sound=0;
        if (g->cell==13) {g->grid^=1;g->sound=5;}
        else if (g->length<HMBTN_NAME_CHARS) {
            g->name[g->length++]=g->cell==27?0xffd:key_glyph(g->cell,g->grid);
            if (g->cell!=27) g->dirty=true;
            g->name[g->length]=0xffff;
        }
    } else if (pressed==(HMBTN_PAD_L1|HMBTN_PAD_L2) || pressed==(HMBTN_PAD_R1|HMBTN_PAD_R2)) {
        g->grid^=1;g->sound=5;
    } else {
        const unsigned cell=move_cell(g->cell,triggered,g->grid);
        if(cell!=g->cell) {g->cell=cell;g->sound=5;}
    }
}
static void birthday_tick(HmbtnNewGame *g,uint32_t pressed,uint32_t triggered)
{
    if (g->message.phase!=HMBTN_MESSAGE_WAIT_END || g->queued) return;
    if (g->birthday_phase==3) {
        if (pressed==HMBTN_PAD_CROSS && g->choice==0) {
            g->step=19;g->sound=0;g->message.phase=HMBTN_MESSAGE_CLOSING;g->message.box_frame=0;
        } else if (pressed==HMBTN_PAD_CROSS || pressed==HMBTN_PAD_CIRCLE) {
            g->birthday_phase=2;g->birthday_value=g->day;g->sound=1;queue(g,0x3a9);
        } else if (triggered & (HMBTN_PAD_UP|HMBTN_PAD_DOWN)) {g->choice^=1;g->sound=5;}
        return;
    }
    if (pressed==HMBTN_PAD_CROSS) {
        g->sound=0;
        if (g->birthday_phase==1) {
            g->season=g->birthday_value;g->birthday_value=0;g->birthday_phase=2;
        } else {
            g->day=g->birthday_value;g->birthday_value=0;g->birthday_phase=3;g->choice=0;queue(g,0x3a7);
        }
        return;
    }
    if (pressed==HMBTN_PAD_CIRCLE) {
        g->sound=1;
        if (g->birthday_phase==2) {g->birthday_phase=1;g->birthday_value=g->season;}
        else {g->step=13;g->message.phase=HMBTN_MESSAGE_CLOSING;g->message.box_frame=0;}
        return;
    }
    int value=(int)g->birthday_value;
    if (g->birthday_phase==1) {
        if (triggered==HMBTN_PAD_LEFT) value=(value+3)%4;
        if (triggered==HMBTN_PAD_RIGHT) value=(value+1)%4;
    } else {
        switch (triggered & (HMBTN_PAD_UP|HMBTN_PAD_DOWN|HMBTN_PAD_LEFT|HMBTN_PAD_RIGHT)) {
        case HMBTN_PAD_UP: value-=7;if(value<0) {value+=28;if(value<23)value+=7;} break;
        case HMBTN_PAD_DOWN: value+=7;if(value>=30) {value-=28;if(value>=7)value-=7;} break;
        case HMBTN_PAD_LEFT: value=(value+29)%30;break;
        case HMBTN_PAD_RIGHT: value=(value+1)%30;break;
        default:break;
        }
    }
    if ((unsigned)value!=g->birthday_value) {g->birthday_value=(unsigned)value;g->sound=5;}
}
void hmbtn_new_game_tick(HmbtnNewGame *g,uint32_t pressed,uint32_t held)
{
    g->sound=-1;
    uint32_t triggered=pressed;
    if(held==g->previous_held) {
        if(!g->repeating) {
            if(++g->repeat_frames>20) {g->repeating=true;g->repeat_frames=0;}
        } else if(++g->repeat_frames%7==0) triggered|=held;
    } else {g->repeat_frames=0;g->repeating=false;}
    g->previous_held=held;
    const bool controlled=hmbtn_new_game_name_editor(g) || g->step==18 || g->step==26;
    g->sound = hmbtn_message_tick(&g->message,controlled?0:pressed,controlled?0:held);
    if(g->queued && g->message.phase==HMBTN_MESSAGE_CLOSED) {
        request(g,g->queued);g->queued=0;return;
    }
    switch(g->step) {
    case 11:request(g,0x3a2);g->step=12;break;
    case 12:if(g->message.phase==HMBTN_MESSAGE_CLOSED) g->step=13;break;
    case 13:if(g->message.phase==HMBTN_MESSAGE_CLOSED) {request(g,0x397);g->step=14;} break;
    case 14:if(g->message.phase==HMBTN_MESSAGE_CLOSED) {start_name(g);g->step=15;} break;
    case 15:case 21:case 24:name_tick(g,pressed,triggered);break;
    case 16:if(g->message.phase==HMBTN_MESSAGE_CLOSED) {request(g,0x3a5);g->step=17;} break;
    case 17:if(g->message.phase==HMBTN_MESSAGE_CLOSED) {
        g->birthday_phase=1;g->birthday_value=0;g->choice=0;request(g,0x3a9);g->step=18;
    } break;
    case 18:birthday_tick(g,pressed,triggered);break;
    case 19:if(g->message.phase==HMBTN_MESSAGE_CLOSED) {request(g,0x3a4);g->step=20;} break;
    case 20:if(g->message.phase==HMBTN_MESSAGE_CLOSED) {start_name(g);g->step=21;} break;
    case 22:if(g->message.phase==HMBTN_MESSAGE_CLOSED) {request(g,0x3a3);g->step=23;} break;
    case 23:if(g->message.phase==HMBTN_MESSAGE_CLOSED) {start_name(g);g->step=24;} break;
    case 25:if(g->message.phase==HMBTN_MESSAGE_CLOSED) {g->choice=0;request(g,0x3aa);g->step=26;} break;
    case 26:
        if(g->message.phase!=HMBTN_MESSAGE_WAIT_END) break;
        if(triggered & (HMBTN_PAD_UP|HMBTN_PAD_DOWN)) {g->choice^=1;g->sound=5;}
        else if(pressed & HMBTN_PAD_CROSS) {
            g->sound=0;g->message.phase=HMBTN_MESSAGE_CLOSING;g->message.box_frame=0;
            if(g->choice==0) {g->step=27;g->fade=0;} else g->step=13;
        }
        break;
    case 27:if(++g->fade>=10) g->step=28;break;
    case 28:g->finished=true;g->step=100;break;
    default:break;
    }
}
bool hmbtn_message_box(const HmbtnMessage *m, int *x, int *y, int *w, int *h)
{
    *x = 26; *y = 164; *w = 267; *h = 62;
    if (m->phase == HMBTN_MESSAGE_CLOSED) return false;
    if (m->phase != HMBTN_MESSAGE_OPENING && m->phase != HMBTN_MESSAGE_CLOSING) return true;
    const int duration = m->phase == HMBTN_MESSAGE_OPENING ? 12 : 6;
    int t = m->phase == HMBTN_MESSAGE_OPENING ? (int)m->box_frame : duration + 6 - (int)m->box_frame;
    if (t < 6) {
        *w = 2 * (((267 << 11) / 6 * t) / 4096); *h = 4;
        *x = 26 + 267/2 - *w/2; *y = 164 + 62/2;
    } else {
        *h = 2 * (((62 << 11) / duration * (t-6)) / 4096);
        if (*h < 4) *h = 4;
        *y = 164 + 62/2 - *h/2;
    }
    return *w > 0;
}
