/* The farm's animation and item art tables of contents, and the weather and
 * resource tables. */
#include "types.h"
#include "anim.h"
#include "farm_data.h"

/* The tables of contents of the farm's slot packs (AnimPackToc), whose .ANI
 * files start at 0: the two sprite sets in slots 0 and 1 (the same list for both),
 * the four seasons' art in slots 2..5 and the toys in slot 6. */
static AnimPackToc farm_slot_toc_0 = {10, {
    0, 2668, 5372, 6212, 8444, 10496, 12668, 13576, 15900, 18072, 24312}};
static AnimPackToc farm_slot_toc_1 = {10, {
    0, 2668, 5372, 6212, 8444, 10496, 12668, 13576, 15900, 18072, 24312}};
static AnimPackToc farm_slot_toc_6 = {5, {0, 1008, 3072, 3688, 4236, 5376}};
static AnimPackToc farm_slot_toc_2 = {5, {0, 1380, 1928, 3212, 3408, 3604}};
static AnimPackToc farm_slot_toc_3 = {5, {0, 1380, 2664, 3008, 3396, 3608}};
static AnimPackToc farm_slot_toc_4 = {5, {0, 1692, 3072, 4356, 4552, 4748}};
static AnimPackToc farm_slot_toc_5 = {3, {0, 1692, 3728, 5012}};
/* By slot, the packs farm_load_resource and farm_load_season_art read. */
AnimPackToc *farm_slot_tocs[7] = {
    &farm_slot_toc_0, &farm_slot_toc_1, &farm_slot_toc_2, &farm_slot_toc_3, &farm_slot_toc_4,
    &farm_slot_toc_5, &farm_slot_toc_6,
};
/* The tables of contents of the item art packs (farm_load_item_art), by item art set. */
static AnimPackToc farm_item_toc_0 = {35, {
    0, 200, 400, 632, 832, 1032, 1196, 1396, 1648, 1900, 2172, 2468, 2712, 2956, 3228,
    3428, 3592, 3676, 3836, 3920, 4040, 4124, 4300, 4428, 4556, 4640, 4724, 4808, 4892,
    4976, 5060, 5144, 5228, 5348, 5468, 5588}};
static AnimPackToc farm_item_toc_1 = {42, {
    0, 188, 388, 620, 820, 1008, 1128, 1440, 1524, 1608, 1692, 1776, 1860, 2196, 2532,
    2804, 3076, 3388, 3632, 3988, 4188, 4360, 4444, 4596, 4680, 4800, 5080, 5164, 5292,
    5420, 5516, 5612, 5708, 5804, 5900, 5996, 6092, 6188, 6308, 6428, 6548, 6632,
    6716}};
static AnimPackToc farm_item_toc_2 = {43, {
    0, 212, 436, 700, 924, 1136, 1264, 1584, 1668, 1752, 1836, 1920, 2004, 2316, 2584,
    2852, 3120, 3480, 3776, 3904, 4104, 4284, 4368, 4536, 4620, 4740, 5036, 5120, 5248,
    5376, 5460, 5544, 5628, 5712, 5796, 5880, 5964, 6048, 6132, 6564, 6216, 6332, 6448,
    6752}};
static AnimPackToc farm_item_toc_3 = {29, {
    0, 208, 416, 656, 864, 1072, 1244, 1408, 1492, 1652, 1736, 1856, 1940, 2068, 2196,
    2280, 2364, 2448, 2532, 2616, 2700, 2784, 2868, 2988, 3108, 3228, 3512, 3720, 3928,
    4092}};
static AnimPackToc farm_item_toc_4 = {31, {
    0, 200, 400, 600, 756, 1008, 1260, 1532, 1828, 2072, 2408, 2744, 3016, 3288, 3600,
    3904, 4140, 4376, 4612, 4924, 5124, 5368, 5612, 5884, 6156, 6512, 6676, 6760, 7032,
    7340, 7460, 7580}};
static AnimPackToc farm_item_toc_5 = {34, {
    0, 304, 616, 972, 1284, 1556, 1864, 1984, 2104, 2224, 2344, 2548, 2668, 2788, 2908,
    3028, 3148, 3268, 3388, 3508, 3628, 3932, 4016, 4136, 4256, 4376, 4460, 4544, 4640,
    4736, 4832, 4928, 5024, 5120, 5404}};
static AnimPackToc farm_item_toc_6 = {21, {
    0, 180, 264, 348, 432, 560, 688, 772, 856, 984, 1068, 1152, 1236, 1320, 1404, 1488,
    1572, 1656, 1740, 1836, 2236, 2320}};
static AnimPackToc farm_item_toc_7 = {16, {
    0, 128, 212, 392, 476, 560, 644, 728, 812, 896, 980, 1064, 1160, 1244, 1328, 1412,
    1508}};
static AnimPackToc farm_item_toc_8 = {45, {
    0, 120, 240, 360, 480, 676, 760, 844, 928, 1012, 1096, 1180, 1300, 1420, 1540, 1660,
    1756, 1852, 1948, 2044, 2140, 2236, 2332, 2428, 2524, 2620, 2704, 2788, 2872, 2956,
    3040, 3136, 3256, 3376, 3496, 3580, 3664, 3748, 3832, 3916, 4000, 4096, 4192, 4288,
    4408, 4692}};
static AnimPackToc farm_item_toc_9 = {20, {
    0, 128, 256, 384, 504, 624, 744, 864, 984, 1068, 1188, 1308, 1428, 1512, 1596, 1692,
    1788, 1884, 1968, 2168, 2252}};
AnimPackToc *farm_item_tocs[10] = {
    &farm_item_toc_0, &farm_item_toc_1, &farm_item_toc_2, &farm_item_toc_3, &farm_item_toc_4,
    &farm_item_toc_5, &farm_item_toc_6, &farm_item_toc_7, &farm_item_toc_8, &farm_item_toc_9,
};
/* The tables of contents of the farm's animation packs (AnimPackToc), one per
 * resource id, indexed by it. Each starts at its header size, 4 * (count + 1): the
 * pack opens with the count and the request id of each .ANI. Packs 35..49 end with the
 * start of a second texture pack. */
static AnimPackToc farm_pack_toc_0 = {2, {12, 1008, 2200}};
static AnimPackToc farm_pack_toc_1 = {5, {24, 864, 1704, 2544, 3384, 4224}};
static AnimPackToc farm_pack_toc_2 = {4, {20, 1000, 2088, 2992, 3752}};
static AnimPackToc farm_pack_toc_3 = {3, {16, 1128, 1968, 2808}};
static AnimPackToc farm_pack_toc_4 = {3, {16, 1052, 2148, 3244}};
static AnimPackToc farm_pack_toc_5 = {5, {24, 1120, 2232, 3136, 3976, 5072}};
static AnimPackToc farm_pack_toc_6 = {5, {24, 864, 1768, 2672, 3432, 4500}};
static AnimPackToc farm_pack_toc_7 = {5, {24, 488, 1328, 2424, 3264, 4024}};
static AnimPackToc farm_pack_toc_8 = {4, {20, 892, 1988, 3084, 4276}};
static AnimPackToc farm_pack_toc_9 = {2, {12, 916, 2012}};
static AnimPackToc farm_pack_toc_10 = {6, {28, 932, 1772, 2612, 3724, 4564, 5404}};
static AnimPackToc farm_pack_toc_11 = {1, {8, 912}};
static AnimPackToc farm_pack_toc_12 = {7, {32, 872, 1712, 2552, 3456, 4552, 5744, 6584}};
static AnimPackToc farm_pack_toc_13 = {4, {20, 1132, 2104, 2864, 3784}};
static AnimPackToc farm_pack_toc_17 = {18, {
    76, 1172, 2076, 3056, 4144, 5240, 6352, 7448, 8560, 9464, 10436, 11532, 12436,
    13532, 14436, 15408, 16520, 17616, 18456}};
static AnimPackToc farm_pack_toc_18 = {17, {
    72, 1168, 2148, 3236, 4332, 5444, 6348, 7460, 8364, 9336, 10432, 11528, 12432,
    13528, 14500, 15612, 16708, 17548}};
static AnimPackToc farm_pack_toc_19 = {11, {
    48, 1144, 2180, 3292, 4388, 5360, 6456, 7428, 8524, 9364, 10432, 11272}};
static AnimPackToc farm_pack_toc_20 = {18, {
    76, 1172, 2076, 3056, 4144, 5240, 6352, 7324, 8196, 9292, 10388, 11292, 12388,
    13292, 14264, 15376, 16472, 17312, 18232}};
static AnimPackToc farm_pack_toc_21 = {18, {
    76, 1172, 2260, 3356, 4468, 5372, 6484, 7456, 8552, 9424, 10520, 11616, 12520,
    13616, 14588, 15700, 16796, 17636, 18556}};
static AnimPackToc farm_pack_toc_22 = {12, {
    52, 1148, 2184, 3296, 4392, 5364, 6460, 7556, 8528, 9624, 10464, 11384, 12452}};
static AnimPackToc farm_pack_toc_23 = {17, {
    72, 1168, 2072, 3052, 4140, 5236, 6348, 7320, 8416, 9320, 10416, 11320, 12416,
    13320, 14292, 15404, 16500, 17420}};
static AnimPackToc farm_pack_toc_24 = {18, {
    76, 1172, 2260, 3356, 4468, 5372, 6484, 7456, 8552, 9648, 10744, 11648, 12744,
    13648, 14744, 15716, 16828, 17924, 18844}};
static AnimPackToc farm_pack_toc_25 = {12, {
    52, 1148, 2184, 3296, 4392, 5364, 6460, 7556, 8528, 9624, 10544, 11612, 12452}};
static AnimPackToc farm_pack_toc_26 = {17, {
    72, 1168, 2072, 3052, 4140, 5176, 6272, 7384, 8356, 9452, 10356, 11452, 12356,
    13328, 14440, 15536, 16376, 17296}};
static AnimPackToc farm_pack_toc_27 = {19, {
    80, 920, 2008, 3044, 4140, 5252, 6092, 6932, 8044, 9016, 9856, 10696, 11600, 12440,
    13412, 14524, 15620, 16716, 17636, 18476}};
static AnimPackToc farm_pack_toc_28 = {13, {
    56, 1152, 2188, 3300, 4396, 5368, 6464, 7560, 8532, 9628, 10468, 11388, 12456,
    13296}};
static AnimPackToc farm_pack_toc_29 = {18, {
    76, 1172, 2076, 3056, 4144, 5240, 6352, 7324, 8564, 9468, 10564, 11468, 12440,
    13552, 14648, 15488, 16408, 17328, 18712}};
static AnimPackToc farm_pack_toc_30 = {17, {
    72, 1168, 2256, 3352, 4464, 5368, 6480, 7452, 8548, 9644, 10740, 11644, 12740,
    13712, 14824, 15920, 16760, 17680}};
static AnimPackToc farm_pack_toc_31 = {13, {
    56, 1152, 2188, 3300, 4396, 5368, 6464, 7436, 8532, 9372, 10292, 11360, 12200,
    13404}};
static AnimPackToc farm_pack_toc_32 = {19, {
    80, 1176, 2080, 3060, 4148, 5244, 6148, 7052, 8024, 9120, 10024, 10928, 11900,
    13012, 14108, 14948, 15868, 16788, 17772, 18612}};
static AnimPackToc farm_pack_toc_33 = {18, {
    76, 1172, 2260, 3356, 4468, 5372, 6484, 7620, 8592, 9688, 10784, 11880, 12784,
    13880, 14852, 15964, 17060, 17900, 18820}};
static AnimPackToc farm_pack_toc_34 = {14, {
    60, 1156, 2192, 3600, 4504, 5600, 6572, 7668, 8640, 9736, 10576, 11496, 12564,
    13404, 14608}};
static AnimPackToc farm_pack_toc_14 = {17, {
    72, 912, 1752, 2732, 3820, 4916, 6028, 6868, 7708, 8680, 9584, 10424, 11520, 12360,
    13280, 14484, 15324, 16164}};
static AnimPackToc farm_pack_toc_15 = {20, {
    84, 1180, 2268, 3364, 4476, 5316, 6236, 7140, 8112, 9208, 10080, 10984, 12080,
    13052, 14164, 15260, 16100, 17020, 17860, 18700, 19540}};
static AnimPackToc farm_pack_toc_16 = {13, {
    56, 1152, 2188, 3300, 4396, 5368, 6464, 7560, 8656, 9496, 10416, 11484, 12324,
    13528}};
static AnimPackToc farm_pack_toc_35 = {2, {12, 2264, 2960, 16512}};
static AnimPackToc farm_pack_toc_36 = {2, {12, 2264, 4420, 17972}};
static AnimPackToc farm_pack_toc_37 = {2, {12, 2264, 4192, 17744}};
static AnimPackToc farm_pack_toc_38 = {2, {12, 2112, 2808, 15536}};
static AnimPackToc farm_pack_toc_39 = {2, {12, 2112, 4268, 16996}};
static AnimPackToc farm_pack_toc_40 = {2, {12, 2112, 4040, 16768}};
static AnimPackToc farm_pack_toc_41 = {2, {12, 2036, 2732, 12972}};
static AnimPackToc farm_pack_toc_42 = {2, {12, 2036, 4192, 14432}};
static AnimPackToc farm_pack_toc_43 = {2, {12, 2036, 3964, 14204}};
static AnimPackToc farm_pack_toc_44 = {2, {12, 2164, 2860, 16492}};
static AnimPackToc farm_pack_toc_45 = {2, {12, 2164, 4320, 17952}};
static AnimPackToc farm_pack_toc_46 = {2, {12, 2164, 4092, 17724}};
static AnimPackToc farm_pack_toc_47 = {2, {12, 1912, 2608, 15320}};
static AnimPackToc farm_pack_toc_48 = {2, {12, 1912, 4068, 16780}};
static AnimPackToc farm_pack_toc_49 = {2, {12, 1912, 3840, 16552}};
/* By resource id. */
AnimPackToc *farm_pack_tocs[50] = {
    &farm_pack_toc_0, &farm_pack_toc_1, &farm_pack_toc_2, &farm_pack_toc_3,
    &farm_pack_toc_4, &farm_pack_toc_5, &farm_pack_toc_6, &farm_pack_toc_7,
    &farm_pack_toc_8, &farm_pack_toc_9, &farm_pack_toc_10, &farm_pack_toc_11,
    &farm_pack_toc_12, &farm_pack_toc_13, &farm_pack_toc_14, &farm_pack_toc_15,
    &farm_pack_toc_16, &farm_pack_toc_17, &farm_pack_toc_18, &farm_pack_toc_19,
    &farm_pack_toc_20, &farm_pack_toc_21, &farm_pack_toc_22, &farm_pack_toc_23,
    &farm_pack_toc_24, &farm_pack_toc_25, &farm_pack_toc_26, &farm_pack_toc_27,
    &farm_pack_toc_28, &farm_pack_toc_29, &farm_pack_toc_30, &farm_pack_toc_31,
    &farm_pack_toc_32, &farm_pack_toc_33, &farm_pack_toc_34, &farm_pack_toc_35,
    &farm_pack_toc_36, &farm_pack_toc_37, &farm_pack_toc_38, &farm_pack_toc_39,
    &farm_pack_toc_40, &farm_pack_toc_41, &farm_pack_toc_42, &farm_pack_toc_43,
    &farm_pack_toc_44, &farm_pack_toc_45, &farm_pack_toc_46, &farm_pack_toc_47,
    &farm_pack_toc_48, &farm_pack_toc_49,
};
/* Three-word records for the weather drift: drift_by_hour is per time of day,
 * drift_by_weather the other set. */
s32 drift_by_hour[4][3] = {
    {0x14000, 0, 0},
    {0, 0, 0},
    {0x5000, 0x1E000, 0x28000},
    {0x50000, 0x46000, 0x2D000},
};
s32 drift_by_weather[8][3] = {
    {0x3C000, 0x2D000, 0x7800},
    {0x3C000, 0x2D000, 0x7800},
    {0x3C000, 0x2D000, 0x7800},
    {0x78000, 0x5A000, 0xF000},
    {0x5000, 0x28000, 0x28000},
    {0x46000, 0xA000, 0xA000},
    {0xA000, 0xA000, 0x46000},
    {0, 0, 0},
};
/* The position of each house door (x, y). */
HouseDoorPosition house_door_positions[3] = {
    {-90, 541}, {791, 258}, {902, 1804},
};
/* One word per place, 0 or 0x1000 for the first places; from the 78th word on, lists of
 * places, each ended by -1, that resource_lists points into. */
s16 place_entries[408] = {
    0, 0, 4096, 4096, 4096, 4096, 4096, 0, 0, 4096, 4096, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4096,
    4096, 4096, 4096, 4096, 0, 4096, 4096, 0, 4096, 4096, 4096, 4096, 0, 4096, 4096, 4096,
    4096, 0, 4096, 4096, 0, 0, 0, 0, 0, 0, 0, 4096, 4096, 0, 0, 4096, 0, 0, 4096, 4096, 4096,
    4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 0, 4096, 4096, 4096, 4096, 4096, 4096,
    4096, 4096, 4096, 4096, 35, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 81, 85, -1, -1,
    -1, 102, 103, 132, 131, 80, 134, 232, -1, -1, -1, -1, -1, -1, -1, -1, 161, 162, 163, 42,
    58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 82, 86, -1, -1, -1,
    102, 103, 132, 73, 131, 134, 232, -1, -1, -1, -1, -1, -1, -1, -1, 161, 162, 163, -1, -1, 0,
    43, 58, 59, 60, 61, 62, 63, 64, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 83, 87, 80, -1, -1,
    102, 103, 132, 84, 131, 134, 232, -1, -1, -1, -1, -1, -1, -1, -1, 82, 133, 314, 161, 162,
    163, 29, 315, 59, 60, 61, 316, 317, -1, -1, 102, 103, 132, -1, 134, 232, -1, -1, -1, -1,
    -1, -1, -1, -1, 161, 162, 163, 391, -1, -1, -1, 31, 58, 61, 62, 63, 65, 66, 67, 68, 69, 70,
    71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 85, 86, -1, -1, 84, 64, 134, 232, 45,
    81, 82, 83, 85, 86, 106, 107, 108, 109, 110, 375, 84, 165, 166, 167, 296, 297, 298, 49, 50,
    51, 52, 53, 71, -1, 154, 153, 33, 111, 89, -1, 161, 162, 163, 240, 241, 242, -1, -1, -1,
    235, 233, 236, 62, 391, 21, -1, -1, 111, 113, 114, 115, 116, 117, 118, 119, 120, 121, -1,
    -1, -1, -1, -1, -1, 55, -1, -1, 20, 96, 97, 98, 99, 122, 123, 124, 312, 102, 161, 162, 163,
    239, -1, 235, 233, 236, -1, 62, -1, 0, 34, 58, 59, 60, 61, 62, 64, 81, 82, 83, 85, 86, 88,
    89, 90, 91, 92, 93, 94, 95, 84, -1, -1, 161, 162, 163, 237, 238, -1, -1, 235, 233, 236, -1,
    391, 0, 16, 104, -1, -1, -1, -1, 134, 232, 105, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};
/* The starts of the lists in place_entries. Each list runs to the -1 that ends it; no length is stored. */
s16 *resource_lists[10] = {
    &place_entries[78], &place_entries[114], &place_entries[158], &place_entries[202], &place_entries[232],
    &place_entries[354], &place_entries[310], &place_entries[390], &place_entries[264], &place_entries[332],
};
