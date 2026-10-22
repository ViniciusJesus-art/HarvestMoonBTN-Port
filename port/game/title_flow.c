#include "title_flow.h"
#include <string.h>

static void reset_counters(HmbtnTitleFlow *f)
{
    f->logo_frames = 0;
    f->menu_row = 1;
    f->slide = -160;
    f->drop = 48;
    f->wipe = 320;
}

void hmbtn_title_flow_init(HmbtnTitleFlow *f)
{
    memset(f, 0, sizeof(*f));
    reset_counters(f);
    f->step = HMBTN_TITLE_PUBLISHER_IN;
}

/* title_logo_begin/wait/slide_in/drop/wipe_out/blink/wipe_end/reset.
 * Capture draw coordinates before advancing, as the original functions do. */
static void logo_tick(HmbtnTitleFlow *f)
{
    f->visible = 0;
    f->draw_slide = f->draw_drop = f->draw_wipe = 0;
    switch (f->logo_step) {
    case HMBTN_LOGO_BEGIN:
        f->logo_frames = 0;
        f->logo_step = HMBTN_LOGO_WAIT;
        break;
    case HMBTN_LOGO_WAIT:
        if (f->logo_frames >= 30) f->logo_step = HMBTN_LOGO_SLIDE;
        break;
    case HMBTN_LOGO_SLIDE:
        f->visible = 1;
        f->draw_slide = f->slide;
        if (++f->slide >= 0) f->logo_step = HMBTN_LOGO_DROP;
        break;
    case HMBTN_LOGO_DROP:
        f->visible = 3;
        f->draw_drop = f->drop;
        if (--f->drop <= 0) f->logo_step = HMBTN_LOGO_WIPE_IN;
        break;
    case HMBTN_LOGO_WIPE_IN:
        f->visible = 7;
        f->draw_wipe = f->wipe;
        f->wipe -= 2;
        if (f->wipe <= 0) {
            f->logo_frames = 0;
            f->logo_step = HMBTN_LOGO_BLINK;
        }
        break;
    case HMBTN_LOGO_BLINK:
        f->visible = f->logo_frames % 30 < 15 ? 7 : 3;
        if (f->logo_frames >= 90) f->logo_step = HMBTN_LOGO_WIPE_OUT;
        break;
    case HMBTN_LOGO_WIPE_OUT:
        f->visible = 7;
        f->draw_wipe = f->wipe;
        f->wipe -= 2;
        if (f->wipe <= -160) f->logo_step = HMBTN_LOGO_RESET;
        break;
    case HMBTN_LOGO_RESET:
        f->visible = 3;
        f->wipe = 320;
        f->logo_step = HMBTN_LOGO_WIPE_IN;
        break;
    }
}

/* Host adaptation of title_sequence_step(), cases 0..9. Host loading has no
 * fabricated CD delay: publisher fades in/out once resources are ready. */
void hmbtn_title_flow_tick(HmbtnTitleFlow *f, uint32_t pressed)
{
    f->sound = HMBTN_TITLE_SOUND_NONE;
    if (f->step == HMBTN_TITLE_PUBLISHER_IN) {
        if (++f->brightness >= HMBTN_TITLE_PUBLISHER_FADE_TICKS)
            f->step = HMBTN_TITLE_PUBLISHER_OUT;
        return;
    }
    if (f->step == HMBTN_TITLE_PUBLISHER_OUT) {
        if (--f->brightness == 0) f->step = HMBTN_TITLE_INTRO_FADE_IN;
        return;
    }
    f->logo_frames = (f->logo_frames + 1) % 108000;
    if (f->step <= HMBTN_TITLE_INTRO_FADE_OUT) logo_tick(f);
    else {
        f->visible = 7;
        f->draw_slide = f->draw_drop = f->draw_wipe = 0;
    }
    switch (f->step) {
    case HMBTN_TITLE_INTRO_FADE_IN:
        f->brightness = 0;
        f->step = HMBTN_TITLE_INTRO_FADE_WAIT;
        break;
    case HMBTN_TITLE_INTRO_FADE_WAIT:
        if (++f->brightness >= HMBTN_TITLE_FADE_TICKS) {
            f->idle_frames = 0;
            f->step = HMBTN_TITLE_INTRO;
        }
        break;
    case HMBTN_TITLE_INTRO:
        ++f->idle_frames;
        if (pressed & (HMBTN_PAD_START | HMBTN_PAD_CROSS)) {
            f->sound = HMBTN_TITLE_SOUND_CONFIRM;
            f->step = HMBTN_TITLE_INTRO_FADE_OUT;
        } else if (f->idle_frames >= HMBTN_TITLE_IDLE_TICKS) {
            f->destination = HMBTN_TITLE_DESTINATION_DEMO;
            f->step = HMBTN_TITLE_UNAVAILABLE;
        }
        break;
    case HMBTN_TITLE_INTRO_FADE_OUT:
        if (f->brightness) --f->brightness;
        if (!f->brightness) {
            reset_counters(f);
            f->step = HMBTN_TITLE_SCENE_FADE_IN;
        }
        break;
    case HMBTN_TITLE_SCENE_FADE_IN:
        f->brightness = 0;
        f->step = HMBTN_TITLE_SCENE_FADE_WAIT;
        break;
    case HMBTN_TITLE_SCENE_FADE_WAIT:
        if (++f->brightness >= HMBTN_TITLE_FADE_TICKS) {
            f->logo_frames = 0;
            f->step = HMBTN_TITLE_START_WAIT;
        }
        break;
    case HMBTN_TITLE_START_WAIT:
        if (f->logo_frames >= HMBTN_TITLE_IDLE_TICKS) {
            f->destination = HMBTN_TITLE_DESTINATION_DEMO;
            f->step = HMBTN_TITLE_UNAVAILABLE;
        }
        if (pressed & HMBTN_PAD_START) {
            f->sound = HMBTN_TITLE_SOUND_CONFIRM;
            reset_counters(f);
            f->destination = HMBTN_TITLE_DESTINATION_NONE;
            f->step = HMBTN_TITLE_MENU;
        }
        break;
    case HMBTN_TITLE_MENU:
        if (pressed & (HMBTN_PAD_UP | HMBTN_PAD_DOWN | HMBTN_PAD_SELECT)) {
            f->sound = HMBTN_TITLE_SOUND_CURSOR;
            f->menu_row = (uint16_t)((f->menu_row + 1u) % 2);
        } else if (pressed & (HMBTN_PAD_START | HMBTN_PAD_CROSS)) {
            f->sound = HMBTN_TITLE_SOUND_CONFIRM;
            f->step = f->menu_row == 0
                ? HMBTN_TITLE_NEW_GAME : HMBTN_TITLE_CONTINUE_FADE_WAIT;
        }
        break;
    case HMBTN_TITLE_NEW_GAME:
        f->step = HMBTN_TITLE_NEW_GAME_TEXT_OPEN;
        break;
    case HMBTN_TITLE_CONTINUE_FADE_WAIT:
        if (f->brightness) --f->brightness;
        if (!f->brightness) {
            f->destination = HMBTN_TITLE_DESTINATION_CONTINUE;
            f->step = HMBTN_TITLE_UNAVAILABLE;
        }
        break;
    case HMBTN_TITLE_UNAVAILABLE:
        /* Diagnostic stays in the terminal; no host UI over game pixels. */
        if (pressed & (HMBTN_PAD_START | HMBTN_PAD_CROSS | HMBTN_PAD_CIRCLE)) {
            f->destination = HMBTN_TITLE_DESTINATION_NONE;
            f->brightness = HMBTN_TITLE_FADE_TICKS;
            f->step = HMBTN_TITLE_MENU;
        }
        break;
    default: break;
    }
}
