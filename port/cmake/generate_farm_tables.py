"""Extract numeric native views of the existing C tables. No game assets/SDK."""
import re
import sys
from pathlib import Path


def clean(text):
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)


def body(text, name):
    match = re.search(r"\b" + name + r"\s*(?:\[[^;=]*)?=\s*\{", text)
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

    grids = fields(body(tables, "picture_grids"))
    grid = fields(grids[25][1:-1])[0]
    rows = fields(grid[1:-1])
    offsets = {}
    for y, row in enumerate(rows):
        for x, value in enumerate(fields(row[1:-1])):
            part = int(value, 0)
            if part:
                offsets[part] = (x-6, y-6)
    if set(offsets) != set(range(1, 10)):
        raise ValueError("Unexpected original seed grid")
    Path(output).with_name("seed_grid.inc").write_text(
        "/* farm_tables.c picture_grids[25], original nine seed targets. */\n"
        "static const int seed_grid[9][2]={" +
        ",".join("{%d,%d}" % offsets[i] for i in range(1, 10)) + "};\n")
    # All tool reach grids, in source part order. No reconstructed rectangles.
    tool_rows=[]
    for entry in grids[:25]:
        rows=fields(fields(entry[1:-1])[0][1:-1]);parts={}
        for y,row in enumerate(rows):
            for x,value in enumerate(fields(row[1:-1])):
                if int(value,0): parts[int(value,0)]=(x-6,y-6)
        if set(parts)!=set(range(1,len(parts)+1)) or len(parts)>25:
            raise ValueError("Unexpected original tool grid")
        tool_rows.append((len(parts),"{"+",".join("{%d,%d}" % parts[i] for i in range(1,len(parts)+1))+"}"))
    greenhouse=clean((src / "overlay/11_GF_FARM/field/greenhouse.c").read_text())
    player=clean((src / "overlay/11_GF_FARM/people/player.c").read_text())
    rules=["/* Original greenhouse level tables, tool charge counts and picture grids. */"]
    for name in ("greenhouse_level_values","greenhouse_level_alt_values"):
        rules.append("static const unsigned "+name+"[]={"+body(greenhouse,name)+"};")
    rules.append("static const unsigned tool_grid_counts[]={"+",".join(str(n) for n,_ in tool_rows)+"};")
    rules.append("static const int8_t tool_grids[25][25][2]={"+",".join(row for _,row in tool_rows)+"};")
    for name in ("tool_charge_steps_a","tool_charge_steps_b","tool_charge_steps_c"):
        rules.append("static const unsigned "+name+"[]={"+body(player,name)+"};")
    rules.append("static const int tool_stamina_costs[5][5]={"+body(player,"tool_stamina_costs")+"};")
    Path(output).with_name("tool_rules.inc").write_text("\n".join(rules)+"\n")
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
    greenhouse=clean((src / "overlay/11_GF_FARM/field/greenhouse.c").read_text())
    out.append("static const HmbtnGreenhouseUV greenhouse_uvs[29]={"+body(greenhouse,"greenhouse_quad_uvs")+"};")
    sound = clean((src / "main/sound/sound.c").read_text())
    cd_audio=clean((src / "main/sound/cd_audio.c").read_text())
    xa_tracks=fields(body(cd_audio,"cd_audio_tracks"))
    if len(xa_tracks)!=26: raise ValueError("Unexpected original XA track count")
    Path(output).with_name("xa_tracks.inc").write_text(
        "/* cd_audio.c:cd_audio_tracks, volume countdown / file / channel. */\n"
        "static const unsigned xa_tracks[26][3]={"+",".join(xa_tracks)+"};\n")
    out.append("static const HmbtnFarmSound farm_sounds[]={" + body(sound, "sound_defs") + "};")
    Path(output).write_text("\n".join(out) + "\n")
    swap=clean((src / "overlay/11_GF_FARM/menu/storage_swap.c").read_text())
    Path(output).with_name("storage_spots.inc").write_text(
        "/* Original storage_swap.c positions and directional links. */\n"
        "static const HmbtnStorageSpot storage_spots[18]={"+body(swap,"storage_spots")+"};\n")
    farm_data=clean((src / "overlay/11_GF_FARM/farm_data.c").read_text())
    places=fields(body(farm_data,"place_entries"))[:78]
    player=clean((src / "overlay/11_GF_FARM/people/player.c").read_text())
    wake=fields(body(farm_data,"house_door_positions"))
    sleep=fields(fields(body(player,"result_camera_pairs"))[0][1:-1])
    if len(wake)!=3 or len(sleep)!=3:
        raise ValueError("Unexpected original house wake/sleep positions")
    Path(output).with_name("house_layouts.inc").write_text(
        "/* farm_data.c:house_door_positions; player.c:result_camera_pairs. */\n"
        "static const int32_t house_wake[3][2]={"+",".join(wake)+"};\n"
        "static const int32_t house_sleep[3][2]={"+",".join(sleep)+"};\n")
    load=clean((src / "overlay/11_GF_FARM/farm_load.c").read_text())
    action_base=re.search(r"#define FARM_SECTOR_ROW_BASE\s+(0x[0-9A-Fa-f]+)",load)
    action_sectors=re.search(r"#define FARM_SECTOR_ROW_SECTORS\s+(\d+)",load)
    action_banks=fields(body(player,"player_anim_banks"))
    if not action_base or not action_sectors or int(action_sectors[1])!=7 or len(action_banks)!=28:
        raise ValueError("Unexpected original player action bank layout")
    Path(output).with_name("action_resources.inc").write_text(
        "/* Original farm_load_sector_row and player_anim_banks; logical row ids. */\n"
        f"static const uint32_t player_action_base={action_base[1]};\n"
        "static const uint16_t player_action_banks[28]={"+",".join(action_banks)+"};\n")
    Path(output).with_name("clock_tables.inc").write_text(
        "/* Original clock/weather data; generated, never PS1 pointers. */\n"
        "static const int32_t drift_by_hour[4][3]={"+body(farm_data,"drift_by_hour")+"};\n"
        "static const int32_t drift_by_weather[8][3]={"+body(farm_data,"drift_by_weather")+"};\n"
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
    Path(output).with_name("clock_dates.inc").write_text(
        "/* Original first 14 clock_date_table entries (1-based lookup). */\n"
        "static const unsigned clock_dates[14][2]={"+
        ",".join("{"+",".join(row)+"}" for row in dates)+"};\n")
    event_requests=clean((src / "overlay/11_GF_FARM/event/event_request.c").read_text())
    defaults=[255]*128
    reset=re.search(r"void scene_item_queue_reset\(void\)\s*\{(.*?)\n\}",event_requests,re.S)
    if not reset: raise ValueError("Missing original scene_item_queue_reset")
    initial=re.findall(r"game_state\.days\.count0\[(0x[0-9A-Fa-f]+|[0-9]+)\]\s*=\s*(0x[0-9A-Fa-f]+|[0-9]+)\s*;",reset.group(1))
    if not initial: raise ValueError("Missing original day-counter initialization")
    for index,value in initial:
        index,value=int(index,0),int(value,0)
        if index>=128 or value>255: raise ValueError("Invalid original day-counter default")
        defaults[index]=value
    Path(output).with_name("event_day_defaults.inc").write_text(
        "/* Original scene_item_queue_reset numeric defaults. */\n"
        "static const uint8_t event_day_count0[128]={"+",".join(map(str,defaults))+"};\n")
    milestones = clean((src / "overlay/11_GF_FARM/event/milestones.c").read_text())
    threshold = int(re.search(r"#define GROUP_SHIPMENT_THRESHOLD\s+(\d+)", milestones).group(1))
    groups = [[number(v) for v in fields(body(milestones, "milestone_group_" + group + "_ids"))[:4]]
              for group in "abc"]
    if any(len(group) != 4 or any(bins[item] < 0 for item in group) for group in groups):
        raise ValueError("Unexpected original crop shipment groups")
    Path(output).with_name("shipping_tables.inc").write_text(
        "/* Original estate shipment bins and clock_date_index dates. */\n"
        "static const int8_t shipping_bins[380]={"+",".join(map(str,bins))+"};\n"
        "static const unsigned seed_shipment_groups[3][4]={"+
        ",".join("{"+",".join(map(str, group))+"}" for group in groups)+"};\n"
        f"enum {{ SEED_SHIPMENT_THRESHOLD={threshold} }};\n"
        "static const unsigned shipping_special_dates[14][2]={"+
        ",".join("{"+",".join(row)+"}" for row in dates)+"};\n"
        f"enum {{ SHIPPING_FISH_FIRST={names['ITEM_LARGE_FISH']}, SHIPPING_HONEY={names['ITEM_HONEY']} }};\n")
    shop = clean((src / "overlay/11_GF_FARM/menu/shop_dialogues.c").read_text())
    # Building offers and overnight countdown, not the unrelated tool-upgrade screen.
    offers = fields(body(shop, "shop_silo_offers"))
    if len(offers) != 5:
        raise ValueError("Unexpected original construction sequence")
    milestones = clean((src / "overlay/11_GF_FARM/event/milestones.c").read_text())
    timer = re.search(r"void milestone_arm_countdown\(void\)\s*\{\s*game_state.progress.estate.stampTimer = (\d+);", milestones)
    limit = re.search(r"#define SHOP_LUMBER_MAX (\d+)", shop)
    Path(output).with_name("construction_tables.inc").write_text(
        "/* Original shop_silo_offers and milestone_arm_countdown (--timer < 0). */\n"
        "static const HmbtnConstructionOffer construction_offers[]={" + ",".join(offers) + "};\n"
        f"enum {{ CONSTRUCTION_MORNINGS={int(timer.group(1))+1}, LUMBER_MAX={limit.group(1)}, LUMBER_ITEM={names['ITEM_LUMBER']} }};\n")
    mine = clean((src / "overlay/11_GF_FARM/field/mine.c").read_text())
    mine_out=["/* Original field/mine.c tables. */"]
    for name in ["mine_layer_weights", "mine_layer_weights_mine"]:
        mine_out.append("static const int16_t "+name+"[10][8]={"+body(mine,name)+"};")
    for name in ["mine_layer_events", "mine_layer_events_mine"]:
        mine_out.append("static const int16_t "+name+"[8]={"+",".join(str(number(v)) for v in fields(body(mine,name)))+"};")
    words=body(mine,"mine_messages").replace("TEXT_CODE_SPACE","0xffd").replace("TEXT_LINE_BREAK","0xfffe").replace("TEXT_END","0xffff")
    mine_out.append("static const uint16_t mine_messages[68]={"+words+"};")
    mine_out.append("static const MineTexture mine_cell_textures[3]={"+body(mine,"mine_cell_textures")+"};")
    Path(output).with_name("mine_tables.inc").write_text("\n".join(mine_out)+"\n")
    livestock = clean((src / "overlay/11_GF_FARM/animals/livestock.c").read_text())
    animals = clean((src / "overlay/11_GF_FARM/animals/animals.c").read_text())
    animal_header = clean((src / "overlay/11_GF_FARM/animals/animals.h").read_text())
    farm_load = clean((src / "overlay/11_GF_FARM/farm_load.c").read_text())
    farm_data = clean((src / "overlay/11_GF_FARM/farm_data.c").read_text())
    defines = re.findall(r"#define (LIVESTOCK_\w+)\s+(0x[0-9a-fA-F]+|[0-9]+)\b", livestock + "\n" + animal_header + "\n" + clean((src / "overlay/11_GF_FARM/animals/livestock.h").read_text()))
    native = ["/* Original livestock.c constants/frame tables and animals.c placement. */"]
    native.extend(f"enum {{ {key}={value} }};" for key, value in defines)
    messages = re.findall(r"(LIVESTOCK_MSG_\w+)\s*=\s*(0x[0-9a-fA-F]+)", animal_header)
    native.extend(f"enum {{ {key}={value} }};" for key, value in messages)
    native.append("static const NativeFrameRow cow_frames[9]={"+body(livestock,"livestock_frame_table")+"};")
    native.append("static const NativeFrame sheep_frames[6][4]={"+body(livestock,"livestock_frame_table_b")+"};")
    native.append("static const int32_t row_spots[6][4]={"+body(animals,"farm_row_spots")+"};")
    native.append("static const NativeGateRow animal_gates[4]={"+body(probe,"farm_zone_rows")+"};")
    Path(output).with_name("livestock_tables.inc").write_text("\n".join(native)+"\n")
    offsets = fields(body(farm_load,"farm_slot_file_offsets"))
    toc = fields(body(farm_data,"farm_slot_toc_0"))
    Path(output).with_name("livestock_art_tables.inc").write_text(
        "/* farm_load.c slot 0 and farm_data.c:farm_slot_toc_0. */\n"
        f"enum {{ LIVESTOCK_FILE_START={offsets[0]}, LIVESTOCK_FILE_BYTES={offsets[1]}-{offsets[0]}, LIVESTOCK_BARN_START={offsets[1]}, LIVESTOCK_BARN_BYTES={offsets[2]}-{offsets[1]} }};\n"
        "static const unsigned livestock_offsets[]={"+toc[1][1:-1]+"};\n")
    text_bank = clean((src / "main/text/text_bank.c").read_text())
    setup_tools = shop[shop.index("static s32 shop_tool_upgrade_menu_setup(void)\n{"):
                       shop.index("static s32 shop_greenhouse_start(void)",shop.index("static s32 shop_tool_upgrade_menu_setup(void)\n{"))]
    tool_labels = re.findall(r"\*text = LOCALIZED_NAME\(text_bank.names.(brush|milker|clippers)\)",setup_tools)
    tool_entries = re.findall(r"END_ENTRY\((ITEM_(?:BRUSH|MILKER)),\s*0,\s*(0x[0-9A-Fa-f]+)\)",setup_tools)
    tool_entries += re.findall(r"\*entryValue = (ITEM_CLIPPERS);\s*\*entryFlag = 0;\s*\*label46 = (0x[0-9A-Fa-f]+);",setup_tools)
    if len(tool_labels)!=3 or len(tool_entries)!=3:
        raise ValueError("Unexpected original animal-tool menu")
    tool_menu = ["/* shop_tool_upgrade_menu_setup: original labels, order, item ids/descriptions. */"]
    for label in tool_labels:
        words = re.search(r"\."+label+r"\s*=\s*(\{[^}]+\})",text_bank).group(1)
        tool_menu.append(f"static const uint16_t tool_name_{label}[]={words};")
    tool_menu.append("static const HmbtnSeedShopEntry animal_tools[]={")
    for label,(item,description) in zip(tool_labels,tool_entries):
        tool_menu.append(f"{{{names[item]},{description},tool_name_{label},sizeof(tool_name_{label})/sizeof(uint16_t)}},")
    tool_menu.append("};")
    Path(output).with_name("animal_tool_menu.inc").write_text("\n".join(tool_menu)+"\n")
    Path(output).with_name("saibara_gifts.inc").write_text(
        "/* shop_tool_upgrade_menu_setup: original random present ids. */\n"
        "static const unsigned saibara_gifts[]={"+body(shop,"girls_present_messages")+"};\n")
    # Storage navigation from item_storage.c, for all three rucksack levels.
    storage = clean((src / "overlay/11_GF_FARM/menu/item_storage.c").read_text())
    links = [fields(row[1:-1])[2] for row in fields(body(storage, "item_markers"))]
    if len(links) != 18:
        raise ValueError("Unexpected storage cursor table")
    Path(output).with_name("storage_links.inc").write_text(
        "/* item_storage.c:item_markers, directions by rucksack level. */\n"
        "static const uint8_t storage_links[18][12]={" + ",".join(links) + "};\n")
    # Reuse the complete original recipe definitions, bonuses and priority order.
    kitchen_header = clean((src / "overlay/11_GF_FARM/menu/house.h").read_text())
    for enum_text, enum_name in ((kitchen_header,"Utensil"),(kitchen_header,"UtensilFlag"),(header,"SeasoningFlag")):
        section = re.search(r"enum " + enum_name + r"\s*\{([^}]+)\}",enum_text).group(1)
        current = -1
        for entry in fields(section):
            pair = entry.split("=",1)
            if len(pair)==2:
                expression = re.sub(r"\b[A-Za-z_]\w*\b",lambda m:str(names[m[0]]),pair[1])
                current = eval(expression,{"__builtins__":{}},{})
            else:
                current += 1
            names[pair[0].strip()] = current
    recipes_source = clean((src / "main/game/cooking_recipes.c").read_text())
    recipe_data = recipes_source[recipes_source.index("static RecipeBonus"):]
    recipe_labels = list(dict.fromkeys(re.findall(r"DISH_NAME\((\w+)\)",recipe_data)))
    kitchen = ["/* Original cooking_recipes.c tables, numeric ids and native pointers. */"]
    for label in recipe_labels:
        words = re.search(r"\."+label+r"\s*=\s*(\{[^}]+\})",text_bank).group(1)
        kitchen.append(f"static const uint16_t dish_{label}[]={words};")
    recipe_data = recipe_data.replace("static RecipeBonus", "static const HmbtnRecipeBonus")
    recipe_data = recipe_data.replace("Recipe recipes[RECIPE_COUNT]", "static const HmbtnRecipe recipes[]")
    recipe_data = re.sub(r"DISH_NAME\((\w+)\)",r"dish_\1",recipe_data)
    recipe_data = re.sub(r"\b(?:ITEM|UTENSIL_FLAG|SEASONING_FLAG)_\w+\b",lambda m:str(names[m[0]]),recipe_data)
    recipe_data = recipe_data.replace("COOKING_FIASCO_QUALITY","246").replace("{}","{0}")
    kitchen.append(recipe_data)
    original_house = clean((src / "overlay/11_GF_FARM/menu/house.c").read_text())
    groups = re.sub(r"\bITEM_\w+\b",lambda m:str(names[m[0]]),body(original_house,"cooking_value_codes"))
    kitchen.append("static const uint16_t cooking_groups[4][4]={"+groups+"};")
    kitchen.append("static const unsigned cooking_option_items[12]={" + ",".join(str(names[n]) for n in
        ("ITEM_KNIFE","ITEM_FRYING_PAN","ITEM_POT","ITEM_MIXER","ITEM_WHISK","ITEM_ROLLING_PIN","ITEM_OVEN",
         "ITEM_SUGAR","ITEM_SALT","ITEM_VINEGAR","ITEM_SOY_SAUCE","ITEM_MISO")) + "};")
    kitchen.append(f"enum {{ COOKING_FIASCO={names['ITEM_FOOD_FIASCO']} }};")
    Path(output).with_name("cooking_tables.inc").write_text("\n".join(kitchen)+"\n")
    # Base seasonal menus preserve order, ids, descriptions and original names.
    # Conditional entries are metadata; runtime gates them by authoritative shipments.
    text_bank = clean((src / "main/text/text_bank.c").read_text())
    seed_names = text_bank[text_bank.index(".seeds = {"):]
    seed_menu = ["/* Generated from shop_seed_menu_setup_a/b/c and text_bank.names.seeds. */"]
    emitted = set()
    conditional = []
    for season, function, following in (("spring", "a", "b"), ("summer", "b", "c"), ("fall", "c", "d")):
        setup = shop[shop.index(f"static s32 shop_seed_menu_setup_{function}(void) {{"):
                     shop.index(f"static s32 shop_seed_menu_setup_{following}(void) {{")]
        entries = re.findall(r"ADD_ENTRY\(SHOP_NAME_AT\(base, text_bank.names.seeds.(\w+)\),\s*(0x[0-9A-Fa-f]+),\s*1,\s*(0x[0-9A-Fa-f]+)\)", setup)
        if len(entries) != 4:
            raise ValueError(f"Unexpected original base {season} seed menu")
        bonus = re.search(r"\*text = SHOP_NAME_AT\(base, text_bank.names.seeds.(\w+)\);", setup).group(1)
        item = re.search(r"\*entryValue = (0x[0-9A-Fa-f]+);", setup).group(1)
        description = re.search(r"\*label46 = (0x[0-9A-Fa-f]+);", setup).group(1)
        conditional.append((bonus, item, description))
        for label, item, description in entries:
            if label not in emitted:
                label_words = re.search(r"\." + label + r"\s*=\s*(\{[^}]+\})", seed_names).group(1)
                seed_menu.append(f"static const uint16_t seed_name_{label}[]={label_words};")
                emitted.add(label)
        seed_menu.append(f"static const HmbtnSeedShopEntry {season}_seeds[]={{")
        for label, item, description in entries:
            seed_menu.append(f"{{{item},{description},seed_name_{label},sizeof(seed_name_{label})/sizeof(uint16_t)}},")
        seed_menu.append("};")
    for label, item, description in conditional:
        label_words = re.search(r"\." + label + r"\s*=\s*(\{[^}]+\})", seed_names).group(1)
        seed_menu.append(f"static const uint16_t seed_name_{label}[]={label_words};")
        emitted.add(label)
    seed_menu.append("static const HmbtnSeedShopEntry conditional_seeds[]={")
    for label, item, description in conditional:
        seed_menu.append(f"{{{item},{description},seed_name_{label},sizeof(seed_name_{label})/sizeof(uint16_t)}},")
    seed_menu.append("};")
    setup = shop[shop.index("static s32 shop_seed_menu_setup_d(void) {"):shop.index("static s32 shop_wine_menu_setup(void)", shop.index("static s32 shop_seed_menu_setup_d(void) {"))]
    entries = re.findall(r"text_bank.names.seeds.(\w+)\).*?,\s*(0x[0-9A-Fa-f]+),\s*1,\s*(0x[0-9A-Fa-f]+)\)", setup)
    if len(entries) != 8 or [int(e[1],16) for e in entries[:3]] != [0x26,0x2b,0x30]:
        raise ValueError("Unexpected original Won seed menu")
    for label, item, description in entries:
        if label not in emitted:
            label_words = re.search(r"\." + label + r"\s*=\s*(\{[^}]+\})", seed_names).group(1)
            seed_menu.append(f"static const uint16_t seed_name_{label}[]={label_words};")
            emitted.add(label)
    seed_menu.append("static const HmbtnSeedShopEntry won_seeds[]={")
    for label, item, description in entries:
        seed_menu.append(f"{{{item},{description},seed_name_{label},sizeof(seed_name_{label})/sizeof(uint16_t)}},")
    seed_menu.append("};")
    Path(output).with_name("seed_menu.inc").write_text("\n".join(seed_menu) + "\n")
    keys = fields(body(shop, "shop_key_values"))
    if len(keys) % 2:
        raise ValueError("Unpaired original shop keys")
    Path(output).with_name("shop_keys.inc").write_text(
        "/* shop_dialogues.c:shop_key_values, estate record key -> dialogue. */\n"
        "static const unsigned shop_keys[][2]={" +
        ",".join("{%d,%d}" % (number(keys[i]), number(keys[i+1])) for i in range(0, len(keys), 2)) + "};\n")
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
    greenhouse_draw=field.split("void greenhouse_draw(void)",1)[1].split("static void field_tile_reset",1)[0]
    greenhouse_sprites=re.findall(r"case (ITEM_\w+):\s*if \(field_cell_look_anim\(kind, kind \+ 2, (\d+),",greenhouse_draw)
    if len(greenhouse_sprites)!=21:
        raise ValueError("Unexpected original greenhouse crop sprites")
    Path(output).with_name("estate_art_tables.inc").write_text(
        "/* Original farm item art packs. */\n"
        "static const uint32_t estate_art_offsets[11]={"+body(clean((src / "overlay/11_GF_FARM/farm_load.c").read_text()),"farm_item_file_offsets")+"};\n"
        "static const HmbtnEstateArtToc estate_art_tocs[10]={"+",".join(art)+"};\n"
        "static const unsigned greenhouse_crop_sprites[][2]={"+
        ",".join("{%d,%s}" % (names[item],sprite) for item,sprite in greenhouse_sprites)+"};\n")
    kitchen_positions=[]
    for level in (1,2):
        rows=fields(body(estate,f"estate_result_rows_house{level}"))[2:]
        if len(rows)!=8: raise ValueError("Unexpected original kitchen appliance layout")
        kitchen_positions.append("{"+",".join(fields(row[1:-1])[0] for row in rows)+"}")
    Path(output).with_name("kitchen_positions.inc").write_text(
        "/* estate.c:estate_result_rows_house1/2; utensil ANI indices 4..11. */\n"
        "static const int32_t kitchen_positions[2][8][3]={"+",".join(kitchen_positions)+"};\n")
    sections=clean((src / "overlay/common/farm_section_table.h").read_text())
    section_offsets=[int(v,0) for v in fields(body(sections,"section_table"))]
    if len(section_offsets)!=1021:
        raise ValueError("Unexpected scene section count")
    Path(output).with_name("scene_sections.inc").write_text(
        "/* Original seasonal section offsets. */\nstatic const uint32_t scene_sections[255][3]={\n"+
        "\n".join("{%d,%d,%d}," % tuple(section_offsets[i*4:i*4+3]) for i in range(255))+"\n};\n")
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
    fishing=clean((src / "overlay/11_GF_FARM/field/fishing.c").read_text())
    fish=["/* Original fishing.c numeric tables; no host pointers. */"]
    for key,value in names.items():
        if key.startswith("ITEM_"): fish.append(f"#define {key} {value}")
    for key,value in flag_values.items(): fish.append(f"#define {key} {value}")
    waits=fields(body(fishing,"fishing_bite_ranges"))
    if len(waits)!=28: raise ValueError("Unexpected fishing time ranges")
    fish.append("static const unsigned bite_ranges[4][7]={"+",".join("{"+",".join(waits[i:i+7])+"}" for i in range(0,28,7))+"};")
    for name in ["fishing_bite_amounts","fishing_spot_catches_1","fishing_spot_catches_2","fishing_spot_catches_3","fishing_spot_catches_4","fishing_fallback_catch"]:
        fish.append(f"static const uint16_t {name}[]={{"+",".join(str(number(x)) for x in fields(body(fishing,name)))+"};")
    season_offsets=re.findall(r"&fishing_bite_amounts\[(\d+)\]",body(fishing,"fishing_season_catches"))
    if len(season_offsets)!=4: raise ValueError("Unexpected seasonal fishing catches")
    fish.append("static const unsigned season_catches[4]={"+",".join(season_offsets)+"};")
    Path(output).with_name("fishing_tables.inc").write_text("\n".join(fish)+"\n")
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
    reactions=clean((src / "main/game/picture_codes.c").read_text())
    reaction_rows=fields(body(reactions,"picture_codes"))
    if len(reaction_rows)!=174 or any(len(fields(row[1:-1]))!=4 for row in reaction_rows):
        raise ValueError("Unexpected original picture reaction table")
    Path(output).with_name("item_reactions.inc").write_text(
        "/* Original picture_codes.c: preserve C octal literals, four 32-bit words per row. */\n"
        "static const uint32_t picture_codes[174][4]={"+",".join(reaction_rows)+"};\n")
    event=clean((src / "overlay/11_GF_FARM/event/event_script.c").read_text())
    commands=clean((src / "overlay/11_GF_FARM/event/event_commands.c").read_text())
    offsets=fields(body(event,"event_script_offsets"))
    Path(output).with_name("event_resources.inc").write_text(
        "/* Original event_script_offsets and section_table event-table ranges. */\n"
        "static const uint32_t event_offsets[]={"+",".join(offsets)+"};\n"
        "static const uint32_t event_tables[255][2]={"+",".join(
            "{%d,%d}" % (section_offsets[i*4+2],section_offsets[i*4+3]) for i in range(255))+"};\n")
    Path(output).with_name("bride_birthdays.inc").write_text(
        "/* Original event_commands.c: item_places. */\n"
        "static const int16_t bride_birthdays[2][5][3]={"+body(commands,"item_places")+"};\n")
    birthdays=re.search(r"birthday_moves\s*=\s*\{\s*(\{.*?\})\s*,\s*-1",commands,re.S).group(1)
    Path(output).with_name("village_talk_tables.inc").write_text(
        "/* Original conversation ranges and birthday tables. */\n"
        "static const uint32_t talk_ranges[37][2]={"+",".join(
            "{0,0}" if i in [0,27,30,33] else "{"+offsets[400 if i==35 else 350+i]+","+offsets[401 if i==35 else 351+i]+"}"
            for i in range(37))+"};\n"
        "static const int16_t birthdays[28][3]="+birthdays+";\n")



if __name__ == "__main__":
    main(*sys.argv[1:])
