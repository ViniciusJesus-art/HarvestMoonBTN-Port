/* Memory-card test screen: shows the card's files, formats and unformats it, and
 * renames the save files to their released names. */

#include <sys/types.h>
#include <stdio.h>
#include <kernel.h>
#include <libgte.h>
#include <libgpu.h>
#include <libapi.h>
#include <libmcrd.h>
#include <libmcx.h>
#include "types.h"
#include "main.h"
#include "mcard.h"
#include "pad.h"

#define MCARD_BLOCK_SIZE 8192

/* The card statuses the card test reports and prints. */
enum McardStatus {
    MCARD_BROKEN = -2,        /* "Broken" */
    MCARD_UNFORMAT_PDA = -3,  /* "Unformat PDA" */
    MCARD_NONE = -4,          /* "None" */
    MCARD_NEW = -5,           /* "New" */
    MCARD_UNFORMAT_CARD = -6, /* "Unformat Card" */
    MCARD_PDA = -7,           /* "PDA" */
    MCARD_CARD = -8,          /* "Card" */
};

/* A card operation's result, as the status line shows it: done (1), refused or
 * failed (-1), or no answer from the card (0). The directory listing (2) is a
 * status of its own. */
enum McardOp {
    MCARD_OP_NOT_DONE = 0,
    MCARD_OP_ERROR = -1,
    MCARD_OP_DONE = 1,
    MCARD_OP_LISTING = 2,
};

/* The card in port 0, slot 0 is the only one the test screen uses. */
#define CARD_PORT 0
#define CARD_SLOT 0
/* MemCardGetDirentry's directory entries, and the most it returns. */
#define MCARD_MAX_ENTRIES 15
/* How many times CARD_RENAME retries a rename that failed. */
#define MCARD_RENAME_RETRIES 3

static s32 mcard_format(void);
static int mcard_unformat(void);
static int mcard_list_dir(void);
static s16 mcard_probe_type(void);
static s32 mcard_rename_save_files(void);

/* The six menu lines mcard_screen_tick prints. */
static char mcard_menu_lines[6][20] = {
    " START  : REBOOT   ",
    "M A R U : INFO     ",
    "BA  TSU : DIRECTORY",
    "SANKAKU : UNFORMAT ",
    "SHIKAKU : FORMAT   ",
    "SELECT  : RENAME   ",
};

/* MemCardGetDirentry's file count. */
static long mcard_file_count = 0;
/* The last command's status: 0 idle, an McardStatus, or an McardOp result. */
static long mcard_status = 0;

/* The two paths mcard_rename_save_files renames between. */
static char card_old_path[32];
static char card_new_path[32];
/* MemCardGetDirentry's directory entries, read back by the status screen. */
static struct DIRENTRY mcard_dirents[MCARD_MAX_ENTRIES];
/* The directory summary line mcard_list_dir writes. */
static char mcard_info_text[784];


/* One frame of the test screen: the menu, then the status line while one is showing
 * (a pressed button clears it), else the command for the pressed button. */
void mcard_screen_tick(void) {
    s32 i;
    s32 result;

    FntPrint("\n\n<<Ez MemoryCard Util.>>\n\n");
    for (i = 0; i < 6; i++) {
        FntPrint(" %s \n", mcard_menu_lines[i]);
    }
    FntPrint("\n");

    if (mcard_status == MCARD_OP_ERROR) {
        FntPrint("\n\n     <<<Error!>>>\n");
        if (input_pad1->pressed != 0) {
            mcard_status = 0;
        }
    } else if (mcard_status == MCARD_OP_DONE) {
        FntPrint("\n\n     <<<Done.>>>\n");
        if (input_pad1->pressed != 0) {
            mcard_status = 0;
        }
    } else if (mcard_status == MCARD_OP_LISTING) {
        for (i = 0; i < mcard_file_count; i++) {
            FntPrint("%s\n", mcard_dirents[i].name);
        }
        if (mcard_file_count != 0) {
            FntPrint("\n");
        }
        FntPrint("%s\n", mcard_info_text);
        if (input_pad1->pressed != 0) {
            mcard_status = 0;
        }
    } else if (mcard_status != 0) {
        FntPrint("STATUS : ");
        switch (mcard_status) {
        case MCARD_BROKEN:
            FntPrint("Broken\n");
            break;
        case MCARD_UNFORMAT_PDA:
            FntPrint("Unformat PDA\n");
            break;
        case MCARD_NONE:
            FntPrint("None\n");
            break;
        case MCARD_NEW:
            FntPrint("New\n");
            break;
        case MCARD_UNFORMAT_CARD:
            FntPrint("Unformat Card\n");
            break;
        case MCARD_PDA:
            FntPrint("PDA\n");
            break;
        case MCARD_CARD:
            FntPrint("Card\n");
            break;
        }
        if (input_pad1->pressed != 0) {
            mcard_status = 0;
        }
    } else {
        result = 0;
        switch (input_pad1->pressed) {
        case PAD_START:
            main_request_scene(SCENE_DEBUG);
            break;
        case PAD_TRIANGLE:
            result = mcard_unformat();
            break;
        case PAD_SQUARE:
            result = mcard_format();
            break;
        case PAD_CROSS:
            result = mcard_list_dir();
            break;
        case PAD_CIRCLE:
            result = mcard_probe_type();
            break;
        case PAD_SELECT:
            result = mcard_rename_save_files();
            break;
        }
        if (result != 0) {
            mcard_status = result;
        }
    }
}

/* Format the card in port 0. Returns 1 when the format succeeds, -1 when the card
 * answers 1 or 2, and 0 for any other answer. */
static s32 mcard_format(void) {
    long syncCmd;
    long syncResult;
    s32 ret;

    ret = MCARD_OP_NOT_DONE;
    MemCardSync(CARD_PORT, &syncCmd, &syncResult);
    syncResult = MemCardFormat(CARD_PORT);
    if (syncResult != 0) {
        if (syncResult >= 0) {
            if (syncResult < 3) {
                ret = MCARD_OP_ERROR;
            }
        }
    } else {
        ret = MCARD_OP_DONE;
    }
    return ret;
}

/* Unformat the card in port 0. The card's answer maps to MCARD_OP_DONE (1) or
 * MCARD_OP_ERROR (-1 or 0); any other answer gives 0. */
static int mcard_unformat(void) {
    long status1;
    long status2;
    int result = 0;

    MemCardSync(CARD_PORT, &status1, &status2);
    status2 = MemCardUnformat(CARD_PORT);
    switch (status2) {
    case MCARD_OP_ERROR:
    case MCARD_OP_NOT_DONE:
        result = MCARD_OP_ERROR;
        break;
    case MCARD_OP_DONE:
        result = MCARD_OP_DONE;
        break;
    }
    return result;
}


/* Read the card's directory into mcard_dirents and build the summary line: the file
 * count and the blocks they take (each file rounded up to a whole block). */
static int mcard_list_dir(void) {
    long status1;
    long status2;
    int i;
    int n;
    int blocks;

    MemCardSync(CARD_PORT, &status1, &status2);
    MemCardGetDirentry(CARD_PORT, "*", mcard_dirents, &mcard_file_count, 0, MCARD_MAX_ENTRIES);

    blocks = 0;
    n = mcard_file_count;
    for (i = 0; i < n; i++) {
        long size = mcard_dirents[i].size;
        long q = size / MCARD_BLOCK_SIZE;
        if (size & (MCARD_BLOCK_SIZE - 1)) {
            blocks += q + 1;
        } else {
            blocks += q;
        }
    }
    sprintf(mcard_info_text, "%d file(s) %d block(s)\n", mcard_file_count, blocks);
    return MCARD_OP_LISTING;
}

/* Check the card in slot 0 and return one of the MCARD_* statuses
   mcard_screen_tick prints; otherwise the last sync's return value. A card that
   answers libmcrd is then asked by libmcx whether it is a PDA. */
static s16 mcard_probe_type(void) {
    long cmd;
    long result;
    s32 status;

    MemCardSync(CARD_PORT, &cmd, &result);
    MemCardAccept(CARD_PORT);
    status = MemCardSync(CARD_PORT, &cmd, &result);
    switch (result) {
    case McErrCardInvalid:
        status = MCARD_BROKEN;
        break;
    case McErrNotFormat:
        McxCardType(CARD_PORT);
        status = McxSync(CARD_PORT, &cmd, &result);
        switch (result) {
        case McxErrSuccess:
            status = MCARD_UNFORMAT_PDA;
            break;
        case McxErrInvalid:
            status = MCARD_UNFORMAT_CARD;
            break;
        case McxErrNewCard:
            status = MCARD_NEW;
            break;
        case McxErrNoCard:
            status = MCARD_NONE;
            break;
        }
        break;
    case McErrNone:
        McxCardType(CARD_PORT);
        status = McxSync(CARD_PORT, &cmd, &result);
        switch (result) {
        case McxErrSuccess:
            status = MCARD_PDA;
            break;
        case McxErrInvalid:
            status = MCARD_CARD;
            break;
        case McxErrNewCard:
            status = MCARD_NEW;
            break;
        case McxErrNoCard:
            status = MCARD_NONE;
            break;
        }
        break;
    case McErrNewCard:
        status = MCARD_NEW;
        break;
    case McErrCardNotExist:
        status = MCARD_NONE;
        break;
    }
    return status;
}

/* Rename a save file on the card in port 0, slot 0 (libmcrd's "bu<port><slot>:<name>")
 * from its development name to the released one, trying up to four times. */
#define CARD_RENAME(old_name, new_name)                                             \
    {                                                                               \
        s32 i;                                                                      \
                                                                                    \
        sprintf(card_old_path, "bu%d%d:%s", CARD_PORT, CARD_SLOT, old_name);        \
        sprintf(card_new_path, "bu%d%d:%s", CARD_PORT, CARD_SLOT, new_name);        \
        i = 0;                                                                      \
        do {                                                                        \
            if (rename(card_old_path, card_new_path) == 1) {                        \
                break;                                                              \
            }                                                                       \
            if (i >= MCARD_RENAME_RETRIES) {                                        \
                break;                                                              \
            }                                                                       \
            i++;                                                                    \
        } while (1);                                                                \
    }

/* The save files were written under product code 00000 during development; the released
 * game uses SLPS-02489. */
static s32 mcard_rename_save_files(void) {
    CARD_RENAME("BISLPS-00000-FARM0", "BISLPS-02489-FARM0")
    CARD_RENAME("BISLPS-00000-FARM1", "BISLPS-02489-FARM1")
    CARD_RENAME("BISLPS-00000-FARM2", "BISLPS-02489-FARM2")
    CARD_RENAME("BISLPSP00000-FARMP", "BISLPSP02489-FARMP")
    return MCARD_OP_DONE;
}
