# 11_GF_FARM, an overlay (11_GF_FARM.bin in A_FILE). The source list is in link order.

psx_overlay(11_GF_FARM FILE 11_GF_FARM.bin REGION low OWN_COMMONS
    SOURCES
        header.c
        farm_main.c
        farm_load.c
        farm_probe.c
        world/weather.c
        world/clock.c
        farm_frame.c
        people/villager.c
        people/player.c
        demo.c
        animals/animals.c
        animals/animal_actions.c
        picture_cells.c
        field/field.c
        world/scene_view.c
        event/event_request.c
        world/scene.c
        world/clock_events.c
        animals/livestock.c
        animals/chicken.c
        event/event_script.c
        event/event_commands.c
        farm_data.c
        event/cue_history.c
        estate.c
        menu/menus.c
        menu/item_storage.c
        menu/calendar.c
        menu/storage_swap.c
        menu/status_pages.c
        menu/options_menu.c
        menu/house.c
        menu/shop_page.c
        farm_init.c
        world/doors.c
        event/event_rules.c
        event/cue.c
        event/messages.c
        animals/dog.c
        animals/horse.c
        field/pond.c
        people/harvest_sprites.c
        event/milestones.c
        people/family.c
        field/mine.c
        menu/shop_dialogues.c
        field/greenhouse.c
        menu/photo_album.c
        field/fishing.c
        field/field_item_requests.c
        farm_vars.c
    # Filler the linker leaves in the alignment gap after a file's .data,
    # which no object holds.
    GAP menu/house.c .data 0x33 0x32
    GAP farm_init.c .data 0x0d 0x0a
    GAP event/messages.c .data 0x09 0x2e
    GAP people/harvest_sprites.c .data 0x0d 0x0a
    GAP field/greenhouse.c .data 0x74 0x65
    GAP menu/photo_album.c .data 0x09 0x30
    GAP field/field_item_requests.c .data 0x09 0x30
    GAP farm_vars.c .data 0x74 0x65 0x09 0x30 0x0d 0x0a)
