/* Farm event scripts: the step interpreter, the chunks of script data kept in VRAM, the
 * actors the events draw, entering and leaving an event, and the event images. */
#include <memory.h>
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libapi.h>
#include "types.h"
#include "actor.h"
#include "afile.h"
#include "anim.h"
#include "animals.h"
#include "cue.h"
#include "dog.h"
#include "estate.h"
#include "event_commands.h"
#include "event_flags.h"
#include "event_request.h"
#include "event_script.h"
#include "farm_data.h"
#include "farm_probe.h"
#include "farm_state.h"
#include "farm_tables.h"
#include "items.h"
#include "main.h"
#include "picture_cells.h"
#include "player.h"
#include "scene.h"
#include "screen_fade.h"
#include "sound.h"
#include "sound_request.h"
#include "texture_pack.h"
#include "villager.h"

/* The script data is cut into this many chunks, and the archive file it is read from. */
#define EVENT_CHUNK_COUNT 50
#define EVENT_SCRIPT_AFILE 0xC

/* A script step's type byte, its jump bytes and its flags. The handler returns
 * EVENT_STEP_DONE when the step is finished and the jump it names applies. */
enum EventStepCode {
    EVENT_STEP_END = 0xFF,          /* type: the script ends here */
    EVENT_STEP_NEXT = 0xFF,         /* a jump byte: go on to the following step */
    EVENT_STEP_STOP = 1,            /* flags bit 0: leave the script loop after this step */
    EVENT_STEP_DONE = 1,
};

/* The slot flags of a block's entries. */
enum EventSlotFlag {
    EVENT_SLOT_TEXTURE = 0x8000,    /* the entry is a texture, loaded with the block */
    EVENT_SLOT_MARKED = 0x4000,     /* the entry the script is read from */
    EVENT_SLOT_VILLAGER = 0x2000,
    EVENT_SLOT_NOT_STAGED = 0x1000,
    EVENT_SLOT_STAGED = 0x800,      /* the entry is bound to a staging sprite */
    EVENT_SLOT_ACTOR = 0x3C00,      /* any of the actor bits: the entry takes part in the event */
    EVENT_SLOT_FLAG_BITS = 0xFC00,  /* the flag bits; the rest of the word is the id */
};

/* The header flags of a farm data block. */
enum EventBlockFlag {
    EVENT_BLOCK_FLAG_ORIGIN_CORNER = 0x20,  /* drawn from the screen's corner, not from x 384, and the event holds resource E */
};

/* The slot codes that bind an actor other than a villager or a staging sprite. */
enum EventActorCode {
    EVENT_ACTOR_PLAYER = 0x401,     /* the player's sprite */
    EVENT_ACTOR_ANIM8 = 0x404,      /* farm_area.anim8, on the unnamed villager 27 sprite */
    EVENT_ACTOR_PACK_1 = 0x405,     /* slot animation pack 1, on the unnamed villager 27 sprite */
    EVENT_ACTOR_PACK_5 = 0x406,     /* slot animation pack 5, on the unnamed villager 27 sprite */
};

/* The cue of a villager is drawn this far above its sprite; a spouse at home is drawn this far nearer. */
enum {
    VILLAGER_CUE_LIFT = 45,
    VILLAGER_SPOUSE_DEPTH_STEP = 5,
};

typedef struct FarmPositionObject
{
    u8 pad00[0x8];
    u16 x;
    u8 pad0A[0x2];
    u16 y;
    u8 pad0E[0x2];
    u16 z;
} FarmPositionObject;

/* The header, the entry base, then one flag per entry. entryOffsets[i] is the offset of entry i. */
typedef struct FarmEventBlock
{
    u16 header;
    s16 entryBase;
    s16 slotFlags[24];
    s32 entryOffsets[24];
} FarmEventBlock;

static s32 event_chunk_checksum(u32 *src, u32 size);
static s32 event_script_start(void);
static s32 event_script_run(void);
static s32 event_script_finish(void);
static s32 event_script_load_wait(void);
static void event_camera_follow_actor(void);
static void event_villagers_run(void);
static void event_objects_draw(void);
static void event_image_draw(void);
static void event_villager_tick(FarmVillager *villager);
static void event_object_draw(FarmPositionObject *object, AnimState *item);
static void event_block_unpack(u16 *buffer);
static s32 event_block_first_size(FarmEventBlock *block);
static s32 event_block_entry(FarmEventBlock *block, s32 index);
static u8 *event_block_entry_ptr(FarmEventBlock *block, s32 index);
static void event_block_bind_sprites(FarmEventBlock *block);
static s32 event_block_entry_length(FarmEventBlock *block);
static s32 event_block_find_marked(FarmEventBlock *block);
static void event_block_collect_results(FarmEventBlock *block);
static void event_actors_load_sprites(FarmEventBlock *block);

/* Checksum each of the block's 50 chunks into chunk_checksums and upload the block's
 * start to vram_stash_rect, offset by (x, y). The rest of the chunk data goes below the
 * main region base (recorded in vram_stash_tail) unless a request is busy and there is
 * no room. */
static s32 vram_stash_block(u8 *data, s16 x, s16 y)
{
    RECT rect;
    u8 *p;
    s32 i;
    s32 size;
    u32 dest;

    p = data;
    for (i = 0; i < EVENT_CHUNK_COUNT; i++)
    {
        size = vram_chunk_offsets[i + 1] - vram_chunk_offsets[i];
        chunk_checksums[i] = event_chunk_checksum((u32 *)p, size);
        p += size;
    }
    p = data;
    rect = vram_stash_rect;
    rect.x += x;
    rect.y += y;
    LoadImage(&rect, (u_long *)p);
    p += vram_stash_rect.w * vram_stash_rect.h * 2;
    if (event_flag_get(EVENT_FLAG_REQUEST_BUSY) != 1)
    {
        size = (data + vram_chunk_offsets[EVENT_CHUNK_COUNT] - vram_chunk_offsets[0]) - p;
        if (main_region_free_bytes() >= size)
        {
            vram_stash_tail = main_get_region_base() - size;
            dest = vram_stash_tail;
            if (dest & 3)
            {
                dest -= dest & 3;
            }
            vram_stash_tail = dest;
            memcpy((void *)dest, p, size);
            DrawSync(0);
            return 0;
        }
    }
    DrawSync(0);
    return 0;
}

/* The script being run in the results buffer: its base, the cursor at the current step,
 * and the leading section of the block with the offsets of its entries. Reset when a
 * script starts. */
static u8 *event_script_base;
static u8 *event_script_cursor;
u32 event_block_base;
s32 event_block_offsets[25];
u32 event_step_index;
u32 event_script_unused_80129228[2];
/* Staging sprites for the event's actors. */
AnimState event_staging_sprites[10];
/* Offset applied when the event's sprites are placed. */
s16 event_sprite_offset_x;
s16 event_sprite_offset_y;
u32 event_block_size;
s16 event_request_status[4];
s32 chunk_checksums[EVENT_CHUNK_COUNT];
u32 vram_stash_tail;

/* Read chunk `chunk` of the block that vram_stash_block uploaded back into `buffer`.
 * The part of the chunk that is still in VRAM comes back with StoreImage; the
 * part past the image's last row comes from the copy at vram_stash_tail. Returns
 * the chunk's data if its checksum still matches chunk_checksums[chunk], else 0. */
u8 *event_chunk_read_back(u8 *buffer, s16 x, s16 y, u32 chunk)
{
    RECT rect;
    RECT image;
    s32 start;
    s32 size;
    s32 row;
    s32 column;
    s32 end;
    s32 rows;
    u8 *data;
    u8 *tail;

    if (chunk >= EVENT_CHUNK_COUNT)
    {
        return 0;
    }
    start = vram_chunk_offsets[chunk];
    size = vram_chunk_offsets[chunk + 1] - start;
    rect = vram_stash_rect;
    rect.x += x;
    rect.y += y;
    image = rect;
    row = start / 2 / rect.w;
    column = start / 2 % rect.w;
    end = (start + size) / 2;
    rows = end / rect.w - row;
    if (end % rect.w != 0)
    {
        rows++;
    }
    rect.h = rows;
    rect.y += row;
    if (rect.y + rect.h > image.y + image.h)
    {
        if (rect.y < image.y + image.h)
        {
            rect.h -= rect.y + rect.h - (image.y + image.h);
            StoreImage(&rect, (u_long *)buffer);
            DrawSync(0);
            data = buffer + column * 2;
            tail = buffer + rect.w * rect.h * 2;
            memcpy(tail, (void *)vram_stash_tail,
                   vram_chunk_offsets[chunk + 1] - vram_stash_rect.w * vram_stash_rect.h * 2);
        }
        else
        {
            start -= vram_stash_rect.w * vram_stash_rect.h * 2;
            data = (u8 *)vram_stash_tail + start;
        }
    }
    else
    {
        StoreImage(&rect, (u_long *)buffer);
        DrawSync(0);
        data = buffer + column * 2;
    }
    if (chunk_checksums[chunk] != event_chunk_checksum((u32 *)data, size))
    {
        return 0;
    }
    return data;
}

/* Start of each script's data in the script file. Script i spans [i]..[i + 1]. */
static s32 event_script_offsets[452] = {
    0x00000,
    0x00000,
    0x07800,
    0x0D000,
    0x12800,
    0x1C800,
    0x22000,
    0x26800,
    0x29800,
    0x2D000,
    0x2F000,
    0x31000,
    0x36000,
    0x41800,
    0x45000,
    0x48800,
    0x4B800,
    0x4D800,
    0x4F800,
    0x52800,
    0x54800,
    0x57800,
    0x59800,
    0x5F800,
    0x62800,
    0x65800,
    0x68800,
    0x6A800,
    0x6C800,
    0x6E800,
    0x70800,
    0x72800,
    0x78000,
    0x7C000,
    0x80000,
    0x84000,
    0x89800,
    0x8B800,
    0x8D800,
    0x8F800,
    0x92000,
    0x97800,
    0x98000,
    0x9B800,
    0xA0000,
    0xA0800,
    0xA4000,
    0xA7800,
    0xAB000,
    0xAE800,
    0xB2800,
    0xB6000,
    0xBD800,
    0xBD800,
    0xBF800,
    0xC3000,
    0xC6800,
    0xCA000,
    0xCD800,
    0xD1000,
    0xD4800,
    0xD6800,
    0xDA000,
    0xDE000,
    0xE3800,
    0xE8800,
    0xEC000,
    0xEE000,
    0x108000,
    0x122000,
    0x13E000,
    0x159800,
    0x175800,
    0x192000,
    0x197800,
    0x199800,
    0x19B800,
    0x1A5800,
    0x1A9800,
    0x1AB800,
    0x1B2000,
    0x1B4000,
    0x1B6000,
    0x1B8000,
    0x1BA000,
    0x1BC000,
    0x1D6000,
    0x1F2000,
    0x20D000,
    0x228800,
    0x244000,
    0x25F800,
    0x261800,
    0x263800,
    0x265800,
    0x267800,
    0x26A000,
    0x274800,
    0x27E800,
    0x27F000,
    0x27F800,
    0x281800,
    0x283800,
    0x285800,
    0x289800,
    0x293800,
    0x295800,
    0x297800,
    0x298800,
    0x29B000,
    0x29D000,
    0x2A7000,
    0x2B1800,
    0x2BB800,
    0x2BD000,
    0x2C2800,
    0x2CC800,
    0x2CE800,
    0x2DA000,
    0x309800,
    0x315000,
    0x321000,
    0x323000,
    0x323000,
    0x323800,
    0x324000,
    0x327000,
    0x329000,
    0x32B800,
    0x32D800,
    0x32E000,
    0x32E800,
    0x330800,
    0x332800,
    0x334800,
    0x337000,
    0x341800,
    0x348800,
    0x34A800,
    0x34C800,
    0x34E800,
    0x351000,
    0x353000,
    0x355000,
    0x357000,
    0x359000,
    0x35B000,
    0x360000,
    0x360000,
    0x366800,
    0x36C000,
    0x370800,
    0x371000,
    0x37A000,
    0x37C000,
    0x37E000,
    0x381000,
    0x38A000,
    0x391000,
    0x395800,
    0x39D000,
    0x3A6800,
    0x3AC000,
    0x3BE800,
    0x3CF800,
    0x3E4000,
    0x3F9000,
    0x40B000,
    0x40D800,
    0x410800,
    0x413000,
    0x415800,
    0x417800,
    0x41D800,
    0x41D800,
    0x41D800,
    0x41D800,
    0x41D800,
    0x423800,
    0x429800,
    0x430000,
    0x438800,
    0x43D800,
    0x442800,
    0x444800,
    0x446800,
    0x44B800,
    0x450000,
    0x455000,
    0x459800,
    0x45E000,
    0x462800,
    0x467800,
    0x46C000,
    0x470800,
    0x475000,
    0x479800,
    0x47B800,
    0x47C800,
    0x480800,
    0x48B800,
    0x497800,
    0x49C000,
    0x4A5000,
    0x4A9000,
    0x4B4800,
    0x4B8000,
    0x4BC800,
    0x4C1000,
    0x4C4800,
    0x4C6800,
    0x4CC000,
    0x4D1800,
    0x4D6800,
    0x4DC000,
    0x4E1000,
    0x4E6800,
    0x4EC000,
    0x4F1000,
    0x4F6800,
    0x4FB800,
    0x501000,
    0x506800,
    0x50B800,
    0x511000,
    0x516000,
    0x51B800,
    0x521000,
    0x526000,
    0x52B800,
    0x530800,
    0x539000,
    0x541800,
    0x548800,
    0x551000,
    0x559800,
    0x567800,
    0x575800,
    0x582000,
    0x590000,
    0x59E000,
    0x5A7000,
    0x5AE800,
    0x5B7800,
    0x5C0800,
    0x5CA000,
    0x5D3000,
    0x5DA800,
    0x5E3800,
    0x5EC800,
    0x5F6000,
    0x5FC800,
    0x603000,
    0x609800,
    0x610000,
    0x616800,
    0x61E000,
    0x626000,
    0x62D000,
    0x635000,
    0x63C800,
    0x63E800,
    0x640800,
    0x642800,
    0x644800,
    0x646800,
    0x646800,
    0x646800,
    0x646800,
    0x64D800,
    0x64F800,
    0x66C800,
    0x678000,
    0x691800,
    0x694800,
    0x697800,
    0x698000,
    0x698000,
    0x698800,
    0x69E800,
    0x6A1800,
    0x6A4000,
    0x6A9000,
    0x6AD000,
    0x6B1000,
    0x6B3000,
    0x6B5000,
    0x6B7000,
    0x6B9000,
    0x6BB000,
    0x6BB800,
    0x6BE000,
    0x6C0800,
    0x6C3000,
    0x6C5800,
    0x6C8000,
    0x6CA800,
    0x6CD000,
    0x6CF800,
    0x6D2000,
    0x6D2800,
    0x6D3000,
    0x6D8000,
    0x6DA000,
    0x6DA800,
    0x6E5000,
    0x6EE000,
    0x6EE800,
    0x6EF000,
    0x6EF800,
    0x6F0000,
    0x6F0800,
    0x6F1000,
    0x6F1800,
    0x6F2000,
    0x6F2800,
    0x6F3000,
    0x6F3800,
    0x6F4000,
    0x6F4800,
    0x6F5000,
    0x6F5800,
    0x6F6000,
    0x6F6800,
    0x6F7000,
    0x6F7800,
    0x6F8000,
    0x6F8800,
    0x6F9000,
    0x6F9800,
    0x6FA000,
    0x6FA800,
    0x6FB000,
    0x6FB800,
    0x6FC000,
    0x6FC800,
    0x6FD000,
    0x6FD800,
    0x6FE000,
    0x6FE800,
    0x6FF000,
    0x6FF800,
    0x700000,
    0x700800,
    0x701000,
    0x701800,
    0x702000,
    0x702800,
    0x705000,
    0x722000,
    0x72D800,
    0x747000,
    0x747800,
    0x748000,
    0x749000,
    0x74A000,
    0x74B000,
    0x74B800,
    0x74C800,
    0x74D800,
    0x74E800,
    0x74F800,
    0x750800,
    0x751800,
    0x752000,
    0x753000,
    0x754000,
    0x754800,
    0x755000,
    0x755800,
    0x756800,
    0x757800,
    0x758000,
    0x758800,
    0x759000,
    0x75A000,
    0x75A800,
    0x75B000,
    0x75B800,
    0x75C800,
    0x75D000,
    0x75E000,
    0x75E800,
    0x75F000,
    0x75F800,
    0x760800,
    0x761800,
    0x762800,
    0x763800,
    0x764800,
    0x765000,
    0x765800,
    0x766000,
    0x766800,
    0x767000,
    0x767800,
    0x768000,
    0x769000,
    0x769800,
    0x76A800,
    0x76B000,
    0x76B800,
    0x781800,
    0x7A9000,
    0x7B8000,
    0x7EA000,
    0x7EC000,
    0x7EE000,
    0x7F0000,
    0x7F2000,
    0x7F4000,
    0x7F6000,
    0x813800,
    0x815800,
    0x837000,
    0x852800,
    0x854800,
    0x86D000,
    0x883000,
    0x885000,
    0x8A1000,
    0x8B2800,
    0x8B4800,
    0x8CE000,
    0x8D0000,
    0x8F3800,
    0x900000,
    0x902000,
    0x91D000,
    0x91F800,
    0x93B800,
    0x93D800,
    0x93E000,
    0x948000,
    0x952000,
    0x95B800,
    0x965000,
    0x96D800,
    0x97D800,
    0x98C000,
    0x98E000,
    0x990000,
    0x992000,
    0x994000,
    0x9A6000,
    0x9C7800,
    0x9E9000,
    0xA0A800,
    0xA2C000,
    0xA48000,
    0xA5E000,
    0xA60000,
};

/* Start of each chunk in the block's data. Chunk i spans [i]..[i + 1]; the last entry is the end. */
s32 vram_chunk_offsets[] = {
    0x00000000,
    0x000005EC,
    0x00000C1C,
    0x00001388,
    0x00001BB0,
    0x00002200,
    0x000027F0,
    0x00002FFC,
    0x000036C8,
    0x00003E5C,
    0x00004580,
    0x00004CC0,
    0x00005390,
    0x00005960,
    0x0000610C,
    0x00006798,
    0x00006D1C,
    0x00007214,
    0x000077BC,
    0x00007E78,
    0x00008600,
    0x00008C18,
    0x00009228,
    0x0000986C,
    0x00009F9C,
    0x0000A5E0,
    0x0000AB88,
    0x0000AB9C,
    0x0000B248,
    0x0000B5BC,
    0x0000BF40,
    0x0000C45C,
    0x0000C994,
    0x0000CFC0,
    0x0000D68C,
    0x0000E050,
    0x0000E6D8,
    0x0000ED74,
    0x0000F738,
    0x0000FD80,
    0x0000FD94,
    0x0000FDA8,
    0x0000FDBC,
    0x0000FDD0,
    0x0000FDE4,
    0x0000FDF8,
    0x00010858,
    0x00010EC8,
    0x0001188C,
    0x00011DE8,
    0x000120E8,
};

/* The VRAM rectangle that block images are stashed in. */
RECT vram_stash_rect = {320, 0, 128, 255};

/* Size of the chunk for item id; the chunks are numbered from ITEM_HORSE. */
static s32 event_chunk_size(s32 id)
{
    id -= ITEM_HORSE;
    if ((u32)id >= EVENT_CHUNK_COUNT)
    {
        return 0;
    }
    return vram_chunk_offsets[id + 1] - vram_chunk_offsets[id];
}

/* Sum of the words of a chunk; size must be a whole number of words. */
static s32 event_chunk_checksum(u32 *src, u32 size)
{
    u32 i;
    s32 sum;
    if ((u32)src & 3)
    {
        SystemError(0x43, 0x190);
    }
    if (size & 3)
    {
        SystemError(0x43, 0x191);
    }
    i = 0;
    sum = 0;
    while (i < (size >> 2))
    {
        sum += *src++;
        i++;
    }
    return sum;
}

/* The handler for each step type, indexed by the step's type byte. */
static const FarmScriptHandler event_script_handlers[113] = {
    farm_nudge_axis,
    farm_nudge_cue,
    event_message_start,
    event_cmd_wait_frames,
    event_cmd_set_position,
    event_cmd_set_sprite,
    event_cmd_set_actor_hidden,
    event_cmd_check_location,
    event_cmd_set_target,
    event_cmd_camera_glide,
    event_cmd_player_lock,
    event_cmd_talk_end,
    event_cmd_face_player,
    event_cmd_test_var,
    event_cmd_test_var_slot,
    event_cmd_test_slot,
    event_cmd_test_slots,
    event_cmd_test_var_range,
    event_cmd_test_slot_range,
    event_cmd_set_var,
    event_cmd_set_var_from_slot,
    event_cmd_slot_arith,
    event_cmd_slot_arith_var,
    event_cmd_wait_control,
    event_cmd_set_mode,
    event_cmd_select_mode,
    event_cmd_stop_music,
    event_cmd_fade,
    event_cmd_event_bracket,
    event_cmd_has_room,
    event_item_can_take,
    event_cue_issue,
    event_item_store,
    event_cmd_name_entry,
    event_cmd_player_near,
    event_cmd_actor_reset,
    event_cmd_wait_time,
    event_cmd_enter_mode,
    event_cmd_random,
    event_cmd_set_dispatch,
    event_cmd_door,
    event_cmd_door_wait_moving,
    event_cmd_fill_screen,
    event_cmd_cue_reset,
    event_cmd_change_gold,
    event_cmd_scatter_crops,
    event_cmd_stash_items,
    event_cmd_result_select,
    event_cmd_chicken_slots,
    event_cmd_horse_spawn,
    event_cmd_queue_event,
    event_cmd_load_record,
    event_cmd_publish_item,
    event_cmd_item_enable,
    event_cmd_publish_offset,
    event_cmd_get_item_id,
    event_cmd_zone_entered,
    event_cmd_gift_reaction,
    event_cmd_get_held_item,
    event_cmd_wait_text_bank,
    event_cmd_set_position_vars,
    event_cmd_set_player_position,
    event_cmd_player_step,
    event_cmd_player_sprite,
    event_cmd_set_facing,
    event_cmd_receive_item,
    event_cmd_event_flag,
    event_cmd_item_has_reaction,
    event_cmd_actor_reset_vars,
    event_cmd_player_flags,
    event_cmd_sound_play,
    event_cmd_release_villager,
    event_cmd_camera_glide_vars,
    event_cmd_get_position,
    event_cmd_get_camera_centre,
    event_cmd_check_gift,
    event_cmd_skip_to_time,
    event_cmd_fade_music,
    event_cmd_reset_schedule,
    event_cmd_result_store,
    event_cmd_return,
    event_cmd_dispatch,
    event_cmd_walk_axis,
    event_cmd_stop_actor,
    event_cmd_presentation_cues,
    event_cmd_walk_actors,
    event_cmd_actor_walk_lane,
    event_cmd_test_var_by_slot,
    event_cmd_set_var_by_slot,
    event_cmd_set_villager_place,
    event_cmd_five_actor_dance,
    event_cmd_apply_var_table,
    event_cmd_object_show,
    event_cmd_walk_route,
    event_cmd_is_birthday,
    event_cmd_store_levels,
    event_cmd_contest_stage,
    event_cmd_adjust_present_villagers,
    event_cmd_select_livestock,
    event_cmd_player_walk,
    event_cmd_set_pending_pair,
    event_cmd_pick_bride,
    event_cmd_affection_add,
    event_cmd_result_direction,
    event_actor_position,
    event_cmd_villager_available,
    event_cmd_tomato_comment,
    event_cmd_fireworks_flash,
    event_cmd_result_cell,
    event_cmd_camera_pull_back,
    event_cmd_action_state,
    event_cmd_current_record,
    event_cmd_ending_text,
};

/* The handler for each request state, indexed by requests.state. */
static s32 (*const event_state_handlers[4])(void) = {
    event_script_start,
    event_script_run,
    event_script_finish,
    event_script_load_wait,
};

/* Clear the player's script input, then run the handler for the current request state
 * and clear the gift-pending flag. */
s32 event_state_step(void)
{
    FarmState *state = &game_state;
    s32 result;

    state->player.scriptHeld = 0;
    state->player.scriptPressed = 0;
    result = event_state_handlers[state->requests.state]();
    state->statusFlags &= ~STATUS_FLAG_GIFT_PENDING;
    return result;
}

/* The step the script cursor is at. */
FarmScriptStep *event_result_cursor(void)
{
    return (FarmScriptStep *)event_script_cursor;
}

static inline void set_colours(FrameState *fb, u8 r, u8 g, u8 b)
{
    fb->drawEnv.r0 = r;
    fb->drawEnv.g0 = g;
    fb->drawEnv.b0 = b;
    fb++;
    fb->drawEnv.r0 = r;
    fb->drawEnv.g0 = g;
    fb->drawEnv.b0 = b;
}

/* With EVENT_FLAG_REQUEST_BUSY set and a pending mode (-1 means none) that differs from
 * the current one, switch to it: blank the frame colours, restart the mode's sound,
 * then wait for the sound fader before restoring the colours. Otherwise pendingMode
 * is reset to -1. */
void farm_apply_pending_mode(void)
{
    FarmState *farm;
    FrameState *fb;
    s16 mode;
    u8 r;
    u8 g;
    u8 b;

    if (event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 1)
    {
        farm = &game_state;
        mode = farm->pendingMode;
        if (farm_area.mode.currentMode != mode && mode != -1)
        {
            fb = frame_records;
            r = fb->drawEnv.r0;
            g = fb->drawEnv.g0;
            b = fb->drawEnv.b0;
            set_colours(fb, 0, 0, 0);
            DrawSync(0);
            main_frame_clear();
            sound_set_seq_arg(0, STAGING_BUFFER);
            sound_seq_stop_first(0);
            sound_release_from(2, 0);
            farm_area.mode.modeScene = farm_area.mode.data->head.sceneId;
            sound_request(mode, 0);
            farm_area.mode.currentMode = mode;
            farm->pendingMode = -1;
            do
            {
                sound_load_tick();
            } while (sound_fader_ready() == 0);
            set_colours(frame_records, r, g, b);
        }
    }
    else
    {
        game_state.pendingMode = -1;
    }
}

void event_request_start(void)
{
    game_state.requests.state = FARM_REQUEST_FINISH;
    event_cmd_enter_mode(0, 0, 0, 0);
    game_state.player.scriptHeld = 0;
    game_state.player.scriptPressed = 0;
}

/* Read archive files 0x1D and 0x1E into the frame buffer and upload the map image from it,
 * unless a request is busy or a demo or input replay is running. */
static s32 event_load_archive_pair(void)
{
    u8 *buffer;
    if (event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 1)
    {
        return 0;
    }
    if ((game_state.globalFlags & (GLOBAL_FLAG_DEMO_START | GLOBAL_FLAG_INPUT_REPLAY)) != 0)
    {
        return 0;
    }
    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    buffer = frame_records[0].primStart;
    afile_read_range(0x1D, 0x1E, buffer, buffer);
    event_map_upload(buffer);
    return 0;
}

/* Stash the map image at dst in VRAM, shifted 256 pixels across unless the area's
 * AREA_FLAG_BIT15 is set (no shift at PLACE_1B). Skipped at PLACE_2C, during a busy request,
 * and during a demo or input replay. */
void event_map_upload(u8 *dst)
{
    s16 mode;
    s16 status;
    FarmZoneHeader *record;

    if (event_flag_get(EVENT_FLAG_REQUEST_BUSY) != 1)
    {
        if ((game_state.globalFlags & (GLOBAL_FLAG_DEMO_START | GLOBAL_FLAG_INPUT_REPLAY)) == 0)
        {
            mode = farm_area.mode.place.location;
            if (mode != PLACE_2C)
            {
                record = &farm_area.mode.data->head;
                status = ((record->flags & AREA_FLAG_BIT15) == 0) << 8;
                if (mode == PLACE_1B)
                {
                    status = 0;
                }
                vram_stash_block(dst, status, 0);
                event_request_status[0] = status;
            }
        }
    }
}

/* The script id of the current request while one is busy, else 0. */
s32 event_current_script_id(void)
{
    s32 result;
    if (event_flag_get(EVENT_FLAG_REQUEST_BUSY) != 0)
        result = game_state.request->args[0];
    else
        result = 0;
    return result;
}

/* Start the current event script (args[0] of the request). Unless args[2] is 1, its done
 * flag is set first. A cached script is expanded from the cache. Otherwise it is read from
 * archive file EVENT_SCRIPT_AFILE: in the background when it fits (finished by
 * event_script_load_wait; not for the god map or a load screen), or else now, into a
 * frame buffer's work area. */
static s32 event_script_start(void)
{
    u8 *data; /* the cached script, then the read buffer */
    s16 id;
    s32 start;
    s32 size;
    s32 load;
    s32 background;
    u8 *result;
    FrameState *fb;
    FarmState *farm;

    data = 0;
    if (game_state.request->args[2] != 1)
    {
        event_flag_set(game_state.request->args[0] + EVENT_DONE_FLAG_BASE, 1);
    }
    game_state.request->args[1] = 0;
    game_state.request->args[3] = 0;
    game_state.request->args[4] = 0;
    id = game_state.request->args[0];
    start = event_script_offsets[id];
    size = event_script_offsets[id + 1] - start;
    load = 1;
    event_sources_reset();
    event_enter();
    if ((u32)(id - ITEM_HORSE) < EVENT_CHUNK_COUNT)
    {
        data = rle_load_cached(game_state.work.sector, id);
        load = (data == 0);
    }
    background = 0;
    if (load)
    {
        if (size < main_region_free_bytes() && farm_area.mode.eventMode != FARM_MODE_LOAD_SCREEN)
        {
            background = (id != ITEM_GOD_MAP);
        }
        if (background)
        {
            afile_read_bytes(EVENT_SCRIPT_AFILE, start, size, main_region_free_start(), 0, 1);
            game_state.requests.state = FARM_REQUEST_LOAD_WAIT;
            return 0;
        }
        DrawSync(0);
        fb = frame_records;
        if (fb->primSize < size)
        {
            ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
            data = fb->primStart;
            if (fb->primSize * 2 < size)
            {
                main_frame_clear();
            }
        }
        else if (current_frame == frame_records)
        {
            data = fb[1].primStart;
        }
        else
        {
            data = fb->primStart;
        }
        afile_read_bytes(EVENT_SCRIPT_AFILE, start, size, data, 0, 1);
        while ((result = afile_read_bytes(EVENT_SCRIPT_AFILE, start, size, data, 0, 0)) == 0)
        {
        }
        event_block_unpack((u16 *)result);
    }
    else
    {
        rle_expand(game_state.work.results, data, event_chunk_size(id));
        if ((u32)event_chunk_size(id) >= 3297)
        {
            SystemError('S', game_state.request->args[0]);
        }
        villager_sprites_copy();
    }
    event_script_base = event_script_cursor = game_state.work.results;
    event_step_index = 0;
    event_vars_clear(0, 0, 0, 0);
    farm_pairs_reset();
    farm = &game_state;
    farm->pendingMode = -1;
    farm->requests.state = FARM_REQUEST_RUN;
    return 0;
}

#define CURRENT_EVENT ((FarmScriptStep *)event_script_cursor)

/* Run the farm event script from the current step until a step asks to stop or the
 * script ends (the request then moves to finish), then run the villagers, draw the
 * objects and follow the camera actor. The steps are skipped while the menu is open,
 * and nothing runs while a target load is pending. */
static s32 event_script_run(void)
{
    FarmScriptStep *event;

    if (farm_area.mode.eventMode == FARM_MODE_TARGET_LOAD)
    {
        return 0;
    }
    if (!(game_state.globalFlags & GLOBAL_FLAG_MENU_OPEN))
    {
        while (1)
        {
            if (CURRENT_EVENT->type == EVENT_STEP_END)
            {
                game_state.requests.state = FARM_REQUEST_FINISH;
                break;
            }
            event = CURRENT_EVENT;
            if (event_script_handlers[event->type](event->command, event->operand, event->x, event->y) == EVENT_STEP_DONE)
            {
                if (CURRENT_EVENT->nextDone == EVENT_STEP_NEXT)
                {
                    event_script_cursor += sizeof(FarmScriptStep);
                    event_step_index++;
                }
                else
                {
                    event_script_cursor = event_script_base + CURRENT_EVENT->nextDone * sizeof(FarmScriptStep);
                    event_step_index = CURRENT_EVENT->nextDone;
                }
            }
            else
            {
                if (CURRENT_EVENT->nextOther == EVENT_STEP_NEXT)
                {
                    event_script_cursor += sizeof(FarmScriptStep);
                    event_step_index++;
                }
                else
                {
                    event_script_cursor = event_script_base + CURRENT_EVENT->nextOther * sizeof(FarmScriptStep);
                    event_step_index = CURRENT_EVENT->nextOther;
                }
            }
            if (event->flags & EVENT_STEP_STOP)
            {
                break;
            }
        }
    }
    event_villagers_run();
    event_objects_draw();
    event_camera_follow_actor();
    return 0;
}

/* End a finished event: clear its villagers and actors, then re-enter the next queued
 * event or exit, and set the request back to start. */
static s32 event_script_finish(void)
{
    FarmState *state;
    FarmState *result;
    state = &game_state;
    state->globalFlags &= ~GLOBAL_FLAG_CAMERA_BUSY;
    event_objects_draw();
    event_camera_follow_actor();
    farm_pairs_reset();
    villagers_clear_flags();
    state->player.scriptSteps = 0;
    state->progress.entrySubStep = 0;
    state->statusFlags &= ~STATUS_FLAG_EVENT_HOLDS_RESOURCE_E;
    event_sources_reset();
    if (event_queue_has_entries() == 1)
    {
        event_enter();
    }
    else
    {
        event_exit();
    }
    result = &game_state;
    result->requests.state = FARM_REQUEST_START;
    return 1;
}

/* Finish the background read of the current script: retry until the data arrives, then
 * reset the result cursors and state to run it. Gives up once the fade has settled. */
static s32 event_script_load_wait(void)
{
    FarmState *farm;
    s32 index;
    s32 start;
    s32 length;
    u8 *result;

    index = game_state.request->args[0];
    start = event_script_offsets[index];
    length = event_script_offsets[index + 1] - start;
    do
    {
        result = afile_read_bytes(EVENT_SCRIPT_AFILE, start, length, main_region_free_start(), 0, 0);
        if (result != 0)
        {
            event_block_unpack((u16 *)result);
            event_script_cursor = game_state.work.results;
            event_script_base = game_state.work.results;
            event_step_index = 0;
            event_vars_clear(0, 0, 0, 0);
            farm_pairs_reset();
            farm = &game_state;
            farm->pendingMode = -1;
            farm->requests.state = FARM_REQUEST_RUN;
            return 0;
        }
    } while (fade_is_settled() == 0);
    return 0;
}

/* While the camera is busy, centre the scroll on the villager the camera record names. */
static void event_camera_follow_actor(void)
{
    s32 result;
    VECTOR *source;

    if ((game_state.globalFlags & GLOBAL_FLAG_CAMERA_BUSY) != 0)
    {
        result = event_camera_record();
        if (result != -2)
        {
            result = event_camera_record();
            if (result != -1)
            {
                result = event_camera_record();
                source = &game_state.villagers[result].ePosition;
                farm_area.mode.scroll.centre.vx = source->vx;
                farm_area.mode.scroll.centre.vy = source->vy;
                farm_area.mode.scroll.centre.vz = source->vz;
            }
        }
    }
}

/* Tick each villager that is present and in the event: its cue counts down and it is
 * drawn. Not during a transition. */
static void event_villagers_run(void)
{
    s32 remaining;
    s32 mask;
    FarmVillager *base;
    FarmVillager *record;
    ItemCue *cue;

    if (!(game_state.globalFlags & GLOBAL_FLAG_TRANSITION))
    {
        mask = (VILLAGER_FLAG_PRESENT | VILLAGER_FLAG_IN_EVENT) << 16;
        base = game_state.villagers;
        cue = &base->cue;
        record = base;
        remaining = 36;
        do
        {
            /* the flags are the high half of the word: both must be set */
            if ((*(s32 *)&record->prevAction & mask) == mask)
            {
                cue_countdown(cue);
                event_villager_tick(record);
            }
            cue = (ItemCue *)((u8 *)cue + sizeof(FarmVillager));
            remaining--;
            record++;
        } while (remaining >= 0);
    }
}

/* Draw the enabled estate objects in the event's record slots, then the event image.
 * Not during a transition. */
static void event_objects_draw(void)
{
    s32 slot;
    FarmEstateRecord *record;
    AnimState *sprite;

    if ((game_state.globalFlags & GLOBAL_FLAG_TRANSITION) != 0)
    {
        return;
    }
    sprite = event_staging_sprites;
    for (slot = 0; slot < 10; slot++)
    {
        record = &game_state.runs.records[17][slot];
        if ((record->recordFlags & ESTATE_RECORD_ENABLED) != 0)
        {
            event_object_draw((FarmPositionObject *)record, sprite + slot);
        }
    }
    event_image_draw();
}

/* Queue the event image at its point above the camera, unless it is hidden or a
 * transition is on. */
static void event_image_draw(void)
{
    FarmScreenPoint result;
    SVECTOR input;

    if ((game_state.globalFlags & GLOBAL_FLAG_TRANSITION) == 0 && game_state.image.hidden == 0)
    {
        input.vx = game_state.camera.vx + 150;
        input.vy = game_state.camera.vy - 100;
        input.vz = game_state.camera.vz - 150;
        if (estate_point_project(&input, &result) != 0)
        {
            farm_image_queue(&current_frame->ot[result.depth >> 2], result.x, result.y);
        }
    }
}

/* Move a villager by its speed (unless the farm is paused), then project it and
 * draw its sprite, cue and marker. The sprite animates while the farm is running,
 * or in one of five scenes that keep animating during a pause. */
static void event_villager_tick(FarmVillager *villager)
{
    FarmScreenPoint screen;
    SVECTOR input;
    s32 paused;
    u_long *position;
    s16 scene;

    paused = game_state.globalFlags & GLOBAL_FLAG_PAUSED;
    if (!paused)
    {
        if (villager->eSpeed.vx != 0 || villager->eSpeed.vy != 0 || villager->eSpeed.vz != 0)
        {
            villager->ePosition.vx += villager->eSpeed.vx;
            villager->ePosition.vy += villager->eSpeed.vy;
            villager->ePosition.vz += villager->eSpeed.vz;
            farm_settle_on_ground(&villager->ePosition, 0xC000);
        }
    }

    input.vx = villager->ePosition.vx + 150;
    input.vy = villager->ePosition.vy - 100;
    input.vz = villager->ePosition.vz - 150;
    if (estate_point_project(&input, &screen) != 0)
    {
        position = &current_frame->ot[screen.depth >> 2];
        if (event_flag_get(EVENT_FLAG_SPOUSE_AT_HOME) != 0 && villager->id == game_state.family.wifeVillager)
        {
            position -= VILLAGER_SPOUSE_DEPTH_STEP;
        }
        anim_set_tick_flag(0);
        sprite_draw_traced(position, current_frame->primCursor.ft4, &villager->sprite, villager->sprite.animation, villager->sprite.frame,
                           screen.x, screen.y, villager->sprite.scale, 0, ANIM_DRAW_CURSOR);
        cue_draw_frame(&villager->cue, position, screen.x, screen.y - VILLAGER_CUE_LIFT);
        if (!(villager->placeFlags & VILLAGER_PLACE_NO_MARKER))
        {
            farm_draw_cursor(position, screen.x, screen.y, (villager->placeFlags & VILLAGER_PLACE_CURSOR_ALT) != 0);
        }
    }

    if (!paused || (scene = game_state.request->args[0]) == 0x19D ||
        scene == 0x1BC || scene == 0x1BD || scene == 0x1BE || scene == 0x1BF)
    {
        anim_tick(&villager->sprite);
    }
}

/* Draw an estate object's sprite, and tick its animation unless the farm is paused. */
static void event_object_draw(FarmPositionObject *object, AnimState *item)
{
    FarmScreenPoint result;
    SVECTOR input;

    input.vx = object->x + 150;
    input.vy = object->y - 100;
    input.vz = object->z - 150;
    if (estate_point_project(&input, &result) != 0)
    {
        sprite_draw_traced(&current_frame->ot[result.depth >> 2], current_frame->primCursor.ft4, item,
                           item->animation, item->frame, result.x, result.y, item->scale, 0, ANIM_DRAW_CURSOR);
    }
    if ((game_state.globalFlags & GLOBAL_FLAG_PAUSED) == 0)
    {
        anim_tick(item);
    }
}

/* Unpack a farm data block. The draw origin is set to (384, 0), or to (0, 0) with
 * EVENT_BLOCK_FLAG_ORIGIN_CORNER, which also holds resource E. The marked section, at
 * most 3296 bytes, is copied into the script results. Any leading section is moved to
 * the top of free memory, and its position is recorded in event_block_base. */
static void event_block_unpack(u16 *buffer)
{
    u32 size;
    s32 top;
    u32 length;
    s32 entry;
    u8 *results;
    FarmEventBlock *block;

    event_sprite_offset_x = 384;
    event_sprite_offset_y = 0;
    if (buffer[0] & EVENT_BLOCK_FLAG_ORIGIN_CORNER)
    {
        event_sprite_offset_x = 0;
        event_sprite_offset_y = 0;
        game_state.statusFlags |= STATUS_FLAG_EVENT_HOLDS_RESOURCE_E;
    }
    block = (FarmEventBlock *)buffer;
    size = event_block_first_size(block);
    top = main_get_region_base();
    event_block_size = size;
    top -= size;
    villager_sprites_copy();
    length = event_block_entry_length(block);
    if (length > 3296)
    {
        SystemError(0x53, game_state.request->args[0]);
    }
    entry = event_block_find_marked(block);
    results = game_state.work.results;
    memcpy(results, event_block_entry_ptr(block, entry), length);
    event_block_bind_sprites(block);
    if (size != 0)
    {
        memmove((unsigned char *)top, event_block_entry_ptr(block, 0), size);
        event_block_base = top;
        event_block_collect_results(block);
        event_actors_load_sprites(block);
        farm_actions_clear(1);
    }
    else
    {
        event_block_base = 0;
    }
}

/* Offset of the first entry whose actor bits are clear, or 0 if there is none. */
static s32 event_block_first_size(FarmEventBlock *block)
{
    s16 *entry_flags;
    s32 entry;
    s32 flags;
    entry = 2;
    flags = block->slotFlags[0];
    if ((flags & EVENT_SLOT_ACTOR) != 0)
    {
        entry_flags = &block->slotFlags[0];
        do
        {
            entry++;
            entry_flags++;
            if (entry >= 26)
            {
                break;
            }
            flags = *entry_flags;
        } while ((flags & EVENT_SLOT_ACTOR) != 0);
    }
    if (entry == 26)
    {
        return 0;
    }
    return event_block_entry(block, entry - 2);
}

/* Offset of entry index in the block. */
static s32 event_block_entry(FarmEventBlock *block, s32 index)
{
    return *(s32 *)((index << 2) + (s32)block->entryOffsets);
}

/* Data of entry index in the block. */
static u8 *event_block_entry_ptr(FarmEventBlock *block, s32 index)
{
    s32 base;
    u8 *data;
    base = block->entryBase;
    data = (u8 *)&block->entryOffsets[base + 1];
    return data + event_block_entry(block, index);
}

/* Load each texture entry (from slot 2 on) with the block's draw origin. */
static void event_block_bind_sprites(FarmEventBlock *block)
{
    s32 i;
    s16 *flag;

    for (i = 2; i < 26; i++)
    {
        flag = &block->slotFlags[i - 2];
        if (*flag & EVENT_SLOT_TEXTURE)
        {
            texture_pack_load_keep_wide(event_block_entry_ptr(block, i - 2), event_sprite_offset_x, event_sprite_offset_y, event_sprite_offset_x,
                                        event_sprite_offset_y);
        }
    }
}

/* Length of the marked entry: up to the offset of the entry after it. */
static s32 event_block_entry_length(FarmEventBlock *block)
{
    s32 index;
    s32 next;
    index = (s32)(short)event_block_find_marked(block);
    next = event_block_entry(block, index + 1);
    return next - event_block_entry(block, index);
}

/* Index of the first entry with the marked flag set. Errors out if there is none. */
static s32 event_block_find_marked(FarmEventBlock *block)
{
    s32 slot;
    s16 flag;

    for (slot = 2; slot < 26; slot++)
    {
        flag = block->slotFlags[slot - 2];
        if (flag & EVENT_SLOT_MARKED)
            break;
    }
    if (slot == 26)
        SystemError(0x4E, game_state.request->args[0]);
    return slot - 2;
}

/* Copy the offset of each actor entry into event_block_offsets, in slot order. */
static void event_block_collect_results(FarmEventBlock *block)
{
    s16 *entry_flags;
    s16 flags;
    s32 *result;
    s32 result_count;
    s32 entry;

    result_count = 0;
    entry = 2;
    entry_flags = &block->slotFlags[0];
    result = event_block_offsets;
    do
    {
        flags = *entry_flags;
        if ((flags & EVENT_SLOT_ACTOR) != 0)
        {
            if (result_count >= 26)
            {
                SystemError(0x41, 0x3E7);
            }
            *result = event_block_entry(block, entry - 2);
            result++;
            result_count++;
        }
        entry++;
        entry_flags++;
    } while (entry < 26);
}

/* The scene id of the event the farm is running. */
#define CURRENT_SCENE() (game_state.request->args[0])

/* Bind each entry's sprite by its slot code: the player, a villager (which joins the
 * event), one of the generic event actors, or a staging sprite. */
static void event_actors_load_sprites(FarmEventBlock *block)
{
    s32 i;
    s16 code;
    s32 id;
    struct AnimFile *file;
    struct AnimState *sprite;
    s16 scene;
    s16 *flag;

    for (i = 2; i < 26; i++)
    {
        file = (struct AnimFile *)(event_block_base + event_block_offsets[i - 2]);
        flag = &block->slotFlags[i - 2];
        code = *flag;
        id = code & ~EVENT_SLOT_FLAG_BITS;
        if (code == EVENT_ACTOR_PLAYER)
        {
            anim_bind_file(&game_state.player.sprite, file);
            anim_set_offsets(&game_state.player.sprite, event_sprite_offset_x, event_sprite_offset_y, 0, 0);
        }
        else if (code & EVENT_SLOT_VILLAGER)
        {
            sprite = &game_state.villagers[id].sprite;
            anim_bind_file(sprite, file);
            if (id == ITEM_BLUE_FEATHER)
            {
                scene = CURRENT_SCENE();
                if (scene == 0xB6 || scene == 0x131)
                {
                    anim_set_offsets(&game_state.villagers[CHARACTER_UNNAMED_33].sprite, event_sprite_offset_x, event_sprite_offset_y, 0, 0);
                }
                else
                {
                    anim_set_offsets(&game_state.villagers[CHARACTER_UNNAMED_33].sprite, event_sprite_offset_x, event_sprite_offset_y, event_sprite_offset_x, event_sprite_offset_y);
                }
            }
            else
            {
                anim_set_offsets(sprite, event_sprite_offset_x, event_sprite_offset_y, event_sprite_offset_x, event_sprite_offset_y);
            }
            if (CURRENT_SCENE() != 0xA1)
            {
                game_state.villagers[id].flags |= VILLAGER_FLAG_IN_EVENT;
            }
        }
        else if (code == EVENT_ACTOR_ANIM8)
        {
            anim_bind_file(&game_state.villagers[CHARACTER_UNNAMED_27].sprite, (struct AnimFile *)farm_area.anim8);
            game_state.villagers[id].flags |= VILLAGER_FLAG_IN_EVENT;
        }
        else if (code == EVENT_ACTOR_PACK_1)
        {
            anim_bind_file(&game_state.villagers[CHARACTER_UNNAMED_27].sprite,
                           (struct AnimFile *)&farm_area.slotAnims[ANIM_PACK_START(farm_slot_tocs[0], 1)]);
            game_state.villagers[id].flags |= VILLAGER_FLAG_IN_EVENT;
        }
        else if (code == EVENT_ACTOR_PACK_5)
        {
            anim_bind_file(&game_state.villagers[CHARACTER_UNNAMED_27].sprite,
                           (struct AnimFile *)&farm_area.slotAnims[ANIM_PACK_START(farm_slot_tocs[0], 5)]);
            game_state.villagers[id].flags |= VILLAGER_FLAG_IN_EVENT;
        }
        else if (!(code & EVENT_SLOT_NOT_STAGED))
        {
            if (code & EVENT_SLOT_STAGED)
            {
                event_actor_slot_bind(file, id);
            }
        }
    }
}

/* Entering and leaving an event: the villagers and actors it binds, and their pairs. */

#define PENDING_RESULT_NONE -1
#define PENDING_RESULT_COUNT 5

typedef struct FarmCopyWords {
    u32 words[12];
} FarmCopyWords;
typedef struct FarmCopyRecord {
    FarmCopyWords copy;
    u8 pad[0x1D4 - 0x30];
} FarmCopyRecord;
/* Copy each villager's sprite80 over its sprite, and call villager_load_request for
 * each one when the request is 0x82. */
void villager_sprites_copy(void) {
    FarmCopyRecord *destination;
    FarmCopyRecord *source;
    FarmCopyWords *source_words;
    FarmCopyWords *destination_words;
    s32 index;
    index = 0;
    do {
        destination = (FarmCopyRecord *)&game_state.villagers[0].sprite;
        source = (FarmCopyRecord *)&((FarmCopyWords *)destination)[-1];
        source_words = &source[index].copy;
        destination_words = &destination[index].copy;
        *destination_words = *source_words;
        if (game_state.request->args[0] == 0x82) {
            villager_load_request((s16)index, &game_state.villagers[index].sprite);
        }
        index++;
    } while (index < CHARACTER_COUNT);
}

/* Bind file to the first free staging sprite, and record result as its item id. */
void event_actor_slot_bind(void *file, s32 result) {
    s32 count;
    count = 0;
    do {
        if (event_staging_sprites[count].source == 0) {
            anim_bind_file(&event_staging_sprites[count], file);
            anim_set_offsets(&event_staging_sprites[count], event_sprite_offset_x, event_sprite_offset_y, event_sprite_offset_x, event_sprite_offset_y);
            game_state.runs.records[17][count].itemId = result;
            return;
        }
        count++;
    } while (count < 10);
}

/* Free the staging sprites, reset the estate items of run 17 and hide the event image. */
void event_sources_reset(void) {
    s32 i;
    for (i = 9; i >= 0; i--) {
        event_staging_sprites[i].source = 0;
    }
    estate_items_reset(game_state.runs.records[17], game_state.runs.counts[17]);
    game_state.image.hidden = 1;
}

/* Start an event: set EVENT_FLAG_EVENT_ACTIVE, lock the player (unless it is fishing or
 * in a mode animation), run the presentation cues for a script outside the item chunks
 * in an area without AREA_FLAG_BIT12, then reset the player's cue and hide the event image. */
void event_enter(void) {
    FarmLoadArea *window;
    FarmState *farm;
    FarmZoneHeader *object;
    ItemCue *cue;
    event_flag_set(EVENT_FLAG_EVENT_ACTIVE, 1);
    if ((game_state.player.stateFlags & (PLAYER_FLAG_FISHING | PLAYER_FLAG_MODE_ANIM)) == 0) {
        event_cmd_player_lock(0, 0, 0, 0);
    }
    window = &farm_area;
    object = &window->mode.data->head;
    if ((object->flags & AREA_FLAG_BIT12) == 0) {
        farm = &game_state;
        if ((u32)((u16)farm->request->args[0] - ITEM_HORSE) >= EVENT_CHUNK_COUNT) {
            event_cmd_presentation_cues(0, 0, 0, 0);
        }
    }
    cue = &game_state.player.cue;
    cue_reset(cue);
    farm = &game_state;
    farm->image.hidden = 1;
    farm->statusFlags &= ~STATUS_FLAG_TARGET_RELOAD;
}

/* End an event: clear EVENT_FLAG_EVENT_ACTIVE, unload the event record, unlock the player
 * and clear its action and talk state. An item the player has no room for is shown. The
 * first trigger stamp and the dog are placed again when they are on the event cell. */
void event_exit(void) {
    FarmState *farm;
    FarmState *farm_base;
    s16 status;

    event_flag_set(EVENT_FLAG_EVENT_ACTIVE, 0);
    event_cmd_load_record(-2, 0, 0, 0);
    event_cmd_player_lock(1, 0, 0, 0);
    farm = &game_state;
    farm->player.talkVillager = -1;
    farm->player.currentAction = 0;
    farm->player.stateFlags &= ~PLAYER_FLAG_EVENT_SPRITE;
    farm->statusFlags &= ~STATUS_FLAG_NO_TARGET_ACTION;
    farm->player.latches &= ~PLAYER_LATCH_FATIGUE_LOCKED;
    if (event_cmd_has_room(1, 0, 0, 0) == 0 && farm->player.mode != PLAYER_MODE_STORE_ITEM) {
        farm->player.stateFlags = (farm->player.stateFlags & ~PLAYER_FLAG_BIT14) | PLAYER_FLAG_SHOWING_ITEM;
    }
    farm_base = &game_state;
    if (farm_base->runs.records[18]->location == PLACE_EVENT_CELL) {
        estate_trigger_stamp_first();
    }
    status = farm_base->animals.dog.actor.place.location;
    if (status == PLACE_EVENT_CELL) {
        dog_place(0);
    }
}

/* Clear the present and in-event flags of every villager. */
void villagers_clear_flags(void) {
    FarmVillager *p = game_state.villagers;
    int i = 36;
    do {
        p->flags &= ~(VILLAGER_FLAG_PRESENT | VILLAGER_FLAG_IN_EVENT);
        p++;
        i--;
    } while (i >= 0);
}

/* Set every pending result to none. */
void farm_pairs_reset(void) {
    s32 i;

    for (i = 0; i < PENDING_RESULT_COUNT; i++) {
        game_state.pendingResults[i].id = PENDING_RESULT_NONE;
        game_state.pendingResults[i].value = PENDING_RESULT_NONE;
    }
}

/* The event images: the run-length expansion of image data and the sprite primitive that draws it. */

/* The 50 event chunks are numbered by item id, from the first of them. */
#define RLE_CHUNK_BASE ITEM_HORSE

/* Read back the cached chunk for item index into dst, or 0 if it is no longer intact. */
u8 *rle_load_cached(u8 *dst, s32 index) {
    s32 chunk = index - RLE_CHUNK_BASE;
    return event_chunk_read_back(dst, event_request_status[0], 0, chunk);
}

/* Expand run-length data: a run is the marker byte, the value and the run length;
 * any other byte is copied. `count` is the number of runs and literals. */
void rle_expand(u8 *dst, u8 *src, s32 count) {
    u8 marker;
    u8 value;
    s32 run;
    s32 i;

    marker = *src++;
    i = 0;
    while (i < count) {
        if (*src == marker) {
            src++;
            value = *src++;
            for (run = *src; run != 0; run--) {
                *dst++ = value;
            }
        } else {
            *dst++ = *src;
        }
        i++;
        src++;
    }
}

/* A textured sprite primitive, and a draw-mode primitive for its texture page. */
typedef struct {
    u8 addr[3];
    u8 len;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 w, h;
} SpritePrim;

typedef struct {
    u8 addr[3];
    u8 len;
    u32 code[2];
} DrawModePrim;

/* Queue the farm image as an opaque sprite at (x, y), followed by a draw-mode
 * primitive selecting its texture page (dithering and drawing to the display
 * area enabled). */
void farm_image_queue(void *ot, s16 x, s16 y) {
    FarmGridEntry *image;
    SpritePrim *sprite;
    DrawModePrim *mode;
    u16 pageX;
    u16 pageY;
    u32 tpage;

    sprite = gs_work_alloc(sizeof(SpritePrim));
    mode = gs_work_alloc(sizeof(DrawModePrim));
    image = &game_state.image;

    sprite->len = 4;
    sprite->code = 0x64;
    sprite->clut = GetClut(image->clutX, image->clutY);
    sprite->r0 = 0x80;
    sprite->g0 = 0x80;
    sprite->b0 = 0x80;
    sprite->x0 = x;
    sprite->y0 = y;
    sprite->code &= ~2;
    sprite->w = image->w;
    sprite->h = image->h;
    sprite->u0 = (image->x % 64) * 4;
    sprite->v0 = image->y;
    AddPrim(ot, sprite);

    pageX = (u16)image->clutX;
    pageY = (u16)image->clutY & 0xFF00;
    tpage = ((pageY & 0x100) >> 4) | ((pageX & 0x3C0) >> 6) | ((pageY & 0x200) << 2);
    mode->len = 1;
    mode->code[0] = tpage | 0xE1000600;
    AddPrim(ot, mode);
}
