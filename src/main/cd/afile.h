#ifndef AFILE_H
#define AFILE_H


/* A_FILE.BIN, the game archive: reading a file by name, by range or by sector. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

/* One entry of the A_FILE directory. */
typedef struct AFileRecord {
    u32  flag;              /* 1 for most objcg\ entries, 0 for the obj\ overlays */
    char path[0x20];        /* ASCII path ("objcg\gf.bin", ...), NUL-padded */
    u32  pos;               /* entry start byte position on disc */
} AFileRecord;

/* What CdReadSync reports: 0 when the read has finished, -1 when it failed, and
 * a positive count while it is still reading. */
enum CdReadStatus {
    CD_READ_DONE = 0,
    CD_READ_ERROR = -1
};

/* A CD sector is 2048 bytes: the sector is the byte offset >> 11, the rest the offset in it. */
#define DISC_SECTOR_SIZE 0x800
#define DISC_SECTOR_SHIFT 11
#define DISC_SECTOR_MASK 0x7FF

/* The staging buffer, in the region at region_base: file reads land here. */
#define STAGING_BUFFER (region_base + 0x4290)

/* Where a picture's CLUT is staged, behind the picture itself. */
#define STAGING_CLUT (STAGING_BUFFER + 0x800)

s32 afile_read_range(s32 startIndex, s32 endIndex, u8 *dst, u8 *buf);
s32 afile_init(void);
s32 afile_read_async(s16, u32, u32, u8 *, s32);
s32 afile_poll_range(void);
void afile_read_sectors(s32 unused, u32 start, u32 end, s32 mode, u8 *destination);
u8 *afile_read_bytes(s32 file, s32 offset, s32 size, u8 *buf, s32 align, s32 restart);

/* The overlays in A_FILE, by member number. */
enum {
    AFILE_DEBUG = 8,
    AFILE_PDA_SAMPLE = 10,
    AFILE_FARM = 11,
    AFILE_SAVELOAD = 15,
    AFILE_MG1 = 17,
    AFILE_MG2 = 19,
    AFILE_MG3 = 21,
    AFILE_MG4 = 23,
    AFILE_MG5 = 25,
    AFILE_TITLE = 27,
    AFILE_STAFF = 28,
    AFILE_SWIND = 30,
    AFILE_MCARD = 31
};

#endif /* AFILE_H */
