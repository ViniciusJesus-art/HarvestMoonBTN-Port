"""Extract numeric native views of the existing C tables. No game assets/SDK."""
import re
import sys
from pathlib import Path


def clean(text):
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)


def body(text, name):
    match = re.search(r"\b" + name + r"\s*\[[^;=]*=\s*\{", text)
    if not match:
        raise ValueError(f"Missing original table: {name}")
    start = match.end()
    depth = 1
    for i in range(start, len(text)):
        depth += (text[i] == "{") - (text[i] == "}")
        if not depth:
            return text[start:i]
    raise ValueError(f"Unterminated table: {name}")


def fields(text):
    result, depth, start = [], 0, 0
    for i, char in enumerate(text + ","):
        if char == "," and not depth:
            value = text[start:i].strip()
            if value:
                result.append(value)
            start = i + 1
        depth += (char == "{") - (char == "}")
    if depth:
        raise ValueError("Unbalanced original initializer")
    return result


def main(root, output):
    src = Path(root) / "src"
    tables = clean((src / "main/game/farm_tables.c").read_text())
    items = clean((src / "include/items.h").read_text())
    header = clean((src / "main/game/farm_tables.h").read_text())
    names = {}
    for text, enum in [(items, "Item"), (header, "Crop")]:
        section = re.search(r"enum " + enum + r"\s*\{([^}]+)\}", text).group(1)
        current = -1
        for entry in fields(section):
            pair = entry.split("=", 1)
            current = int(pair[1].strip(), 0) if len(pair) == 2 else current + 1
            names[pair[0].strip()] = current

    def number(value):
        return names[value] if value in names else int(value, 0)

    out = ["/* Generated from original C tables; do not edit. */"]
    out.append("static const HmbtnFarmItem farm_items[380] = {")
    item_rows = {}
    for entry in fields(body(tables, "item_info")):
        match = re.fullmatch(r"\[([^]]+)\]\s*=\s*\{(.*)\}", entry, re.S)
        if not match:
            raise ValueError("Unsupported item initializer")
        key, values = number(match[1]), fields(match[2])
        if len(values) != 15:
            raise ValueError(f"Unexpected ItemInfo fields: {key}: {len(values)}")
        rule = ",".join(str(number(x)) for x in fields(values[3][1:-1]))
        prefix = ",".join(str(number(values[i])) for i in [0, 1, 2])
        suffix = ",".join(str(number(values[i])) for i in [4, 5, 6, 10, 11, 12, 13, 14])
        if key in item_rows:
            raise ValueError(f"Duplicate item row: {key}")
        class_flags=re.fullmatch(r"\{\s*\.word\s*=\s*(0x[0-9a-fA-F]+)\s*\}",values[8])
        if not class_flags:
            raise ValueError(f"Unexpected class flags: {key}")
        item_rows[key] = f"{{{prefix},{{{rule}}},{suffix},{class_flags[1]}u}},"
    if set(item_rows) != set(range(380)):
        raise ValueError("Original item table must define all 380 rows")
    out.extend(item_rows[key] for key in range(380))
    out.append("};")
    for name, kind, count in [("tile_rules", "HmbtnFarmTileRule", 17),
                              ("crop_table", "HmbtnFarmCrop", 22)]:
        out.append(f"static const {kind} {name}[{count}]={{")
        rows = fields(body(tables, name))
        if len(rows) != count:
            raise ValueError(f"Unexpected {name} row count")
        for row in rows:
            values = [number(v) for v in fields(row[1:-1])]
            if name == "crop_table":
                out.append("{%d,%d,%d,{%s},%d}," % (*values[:3], ",".join(map(str, values[3:7])), values[7]))
            else:
                out.append("{" + ",".join(map(str, values)) + "},")
        out.append("};")
    probe = clean((src / "overlay/11_GF_FARM/farm_probe.c").read_text())
    for name in ["farm_choices_default", "farm_choices_flagged"]:
        out.append(f"static const HmbtnFarmChoice {name}[]={{" + body(probe, name) + "};")
    field = clean((src / "overlay/11_GF_FARM/field/field.c").read_text())
    for name, kind in [("cell_looks", "HmbtnFarmCellUV"), ("column_mirror_table", "uint8_t")]:
        out.append(f"static const {kind} {name}[]={{" + body(field, name) + "};")
    sound = clean((src / "main/sound/sound.c").read_text())
    out.append("static const HmbtnFarmSound farm_sounds[]={" + body(sound, "sound_defs") + "};")
    Path(output).write_text("\n".join(out) + "\n")
    swap=clean((src / "overlay/11_GF_FARM/menu/storage_swap.c").read_text())
    Path(output).with_name("storage_spots.inc").write_text(
        "/* Original storage_swap.c positions and directional links. */\n"
        "static const HmbtnStorageSpot storage_spots[18]={"+body(swap,"storage_spots")+"};\n")
    farm_data=clean((src / "overlay/11_GF_FARM/farm_data.c").read_text())
    places=fields(body(farm_data,"place_entries"))[:78]
    player=clean((src / "overlay/11_GF_FARM/people/player.c").read_text())
    Path(output).with_name("clock_tables.inc").write_text(
        "/* Original clock/weather data; generated, never PS1 pointers. */\n"
        "static const int32_t drift_by_hour[4][3]={"+body(farm_data,"drift_by_hour")+"};\n"
        "static const uint16_t clock_places[78]={"+",".join(places)+"};\n"
        "static const int hour_fatigue[4][2]={"+body(player,"hour_events")+"};\n")
    estate=clean((src / "overlay/11_GF_FARM/estate.c").read_text())
    slots={}
    for entry in fields(re.search(r"enum EstateShipmentSlot\s*\{([^}]+)\}",estate).group(1)):
        key,value=entry.split("=");slots[key.strip()]=int(value.strip(),0)
    bins=[-1]*380
    for item in range(names["ITEM_TURNIP"],names["ITEM_GRASS"]):
        bins[item]=item-names["ITEM_TURNIP"]
    switch=estate.split("s16 estate_item_slot_index(",1)[1].split("switch (id)",1)[1].split("return -1;",1)[0]
    groups=re.findall(r"((?:\s*case ITEM_\w+\s*:)+)\s*return ([^;]+);",switch)
    if len(groups)!=8:
        raise ValueError("Unexpected original shipment bin groups")
    for cases,expression in groups:
        for item in re.findall(r"case (ITEM_\w+)",cases):
            if expression in slots:
                bins[names[item]]=slots[expression]
            elif expression=="ESTATE_SLOT_BLUE_GRASS + (id - ITEM_BLUE_GRASS)":
                bins[names[item]]=slots["ESTATE_SLOT_BLUE_GRASS"]+names[item]-names["ITEM_BLUE_GRASS"]
            else:
                raise ValueError(f"Unsupported shipment bin: {expression}")
    if set(bins)!=set(range(25))|{-1}:
        raise ValueError("Unexpected original shipment bins")
    clock=clean((src / "overlay/11_GF_FARM/world/clock.c").read_text())
    dates=[fields(row[1:-1])[:2] for row in fields(body(clock,"clock_date_table"))[:14]]
    Path(output).with_name("shipping_tables.inc").write_text(
        "/* Original estate shipment bins and clock_date_index dates. */\n"
        "static const int8_t shipping_bins[380]={"+",".join(map(str,bins))+"};\n"
        "static const unsigned shipping_special_dates[14][2]={"+
        ",".join("{"+",".join(row)+"}" for row in dates)+"};\n"
        f"enum {{ SHIPPING_FISH_FIRST={names['ITEM_LARGE_FISH']}, SHIPPING_HONEY={names['ITEM_HONEY']} }};\n")
    records=fields(body(tables,"estate_records"))
    positions=[]
    for record in records[14:46]:
        values=fields(record[1:-1]);point=fields(values[4][1:-1])[:3]
        positions.append("{"+",".join(point)+"}")
    if len(positions)!=32: raise ValueError("Unexpected feed bin positions")
    Path(output).with_name("feed_positions.inc").write_text(
        "/* Original estate records: barn run 2 and coop run 3. */\n"
        "static const int32_t feed_positions[32][3]={"+",".join(positions)+"};\n")
    gather=[]
    for record in records:
        values=fields(record[1:-1]);point=fields(values[4][1:-1])[:3]
        gather.append("{"+",".join(str(number(x)) for x in values[:3])+",{"+",".join(point)+"}}")
    kinds=[]
    for row in fields(body(estate,"estate_kind_values")):
        kinds.append("{"+",".join(str(number(x)) for x in fields(row[1:-1]))+"}")
    Path(output).with_name("gather_tables.inc").write_text(
        "/* Original estate records and seasonal substitutions. */\n"
        "static const HmbtnGatherRecord gather_records[197]={"+",".join(gather)+"};\n"
        "static const unsigned gather_kinds[4][3]={"+",".join(kinds)+"};\n"
        "static const struct { unsigned item,art; } gather_art[]={"+",".join(
            "{%d,%d}" % (names["ITEM_"+name],art) for name,art in [
            ("WEEDS",5),("MOON_DROP_FLOWER",6),("PINK_CAT_FLOWER",7),("BLUE_MAGIC_RED_FLOWER",8),
            ("RED_MAGIC_RED_FLOWER",19),("TOY_FLOWER",9),("ORANGE_CUP_FRUIT",10),
            ("BAMBOO_SHOOT",11),("WILD_GRAPE",12),("MUSHROOM",13),
            ("POISONOUS_MUSHROOM",14),("TRUFFLE",15),("BLUE_GRASS",16),("RED_GRASS",17),("GREEN_GRASS",18)])+"};\n")
    art=[]
    for i in range(10):
        match=re.search(rf"farm_item_toc_{i}\s*=\s*\{{(\d+),\s*\{{([^}}]+)\}}\}}",farm_data)
        if not match: raise ValueError(f"Missing item pack {i}")
        art.append("{"+match[1]+",{"+match[2]+"}}")
    Path(output).with_name("estate_art_tables.inc").write_text(
        "/* Original farm item art packs. */\n"
        "static const uint32_t estate_art_offsets[11]={"+body(clean((src / "overlay/11_GF_FARM/farm_load.c").read_text()),"farm_item_file_offsets")+"};\n"
        "static const HmbtnEstateArtToc estate_art_tocs[10]={"+",".join(art)+"};\n")
    sections=clean((src / "overlay/common/farm_section_table.h").read_text())
    offsets=[int(v,0) for v in fields(body(sections,"section_table"))]
    if len(offsets)!=1021:
        raise ValueError("Unexpected scene section count")
    Path(output).with_name("scene_sections.inc").write_text(
        "/* Original seasonal section offsets. */\nstatic const uint32_t scene_sections[255][3]={\n"+
        "\n".join("{%d,%d,%d}," % tuple(offsets[i*4:i*4+3]) for i in range(255))+"\n};\n")
    sound_offsets=[int(v,0) for v in fields(body(sound,"sound_offsets"))]
    requests=clean((src / "main/sound/sound_request.c").read_text())
    rows=[]
    for i,row in enumerate(fields(body(requests,"song_table"))):
        vab,seq=[int(v,0) for v in fields(row[1:-1])]
        a=vab*2;b=seq*2
        rows.append("{%d,%d,%d,%d,%d}," % (i+1,sound_offsets[a],sound_offsets[a+1]-sound_offsets[a],sound_offsets[b],sound_offsets[b+2]))
    Path(output).with_name("music_rows.inc").write_text("/* Original VAB/SEQ ranges. */\n"+"\n".join(rows)+"\n")
    flags=clean((src / "main/game/event_flags.h").read_text())
    flag_values={key:int(value,0) for key,value in re.findall(r"(EVENT_FLAG_\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)",flags)}
    history=clean((src / "overlay/11_GF_FARM/event/cue_history.c").read_text())
    village=["/* Generated original conditions, schedules and animation packs. */"]
    for key,value in flag_values.items(): village.append(f"#define {key} {value}")
    scripts=[]
    for i in range(37):
        if not re.search(rf"\bcue_history_item_{i}\[",history): scripts.append("{NULL,0}");continue
        values=body(history,f"cue_history_item_{i}")
        values=re.sub(r"CUE_NOT\(([^)]+)\)",r"(0x8000 | \1)",values)
        for key,value in {"CUE_IF":65533,"CUE_THEN":65534,"CUE_ELSE":65532,"CUE_NONE":65531}.items(): values=values.replace(key,str(value))
        village.append(f"static const uint16_t history_{i}[]={{"+values+"};")
        scripts.append(f"{{history_{i},sizeof(history_{i})/sizeof(history_{i}[0])}}")
    village.append("static const HmbtnHistory histories[37]={"+",".join(scripts)+"};")
    rules=clean((src / "overlay/11_GF_FARM/event/event_rules.c").read_text())
    village.append("#define EVENT_RESULT_DEFAULT 0x1CD")
    for key in re.findall(r"static FarmCommandPair (\w+)\[",rules):
        village.append(f"static const HmbtnDoorRule {key}[]={{"+body(rules,key)+"};")
    rows=[]
    for number,count,table in re.findall(r"static FarmScript event_script_\d+\s*=\s*\{(\d+),\s*(\d+),\s*(\w+)\}",rules):
        rows.append(f"{{{number},{count},{table}}}")
    village.append("static const HmbtnDoorScript door_scripts[]={"+",".join(rows)+"};")
    village.append("static const uint32_t villager_pack_offsets[51]={"+body(clean((src / "overlay/11_GF_FARM/farm_load.c").read_text()),"farm_pack_file_offsets")+"};")
    tocs=[]
    for i in range(35):
        match=re.search(rf"farm_pack_toc_{i}\s*=\s*\{{(\d+),\s*\{{([^}}]+)\}}\}}",farm_data)
        if not match: raise ValueError(f"Missing villager pack {i}")
        tocs.append("{"+match[1]+",{"+match[2]+"}}")
    village.append("static const HmbtnVillagePack village_packs[35]={"+",".join(tocs)+"};")
    Path(output).with_name("village_tables.inc").write_text("\n".join(village)+"\n")


if __name__ == "__main__":
    main(*sys.argv[1:])
