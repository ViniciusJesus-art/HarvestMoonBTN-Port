/* Loading the farm's resources from the CD: the sections, the season and item art, the
 * sprite and texture blocks. */

#include <memory.h>
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "types.h"
#include "afile.h"
#include "anim.h"
#include "compiled_out.h"
#include "event_commands.h"
#include "event_flags.h"
#include "event_script.h"
#include "farm_data.h"
#include "farm_data_size.h"
#include "farm_load.h"
#include "farm_state.h"
#include "farm_vars.h"
#include "field.h"
#include "main.h"
#include "scene.h"
#include "texture_pack.h"

/* The 0x770-byte header at the start of the loaded block. */
typedef struct FarmBlockHeader {
    u8 bytes[0x770];
} FarmBlockHeader;

/* The sizes the anim8 block's first and later loads copy, in bytes. */
enum FarmAnim8Size {
    FARM_ANIM8_FIRST_SIZE = 2592,
    FARM_ANIM8_SIZE = 2104,
};

/* The item art sits this far into the region after the slot art. */
#define FARM_ITEM_ART_OFFSET 0x4290
/* The player's second sprite bank sits this far into the farm's load window. */
#define FARM_PLAYER_BANK_OFFSET 0xBE08

/* Where the cabinet and icebox records sit in the region, and the file offsets of the
 * sector rows, the store tables and the sector groups. */
#define FARM_CABINET_ITEMS_OFFSET 0x4410
#define FARM_ICEBOX_ITEMS_OFFSET 0x4590
/* A sector row is seven sectors. */
#define FARM_SECTOR_ROW_SECTORS 7
#define FARM_SECTOR_ROW_BASE 0x03CB8800
#define FARM_STORE_TABLES_FILE 0x3D57800
#define FARM_SECTOR_GROUP_BASE 0x3D5A800
/* A sector group is three sectors of 0x800 bytes. */
#define FARM_SECTOR_GROUP_SECTORS 3

/* How farm_load_resource_id hands a pack on: the slot art, or the resource buffer. */
enum FarmResourceKind {
    FARM_RESOURCE_KIND_NONE = 0,
    FARM_RESOURCE_KIND_SLOT_ART = 1,
    FARM_RESOURCE_KIND_BUFFER = 2,
};

static void farm_load_store_tables(void);

#include "overlay/common/farm_section_table.h"

/* Load farm data section `index`. The section's first part is read from CD unless `cached`
 * is set; its script head becomes the mode, and the store tables reload when the swind flag
 * changed. The rest of the section feeds the texture pack, the farm events and the event map. */
void farm_load_section(s32 index, s32 cached)
{
    u32 *row = &section_table[index * 4];
    FarmLoadArea *far;

    farm_section_size = row[1] - row[0];
    if (cached == 0) {
        afile_read_sectors(0, row[0], farm_section_size, 0, farm_area.sectionBuffer);
    }
    *(FarmModeScriptImage *)&farm_area.mode.script = *(FarmModeScriptImage *)farm_area.sectionBuffer;
    far = &farm_area;
    far->mode.place = far->mode.script.head.place;
    far->mode.data = &farm_area.mode.script;
    if (event_flag_get(EVENT_FLAG_SWIND_CHANGED) == 1) {
        farm_load_store_tables();
    }
    afile_read_sectors(0, row[1], row[2] - row[1], 0, STAGING_BUFFER);
    texture_pack_load(STAGING_BUFFER);
    afile_read_sectors(0, row[2], row[4] - row[2], 0, STAGING_BUFFER);
    farm_events_load((struct FarmTable *)STAGING_BUFFER);
    event_map_upload(STAGING_BUFFER + (row[3] - row[2]));
}

void farm_startup_pause_hook(void) {
}

/* Bind the player's second sprite bank to the farm's load window. */
void farm_load_player_sprite(void) {
    anim_bind_file(&game_state.player.secondSprite, (AnimFile *)farm_area.loadWindow);
    anim_set_bank(&game_state.player.secondSprite, (ImageBank *)(farm_area.loadWindow + FARM_PLAYER_BANK_OFFSET));
}

/* Where the four seasons' slot packs sit in the farm file, indexed by season (SEASON_SPRING
 * to SEASON_WINTER); each entry ends where the next begins, so the fifth is the end of winter's pack. */
static u32 farm_season_file_offsets[5] = {
    0x03845800, 0x03849800, 0x0384D000, 0x03852800, 0x03859000,
};

/* Where the slot packs sit in the farm file; each entry ends where the next begins. */
static u32 farm_slot_file_offsets[7] = {
    0x03810000, 0x03833000, 0x03845800, 0x03849800, 0x0384D000, 0x03852800,
    0x03859000,
};

/* Where the ten item art packs sit in the farm file; each entry ends where the next begins. */
static u32 farm_item_file_offsets[11] = {
    0x0385F800, 0x0386A800, 0x03877800, 0x03883800, 0x0388C000, 0x03898800,
    0x038A3800, 0x038A7800, 0x038AA000, 0x038B0000, 0x038B3000,
};

/* Run boundaries: farm_load_span loads the span between entry[i] and entry[i + 1]. */
static u32 farm_span_offsets[3] = {
    0x037F5800, 0x03805800, 0x0380B000,
};

/* Load span `index`: the bytes between two of farm_span_offsets go to the span buffer. */
void farm_load_span(s32 index) {
    u32 *entry;

    entry = &farm_span_offsets[index];
    afile_read_sectors(0, entry[0], entry[1] - entry[0], 0, STAGING_BUFFER);
    memcpy(farm_area.spanBuffer, STAGING_BUFFER, entry[1] - entry[0]);
}

static s32 farm_anim8_offsets[3] = {
    0x037E3800, 0x037ED800, 0x037F5800,
};

/* Load anim8 part `index` for the farm and place 3: the first part copies its first
 * 2592 bytes, the later ones 2104; the texture pack follows them. */
void farm_load_anim8(s32 index) {
    s16 status;
    s32 amount;
    s32 *entry;
    FarmLoadArea *farm;
    farm = &farm_area;
    if ((farm->mode.place.location == PLACE_FARM) || (farm->mode.place.location == 3)) {
        DrawSync(0);
        ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
        amount = FARM_ANIM8_FIRST_SIZE;
        entry = &farm_anim8_offsets[index];
        afile_read_sectors(0, entry[0], entry[1] - entry[0], 0, STAGING_BUFFER);
        if (index != 0) {
            amount = FARM_ANIM8_SIZE;
        }
        memcpy(farm->anim8, STAGING_BUFFER, amount);
        status = farm->mode.place.location;
        if (status == 3) {
            texture_pack_load_at(STAGING_BUFFER + amount, -256, 0, 0, 0);
        } else {
            texture_pack_load(STAGING_BUFFER + amount);
        }
    }
}

void farm_clear_loaded_slot(void) {
    farm_loaded_slot = -1;
}

static void farm_debug_print(void) {
    char text[0x18];
    DEBUG_PRINT_BUF(text);
}

/* Start or poll the async read of section `index` into the section buffer; returns 1 once
 * it has finished. */
s32 farm_section_load_async(s32 index, s32 restart) {
    u32 *row = &section_table[index * 4];
    return afile_read_bytes(0, row[0], row[1] - row[0],
                            farm_area.sectionBuffer, 1, restart) != 0;
}

/* Read farm resource `slot` from CD into the staging buffer. Its head goes to
 * the animation buffer and the rest is handed on to texture_pack_load. */
void farm_load_resource(s32 slot)
{
    u32 *offsets = &farm_slot_file_offsets[slot];
    AnimPackToc *toc;
    s32 size;

    afile_read_sectors(0, offsets[0], offsets[1] - offsets[0], 0, STAGING_BUFFER);
    farm_area.mode.loadedSlot = slot;
    toc = farm_slot_tocs[slot];
    size = toc->count;
    size = ANIM_PACK_START(toc, size);
    memcpy(farm_area.slotAnims, STAGING_BUFFER, size);
    texture_pack_load(STAGING_BUFFER + size);
}

/* Load the season art of `index`: its animation head goes to seasonArt, the rest to the
 * texture packs. */
void farm_load_season_art(s32 index) {
    u32 *entry;
    AnimPackToc *toc;
    s32 value;

    entry = &farm_season_file_offsets[index];
    toc = farm_slot_tocs[index + 2];
    value = ANIM_PACK_START(toc, toc->count);
    afile_read_sectors(0, entry[0], entry[1] - entry[0], 0, STAGING_BUFFER);
    memcpy(farm_area.seasonArt, STAGING_BUFFER, value);
    texture_pack_load(STAGING_BUFFER + value);
}

/* Load the slot-six art: its animation head goes to slotSixArt, the rest to the texture packs. */
void farm_load_slot_six(void) {
    afile_read_sectors(0, 0x380B000, 0x5000, 0, STAGING_BUFFER);
    farm_copy_words((u32 *)farm_area.slotSixArt, (const u32 *)STAGING_BUFFER, 0x540);
    texture_pack_load(region_base + 0x5790);
}

void farm_load_hook(void) {
}

/* Load item art `id` (none for -1): its animation head goes to itemArt, the texture packs
 * load past FARM_ITEM_ART_OFFSET, and the item's sprites are bound. */
void farm_load_item_art(s32 id) {
    u32 *entry;
    AnimPackToc *toc;
    s32 destination;
    u8 *offset;

    if (id == -1) {
        return;
    }
    entry = farm_item_file_offsets + id;
    toc = farm_item_tocs[id];
    destination = ANIM_PACK_START(toc, toc->count);
    offset = STAGING_BUFFER;
    afile_read_sectors(0, entry[0], entry[1] - entry[0], 0, offset);
    memcpy(farm_area.itemArt, STAGING_BUFFER, destination);
    destination += FARM_ITEM_ART_OFFSET;
    texture_pack_load(region_base + destination);
    item_art_bind_sprites(id);
}

/* Read the fixed 0xA800-byte block into the staging buffer. */
void farm_load_fixed_block(void) {
    afile_read_sectors(0, 0x74800, 0xA800, 0, STAGING_BUFFER);
}

/* A_FILE offsets of the packs farm_load_resource_id loads, by resource id. */
static u32 farm_pack_file_offsets[51] = {
    0x038B3000, 0x038B8800, 0x038C1000, 0x038C7000, 0x038CC800, 0x038D3800, 0x038DD000,
    0x038E6800, 0x038EE000, 0x038F4800, 0x038F8000, 0x03902800, 0x03904000, 0x03910000,
    0x03917800, 0x03938000, 0x0395D800, 0x0397A800, 0x0399D000, 0x039BE000, 0x039D6800,
    0x039F9000, 0x03A1B000, 0x03A35800, 0x03A57800, 0x03A7B000, 0x03A94800, 0x03AB6800,
    0x03ADB800, 0x03AF7800, 0x03B1D000, 0x03B3D800, 0x03B5B000, 0x03B7F800, 0x03BA1800,
    0x03BC1000, 0x03BC6000, 0x03BCD000, 0x03BD3800, 0x03BD8000, 0x03BDE800, 0x03BE5000,
    0x03BE9000, 0x03BEF000, 0x03BF5000, 0x03BFA000, 0x03C01000, 0x03C07800, 0x03C0C000,
    0x03C12800, 0x03C19000,
};

/* Load a farm resource pack by id. Id 14 is picked by the time of day and the
 * weekday; id 0x3E6 by the wife's index and the child's level, and its second
 * part is loaded too. */
void farm_load_resource_id(s16 id, s16 sync)
{
    FarmState *farm;
    FarmState *state;
    s32 kind = FARM_RESOURCE_KIND_NONE;
    s16 index;
    s32 choice;
    AnimPackToc *toc;
    u32 *offsets;
    u8 *buffer;
    s32 size;
    s32 count;

    if (id == -1) {
        return;
    }
    if (id == FARM_RESOURCE_DAY) {
        farm = &game_state;
        id = farm->progress.clock.elapsed.hours / FARM_RESOURCE_HOURS_PER_PART;
        if (id >= FARM_RESOURCE_PARTS) {
            id = FARM_RESOURCE_PARTS - 1;
        }
        if (id == 1) {
            event_flag_set(EVENT_FLAG_NOON_12H, 1);
        }
        if (id == 2) {
            event_flag_set(EVENT_FLAG_EVENING_18H, 1);
        }
        kind = FARM_RESOURCE_KIND_SLOT_ART;
        id += farm->progress.clock.date.weekday * FARM_RESOURCE_PARTS;
        id += FARM_RESOURCE_DAY;
    }
    if (id == FARM_RESOURCE_WIFE) {
        state = &game_state;
        index = state->family.wifeIndex;
        if (index == -1) {
            return;
        }
        id = index * FARM_RESOURCE_PARTS + FARM_RESOURCE_WIFE_BASE;
        kind = FARM_RESOURCE_KIND_BUFFER;
        choice = (u16)state->family.childLevel;
        id += choice;
    }
    toc = farm_pack_tocs[id];
    count = toc->count;
    size = ANIM_PACK_START(toc, count);
    offsets = &farm_pack_file_offsets[id];
    buffer = STAGING_BUFFER;
    if (sync) {
        DrawSync(0);
    }
    afile_read_sectors(0, offsets[0], offsets[1] - offsets[0], 0, buffer);
    if (kind == FARM_RESOURCE_KIND_SLOT_ART) {
        memcpy(farm_area.slotAnims, buffer, size);
    } else {
        memcpy(farm_area.mode.resourceBuffer, buffer, size);
    }
    texture_pack_load(buffer + size);
    if (kind == FARM_RESOURCE_KIND_BUFFER) {
        toc = farm_pack_tocs[id];
        count = toc->count;
        size = ANIM_PACK_START(toc, count + 1);
        texture_pack_load(buffer + size);
    }
}

/* Read sector row `row` (seven sectors) into the sector buffer. */
void farm_load_sector_row(s16 row) {
    s32 a = row;
    s32 p = (a * FARM_SECTOR_ROW_SECTORS) << 11;
    farm_sector_row = row;
    afile_read_sectors(0, p + FARM_SECTOR_ROW_BASE, 0x3800, 0, farm_area.sectorBuffer);
}

/* Read the store's item tables: the toolbox items come from the staging buffer, the cabinet
 * and icebox items from the region. */
static void farm_load_store_tables(void) {
    u16 *source;

    afile_read_sectors(0, FARM_STORE_TABLES_FILE, 0x800, 0, STAGING_BUFFER);
    source = (u16 *)STAGING_BUFFER;
    farm_store_source = source;
    farm_copy_halfwords((u16 *)game_state.progress.estate.toolboxItems, source, 0xC0);
    farm_copy_halfwords((u16 *)game_state.progress.estate.cabinetItems,
                        (u16 *)(region_base + FARM_CABINET_ITEMS_OFFSET), 0xC0);
    farm_copy_halfwords((u16 *)game_state.progress.estate.iceboxItems,
                        (u16 *)(region_base + FARM_ICEBOX_ITEMS_OFFSET), 0xC0);
}

/* Read sector group `group` (three sectors) into the sector buffer. */
void farm_load_sector_group(s32 group) {
    afile_read_sectors(0, ((group * FARM_SECTOR_GROUP_SECTORS) << 11) + FARM_SECTOR_GROUP_BASE, 0x1800, 0, farm_area.sectorBuffer);
}

/* Read the texture block into the staging buffer and load it as texture packs. */
void farm_load_texture_block(void) {
    u8 *p;
    p = STAGING_BUFFER;
    afile_read_sectors(0, 0x3D51000, 0x6800, 0, p);
    texture_pack_load(STAGING_BUFFER);
}

/* Read the 0x4800-byte block into a frame's primitive buffer (the one not in use), keep its
 * 0x770-byte header in the resource buffer, and load the rest as texture packs. */
void farm_load_framebuffer_header(void) {
    FrameState *current;
    FrameState *frames;
    u8 *buffer;

    DrawSync(0);
    current = current_frame;
    frames = frame_records;
    if (current == frames) {
        buffer = frames[1].primStart;
    } else {
        buffer = frames->primStart;
    }
    afile_read_sectors(0, 0x7F000, 0x4800, 0, buffer);
    *(FarmBlockHeader *)farm_area.mode.resourceBuffer = *(FarmBlockHeader *)buffer;
    texture_pack_load(buffer + 0x770);
}
