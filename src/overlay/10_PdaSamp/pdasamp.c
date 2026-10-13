/* PDA sample screen (overlay 10): a debug menu for the memory-card library.
 * Each of the fifteen commands has its own page; the pad edits the page's
 * parameters and action 2 sends the command to the card. */

#include <sys/types.h>
#include <stdio.h>
#include <memory.h>
#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>
#include <libmcrd.h>
#include <libmcx.h>
#include "types.h"
#include "clib.h"
#include "main.h"
#include "pad.h"
#include "pdasamp.h"

/* One display buffer: the drawing environment, then the display environment
 * that shows the other half of VRAM. */
typedef struct {
    DRAWENV draw;
    DISPENV disp;
} DisplayBuffer;

/* The block McxAllInfo fills for card slot 0. */
typedef struct PdaAllInfo {
    u8 aplLo;           /* current application, low byte */
    u8 aplHi;           /* current application, high byte */
    u8 sound;           /* sound enabled */
    u8 infrared;        /* indexes pda_infrared_names[] */
    s32 serial;
    PdaCardInfo date;   /* year hi/lo, month, day, weekday, h, m, s */
    u8 pad10[4];
} PdaAllInfo;

#define CURSOR_BYTE (pda_device_block.bytes[pda_read_cursor + 1])

static void pda_menu_input(u8 *unused);
static void pda_menu_draw(void);
static void pda_menu_draw_commands(void);
static s32 pda_command_dispatch(s32 action, s32 buttons);
static int pda_all_info_page(s32 action);
static int pda_get_apl_page(s32 action);
static int pda_get_time_page(s32 action);
static s32 pda_hide_trans_page(s32 action);
static int pda_get_serial_page(s32 action);
static s32 pda_get_mem_page(s32 action, s32 buttons);
static s32 pda_set_exec_flag_page(s32 action, s32 buttons);
static s32 pda_exec_apl_page(s32 action, s32 buttons);
static s32 pda_current_control_page(s32 mode, s32 buttons);
static s32 pda_flush_access_page(s32 action, s32 buttons);
static s32 pda_set_mem_page(s32 action, s32 buttons);
static s32 pda_show_trans_page(s32 action, s32 buttons);
static s32 pda_set_led_page(s32 action, s32 buttons);
static s32 pda_read_dev_page(s32 action, s32 buttons);
static s32 pda_write_dev_page(s32 action, s32 buttons);
static s32 pda_repeat_tick(s32 frames);

/* The pages' action codes, passed to every page by pda_command_dispatch: the pad
 * input, the page's help text (show), the reserve or the card command (exec), and
 * the result text. */
enum PdaAction {
    PDA_ACTION_INPUT = 0,
    PDA_ACTION_SHOW = 1,
    PDA_ACTION_EXEC = 2,
    PDA_ACTION_RESULT = 3,
};

/* The commands, in the order of the menu's rows (rows are numbered from 1). */
enum PdaCommand {
    PDA_CMD_ALL_INFO = 0,
    PDA_CMD_CURRENT_CONTROL = 1,
    PDA_CMD_FLUSH_ACCESS = 2,
    PDA_CMD_EXEC_APL = 3,
    PDA_CMD_GET_APL = 4,
    PDA_CMD_GET_TIME = 5,
    PDA_CMD_GET_SERIAL = 6,
    PDA_CMD_SET_EXEC_FLAG = 7,
    PDA_CMD_WRITE_DEVICE = 8,
    PDA_CMD_READ_DEVICE = 9,
    PDA_CMD_SHOW_TRANS = 10,
    PDA_CMD_HIDE_TRANS = 11,
    PDA_CMD_SET_MEMORY = 12,
    PDA_CMD_GET_MEMORY = 13,
    PDA_CMD_SET_LED = 14,
    PDA_CMD_COUNT = 15,
};

/* McxSync's status, and the result it reports once the command is done. */
enum PdaSyncStatus {
    PDA_SYNC_EXECUTING = 0,
    PDA_SYNC_DONE = 1,
};

enum PdaResult {
    PDA_RESULT_SUCCEEDED = 0,
    PDA_RESULT_NO_CARD = 1,
    PDA_RESULT_FAILED = 2,
    PDA_RESULT_CARD_CHANGED = 3,
};

/* The cursor positions over the five header words: the address bytes, most
 * significant first (the field is cursor + 5), then the length. */
enum PdaSetmemWord {
    PDA_SETMEM_ADDR_0 = -5,
    PDA_SETMEM_ADDR_1 = -4,
    PDA_SETMEM_ADDR_2 = -3,
    PDA_SETMEM_ADDR_3 = -2,
    PDA_SETMEM_LENGTH_WORD = -1,
};

#define PDA_EXEC_BLOCK_MIN 1
#define PDA_EXEC_BLOCK_MAX 15
#define PDA_MEM_MAX_LEN 128

/* The selected command, the one armed with start (-1 = none), and the exit flag. */
static s32 pda_selected_command = PDA_CMD_ALL_INFO;
static s32 pda_armed_command = -1;
static s32 pda_sample_done = 0;
static const char pda_str_tx_rx[8];
static const char pda_str_tx[4];
static const char pda_str_rx[4];
static const char pda_str_disable[8];
static const char pda_str_sat[4];
static const char pda_str_fri[4];
static const char pda_str_thr[4];
static const char pda_str_wed[4];
static const char pda_str_tue[4];
static const char pda_str_mon[4];
static const char pda_str_sun[4];
/* Infrared state and weekday names. */
static const char *pda_infrared_names[4] = {
    pda_str_disable, pda_str_rx, pda_str_tx, pda_str_tx_rx};
static const char *pda_weekday_names[7] = {
    pda_str_sun, pda_str_mon, pda_str_tue, pda_str_wed, pda_str_thr, pda_str_fri, pda_str_sat};
/* Weekday names for the get-time page. */
static const char *pda_time_weekday_names[7] = {
    pda_str_sun, pda_str_mon, pda_str_tue, pda_str_wed, pda_str_thr, pda_str_fri, pda_str_sat};
/* Get-memory request: four address bytes and a length. */
static u32 pda_getmem_params[5] = {0x0D, 0x00, 0x01, 0x04, 0x80};
static s32 pda_getmem_tri_hold = 0;
static s32 pda_getmem_sqr_hold = 0;
static s32 pda_getmem_cir_hold = 0;
static s32 pda_getmem_crs_hold = 0;
/* Exec-flag page: application block number (1..15), stepped by triangle and
 * square, and the exec flag, set by circle and cross. */
static s32 pda_exec_block = 1;
static s32 pda_exec_flag = 0;
static s32 pda_exec_tri_hold = 0;
static s32 pda_exec_sqr_hold = 0;
static s32 pda_exec_apl_index = 0;
static s32 pda_exec_apl_tri_hold = 0;
static s32 pda_exec_apl_sqr_hold = 0;
static s32 pda_exec_apl_cir_hold = 0;
static s32 pda_exec_apl_crs_hold = 0;
/* pda_control_infrared_names: entry 0 is "dis", the rest are the words above. */
static const char pda_str_infrared_dis[4];
static const char *pda_control_infrared_names[4] = {
    pda_str_infrared_dis, pda_str_rx, pda_str_tx, pda_str_tx_rx};
/* Set-memory screen: cursor, request words and button hold counters. */
static s32 pda_setmem_cursor = PDA_SETMEM_ADDR_0;
static u32 pda_setmem_params[5] = {0x0D, 0x00, 0x01, 0x04, 0x80};
static s32 pda_setmem_tri_hold = 0;
static s32 pda_setmem_sqr_hold = 0;
static s32 pda_setmem_cir_hold = 0;
static s32 pda_setmem_crs_hold = 0;
/* Show-trans page: direction flag, time-out in seconds, and triangle/square hold counters. */
static u32 pda_trans_shown = 0;
static u32 pda_trans_timeout = 1;
static u32 pda_trans_tri_hold = 0;
static u32 pda_trans_sqr_hold = 0;
/* LED flag, set by circle and cross. */
static s32 pda_led_flag = 0;
static s32 pda_read_cursor = -1;
static s32 pda_read_tri_hold = 0;
static s32 pda_read_sqr_hold = 0;
static s32 pda_read_cir_hold = 0;
static s32 pda_read_crs_hold = 0;
static s32 pda_write_cursor = -1;

static s32 pda_write_tri_hold;
static s32 pda_write_sqr_hold;
static s32 pda_write_cir_hold;
static s32 pda_write_crs_hold;
static u32 pda_menu_last_held;
static u32 pda_menu_last_arm;
static PdaAllInfo pda_all_info;
/* Card-slot results, filled by McxGetApl / McxGetTime / McxGetSerial. */
static long pda_apl_result;
static PdaCardInfo pda_time_info;
static u_long pda_serial_no;
static u8 pda_getmem_buffer[128];
static s32 pda_getmem_field;
static s32 pda_getmem_last_pad;
static s32 pda_exec_apl_last_pad;
static s16 pda_exec_apl_params[6];
static u32 pda_control_sound;
static s32 pda_control_infrared;
static u32 pda_control_last_pad;
/* Flush-access flag (superior), set by circle and cross. */
static s32 pda_flush_access_flag;
static u8 pda_setmem_buffer[128];
static s32 pda_setmem_last_pad;
static s32 pda_read_last_pad;
static s32 pda_write_last_pad;
static PdaDeviceBlock pda_device_block;
/* The status line pda_menu_draw copies each command's result into. */
static char pda_status_line[64];
/* Menu label strings, with colour codes, printed by pda_menu_draw_commands. */
static const char *pda_command_labels[PDA_CMD_COUNT];
/* Dummy argument for pda_menu_input, which does not use it. */
static u8 pda_menu_input_buf[676];

/* PDA sample main loop: set up two 640x240 display buffers and the debug font,
 * then flip, poll and draw every frame until pda_sample_done is set. */
s32 pda_sample_run(void)
{
    DisplayBuffer db[2];
    DisplayBuffer *cdb;

    cdb = 0;
    pda_device_block.field.device = 1;
    pda_device_block.field.fix[0] = 4;
    pda_device_block.field.fix[1] = 1;
    pda_device_block.field.fix[2] = 0;
    pda_device_block.field.fix[3] = 0xD;
    pda_device_block.field.fix[4] = 0x80;
    SetDefDrawEnv(&db[0].draw, 0, 0, 640, 240);
    SetDefDrawEnv(&db[1].draw, 0, 240, 640, 240);
    SetDefDispEnv(&db[0].disp, 0, 240, 640, 240);
    SetDefDispEnv(&db[1].disp, 0, 0, 640, 240);
    FntLoad(960, 0);
    SetDumpFnt(FntOpen(4, 20, 624, 240, 0, 1024));
    db[0].draw.isbg = 1;
    db[1].draw.isbg = 1;
    db[0].draw.r0 = 60;
    db[0].draw.g0 = 120;
    db[0].draw.b0 = 120;
    db[1].draw.r0 = 60;
    db[1].draw.g0 = 120;
    db[1].draw.b0 = 120;
    SetDispMask(1);
    VSync(0);
    while (pda_sample_done == 0) {
        cdb = (cdb == &db[0]) ? &db[1] : &db[0];
        pad_poll_ports();
        pda_menu_input(pda_menu_input_buf);
        pda_menu_draw();
        FntFlush(-1);
        DrawSync(0);
        VSync(0);
        PutDispEnv(&cdb->disp);
        PutDrawEnv(&cdb->draw);
    }
    main_request_scene(SCENE_DEBUG);
    return 0;
}

/* Main menu input: L1/L2 move the selection, start arms the command, select quits. */
static void pda_menu_input(u8 *unused)
{
    s32 buttons;
    s32 held;

    buttons = input_pad1->pressed;
    pda_command_dispatch(0, buttons);
    if (pda_menu_last_held == 0) {
        if (buttons & PAD_L2) {
            pda_selected_command = (pda_selected_command + PDA_CMD_COUNT - 1) % PDA_CMD_COUNT;
        } else if (buttons & PAD_L1) {
            pda_selected_command = (pda_selected_command + 1) % PDA_CMD_COUNT;
        }
    }
    if (pda_menu_last_arm == 0 && (buttons & PAD_START)) {
        if (pda_command_dispatch(2, 0) != 0) {
            pda_armed_command = pda_selected_command;
        } else {
            pda_armed_command = -1;
            strcpy(pda_status_line, "command can't reserve(busy)");
        }
    }
    if (buttons & PAD_SELECT) {
        pda_sample_done = 1;
    }
    held = buttons & (PAD_L2 | PAD_L1);
    pda_menu_last_held = held;
    pda_menu_last_arm = buttons & PAD_START;
    if (held != 0) {
        pda_status_line[0] = 0;
    }
}

/* One frame of the PDA sample screen: highlight the selected command in the
 * menu, report the memory-card command status, then let the command's screen
 * print its help (and, when it is the armed one, its result). */
static void pda_menu_draw(void)
{
    long cmd;
    long result;
    s32 i;
    s32 status;

    for (i = 0; i < PDA_CMD_COUNT; i++) {
        pda_command_labels[i] = "~c888";
    }
    pda_command_labels[pda_selected_command] = "~c585";
    pda_menu_draw_commands();
    FntFlush(-1);
    DrawSync(0);
    FntPrint("\n\n\n\n\n\n\n");

    status = McxSync(1, &cmd, &result);
    switch (status) {
    case PDA_SYNC_EXECUTING:
        strcpy(pda_status_line, "command executing");
        break;
    case PDA_SYNC_DONE:
        switch (result) {
        case PDA_RESULT_SUCCEEDED:
            strcpy(pda_status_line, "command succeeded");
            break;
        case PDA_RESULT_NO_CARD:
            strcpy(pda_status_line, "card not connected");
            break;
        case PDA_RESULT_FAILED:
            strcpy(pda_status_line, "command failed");
            break;
        case PDA_RESULT_CARD_CHANGED:
            if (cmd == McxFuncExecFlag) {
                strcpy(pda_status_line, "command failed (card changed)");
            } else {
                strcpy(pda_status_line, "command succeeded (card changed)");
            }
            break;
        }
        break;
    }

    pda_command_dispatch(1, 0);
    FntPrint("\n");
    if (pda_selected_command == pda_armed_command) {
        pda_command_dispatch(3, 0);
    } else {
        FntPrint("    press start to exec command \n");
    }
    FntPrint("\n    ~c585%s \n~c888", pda_status_line);
}

/* Prints the command menu, the labels in pda_command_labels coloured as set by pda_menu_draw. */
static void pda_menu_draw_commands(void) {
    const char **labels = pda_command_labels;

    FntPrint("  %s1: all info        ~c888 %s 6: get time       ~c888 %s11: show trans    ~c888\n", labels[0], labels[5], labels[10]);
    FntPrint("  %s2: current control ~c888 %s 7: get serial no  ~c888 %s12: hide trans    ~c888\n", labels[1], labels[6], labels[11]);
    FntPrint("  %s3: flush access    ~c888 %s 8: set exec flag  ~c888 %s13: set memory    ~c888\n", labels[2], labels[7], labels[12]);
    FntPrint("  %s4: exec appli      ~c888 %s 9: write decive   ~c888 %s14: get memory    ~c888\n", labels[3], labels[8], labels[13]);
    FntPrint("  %s5: get appli       ~c888 %s10: read device    ~c888 %s15: set led       ~c888\n", labels[4], labels[9], labels[14]);
    FntPrint("\n");
}

/* Routes an action to the page of the selected command. Commands without a page
 * of their own show "no parameter needed". The five read-only commands (all info,
 * get appli, get time, get serial, hide trans) run on exec and print their data on
 * result. */
static s32 pda_command_dispatch(s32 action, s32 buttons)
{
    switch (pda_selected_command) {
    case PDA_CMD_CURRENT_CONTROL:
        return pda_current_control_page(action, buttons);
    case PDA_CMD_FLUSH_ACCESS:
        return pda_flush_access_page(action, buttons);
    case PDA_CMD_EXEC_APL:
        return pda_exec_apl_page(action, buttons);
    case PDA_CMD_SET_EXEC_FLAG:
        return pda_set_exec_flag_page(action, buttons);
    case PDA_CMD_WRITE_DEVICE:
        return pda_write_dev_page(action, buttons);
    case PDA_CMD_READ_DEVICE:
        return pda_read_dev_page(action, buttons);
    case PDA_CMD_SHOW_TRANS:
        return pda_show_trans_page(action, buttons);
    case PDA_CMD_SET_MEMORY:
        return pda_set_mem_page(action, buttons);
    case PDA_CMD_GET_MEMORY:
        return pda_get_mem_page(action, buttons);
    case PDA_CMD_SET_LED:
        return pda_set_led_page(action, buttons);
    }

    if (action == PDA_ACTION_SHOW) {
        FntPrint("    no parameter needed\n");
        return 0;
    }
    if (action == PDA_ACTION_EXEC || action == PDA_ACTION_RESULT) {
        switch (pda_selected_command) {
        case PDA_CMD_ALL_INFO:
            return pda_all_info_page(action);
        case PDA_CMD_GET_APL:
            return pda_get_apl_page(action);
        case PDA_CMD_GET_TIME:
            return pda_get_time_page(action);
        case PDA_CMD_GET_SERIAL:
            return pda_get_serial_page(action);
        case PDA_CMD_HIDE_TRANS:
            return pda_hide_trans_page(action);
        }
    }
    return 0;
}

static const char pda_no_data_msg[20] = "    no data disp \n";
static const char pda_str_tx_rx[8] = "tx/rx";
static const char pda_str_tx[4] = "tx";
static const char pda_str_rx[4] = "rx";
static const char pda_str_disable[8] = "disable";
static const char pda_str_sat[4] = "sat";
static const char pda_str_fri[4] = "fri";
static const char pda_str_thr[4] = "thr";
static const char pda_str_wed[4] = "wed";
static const char pda_str_tue[4] = "tue";
static const char pda_str_mon[4] = "mon";
static const char pda_str_sun[4] = "sun";

static void pda_print_no_data(void) {
    FntPrint(pda_no_data_msg);
}

/* "All info" page: action 2 refreshes the block, anything else prints it. */
static int pda_all_info_page(s32 action)
{
    char buf[16];
    PdaCardInfo *date;

    if (action == PDA_ACTION_EXEC) {
        return McxAllInfo(0, (unsigned char *)&pda_all_info);
    }
    FntPrint("    current application : %d \n", pda_all_info.aplLo | (pda_all_info.aplHi << 8));
    FntPrint("    PDA appli flush access :%s \n", pda_all_info.date.b7 ? "SUPERIOR" : "INFERIOR");
    FntPrint("    current control  sound:%s  infred:%s \n", pda_all_info.sound ? "ENABLE" : "DISABLE",
             pda_infrared_names[pda_all_info.infrared]);
    FntPrint("    serial No. : %x \n", pda_all_info.serial);
    date = &pda_all_info.date;
    sprintf(buf, "%2x%02x", date->b0, date->b1);
    FntPrint("    date: %s/", buf);
    FntPrint("%x/%x %s ", date->b2, date->b3,
             pda_weekday_names[date->b4]);
    sprintf(buf, "%2x:%02x:%02x", date->b5, date->b6,
            date->b7);
    FntPrint("%s \n", buf);
    return 0;
}

/* "Get appli" page: action 2 reads the current application, anything else prints it. */
static int pda_get_apl_page(s32 action) {
    if (action != PDA_ACTION_EXEC) {
        FntPrint("    current application : %d \n", pda_apl_result);
        return 0;
    }
    McxGetApl(0, &pda_apl_result);
}

/* "Get time" page: action 2 reads the card clock, anything else prints it. */
static int pda_get_time_page(s32 action) {
    u8 buf[16];

    if (action != PDA_ACTION_EXEC) {
        sprintf(buf, "%2x%02x", pda_time_info.b0, pda_time_info.b1);
        FntPrint("    date: %s/", buf);
        FntPrint("%x/%x %s ", pda_time_info.b2, pda_time_info.b3, pda_time_weekday_names[pda_time_info.b4]);
        sprintf(buf, "%2x:%02x:%02x", pda_time_info.b5, pda_time_info.b6, pda_time_info.b7);
        FntPrint("%s \n", buf);
        return 0;
    }
    McxGetTime(0, (unsigned char *)&pda_time_info);
}

/* "Hide trans" page: action 2 sends the command, anything else prints the no-data line. */
static s32 pda_hide_trans_page(s32 action) {
    if (action == PDA_ACTION_EXEC) {
        return McxHideTrans(0);
    }
    pda_print_no_data();
    return 0;
}

/* "Get serial" page: action 2 reads the serial number, anything else prints it. */
static int pda_get_serial_page(s32 action) {
    if (action != PDA_ACTION_EXEC) {
        FntPrint("    serial No. : %x \n", pda_serial_no);
        return 0;
    }
    McxGetSerial(0, &pda_serial_no);
}

/* Get-memory page. pda_getmem_params[0..3] are the address bytes and [4] the length;
 * pda_getmem_field selects the field the pad edits. Action 2 reads that range into
 * pda_getmem_buffer, action 3 hex-dumps it. */
static s32 pda_get_mem_page(s32 action, s32 buttons)
{
    s32 held;
    s32 i;
    s32 row;
    u32 offset;
    char buf[16];

    switch (action) {
    case PDA_ACTION_INPUT:
        if (pda_getmem_last_pad == 0) {
            if (buttons & PAD_RIGHT) {
                if (pda_getmem_field < 4) {
                    pda_getmem_field++;
                }
            } else if (buttons & PAD_LEFT) {
                if (pda_getmem_field > 0) {
                    pda_getmem_field--;
                }
            }
        }
        pda_getmem_last_pad = buttons & (PAD_LEFT | PAD_RIGHT);
        if (buttons & PAD_TRIANGLE) {
            held = pda_getmem_tri_hold + 1;
            pda_getmem_tri_hold = held;
            if (pda_repeat_tick(held) != 0) {
                pda_getmem_params[pda_getmem_field] = (pda_getmem_params[pda_getmem_field] + 0x10) & 0xFF;
            }
        } else {
            pda_getmem_tri_hold = 0;
            if (buttons & PAD_SQUARE) {
                held = pda_getmem_sqr_hold + 1;
                pda_getmem_sqr_hold = held;
                if (pda_repeat_tick(held) != 0) {
                    pda_getmem_params[pda_getmem_field] = (pda_getmem_params[pda_getmem_field] + 0xF0) & 0xFF;
                }
            } else {
                pda_getmem_sqr_hold = 0;
            }
        }
        if (pda_getmem_params[4] > PDA_MEM_MAX_LEN) {
            if (pda_getmem_params[4] == 0x90) {
                pda_getmem_params[4] = 0;
            } else if (pda_getmem_params[4] == 0xF0) {
                pda_getmem_params[4] = 0x80;
            } else {
                pda_getmem_params[4] &= 0x7F;
            }
        }
        if (buttons & PAD_CIRCLE) {
            held = pda_getmem_cir_hold + 1;
            pda_getmem_cir_hold = held;
            if (pda_repeat_tick(held) != 0) {
                pda_getmem_params[pda_getmem_field] = (pda_getmem_params[pda_getmem_field] & 0xF0)
                    + (((pda_getmem_params[pda_getmem_field] & 0xF) + 1) & 0xF);
            }
        } else {
            pda_getmem_cir_hold = 0;
            if (buttons & PAD_CROSS) {
                held = pda_getmem_crs_hold + 1;
                pda_getmem_crs_hold = held;
                if (pda_repeat_tick(held) != 0) {
                    pda_getmem_params[pda_getmem_field] = (pda_getmem_params[pda_getmem_field] & 0xF0)
                        + (((pda_getmem_params[pda_getmem_field] & 0xF) + 0xF) & 0xF);
                }
            } else {
                pda_getmem_crs_hold = 0;
            }
        }
        if (pda_getmem_params[4] > PDA_MEM_MAX_LEN) {
            pda_getmem_params[4] -= 0x10;
        }
        return 0;
    case PDA_ACTION_SHOW:
        FntPrint("    right:  pos right     left:  pos left \n");
        FntPrint("    triang: inc upper     squar: dec upper \n");
        FntPrint("    circle: inc lower     cross: dec lower \n");
        FntPrint("    start address :~c558");
        for (i = 0; i < 4; i++) {
            sprintf(buf, "%02x", pda_getmem_params[i]);
            if (i == pda_getmem_field) {
                FntPrint("~c588%s~c558", buf);
            } else {
                FntPrint(buf);
            }
        }
        sprintf(buf, "%02x", pda_getmem_params[4]);
        FntPrint("  ~c888len %s%s~c888 \n", pda_getmem_field == 4 ? "~c588" : "~c558", buf);
        return 0;
    case PDA_ACTION_EXEC:
        return McxGetMem(0, pda_getmem_buffer,
                         (pda_getmem_params[0] << 24) | (pda_getmem_params[1] << 16) |
                         (pda_getmem_params[2] << 8) | pda_getmem_params[3],
                         pda_getmem_params[4]);
    case PDA_ACTION_RESULT:
        for (row = 0; row < 8; row++) {
            offset = row * 16;
            FntPrint("    ");
            for (i = 0; i < 16 && offset + i < pda_getmem_params[4]; i++) {
                sprintf(buf, "%02x", pda_getmem_buffer[offset + i]);
                FntPrint("%s ", buf);
            }
            FntPrint("\n");
            if (pda_getmem_params[4] < row * 16 + i) {
                break;
            }
        }
        return 0;
    default:
        return 0;
    }
}

/* Exec-flag page. Exec commits the block and flag to the card, unless the card is
 * missing or invalid (McErrCardNotExist, McErrCardInvalid): then the status line
 * reports it and the page returns -1. */
static s32 pda_set_exec_flag_page(s32 action, s32 buttons) {
    long cmd;
    long status;
    s32 held;
    s32 value;
    const char *format;
    const char *state;

    switch (action) {
    case PDA_ACTION_INPUT:
        if (buttons & PAD_CIRCLE) {
            pda_exec_flag = 1;
        } else if (buttons & PAD_CROSS) {
            pda_exec_flag = 0;
        }
        if (buttons & PAD_TRIANGLE) {
            held = pda_exec_tri_hold + 1;
            pda_exec_tri_hold = held;
            if (pda_repeat_tick(held) != 0) {
                value = pda_exec_block;
                if (value < PDA_EXEC_BLOCK_MAX) {
                    pda_exec_block = value + 1;
                }
            }
        } else {
            pda_exec_tri_hold = 0;
            if (buttons & PAD_SQUARE) {
                held = pda_exec_sqr_hold + 1;
                pda_exec_sqr_hold = held;
                if (pda_repeat_tick(held) != 0) {
                    value = pda_exec_block;
                    if (value > PDA_EXEC_BLOCK_MIN) {
                        pda_exec_block = value - 1;
                    }
                }
            } else {
                pda_exec_sqr_hold = 0;
            }
        }
        return 0;
    case PDA_ACTION_SHOW:
        FntPrint("    triang: inc appli no.     squar: dec appli no. \n");
        FntPrint("    circle: exec flag on      cross: exec flag off \n");
        value = pda_exec_block;
        FntPrint("    appli(block) number :~c558 %d ~c888\n", value);
        format = "    exec flag :~c558 %s ~c888\n";
        if (pda_exec_flag != 0) {
            state = "on";
        } else {
            state = "off";
        }
        FntPrint(format, state);
        return 0;
    case PDA_ACTION_EXEC:
        MemCardAccept(0);
        MemCardSync(0, &cmd, &status);
        value = (u32)(status - McErrCardNotExist) < 2U;
        if (value) {
            strcpy(pda_status_line, "~c855command failed (no card)");
            return -1;
        }
        return McxExecFlag(0, pda_exec_block, pda_exec_flag);
    case PDA_ACTION_RESULT:
        pda_print_no_data();
        return 0;
    default:
        return 0;
    }
}

/* Exec-application page. pda_exec_apl_params[0] is the application number (-1 = none),
 * [1..4] the four argument bytes; pda_exec_apl_index selects which one the pad edits:
 * left/right move the cursor, up goes to the application number and down to the first
 * argument byte. Triangle/square step the high digit, circle/cross the low digit (cross
 * on an empty application number sets it to -1). Action 2 launches the application
 * with the packed argument. */
static s32 pda_exec_apl_page(s32 action, s32 buttons)
{
    s32 held;
    s32 i;
    s32 arg;
    char buf[16];

    switch (action) {
    case PDA_ACTION_INPUT:
        if (pda_exec_apl_last_pad == 0) {
            if (buttons & PAD_RIGHT) {
                if (pda_exec_apl_index < 4) {
                    pda_exec_apl_index++;
                }
            } else if (buttons & PAD_LEFT) {
                if (pda_exec_apl_index > 0) {
                    pda_exec_apl_index--;
                }
            }
            if (buttons & PAD_UP) {
                pda_exec_apl_index = 0;
            } else if ((buttons & PAD_DOWN) && pda_exec_apl_index == 0) {
                pda_exec_apl_index = 1;
            }
        }
        pda_exec_apl_last_pad = buttons & PAD_ARROWS_MASK;
        if (buttons & PAD_TRIANGLE) {
            held = pda_exec_apl_tri_hold + 1;
            pda_exec_apl_tri_hold = held;
            if (pda_repeat_tick(held) != 0) {
                pda_exec_apl_params[pda_exec_apl_index] = (pda_exec_apl_params[pda_exec_apl_index] + 0x10) & 0xFF;
            }
        } else {
            pda_exec_apl_tri_hold = 0;
            if (buttons & PAD_SQUARE) {
                held = pda_exec_apl_sqr_hold + 1;
                pda_exec_apl_sqr_hold = held;
                if (pda_repeat_tick(held) != 0) {
                    pda_exec_apl_params[pda_exec_apl_index] = (pda_exec_apl_params[pda_exec_apl_index] + 0xF0) & 0xFF;
                }
            } else {
                pda_exec_apl_sqr_hold = 0;
            }
        }
        if (buttons & PAD_CIRCLE) {
            held = pda_exec_apl_cir_hold + 1;
            pda_exec_apl_cir_hold = held;
            if (pda_repeat_tick(held) != 0) {
                pda_exec_apl_params[pda_exec_apl_index] = (pda_exec_apl_params[pda_exec_apl_index] & 0xF0)
                    + (((pda_exec_apl_params[pda_exec_apl_index] & 0xF) + 1) & 0xF);
            }
        } else {
            pda_exec_apl_cir_hold = 0;
            if (buttons & PAD_CROSS) {
                held = pda_exec_apl_crs_hold + 1;
                pda_exec_apl_crs_hold = held;
                if (pda_repeat_tick(held) != 0) {
                    if (pda_exec_apl_index == 0 && pda_exec_apl_params[0] == 0) {
                        pda_exec_apl_params[0] = -1;
                    } else {
                        pda_exec_apl_params[pda_exec_apl_index] = (pda_exec_apl_params[pda_exec_apl_index] & 0xF0)
                            + (((pda_exec_apl_params[pda_exec_apl_index] & 0xF) + 0xF) & 0xF);
                    }
                }
            } else {
                pda_exec_apl_crs_hold = 0;
            }
        }
        if (pda_exec_apl_index == 0 && pda_exec_apl_params[0] != -1) {
            pda_exec_apl_params[0] &= 0xF;
        }
        return 0;
    case PDA_ACTION_SHOW:
        FntPrint("    triang: inc appri no.     squar: dec appli no. \n");
        FntPrint("    application number : %s%d \n", pda_exec_apl_index != 0 ? "~c558" : "~c588", pda_exec_apl_params[0]);
        FntPrint("    ~c888arg for appli :~c558 ");
        for (i = 0; i < 4; i++) {
            arg = i + 1;
            sprintf(buf, "%02x", pda_exec_apl_params[arg]);
            if (arg == pda_exec_apl_index) {
                FntPrint("~c588%s~c558", buf);
            } else {
                FntPrint(buf);
            }
        }
        FntPrint("~c888\n");
        return 0;
    case PDA_ACTION_EXEC:
        return McxExecApl(0, pda_exec_apl_params[0],
                          (pda_exec_apl_params[1] << 24) | (pda_exec_apl_params[2] << 16) |
                          (pda_exec_apl_params[3] << 8) | pda_exec_apl_params[4]);
    case PDA_ACTION_RESULT:
        pda_print_no_data();
        return 0;
    default:
        return 0;
    }
}

static const char pda_str_infrared_dis[4] = "dis";

/* Current-control page: mode 0 reads input, 1 prints the settings, 2 sends them
 * and 3 prints the no-data line. */
static s32 pda_current_control_page(s32 mode, s32 buttons)
{
    s32 infrared;
    const char *format;
    const char *label;

    switch (mode) {
    case PDA_ACTION_INPUT:
        if (pda_control_last_pad == 0) {
            if (buttons & PAD_TRIANGLE) {
                infrared = pda_control_infrared;
                if (infrared < 3) {
                    pda_control_infrared = infrared + 1;
                }
            } else if (buttons & PAD_SQUARE) {
                if (pda_control_infrared > 0) {
                    pda_control_infrared = pda_control_infrared - 1;
                }
            }
        }
        pda_control_last_pad = buttons & (PAD_TRIANGLE | PAD_SQUARE);
        if (buttons & PAD_CIRCLE) {
            pda_control_sound = 1;
        } else if (buttons & PAD_CROSS) {
            pda_control_sound = 0;
        }
        break;
    case PDA_ACTION_SHOW:
        FntPrint("    circle: sound enable      cross: sound disable \n");
        FntPrint("    triang: infred enable     squar: infred disable \n");
        format = "    function allowance  sound ~c558%s~c888  infred ~c558%s~c888 \n";
        if (pda_control_sound != 0) {
            label = "ENA";
        } else {
            label = "DIS";
        }
        FntPrint(format, label, pda_control_infrared_names[pda_control_infrared]);
        break;
    case PDA_ACTION_EXEC:
        return McxCurrCtrl(0, pda_control_sound, pda_control_infrared, 0);
    case PDA_ACTION_RESULT:
        pda_print_no_data();
        break;
    }
    return 0;
}

/* Flush-access page: circle sets the superior flag, cross clears it; action 2 sends it. */
static s32 pda_flush_access_page(s32 action, s32 buttons) {
    const char *format;
    const char *message;

    switch (action) {
    case PDA_ACTION_INPUT:
        if (buttons & PAD_CIRCLE) {
            pda_flush_access_flag = 1;
        } else if (buttons & PAD_CROSS) {
            pda_flush_access_flag = 0;
        }
        return 0;
    case PDA_ACTION_SHOW:
        FntPrint("    circle: flush write superior to spi \n");
        FntPrint("    cross:  spi superior to flush write \n");
        format = "    PDA appli flush access : ~c558%s~c888 \n";
        if (pda_flush_access_flag != 0) {
            message = "SUPERIOR";
        } else {
            message = "INFERIOR";
        }
        FntPrint(format, message);
        return 0;
    case PDA_ACTION_EXEC:
        return McxFlashAcs(0, pda_flush_access_flag);
    case PDA_ACTION_RESULT:
        pda_print_no_data();
        return 0;
    default:
        return 0;
    }
}

/* The set-memory request: four address bytes (most significant first), one
 * per word, then the length. The cursor pda_setmem_cursor runs -5..-1 over these
 * five words and 0..length-1 over the data bytes in pda_setmem_buffer. */
#define SETMEM_LENGTH 4

/* Step the low hex digit of a byte value, leaving the high one alone. */
#define NIBBLE_LOW 0xF
#define NIBBLE_HIGH 0xF0
#define STEP_LOW_NIBBLE(x, step) ((x) = ((x) & NIBBLE_HIGH) + ((((x) & NIBBLE_LOW) + (step)) & NIBBLE_LOW))

/* Set-memory screen. Action 0: the d-pad moves the cursor over the request
 * words and the data grid, triangle/square step the high nibble and
 * circle/cross the low one. 1 prints the editor, 2 sends the request, 3 prints
 * the "no data" line. */
static s32 pda_set_mem_page(s32 action, s32 buttons)
{
    char text[16];
    s32 held;
    s32 field;
    s32 row;
    s32 col;
    s32 base;

    switch (action) {
    case PDA_ACTION_INPUT:
        if (pda_setmem_last_pad == 0) {
            if (buttons & PAD_RIGHT) {
                if (pda_setmem_cursor < (s32)(pda_setmem_params[SETMEM_LENGTH] - 1)) {
                    pda_setmem_cursor++;
                }
            } else if (buttons & PAD_LEFT) {
                if (pda_setmem_cursor >= PDA_SETMEM_ADDR_1) {
                    pda_setmem_cursor--;
                }
            }
            if (buttons & PAD_UP) {
                if (pda_setmem_cursor >= 0 && pda_setmem_cursor < 6) {
                    pda_setmem_cursor = PDA_SETMEM_ADDR_0;
                } else if (pda_setmem_cursor == 6) {
                    pda_setmem_cursor = PDA_SETMEM_ADDR_1;
                } else if (pda_setmem_cursor >= 7 && pda_setmem_cursor < 9) {
                    pda_setmem_cursor = PDA_SETMEM_ADDR_3;
                } else if (pda_setmem_cursor >= 9 && pda_setmem_cursor < 16) {
                    pda_setmem_cursor = PDA_SETMEM_LENGTH_WORD;
                } else if (pda_setmem_cursor >= 16) {
                    pda_setmem_cursor -= 16;
                }
            } else if (buttons & PAD_DOWN) {
                switch (pda_setmem_cursor) {
                case PDA_SETMEM_ADDR_0:
                    if (pda_setmem_params[SETMEM_LENGTH] >= 6) {
                        pda_setmem_cursor = 5;
                    }
                    break;
                case PDA_SETMEM_ADDR_1:
                case PDA_SETMEM_ADDR_2:
                    if (pda_setmem_params[SETMEM_LENGTH] >= 7) {
                        pda_setmem_cursor = 6;
                    }
                    break;
                case PDA_SETMEM_ADDR_3:
                    if (pda_setmem_params[SETMEM_LENGTH] >= 8) {
                        pda_setmem_cursor = 7;
                    }
                    break;
                case PDA_SETMEM_LENGTH_WORD:
                    if (pda_setmem_params[SETMEM_LENGTH] >= 11) {
                        pda_setmem_cursor = 10;
                    }
                    break;
                default:
                    if (pda_setmem_cursor + 16 < (s32)pda_setmem_params[SETMEM_LENGTH]) {
                        pda_setmem_cursor += 16;
                    }
                    break;
                }
                if (pda_setmem_params[SETMEM_LENGTH] != 0 && pda_setmem_cursor < 0) {
                    pda_setmem_cursor = pda_setmem_params[SETMEM_LENGTH] - 1;
                }
            }
        }
        pda_setmem_last_pad = buttons & PAD_ARROWS_MASK;
        field = (pda_setmem_cursor < 0) ? pda_setmem_cursor + 5 : -1;

        if (buttons & PAD_TRIANGLE) {
            held = pda_setmem_tri_hold + 1;
            pda_setmem_tri_hold = held;
            if (pda_repeat_tick(held) != 0) {
                if (pda_setmem_cursor < 0) {
                    pda_setmem_params[field] = (pda_setmem_params[field] + 0x10) & 0xFF;
                } else {
                    pda_setmem_buffer[pda_setmem_cursor] += 0x10;
                }
            }
        } else {
            pda_setmem_tri_hold = 0;
            if (buttons & PAD_SQUARE) {
                held = pda_setmem_sqr_hold + 1;
                pda_setmem_sqr_hold = held;
                if (pda_repeat_tick(held) != 0) {
                    if (pda_setmem_cursor < 0) {
                        pda_setmem_params[field] = (pda_setmem_params[field] + 0xF0) & 0xFF;
                    } else {
                        pda_setmem_buffer[pda_setmem_cursor] += 0xF0;
                    }
                }
            } else {
                pda_setmem_sqr_hold = 0;
            }
        }
        /* The length wraps within 0..0x80. */
        if (field >= 0) {
            if (pda_setmem_params[SETMEM_LENGTH] > PDA_MEM_MAX_LEN) {
                if (pda_setmem_params[SETMEM_LENGTH] == 0x90) {
                    pda_setmem_params[SETMEM_LENGTH] = 0;
                } else if (pda_setmem_params[SETMEM_LENGTH] == 0xF0) {
                    pda_setmem_params[SETMEM_LENGTH] = 0x80;
                } else {
                    pda_setmem_params[SETMEM_LENGTH] &= 0x7F;
                }
            }
        }
        if (buttons & PAD_CIRCLE) {
            held = pda_setmem_cir_hold + 1;
            pda_setmem_cir_hold = held;
            if (pda_repeat_tick(held) != 0) {
                if (pda_setmem_cursor < 0) {
                    STEP_LOW_NIBBLE(pda_setmem_params[field], 1);
                } else {
                    STEP_LOW_NIBBLE(pda_setmem_buffer[pda_setmem_cursor], 1);
                }
            }
        } else {
            pda_setmem_cir_hold = 0;
            if (buttons & PAD_CROSS) {
                held = pda_setmem_crs_hold + 1;
                pda_setmem_crs_hold = held;
                if (pda_repeat_tick(held) != 0) {
                    if (pda_setmem_cursor < 0) {
                        STEP_LOW_NIBBLE(pda_setmem_params[field], 15);
                    } else {
                        STEP_LOW_NIBBLE(pda_setmem_buffer[pda_setmem_cursor], 15);
                    }
                }
            } else {
                pda_setmem_crs_hold = 0;
            }
        }
        if (pda_setmem_params[SETMEM_LENGTH] > PDA_MEM_MAX_LEN) {
            pda_setmem_params[SETMEM_LENGTH] -= 0x10;
        }
        break;
    case PDA_ACTION_SHOW:
        if (pda_setmem_cursor >= 0) {
            field = -1;
        } else {
            field = pda_setmem_cursor + 5;
        }
        FntPrint("    righ: pos right   left: pos left ");
        FntPrint("    up:   pos up      down: pos down\n");
        FntPrint("    trng: inc upper   sqar: dec upper");
        FntPrint("    crcl: inc lower   cros: dec lower\n");
        FntPrint("    start address :~c558");
        for (col = 0; col < 4; col++) {
            sprintf(text, "%02x", pda_setmem_params[col]);
            if (col == field) {
                FntPrint("~c588%s~c558", text);
            } else {
                FntPrint(text);
            }
        }
        sprintf(text, "%02x", pda_setmem_params[SETMEM_LENGTH]);
        FntPrint("  ~c888len %s%s~c888 \n", (field == SETMEM_LENGTH) ? "~c588" : "~c558", text);
        FntPrint("\n~c558");
        for (row = 0; row < 8; row++) {
            base = row * 16;
            FntPrint("    ");
            for (col = 0; col < 16 && base + col < pda_setmem_params[SETMEM_LENGTH]; col++) {
                sprintf(text, "%02x ", pda_setmem_buffer[base + col]);
                if (base + col == pda_setmem_cursor) {
                    FntPrint("~c588%s~c558", text);
                } else {
                    FntPrint(text);
                }
            }
            FntPrint("\n");
            if (pda_setmem_params[SETMEM_LENGTH] < row * 16 + col) {
                break;
            }
        }
        break;
    case PDA_ACTION_EXEC:
        return McxSetMem(0, pda_setmem_buffer,
                         (pda_setmem_params[0] << 24) | (pda_setmem_params[1] << 16) | (pda_setmem_params[2] << 8) | pda_setmem_params[3],
                         pda_setmem_params[SETMEM_LENGTH]);
    case PDA_ACTION_RESULT:
        pda_print_no_data();
        break;
    }
    FntPrint("~c888");
    return 0;
}

/* Show-trans page: circle/cross set the direction (PS to PDA or PDA to PS), triangle/square
 * step the time-out in seconds; action 2 sends both. */
static s32 pda_show_trans_page(s32 action, s32 buttons) {
    s32 value;
    s32 other;
    const char *message;
    const char *format;
    switch (action) {
    case PDA_ACTION_INPUT:
        if (buttons & PAD_CIRCLE) {
            pda_trans_shown = 1;
        } else if (buttons & PAD_CROSS) {
            pda_trans_shown = 0;
        }
        if (buttons & PAD_TRIANGLE) {
            value = pda_trans_tri_hold + 1;
            pda_trans_tri_hold = value;
            if (pda_repeat_tick(value) != 0) {
                other = pda_trans_timeout;
                if (other < 0xFF) {
                    pda_trans_timeout = other + 1;
                }
            }
        } else {
            pda_trans_tri_hold = 0;
            if (buttons & PAD_SQUARE) {
                value = pda_trans_sqr_hold + 1;
                pda_trans_sqr_hold = value;
                if (pda_repeat_tick(value) != 0) {
                    value = pda_trans_timeout;
                    if (value > 0) {
                        pda_trans_timeout = value - 1;
                    }
                }
            } else {
                pda_trans_sqr_hold = 0;
            }
        }
        break;

    case PDA_ACTION_SHOW:
        FntPrint("    circle: PS to PDA    cross: PDA to PS \n");
        FntPrint("    triang: inc time     squar: dec time \n");
        format = "    dir :~c558 %s ~c888\n";
        if (pda_trans_shown != 0) {
            message = "PS to PDA";
        } else {
            message = "PDA to PS";
        }
        FntPrint(format, message);
        FntPrint("    time out :~c558 %d ~c888s \n", pda_trans_timeout);
        break;

    case PDA_ACTION_EXEC:
        return McxShowTrans(0, pda_trans_shown, pda_trans_timeout);

    case PDA_ACTION_RESULT:
        pda_print_no_data();

    default:
        break;
    }
    return 0;
}

/* LED page: circle turns the LED on, cross off; action 2 sends the flag. */
static s32 pda_set_led_page(s32 action, s32 buttons) {
    const char *format;
    const char *message;

    switch (action) {
    case PDA_ACTION_INPUT:
        if (buttons & PAD_CIRCLE) {
            pda_led_flag = 1;
        } else if (buttons & PAD_CROSS) {
            pda_led_flag = 0;
        }
        return 0;
    case PDA_ACTION_SHOW:
        FntPrint("    circle: on   cross: off \n");
        format = "    led :~c558 %s ~c888\n";
        if (pda_led_flag != 0) {
            message = "on";
        } else {
            message = "off";
        }
        FntPrint(format, message);
        return 0;
    case PDA_ACTION_EXEC:
        return McxSetLED(0, pda_led_flag);
    case PDA_ACTION_RESULT:
        pda_print_no_data();
        return 0;
    default:
        return 0;
    }
}

/* Read-device screen. Action 0: the d-pad moves the cursor over the device
 * number and the 32 fix bytes, triangle/square step the high nibble and
 * circle/cross the low one. 1 prints the editor, 2 clears the read buffer and
 * issues the read, 3 prints what was read. */
static s32 pda_read_dev_page(s32 action, s32 buttons)
{
    char text[16];
    s32 held;
    s32 cursor;
    s32 row;
    s32 col;
    u8 byte;
    s32 index;

    switch (action) {
    case PDA_ACTION_INPUT:
        if (pda_read_last_pad == 0) {
            if (buttons & PAD_RIGHT) {
                cursor = pda_read_cursor;
                if (cursor < 31) {
                    pda_read_cursor = cursor + 1;
                }
            } else if (buttons & PAD_LEFT) {
                if (pda_read_cursor >= 0) {
                    pda_read_cursor--;
                }
            }
            if (buttons & PAD_UP) {
                pda_read_cursor -= 16;
                if (pda_read_cursor < 0) {
                    pda_read_cursor = -1;
                }
            } else if (buttons & PAD_DOWN) {
                if (pda_read_cursor < 0) {
                    pda_read_cursor = 0;
                } else if (pda_read_cursor + 16 < 32) {
                    pda_read_cursor += 16;
                }
            }
        }
        pda_read_last_pad = buttons & PAD_ARROWS_MASK;
        if (buttons & PAD_TRIANGLE) {
            held = pda_read_tri_hold + 1;
            pda_read_tri_hold = held;
            if (pda_repeat_tick(held) != 0) {
                CURSOR_BYTE += 0x10;
            }
        } else {
            pda_read_tri_hold = 0;
            if (buttons & PAD_SQUARE) {
                held = pda_read_sqr_hold + 1;
                pda_read_sqr_hold = held;
                if (pda_repeat_tick(held) != 0) {
                    CURSOR_BYTE += 0xF0;
                }
            } else {
                pda_read_sqr_hold = 0;
            }
        }
        if (buttons & PAD_CIRCLE) {
            held = pda_read_cir_hold + 1;
            pda_read_cir_hold = held;
            if (pda_repeat_tick(held) != 0) {
                byte = CURSOR_BYTE;
                CURSOR_BYTE = (byte & 0xF0) + (((byte & 0xF) + 1) & 0xF);
            }
        } else {
            pda_read_cir_hold = 0;
            if (buttons & PAD_CROSS) {
                held = pda_read_crs_hold + 1;
                pda_read_crs_hold = held;
                if (pda_repeat_tick(held) != 0) {
                    byte = CURSOR_BYTE;
                    CURSOR_BYTE = (byte & 0xF0) + (((byte & 0xF) + 15) & 0xF);
                }
            } else {
                pda_read_crs_hold = 0;
            }
        }
        break;
    case PDA_ACTION_SHOW:
        FntPrint("    up/down/right/left:  pos up/down/right/left \n");
        FntPrint("    triang/square: inc/dec upper     circle/cross: inc/dec lower \n");
        FntPrint("    device no : %s%x~c888 \n", (pda_read_cursor == -1) ? "~c588" : "~c558", pda_device_block.field.device);
        for (row = 0; row < 2; row++) {
            FntPrint(row != 0 ? "          " : "    fix :~c558 ");
            for (col = 0; col < 16; col++) {
                index = col + row * 16;
                sprintf(text, "%02x ", pda_device_block.field.fix[index]);
                if (index == pda_read_cursor) {
                    FntPrint("~c588%s~c558", text);
                } else {
                    FntPrint(text);
                }
            }
            FntPrint("\n");
        }
        FntPrint("~c888");
        break;
    case PDA_ACTION_EXEC:
        for (col = 0; col < 128; col++) {
            pda_device_block.field.read[col] = 0;
        }
        return McxReadDev(0, pda_device_block.field.device, pda_device_block.field.fix, pda_device_block.field.read);
    case PDA_ACTION_RESULT:
        for (row = 0; row < 8; row++) {
            FntPrint(row != 0 ? "          " : "    read: ");
            for (col = 0; col < 16; col++) {
                sprintf(text, "%02x", pda_device_block.field.read[row * 16 + col]);
                FntPrint("%s ", text);
            }
            FntPrint("\n");
        }
        break;
    }
    return 0;
}

/* Write-device page: edits the PdaDeviceBlock at pda_device_block.
 * pda_write_cursor is the cursor: -1 is the device number, 0..0x9F a byte of
 * fix[] then read[]. Action 2 writes the device. */
static s32 pda_write_dev_page(s32 action, s32 buttons)
{
    s32 held;
    s32 row;
    s32 i;
    char buf[16];

    switch (action) {
    case PDA_ACTION_INPUT:
        if (pda_write_last_pad == 0) {
            if (buttons & PAD_RIGHT) {
                if (pda_write_cursor < 159) {
                    pda_write_cursor++;
                }
            } else if (buttons & PAD_LEFT) {
                if (pda_write_cursor >= 0) {
                    pda_write_cursor--;
                }
            }
            if (buttons & PAD_UP) {
                pda_write_cursor -= 0x10;
                if (pda_write_cursor < 0) {
                    pda_write_cursor = -1;
                }
            } else if (buttons & PAD_DOWN) {
                if (pda_write_cursor < 0) {
                    pda_write_cursor = 0;
                } else if (pda_write_cursor + 16 < 160) {
                    pda_write_cursor += 0x10;
                }
            }
        }
        pda_write_last_pad = buttons & PAD_ARROWS_MASK;
        if (buttons & PAD_TRIANGLE) {
            held = pda_write_tri_hold + 1;
            pda_write_tri_hold = held;
            if (pda_repeat_tick(held) != 0) {
                pda_device_block.bytes[pda_write_cursor + 1] += 0x10;
            }
        } else {
            pda_write_tri_hold = 0;
            if (buttons & PAD_SQUARE) {
                held = pda_write_sqr_hold + 1;
                pda_write_sqr_hold = held;
                if (pda_repeat_tick(held) != 0) {
                    pda_device_block.bytes[pda_write_cursor + 1] += 0xF0;
                }
            } else {
                pda_write_sqr_hold = 0;
            }
        }
        if (buttons & PAD_CIRCLE) {
            held = pda_write_cir_hold + 1;
            pda_write_cir_hold = held;
            if (pda_repeat_tick(held) != 0) {
                pda_device_block.bytes[pda_write_cursor + 1] = (pda_device_block.bytes[pda_write_cursor + 1] & 0xF0)
                    + (((pda_device_block.bytes[pda_write_cursor + 1] & 0xF) + 1) & 0xF);
            }
        } else {
            pda_write_cir_hold = 0;
            if (buttons & PAD_CROSS) {
                held = pda_write_crs_hold + 1;
                pda_write_crs_hold = held;
                if (pda_repeat_tick(held) != 0) {
                    pda_device_block.bytes[pda_write_cursor + 1] = (pda_device_block.bytes[pda_write_cursor + 1] & 0xF0)
                        + (((pda_device_block.bytes[pda_write_cursor + 1] & 0xF) + 0xF) & 0xF);
                }
            } else {
                pda_write_crs_hold = 0;
            }
        }
        return 0;
    case PDA_ACTION_SHOW:
        FntPrint("    up/down/right/left:  pos up/down/right/left \n");
        FntPrint("    triang/square: inc/dec upper     circle/cross: inc/dec lower \n");
        FntPrint("    device no : %s%x~c888 \n", pda_write_cursor == -1 ? "~c588" : "~c558", pda_device_block.field.device);
        for (row = 0; row < 2; row++) {
            FntPrint(row != 0 ? "          " : "    fix :~c558 ");
            for (i = 0; i < 16; i++) {
                sprintf(buf, "%02x ", pda_device_block.field.fix[i + row * 16]);
                if (i + row * 16 == pda_write_cursor) {
                    FntPrint("~c588%s~c558", buf);
                } else {
                    FntPrint(buf);
                }
            }
            FntPrint("\n");
        }
        for (row = 0; row < 8; row++) {
            FntPrint(row != 0 ? "          " : "    ~c888var :~c558 ");
            for (i = 0; i < 16; i++) {
                sprintf(buf, "%02x ", pda_device_block.field.read[row * 16 + i]);
                if (row * 16 + i == pda_write_cursor - 32) {
                    FntPrint("~c588%s~c558", buf);
                } else {
                    FntPrint(buf);
                }
            }
            FntPrint("\n");
        }
        FntPrint("~c888");
        return 0;
    case PDA_ACTION_EXEC:
        return McxWriteDev(0, pda_device_block.field.device, pda_device_block.field.fix, pda_device_block.field.read);
    case PDA_ACTION_RESULT:
        pda_print_no_data();
        return 0;
    default:
        return 0;
    }
}

/* Key repeat: nonzero on the frames a held button fires (1, 41, then every 15 frames from 81). */
static s32 pda_repeat_tick(s32 frames) {
    s32 result;

    result = 0;
    if ((frames < 81 && (frames / 40) * 40 == frames - 1) || (frames >= 81 && (frames - 80) % 15 == 1)) {
        result = 1;
    }
    return result;
}
