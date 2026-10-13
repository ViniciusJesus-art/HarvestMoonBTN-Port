/* The game's data files on the disc: reading a file by name from the CD, and the entries of the A_FILE archive. */
#include <sys/types.h>
#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>
#include <libcd.h>
#include <memory.h>
#include "types.h"
#include "afile.h"
#include "cd_audio.h"
#include "farm_state.h"

/* Read the whole CD file `name` into `destination`, waiting until the read has drained. */
static s32 cd_read_file(char *name, u_long *destination) {
    CdlFILE file;
    s32 sector;

    ResetCallback();
    DrawSync(0);
    while (CdSearchFile(&file, name) == 0) {
    }
    sector = (file.size + DISC_SECTOR_MASK) >> DISC_SECTOR_SHIFT;
    CdControl(CdlSetloc, (u_char *)&file.pos, 0);
    CdRead(sector, destination, 0x80);
    while (CdReadSync(1, 0) > 0) {
        VSync(0);
    }
    return 0;
}

/* Scratch CdlLOC: the CD position of the sector being read, filled by CdIntToPos. */
CdlLOC afile_cd_loc[2];
/* The arguments of the last range read started, kept so afile_poll_range can restart it. */
s32 afile_range_start;
s32 afile_range_end;
u8 *afile_range_buf;
s32 afile_offset;
s32 afile_start_index;
/* Set to 1 once the CD read has drained; afile_poll_range sets it. */
s32 afile_busy;
/* 1 while a non-blocking A_FILE read is in flight. */
s32 afile_read_active;
s16 afile_read_entry;
s32 afile_read_offset;
u8 *afile_read_dest;
s32 afile_read_size;
s32 afile_unused_8005E424;
s32 afile_sector;
s32 afile_unused_8005E42C;
s32 afile_base_sector;

s32 afile_range_active = 0;
u32 afile_stream_state = 0;

/* The A_FILE members in archive order: each one's flag (1 for most objcg\ entries) and its
 * path, with the start position the header table fills in. */
static AFileRecord afile_members[33] = {
    {1, "objcg\\gf.bin"},
    {1, "objcg\\face.bin"},
    {1, "objcg\\messege0.bin"},
    {1, "objcg\\messege1.bin"},
    {1, "objcg\\messege2.bin"},
    {1, "objcg\\messege3.bin"},
    {1, "objcg\\messege4.bin"},
    {1, "data\\font\\Font_all.tex"},
    {0, "obj\\DEBUG.bin"},
    {1, "objcg\\sound.stm"},
    {0, "obj\\PdaSamp.bin"},
    {0, "obj\\GF_FARM.bin"},
    {1, "objcg\\event_s.bin"},
    {1, "objcg\\evch.bin"},
    {1, "ObjCg\\Status.Bin"},
    {0, "Obj\\saveload.bin"},
    {1, "ObjCg\\Slp.Bin"},
    {0, "Obj\\GF_MG1.Bin"},
    {1, "ObjCg\\MG1.Bin"},
    {0, "Obj\\GF_MG2.Bin"},
    {1, "ObjCg\\MG2.Bin"},
    {0, "Obj\\GF_MG3.Bin"},
    {1, "ObjCg\\MG3.Bin"},
    {0, "Obj\\GF_MG4.Bin"},
    {1, "ObjCg\\MG4.Bin"},
    {0, "Obj\\GF_MG5.Bin"},
    {1, "ObjCg\\MG5.Bin"},
    {0, "Obj\\GF_TITLE.Bin"},
    {0, "Obj\\GF_STAFF.Bin"},
    {1, "ObjCg\\MesEv.Bin"},
    {0, "Obj\\GF_swind.bin"},
    {0, "Obj\\gf_mcard.bin"},
    {1, "NOTHING"},
};

/* Read the bytes of A_FILE entries [startIndex, endIndex) into `dst`, through `buf`: the
 * whole sectors are read there first, then the entries are moved down to `dst`. Waits
 * until the read has finished. */
s32 afile_read_range(s32 startIndex, s32 endIndex, u8 *dst, u8 *buf) {
    u32 *startPos;
    u32 *endPos;
    s32 sectorCount;
    s32 nbytes;
    u32 startWord;
    u32 endWord;
    u32 startSector;
    u8 loc;
    u8 *payload;

    ResetCallback();
    DrawSync(0);
    endPos = &afile_members[endIndex].pos;
    startPos = &afile_members[startIndex].pos;
    do {
        startWord = *startPos;
        endWord = *endPos;
        startSector = startWord >> DISC_SECTOR_SHIFT;
        sectorCount = (endWord >> DISC_SECTOR_SHIFT) - startSector;
        sectorCount = sectorCount + ((endWord & DISC_SECTOR_MASK) != 0);
        afile_offset = startWord & DISC_SECTOR_MASK;
        afile_sector = startSector + afile_base_sector;
        nbytes = endWord - startWord;
        CdSync(0, 0);
        CdControl(CdlSetloc, (u_char *)CdIntToPos(afile_sector, afile_cd_loc), 0);
        loc = 0x80;
        while (CdControl(0xE, &loc, 0) == 0) {
        }
        VSync(3);
        CdRead(sectorCount, (u_long *)buf, 0x80);
    } while (CdReadSync(0, 0) != CD_READ_DONE);
    payload = buf + afile_offset;
    if (dst != payload) {
        memmove(dst, payload, nbytes);
    }
    return 0;
}

/* The size of A_FILE entry `index`, rounded up to whole words: the distance
 * from its start to the next entry's. */
static u32 afile_entry_size(s32 index) {
    u32 start;
    u32 end;

    ResetCallback();
    DrawSync(0);
    if (afile_members[index].flag) {
    }
    end = afile_members[index + 1].pos;
    start = afile_members[index].pos;
    return (end - start + 3) / 4 * 4;
}

/* Read the A_FILE.HDT member table into the members' start positions (through game_state,
 * used as scratch), then find the first sector of A_FILE.BIN on the disc. */
s32 afile_init(void) {
    CdlFILE file;
    u_long *src;
    char *hdtName;
    char *binName;
    s32 i;

    hdtName = "\\A_FILE.HDT;1";
    binName = "\\A_FILE.BIN;1";
    src = (u_long *)&game_state;
    cd_read_file(hdtName, src);
    for (i = 0; i <= 32; i++) {
        afile_members[i].pos = src[i];
    }
    while (CdSearchFile(&file, binName) == 0) {
    }
    afile_base_sector = afile_sector = CdPosToInt(&file.pos);
    return 0;
}

/* Start a non-blocking read of A_FILE entries [startIndex, endIndex) into buf,
 * so that the first entry's data lands at buf itself. Returns 1 while a read is
 * already in flight, else 0 once the read is issued. */
static s32 afile_start_range(s32 startIndex, s32 endIndex, u8 *buf) {
    u32 startPos;
    u32 endPos;
    u32 startSector;
    s32 sectorCount;
    u8 *destination;

    startPos = afile_members[startIndex].pos;
    endPos = afile_members[endIndex].pos;
    afile_start_index = startIndex;
    startSector = startPos >> DISC_SECTOR_SHIFT;
    sectorCount = (endPos >> DISC_SECTOR_SHIFT) - startSector;
    sectorCount += (endPos & DISC_SECTOR_MASK) != 0;
    afile_offset = startPos & DISC_SECTOR_MASK;
    afile_sector = startSector + afile_base_sector;
    if (afile_range_active == 0) {
        while(1) {
            CdSync(0, 0);
            afile_range_start = startIndex;
            afile_range_end = endIndex;
            afile_range_buf = buf;
            destination = buf - afile_offset;
            if (CdControl(CdlSetloc, (u_char *)CdIntToPos(afile_sector, afile_cd_loc), 0) != 0
                && CdRead(sectorCount, (u_long *)destination, 0x80) != 0) {
                afile_range_active = 1;
                break;
            }
        }
        afile_busy = 0;
        return 0;
    }
    return 1;
}

/* Start (no read in flight, or mode 1) or poll a non-blocking read of `size`
 * bytes at `offset` in archive file `entry` into `dest`. Starting retries
 * until CdRead accepts and returns 0; polling returns 1 when the read is done,
 * 0 while it runs (a failed read restarts itself), -1 when none is in flight. */
s32 afile_read_async(s16 entry, u32 offset, u32 size, u8 *dest, s32 mode) {
    s32 sectors;
    u_char control;
    s32 slack = 1; /* one sector more: offset need not start on a sector */

    afile_sector = (afile_members[entry].pos >> DISC_SECTOR_SHIFT) + (offset >> DISC_SECTOR_SHIFT) + afile_base_sector;
    sectors = ((size + DISC_SECTOR_MASK) >> DISC_SECTOR_SHIFT) + slack;
    if (afile_read_active == 0 || mode == 1) {
        while (1) {
            CdReadBreak();
            CdFlush();
            CdSync(0, 0);
            afile_read_entry = entry;
            afile_read_offset = offset;
            afile_read_dest = dest;
            afile_read_size = size;
            if (CdControl(CdlSetloc, (u_char *)CdIntToPos(afile_sector, afile_cd_loc), 0) == 0) {
                continue;
            }
            control = 0x80;
            while (CdControl(0xE, &control, 0) == 0) {
            }
            VSync(3);
            if (CdRead(sectors, (u_long *)dest, 0x80) != 0) {
                afile_read_active = 1;
                return 0;
            }
        }
    }
    if (afile_read_active != 0) {
        switch (CdReadSync(1, 0)) {
        case CD_READ_DONE:
            afile_read_active = 0;
            return 1;
        case CD_READ_ERROR:
            afile_read_active = 0;
            return afile_read_async(entry, offset, size, dest, mode);
        default:
            return 0;
        }
    }
    return -1;
}

/* Whether the last range read has finished. */
static signed int afile_range_done(void) {
    return afile_busy;
}

/* Poll the range read started by afile_start_range: 1 when it has finished, 0 while it runs
 * (a failed read is restarted), -1 when none is in flight. */
s32 afile_poll_range(void) {
    if (afile_range_active != 0) {
        switch (CdReadSync(1, 0)) {
        case CD_READ_DONE:
            afile_range_active = 0;
            afile_busy = 1;
            return 1;
        case CD_READ_ERROR:
            afile_range_active = 0;
            afile_start_range(afile_range_start, afile_range_end, afile_range_buf);
            afile_busy = 0;
            return 0;
        default:
            afile_busy = 0;
            return 0;
        }
    }
    return -1;
}

/* Read `end` bytes from byte `start` of A_FILE.BIN into
 * `destination`, blocking until the read is done. Mode -1 only seeks. */
void afile_read_sectors(s32 unused, u32 start, u32 end, s32 mode, u8 *destination) {
    s32 startSector;
    s32 endSector;
    u8 control;

    startSector = start >> DISC_SECTOR_SHIFT;
    endSector = end >> DISC_SECTOR_SHIFT;
    do {
        afile_sector = startSector + afile_base_sector;
        CdReadBreak();
        CdFlush();
        CdSync(0, 0);
        CdControl(CdlSetloc, (u_char *)CdIntToPos(afile_sector, afile_cd_loc), 0);
        if (mode == -1) {
            break;
        }
        control = 0x80;
        do {
        } while (CdControl(0xE, &control, 0) == 0);
        VSync(3);
        CdRead(endSector, (u_long *)destination, 0x80);
    } while (CdReadSync(0, 0) != CD_READ_DONE);
}

/* True when no streamed read is pending. */
static unsigned int afile_read_idle(void) {
    return afile_stream_state == 0;
}

/* Read `size` bytes at `offset` into file `file` of the archive into `buf`,
 * in whole sectors, without blocking. The first call (or any call with
 * `restart` 1) starts the read and returns 0; later calls return 0 while it
 * runs and, once it has finished, the address of the requested bytes -- moved
 * down to `buf` itself when `align` is 1. A failed read is restarted. */
u8 *afile_read_bytes(s32 file, s32 offset, s32 size, u8 *buf, s32 align, s32 restart) {
    u32 position;
    u32 end;
    u32 firstSector;
    u32 endSector;
    s32 sectors;
    s32 skip;
    u8 *result;
    u8 *data;

    result = 0;
    position = afile_members[file].pos + offset;
    end = position + size;
    endSector = end >> DISC_SECTOR_SHIFT;
    endSector += (end & DISC_SECTOR_MASK) != 0;
    firstSector = position >> DISC_SECTOR_SHIFT;
    skip = position & DISC_SECTOR_MASK;
    sectors = endSector - firstSector;
    if (afile_read_active == 0 || restart == 1) {
        cd_audio_stop();
        afile_sector = firstSector + afile_base_sector;
        CdReadBreak();
        CdFlush();
        CdSync(0, 0);
        CdControl(CdlSetloc, (u_char *)CdIntToPos(afile_sector, afile_cd_loc), 0);
        CdRead(sectors, (u_long *)buf, 0x80);
        afile_read_active = 1;
        if (restart == 1) {
            afile_stream_state = restart;
        }
    } else {
        switch (CdReadSync(1, 0)) {
        case CD_READ_DONE:
            if (align == 1) {
                data = buf + skip;
                if (buf != data) {
                    memmove(buf, data, size);
                }
                result = buf;
            } else {
                result = buf + skip;
            }
            afile_read_active = 0;
            afile_stream_state = 0;
            break;
        case CD_READ_ERROR:
            afile_read_active = 0;
            afile_read_bytes(file, offset, size, buf, align, restart);
            /* fall through: restarted, so still busy */
        default:
            result = 0;
            afile_stream_state = 1;
            break;
        }
    }
    return result;
}
