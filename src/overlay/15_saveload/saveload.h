#ifndef SAVELOAD_H
#define SAVELOAD_H

/* Save and load screens: their types and the shared screen bodies. */

#include "types.h"
#include "actor.h"
#include "clock.h"

void save_reset_cursor(void);

s32 save_menu_choose_two(void);

s32 save_menu_wait_confirm(s16 idleResult, s16 pressResult);

/* The done screens time out after this many frames. */
#define SAVE_DONE_TIMEOUT 91

/* The menu entry that shows a message and waits for confirmation. `label` is a
 * member of text_bank.messages. */
#define SAVELOAD_MENU_ENTRY(name, label, step, restart_step)                            \
    void name(void) {                                                         \
        u_long *ot = current_frame->otTags[0];                                   \
        text_draw_in_frame(&ot[6], &ot[5], LOCALIZED(label));                      \
        save_step = save_menu_wait_confirm((step), 0);                            \
        if (save_menu_index >= SAVE_DONE_TIMEOUT) {                                               \
            save_step = (restart_step);                                      \
        }                                                                     \
    }

/* A confirm screen: shows a message, waits for the player's answer and resets
 * the cursor when the answer is not `id`. */
#define SAVELOAD_CONFIRM_ENTRY(name, label, step, next_step)                               \
    void name(void) {                                                         \
        u_long *p = current_frame->otTags[0];                                    \
        text_draw_in_frame(&p[6], &p[5], LOCALIZED(label));                        \
        save_step = save_menu_wait_confirm((step), (next_step));                               \
        if (save_menu_index >= SAVE_DONE_TIMEOUT)                                                 \
            save_step = (next_step);                                         \
        if (save_step != (step))                                               \
            save_reset_cursor();                                                  \
    }

/* The card-choice screens: they differ only in the failure code they fall to. */
#define SAVELOAD_PROBE_ENTRY(name, code, probe_step)                                      \
    void name(void) {                                                         \
        u_long *ot;                                                           \
        s32 result;                                                           \
                                                                              \
        ot = current_frame->otTags[0];                                           \
        text_draw_in_frame(&ot[6], &ot[5], LOCALIZED(text_bank.messages.select_a_memory_card_slot));            \
        result = save_menu_choose_two();                                             \
        if (result < 2) {                                                     \
            if (result < 0) {                                                 \
                if (result == -1) {                                          \
                    save_confirm_flag = 1;                                          \
                    save_step = (code);                                     \
                }                                                            \
            } else {                                                          \
                save_probe_result = result;                                     \
                save_step = (probe_step);                                   \
            }                                                                 \
        }                                                                     \
    }

/* One save slot's record, as the slot list shows it. */
typedef struct SaveSlotRecord {
    FarmDate date;
    u16 farmName[NAME_LENGTH];   /* 0xFFFF-terminated */
    u16 playerName[NAME_LENGTH];
    u8  id[8];
    s32 tag;                /* SAVE_SLOT_TAG once the slot has been filled */
    u8  pad38[0x3C - 0x38];
} SaveSlotRecord;

/* One entry of the two five-entry transfer lists. */
typedef struct SaveEntry {
    s16 kind;               /* SAVE_ENTRY_EMPTY, 0 an A record, 1/2 a B record */
    s16 index;
    void *data;
} SaveEntry;

void save_choose_card_screen(void);
void save_no_card_slot_screen(void);
void save_damaged_card_screen(void);
void save_format_failed_screen(void);
void save_process_failed_screen(void);
void save_not_enough_space_screen(void);
void save_no_free_space_screen(void);
void save_disconnected_screen(void);
void load_choose_card_screen(void);
void load_no_card_slot_screen(void);
void load_damaged_card_screen(void);
void load_no_loadable_screen(void);
void load_no_diary_here_screen(void);
void load_damaged_diary_screen(void);
void load_failed_screen(void);
void load_disconnected_screen(void);
void save_transfer_choose_card_screen(void);
void save_transfer_no_card_slot_screen(void);
void save_transfer_damaged_card_screen(void);
void save_transfer_no_loadable_screen(void);
void save_transfer_no_diary_here_screen(void);
void save_transfer_damaged_diary_screen(void);
void save_transfer_failed_screen(void);
void save_transfer_disconnected_screen(void);
void save_transfer_no_animal_screen(void);
void save_transfer_hens_full_screen(void);
void save_transfer_partner_hens_full_screen(void);
void save_transfer_cows_full_screen(void);
void save_transfer_partner_cows_full_screen(void);
void save_transfer_write_failed_screen(void);

void save_screen_tick(void);
void load_screen_tick(void);
void save_transfer_screen_tick(void);

#endif /* SAVELOAD_H */
