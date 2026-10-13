/* Farm photo album and picture viewer, the map of the farm's spots that the
 * viewer shows, the ending track dialogue, and the result screen with its
 * top scores and item-choice messages. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include <memory.h>
#include "types.h"
#include "actor.h"
#include "afile.h"
#include "anim.h"
#include "cd_audio.h"
#include "clock.h"
#include "event_flags.h"
#include "event_rules.h"
#include "farm_main.h"
#include "farm_state.h"
#include "fishing.h"
#include "main.h"
#include "menu_window.h"
#include "pad.h"
#include "photo_album.h"
#include "player.h"
#include "pond.h"
#include "prim.h"
#include "scene.h"
#include "screen_fade.h"
#include "sound.h"
#include "sound_request.h"
#include "text.h"
#include "text_bank.h"
#include "texture_pack.h"
#include "ui_icons.h"
#include "villager.h"

/* The three message ids of the result screen's top scores. */
typedef struct ResultTopScores {
    u16 *firstText;
    u16 *secondText;
    u16 *extraText;
} ResultTopScores;

#define PHOTO_SCREEN_SAVE_OFFSET 0x12A90
#define PHOTO_PICTURE_BYTES 59392
#define PHOTO_PICTURE_A_FILE_OFFSET 0x3D17000
#define PHOTO_COUNTDOWN_FRAMES 30
#define PHOTO_FADE_FRAMES 30
#define MESSAGE_BACK_TO_FARM 0x10A
#define MESSAGE_SOLD_OUT 0x1E9
#define ENDING_TRACK_COUNT 10
#define NO_SCORE -1
#define NO_MESSAGE -1
#define AFFECTION_MAX 0xFFFF
#define ANIMAL_AFFECTION_MAX 255

/* A spot on the farm map: its position, and the spot reached in each of four directions. */
typedef struct FarmMapSpot {
    u16 x;
    s16 y;
    u8 next[4];
} FarmMapSpot;

/* The index of a FarmMapSpot's next[] entry: the arrow pressed. MAP_DIR_UP is the
 * default (no other arrow). */
enum FarmMapDirection {
    MAP_DIR_UP = 0,
    MAP_DIR_DOWN = 1,
    MAP_DIR_LEFT = 2,
    MAP_DIR_RIGHT = 3,
};

static void farm_map_tick(void);
static void photo_viewer_show(void);
static void photo_viewer_load_season(void);
static void photo_map_draw(void);
static s32 farm_map_spot_for_place(s16 id);
static void photo_load_picture(s16 index);
static s32 ending_track_step(s32 index);
static s32 ending_count_fish(void);
static void result_screen_run(void);
static void result_screen_init(void);
static void result_pick_top_scores(u16 **out);
static s32 item_slot_message(s16 kind);
static void villagers_add_friendship_all(s32 amount);
static void villagers_add_affection_all(s32 value);
static void item_banks_add(s32 value);
static void villager_add_friendship(s32 id, s32 amount);
static void villager_add_affection(CharacterId villager, s16 amount);
static void animal_add_affection(FarmActor *animal, s16 amount);

/* Header shared by the three picture sprites. */
static u8 photo_sprite_header[612];
static AnimState photo_sprites[3];
/* One-shot request: the track is taken only while the latch is clear, and its
 * argument is clamped to 0..9. */
static u32 ending_track_latch;
static s32 ending_track_arg;

static s16 photo_fade_state = 0;

static s16 photo_map_spot = 0;

static void photo_viewer_reset(void) {
    photo_fade_state = 0;
    photo_map_spot = 0;
}

/* Open the picture viewer from the farm: reset it and start the fade-out. */
void photo_viewer_start(void) {
    photo_viewer_reset();
    photo_fade_state = 1;
}

/* Redraw the full-screen picture: clear the 320x176 area and draw the
 * picture's first sprite at the screen centre. */
static inline void draw_picture(void) {
    window_frame_draw_at(&current_frame->otTags[0][26], 1, 0, 0, 320, 176);
    anim_draw_ft4_advance(&current_frame->otTags[0][25], &photo_sprites[0],
                  HWD0 / 2, VWD0 / 2);
}

/* One entry of the place table: a farm place and the map spot it shows on. */
typedef struct PhotoPlaceSpot {
    u8 place;
    u8 spot;
} PhotoPlaceSpot;

static PhotoPlaceSpot photo_id_pairs[68] = {
    {0x01, 0x00},
    {0x02, 0x00},
    {0x03, 0x00},
    {0x04, 0x00},
    {0x05, 0x00},
    {0x06, 0x00},
    {0x30, 0x01},
    {0x31, 0x01},
    {0x32, 0x01},
    {0x08, 0x02},
    {0x09, 0x02},
    {0x0A, 0x02},
    {0x22, 0x03},
    {0x23, 0x03},
    {0x24, 0x03},
    {0x25, 0x03},
    {0x26, 0x03},
    {0x14, 0x04},
    {0x15, 0x04},
    {0x16, 0x04},
    {0x17, 0x04},
    {0x18, 0x04},
    {0x19, 0x04},
    {0x1D, 0x05},
    {0x1E, 0x05},
    {0x1F, 0x05},
    {0x20, 0x05},
    {0x21, 0x05},
    {0x27, 0x06},
    {0x28, 0x06},
    {0x29, 0x06},
    {0x1A, 0x07},
    {0x1B, 0x07},
    {0x1C, 0x07},
    {0x2A, 0x08},
    {0x2C, 0x09},
    {0x2D, 0x09},
    {0x34, 0x0A},
    {0x35, 0x0A},
    {0x0C, 0x0B},
    {0x11, 0x0B},
    {0x44, 0x0B},
    {0x45, 0x0B},
    {0x46, 0x0B},
    {0x47, 0x0B},
    {0x48, 0x0B},
    {0x49, 0x0B},
    {0x4A, 0x0B},
    {0x4B, 0x0B},
    {0x4C, 0x0B},
    {0x4D, 0x0B},
    {0x0D, 0x0C},
    {0x12, 0x0C},
    {0x38, 0x0C},
    {0x39, 0x0C},
    {0x3A, 0x0C},
    {0x3B, 0x0C},
    {0x3C, 0x0C},
    {0x3D, 0x0C},
    {0x3E, 0x0C},
    {0x3F, 0x0C},
    {0x40, 0x0C},
    {0x41, 0x0C},
    {0x42, 0x0C},
    {0x0E, 0x0D},
    {0x0F, 0x0E},
    {0xFF, 0xFF},
    {0x00, 0x00},
};

static FarmMapSpot photo_map_spots[15] = {
    {137, 94, {3, 10, 11, 7}},
    {185, 121, {7, 2, 0, 8}},
    {210, 140, {8, 2, 10, 9}},
    {167, 67, {4, 0, 13, 5}},
    {197, 46, {4, 7, 3, 5}},
    {231, 62, {4, 8, 3, 6}},
    {266, 90, {5, 9, 7, 6}},
    {204, 85, {4, 1, 0, 6}},
    {234, 112, {5, 2, 1, 6}},
    {261, 134, {6, 9, 2, 9}},
    {140, 150, {0, 10, 11, 2}},
    {80, 100, {13, 10, 12, 0}},
    {48, 78, {13, 11, 12, 3}},
    {87, 52, {14, 11, 12, 3}},
    {109, 25, {14, 0, 13, 4}},
};

/* The screen area saved behind the picture viewer and restored after it. */
static RECT photo_screen_rect = {896, 256, 64, 256};

/* The picture viewer's state machine, stepped once per frame through
 * photo_fade_state. It fades out (1, 2), saves the screen once the transition
 * flag is set (3), sets up and fades in (4, 5), waits while the picture is shown
 * (8), fades out and restores the screen (9), and hands back to the farm (0xA).
 * 0xB and 0xC show the picture again from farm_overview_start. Any other nonzero
 * step runs farm_map_tick with text mode 3. */
void photo_viewer_step(void) {
    u32 palette;

    if (photo_fade_state == 1) {
        fade_start_out(10, 0, 2);
        photo_fade_state = 2;
    } else if (photo_fade_state == 2) {
        if (fade_is_settled() == 1) {
            photo_fade_state = 3;
            game_state.globalFlags |= GLOBAL_FLAG_TRANSITION_BEGIN;
        }
    } else if (photo_fade_state == 3) {
        if (game_state.globalFlags & GLOBAL_FLAG_TRANSITION) {
            StoreImage(&photo_screen_rect, (u_long *)(region_base + PHOTO_SCREEN_SAVE_OFFSET));
            DrawSync(0);
            photo_fade_state = 4;
        }
    } else if (photo_fade_state == 4) {
        photo_viewer_load_season();
        photo_fade_state = 5;
        fade_start_in(10, 0, 2);
    } else if (photo_fade_state == 5) {
        draw_picture();
        if (fade_is_settled() == 1) {
            photo_fade_state = 6;
        }
    } else if (photo_fade_state == 8) {
        draw_picture();
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            fade_start_out(10, 0, 2);
            photo_fade_state = 9;
        }
    } else if (photo_fade_state == 9) {
        draw_picture();
        if (fade_is_settled() == 1) {
            LoadImage(&photo_screen_rect, (u_long *)(region_base + PHOTO_SCREEN_SAVE_OFFSET));
            DrawSync(0);
            game_state.globalFlags |= GLOBAL_FLAG_TRANSITION_END;
            fade_start_in(10, 0, 2);
            photo_fade_state = 0xA;
        }
    } else if (photo_fade_state == 0xA) {
        if (fade_is_settled() == 1) {
            farm_unpause();
            photo_fade_state = 0;
        }
    } else if (photo_fade_state == 0xB) {
        draw_picture();
        if (fade_is_settled() == 1) {
            photo_fade_state = 0xC;
            farm_overview_start();
        }
    } else if (photo_fade_state == 0xC) {
        photo_viewer_load_season();
        photo_fade_state = 5;
    } else if (photo_fade_state != 0) {
        palette = text_get_palette();
        text_set_palette(3);
        farm_map_tick();
        text_set_palette(palette);
    }
}

/* The farm map screen: move the cursor between spots with the d-pad. Circle or Select
 * opens the picture, Triangle or Cross picks the highlighted spot, and Start leaves. */
static void farm_map_tick(void)
{
    s32 direction;
    s32 pressed;
    u32 dpad;

    text_set_spacing(0, 2);
    direction = MAP_DIR_UP;
    if (photo_fade_state != 7 && menu_phase_is_result() != 0) {
        dpad = input_pad1->triggered & PAD_ARROWS_MASK;
        if (dpad != 0) {
            switch (dpad) {
            case 0:
                break;
            case PAD_DOWN:
                direction = MAP_DIR_DOWN;
                break;
            case PAD_LEFT:
                direction = MAP_DIR_LEFT;
                break;
            case PAD_RIGHT:
                direction = MAP_DIR_RIGHT;
                break;
            }
            photo_map_spot = photo_map_spots[photo_map_spot].next[direction];
            sound_play(SOUND_MENU_CURSOR);
        }
    }
    photo_map_draw();
    if (photo_fade_state == 7) {
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            photo_fade_state = 6;
            menu_begin_close();
        }
    } else {
        menu_set_sound_enabled(0);
        text_set_input(0);
        if (farm_area.mode.place.location == PLACE_FARM) {
            menu_request_open(3, 0x3AB);
        } else {
            menu_request_open(3, 0x3B1);
        }
    }
    if (photo_fade_state != 7 && menu_phase_is_result() != 0) {
        pressed = input_pad1->pressed;
        if (pressed & (PAD_CIRCLE | PAD_SELECT)) {
            if (menu_phase_is_result() != 0) {
                photo_viewer_show();
            }
        } else if (pressed & (PAD_TRIANGLE | PAD_CROSS)) {
            sound_play(SOUND_MENU_CONFIRM);
            photo_fade_state = 7;
            menu_begin_close();
            menu_request_open(3, photo_map_spot + 0x385);
        } else if (pressed & PAD_START) {
            if (farm_area.mode.place.location == PLACE_FARM) {
                sound_play(SOUND_MENU_CONFIRM);
                fade_start_out(10, 0, 2);
                photo_fade_state = 0xB;
            } else {
                sound_play(SOUND_MENU_REFUSE);
            }
        }
    }
}

static void photo_viewer_show(void) {
    menu_begin_close();
    sound_play(SOUND_MENU_OPEN_CLOSE);
    photo_fade_state = 8;
}

static void photo_viewer_load_season(void) {
    sound_play(SOUND_MENU_OPEN_CLOSE);
    photo_viewer_reset();
    photo_fade_state = 6;
    photo_load_picture(game_state.progress.clock.date.season);
}

/* Draw the map screen: the frame and picture, the player's and the dog's markers
 * on their spots, and the cursor on the highlighted spot. */
static void photo_map_draw(void) {
    s32 spot;
    FrameState **state_address;
    state_address = &current_frame;
    window_frame_draw_at(&(*state_address)->otTags[0][26], 1, 0, 0, 320, 176);
    anim_draw_ft4_advance(&(*state_address)->otTags[0][25], &photo_sprites[0],
                  HWD0 / 2, VWD0 / 2);
    spot = farm_map_spot_for_place(game_state.player.place.location);
    anim_draw_ft4_advance(&(*state_address)->otTags[0][23], &photo_sprites[1],
                  photo_map_spots[spot].x - 16,
                  photo_map_spots[spot].y);
    spot = farm_map_spot_for_place(game_state.animals.dog.actor.place.location);
    anim_draw_ft4_advance(&(*state_address)->otTags[0][23], &photo_sprites[2],
                  photo_map_spots[spot].x + 16,
                  photo_map_spots[spot].y);
    spot = photo_map_spot;
    ui_draw_icon_0b_1(&(*state_address)->otTags[0][22], photo_map_spots[spot].x,
                  photo_map_spots[spot].y);
}

/* Find the map spot for a farm place: its entry in the second column of photo_id_pairs.
 * Returns -1 when the place has no spot. */
static s32 farm_map_spot_for_place(s16 id) {
    s32 i;

    for (i = 0; photo_id_pairs[i].place != 0xFF; i++) {
        if (id == photo_id_pairs[i].place) return photo_id_pairs[i].spot;
    }
    return -1;
}

/* Load picture `index` (clamped to 0..3) from the disc into the staging
 * buffer, keep its header and set up the three sprites on it. */
static void photo_load_picture(s16 index)
{
    s32 offset;

    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    index = index < 0 ? 0 : (index > 3 ? 3 : index);
    offset = index * PHOTO_PICTURE_BYTES + PHOTO_PICTURE_A_FILE_OFFSET;
    afile_read_bytes(0, offset, PHOTO_PICTURE_BYTES, STAGING_BUFFER, 1, 1);
    do {
    } while (afile_read_bytes(0, offset, PHOTO_PICTURE_BYTES, STAGING_BUFFER, 1, 0) == 0);
    texture_pack_load(STAGING_BUFFER + 1024);
    memcpy(photo_sprite_header, STAGING_BUFFER, sizeof(photo_sprite_header));
    anim_bind_file(&photo_sprites[0], (AnimFile *)photo_sprite_header);
    anim_bind_file(&photo_sprites[1], (AnimFile *)photo_sprite_header);
    anim_bind_file(&photo_sprites[2], (AnimFile *)photo_sprite_header);
    anim_select_range(&photo_sprites[0], 0);
    anim_select_range(&photo_sprites[1], 1);
    anim_select_range(&photo_sprites[2], 2);
    anim_set_frame(&photo_sprites[2], 1);
}

void ending_track_reset(void) {
    ending_track_latch = 0;
    ending_track_arg = 0;
}

void ending_track_request(s32 arg) {
    if (ending_track_latch == 0) {
        ending_track_latch = 1;
        if (arg < 0) arg = 0;
        if (arg >= ENDING_TRACK_COUNT) arg = ENDING_TRACK_COUNT - 1;
        ending_track_arg = arg;
    }
}

/* Show a requested ending track's text in place 0x15: the window opens for the
 * track picked by ending_track_step, and the latch clears once it has closed. */
void ending_track_tick(void) {
    s32 result;

    if (farm_area.mode.data->head.place.location != 0x15) {
        return;
    }
    switch (ending_track_latch) {
    case 0:
        break;
    case 1:
        result = ending_track_step(ending_track_arg);
        menu_request_open(2, result);
        ending_track_latch = 2;
        break;
    case 2:
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            ending_track_latch = 3;
        }
        break;
    case 3:
        ending_track_latch = 0;
        break;
    }
}

/* The message id each ending track's text starts at, by track index. */
static s16 track_message_offsets[10] = {
    0x0172, 0x0175, 0x0178, 0x017B, 0x017F, 0x01EA, 0x0182, 0x0188, 0x0190, 0x0194,
};

/* Advance dialogue track `index` from its current stage and the story flags.
 * Returns the stage plus the track's message offset. */
static s32 ending_track_step(s32 index)
{
    s32 stage;
    s32 offset;
    s32 season;
    u8 *next;
    s32 first;
    s32 second;
    u8 *countA;
    u8 *countB;

    stage = game_state.ending.tracks.stages[index];
    next = &game_state.ending.tracks.stages[index];
    offset = track_message_offsets[index];
    season = game_state.progress.clock.date.season;
    if (stage != 0) {
        switch (index) {
        case 0:
            if (event_flag_get(EVENT_FLAG_PLACE_48_VISITED) == 1) {
                if (stage == 2) {
                    *next = 1;
                } else {
                    *next = 2;
                }
                stage = *next;
            } else {
                *next = 1;
            }
            break;
        case 1:
            if (event_flag_get(EVENT_FLAG_MINE_VISITED) == 1) {
                if (stage == 2) {
                    *next = 1;
                } else {
                    *next = 2;
                }
                stage = *next;
            } else {
                *next = 1;
            }
            break;
        case 2:
            if (event_flag_get(EVENT_FLAG_65) == 1) {
                if (stage == 2) {
                    *next = 1;
                } else {
                    *next = 2;
                }
                stage = *next;
            } else {
                *next = 1;
                stage = 1;
            }
            break;
        case 3:
            if (event_flag_get(EVENT_FLAG_ENDING_TRACK_3_EVENT) == 1) {
                if (stage == 5) {
                    *next = 3;
                } else {
                    *next = stage + 1;
                }
                stage = 6 - *next;
            } else if (ending_count_fish() >= 10) {
                if (stage == 2) {
                    *next = 1;
                } else {
                    *next = 2;
                }
                stage = *next;
            } else {
                *next = 1;
            }
            break;
        case 4:
            if (stage == 2) {
                *next = 1;
            } else {
                *next = stage + 1;
            }
            break;
        case 5:
            if (stage == 5) {
                *next = 1;
            } else {
                *next = stage + 1;
            }
            break;
        case 6:
            if (season == SEASON_SPRING) {
                stage = 5;
            } else if (season == SEASON_FALL) {
                stage = 2;
            } else {
                switch (stage) {
                case 1:
                    *next = 3;
                    break;
                case 3:
                    *next = 4;
                    break;
                case 4:
                default:
                    *next = 1;
                    break;
                }
            }
            break;
        case 7:
            if (stage == 7) {
                *next = 1;
            } else {
                *next = stage + 1;
            }
            break;
        case 8:
            if (stage == 3) {
                *next = 1;
            } else {
                *next = stage + 1;
            }
            break;
        case 9:
            first = game_state.ending.tracks.visits[0];
            second = game_state.ending.tracks.visits[1];
            countA = &game_state.ending.tracks.visits[0];
            countB = &game_state.ending.tracks.visits[1];
            if (event_flag_get(EVENT_FLAG_13D) == 1 || event_flag_get(EVENT_FLAG_143) == 1) {
                if (first >= 2) {
                    if (stage == 4) {
                        *next = 3;
                    } else {
                        *next = 4;
                    }
                } else {
                    (*countA)++;
                    *next = 3;
                }
                stage = *next;
            } else if (game_state.villagers[CHARACTER_MARY].affection >= 55000) {
                if (second >= 2) {
                    if (stage == 6) {
                        *next = 5;
                    } else {
                        *next = 6;
                    }
                } else {
                    (*countB)++;
                    *next = 5;
                }
                stage = *next;
            } else if (stage == 2) {
                *next = 1;
            } else {
                *next = 2;
            }
            break;
        }
    } else {
        *next = stage + 1;
    }
    return stage + offset;
}

/* Count the pond's fish: the slots of pond.fish (POND_FISH of them) that hold one. */
static s32 ending_count_fish(void) {
    s32 count = 0;
    FarmFish *p = game_state.animals.pond.fish;
    s32 n = 98;
    do {
        if (p->item != 0) count++;
        p++;
        n--;
    } while (n >= 0);
    return count;
}

/* The steps of photo_sequence_step (result_screen_run). */
enum PhotoSequenceStep {
    PHOTO_SEQ_FADE_OUT = 1,         /* fades out the scene and its sound */
    PHOTO_SEQ_FADED = 2,            /* once faded and quiet, picks the top scores */
    PHOTO_SEQ_SCORES = 3,           /* shows the three picture texts and the choice */
    PHOTO_SEQ_RESULT = 4,           /* shows the chosen message; 0x10A goes straight back */
    PHOTO_SEQ_PICTURE = 5,          /* shows the picture, waits for the fade */
    PHOTO_SEQ_JINGLE = 6,           /* waits for the jingle (track 0x19) */
    PHOTO_SEQ_COUNTDOWN = 7,        /* counts 30 frames, then fades back */
    PHOTO_SEQ_FADE_IN = 8,          /* fades the farm back in */
    PHOTO_SEQ_DONE = 9,             /* hands over to result_screen_init */
};

static s16 photo_sequence_step = 0;

static ResultTopScores top_score_texts = {0, 0, 0};

static s16 top_score_indices[3] = {NO_SCORE, NO_SCORE, NO_SCORE};

static s16 item_choice_message = NO_MESSAGE;

static s16 photo_countdown = 0;

static u16 saved_status_latch = 0;

static void photo_sequence_reset(void) {
    photo_sequence_step = 0;
    top_score_texts.firstText = 0;
    top_score_texts.secondText = 0;
    top_score_texts.extraText = 0;
    top_score_indices[0] = NO_SCORE;
    top_score_indices[1] = NO_SCORE;
    top_score_indices[2] = NO_SCORE;
    item_choice_message = NO_MESSAGE;
    saved_status_latch = 0;
}

/* Start the photo sequence: clear it, hold the status latch until result_screen_init
 * restores it, and begin the fade-out. */
void photo_sequence_start(void) {
    s16 *p = &photo_sequence_step;
    u16 temp;

    photo_sequence_reset();
    temp = game_state.world.options.statusLatch;
    *p = PHOTO_SEQ_FADE_OUT;
    game_state.world.options.statusLatch = 0;
    saved_status_latch = temp;
}

void photo_sequence_tick(void) {
    s32 palette;
    if (photo_sequence_step != 0) {
        palette = text_get_palette();
        text_set_palette(3);
        result_screen_run();
        text_set_palette(palette);
    }
}

/* The farm photo and album sequence, a step machine on photo_sequence_step. It fades
 * in, reads the three picture texts (result_pick_top_scores), shows the album
 * message, and picks the result message with item_slot_message. Message 0x10A
 * returns straight to the farm. Otherwise it shows the picture, waits, plays the
 * jingle, counts 30 frames and fades back. */
static void result_screen_run(void)
{
    switch (photo_sequence_step) {
    case PHOTO_SEQ_FADE_OUT:
        sound_seq_stop_first(0);
        fade_start_out(PHOTO_FADE_FRAMES, 1, 2);
        sound_fade_out(PHOTO_FADE_FRAMES);
        photo_sequence_step = PHOTO_SEQ_FADED;
        break;
    case PHOTO_SEQ_FADED:
        if (fade_is_settled() == 1 && sound_fader_mode() == SOUND_FADER_IDLE) {
            photo_sequence_step = PHOTO_SEQ_SCORES;
            result_pick_top_scores(&top_score_texts.firstText);
            sound_fade_in(1);
        }
        break;
    case PHOTO_SEQ_SCORES:
        text_set_source(0, top_score_texts.firstText);
        text_set_source(1, top_score_texts.secondText);
        text_set_source(2, top_score_texts.extraText);
        menu_request_open(2, 0xFD);
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            item_choice_message = item_slot_message(menu_choice());
            photo_sequence_step = PHOTO_SEQ_RESULT;
        }
        break;
    case PHOTO_SEQ_RESULT:
        menu_request_open(2, item_choice_message);
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            if (item_choice_message == MESSAGE_BACK_TO_FARM) {
                sound_seq_start_reverb(0, 0, farm_area.mode.currentMode, 0);
                farm_set_message_timing();
                fade_start_in(10, 1, 2);
                sound_fade_in(10);
                photo_sequence_step = PHOTO_SEQ_FADE_IN;
            } else {
                prim_add_tile(current_frame->ot, 0, 0, HWD0, VWD0, 0, 0);
                fade_start_out(10, 0, 1);
                sound_fade_in(10);
                photo_sequence_step = PHOTO_SEQ_PICTURE;
            }
        }
        break;
    case PHOTO_SEQ_PICTURE:
        prim_add_tile(current_frame->ot, 0, 0, HWD0, VWD0, 0, 0);
        if (fade_is_settled() == 1 && sound_fader_mode() == SOUND_FADER_IDLE) {
            cd_audio_play_track(CD_TRACK_PHOTO_JINGLE);
            photo_sequence_step = PHOTO_SEQ_JINGLE;
        }
        break;
    case PHOTO_SEQ_JINGLE:
        if (cd_audio_get_state() == CD_AUDIO_IDLE) {
            photo_countdown = PHOTO_COUNTDOWN_FRAMES;
            photo_sequence_step = PHOTO_SEQ_COUNTDOWN;
        }
        break;
    case PHOTO_SEQ_COUNTDOWN:
        photo_countdown--;
        if (photo_countdown <= 0) {
            fade_start_in(10, 0, 1);
            sound_seq_start_reverb(0, 0, farm_area.mode.currentMode, 0);
            farm_set_message_timing();
            sound_fade_in(10);
            photo_sequence_step = PHOTO_SEQ_FADE_IN;
        }
        break;
    case PHOTO_SEQ_FADE_IN:
        if (fade_is_settled() == 1 && sound_fader_mode() == SOUND_FADER_IDLE) {
            fade_reset();
            photo_sequence_step = PHOTO_SEQ_DONE;
        }
        break;
    case PHOTO_SEQ_DONE:
        result_screen_init();
        break;
    }
}

/* Hand the farm back after the photo sequence: restore the status latch, clear the
 * sequence, unpause, walk the player on and set EVENT_FLAG_RESULT_SCREEN. */
static void result_screen_init(void) {
    FarmPlayer *player;

    game_state.world.options.statusLatch = saved_status_latch;
    photo_sequence_reset();
    player = &game_state.player;
    farm_unpause();
    player->stateFlags &= ~PLAYER_FLAG_SCENE_CHANGE;
    player_walk_start(20);
    event_pending_pair_lookup(0x20, 0, 0, 1);
    farm_area.mode.eventMode = FARM_MODE_ZONE_WALK_WAIT;
    event_flag_set(EVENT_FLAG_RESULT_SCREEN, 1);
}

/* Store a pointer to a choice's text in the text bank. */
static inline void set_text(u16 **slot, s16 text)
{
    *slot = TEXT_AT_OFFSET(text_bank_starts.messages + text);
}

/* The message-choice tables, three s16 per row: a text's offset in the messages
 * and the two messages the row may answer with. */
#define TEXT_OFFSET(name) offsetof(TextMessages, name)
static s16 message_choice_rows[6][3] = {
    {TEXT_OFFSET(littered), 0x01F0, 0x0102},
    {TEXT_OFFSET(razzed_villager), 0x01F1, 0x0103},
    {TEXT_OFFSET(didnt_feed_animal), 0x01F2, 0x0104},
    {TEXT_OFFSET(let_animal_die), 0x01F3, 0x0105},
    {TEXT_OFFSET(was_mean_to_animal), 0x01F4, 0x0106},
    {TEXT_OFFSET(overslept), 0x01F5, 0x0107},
};

static s16 message_extra_rows[19][3] = {
    {TEXT_OFFSET(bothered_villager), 0x0224, 0x0225},
    {TEXT_OFFSET(stalked_villager), 0x0226, 0x0227},
    {TEXT_OFFSET(left_diary_blank), 0x0228, 0x0229},
    {TEXT_OFFSET(covet_others_girl), 0x022A, 0x022B},
    {TEXT_OFFSET(shoplifted), 0x022C, 0x022D},
    {TEXT_OFFSET(doubted_other), 0x022E, 0x022F},
    {TEXT_OFFSET(hate_someone), 0x0230, 0x0231},
    {TEXT_OFFSET(sorry_wrong_room), 0x0232, 0x0233},
    {TEXT_OFFSET(love_the_goddess), 0x0234, 0x0235},
    {TEXT_OFFSET(envy_others_home), 0x0236, 0x0237},
    {TEXT_OFFSET(refused_invitation), 0x0238, 0x0239},
    {TEXT_OFFSET(hate_work), 0x023A, 0x023B},
    {TEXT_OFFSET(want_to_fish_kappa), 0x023C, 0x023D},
    {TEXT_OFFSET(reset_minigame), 0x023E, 0x023F},
    {TEXT_OFFSET(lent_this_game), 0x0240, 0x0241},
    {TEXT_OFFSET(named_animal_funny), 0x0242, 0x0243},
    {TEXT_OFFSET(have_many_diaries), 0x0244, 0x0245},
    {TEXT_OFFSET(drank_too_much), 0x0108, 0x0108},
    {TEXT_OFFSET(dropped_crops), 0x0109, 0x0109},
};

/* Pick the two highest of the six scores in FarmPlayer.counts70 (a tie is broken at
 * random), plus a random extra line, and store their localized texts in `out`
 * and their indices in top_score_indices. */
static void result_pick_top_scores(u16 **out)
{
    s32 i;
    s16 first_score = -1;
    s16 first = -1;
    s16 second_score = -1;
    s16 second = -1;
    s16 swap;
    s16 extra;
    s16 *scores = game_state.player.counts70;

    for (i = 0; i < PLAYER_DAY_COUNTS; i++) {
        if (first_score < scores[i]) {
            first = i;
            first_score = scores[i];
        }
    }
    for (i = 0; i < PLAYER_DAY_COUNTS; i++) {
        if (first != i && second_score < scores[i]) {
            second = i;
            second_score = scores[i];
        }
    }
    if (first_score == second_score && rand_range(2) != 0) {
        swap = first;
        first = second;
        second = swap;
    }
    extra = rand_range(19);
    set_text(&out[0], message_choice_rows[first][0]);
    set_text(&out[1], message_choice_rows[second][0]);
    set_text(&out[2], message_extra_rows[extra][0]);
    top_score_indices[0] = first;
    top_score_indices[1] = second;
    top_score_indices[2] = extra;
}

/* Pick the message for slot `kind` (1-based into top_score_indices). Slot 3 is the
 * extra line, and slot 4 is fixed. Slot 3 picks one of two lines at random.
 * Otherwise the slot's item decides: none in stock gives MESSAGE_SOLD_OUT, one gives
 * column 2 of its row, several give column 1. A real message uses one up and applies
 * the item's effect. */
static s32 item_slot_message(s16 kind)
{
    s16 item;
    s16 message;
    s16 *stock;
    s16 effect;

    if (kind == 4) {
        return MESSAGE_BACK_TO_FARM;
    }
    if (kind == 3) {
        return message_extra_rows[top_score_indices[2]][rand_range(2) + 1];
    }
    stock = game_state.player.counts70;
    item = top_score_indices[kind - 1];
    if (stock[item] == 0) {
        message = MESSAGE_SOLD_OUT;
    } else if (stock[item] == 1) {
        message = message_choice_rows[item][2];
    } else {
        message = message_choice_rows[item][1];
    }
    if (message != MESSAGE_SOLD_OUT) {
        effect = top_score_indices[kind - 1];
        stock[effect]--;
        switch (effect) {
        case 0:
            villagers_add_friendship_all(1);
            villagers_add_affection_all(200);
            break;
        case 1:
            villagers_add_friendship_all(1);
            villagers_add_affection_all(100);
            break;
        case 2:
            item_banks_add(1);
            break;
        case 3:
            villagers_add_friendship_all(10);
            villagers_add_affection_all(500);
            break;
        case 4:
            item_banks_add(1);
            break;
        }
    }
    return message;
}

static void villagers_add_friendship_all(s32 amount) {
    s32 i;
    s16 value = amount;
    for (i = 0; i < CHARACTER_COUNT; i++) {
        villager_add_friendship((s16)i, value);
    }
}

static void villagers_add_affection_all(s32 value) {
    s16 amount = value;
    s32 i = 0;
    do {
        villager_add_affection(i, amount);
        i++;
    } while (i < CHARACTER_COUNT);
}

/* Add the amount to the affection of each chicken and livestock the farm keeps: up to
 * MAX_CHICKENS and MAX_LIVESTOCK with the coop and barn, else CHICKENS_NO_COOP and
 * LIVESTOCK_NO_BARN. */
static void item_banks_add(s32 value) {
    s16 amount = value;
    s32 i;

    for (i = 0; i < (game_state.progress.estate.coopLevel == 0 ? CHICKENS_NO_COOP : MAX_CHICKENS); i++) {
        animal_add_affection(&game_state.animals.chickens[i].actor, amount);
    }
    for (i = 0; i < (game_state.progress.estate.barnLevel == 0 ? LIVESTOCK_NO_BARN : MAX_LIVESTOCK); i++) {
        animal_add_affection(&game_state.animals.livestock[i].actor, amount);
    }
}

static void villager_add_friendship(s32 id, s32 amount) {
    CharacterId villager = id;
    s16 add = amount;

    if (game_state.villagers[villager].friendship < FRIENDSHIP_MAX - add) {
        game_state.villagers[villager].friendship += add;
    } else {
        game_state.villagers[villager].friendship = FRIENDSHIP_MAX;
    }
}

static void villager_add_affection(CharacterId villager, s16 amount) {
    u16 cur = game_state.villagers[villager].affection;
    if (cur < AFFECTION_MAX - amount)
        game_state.villagers[villager].affection = cur + amount;
    else
        game_state.villagers[villager].affection = AFFECTION_MAX;
}

static void animal_add_affection(FarmActor *animal, s16 amount) {
    if (animal->affection < (ANIMAL_AFFECTION_MAX - amount)) {
        animal->affection = animal->affection + amount;
    } else {
        animal->affection = ANIMAL_AFFECTION_MAX;
    }
}
