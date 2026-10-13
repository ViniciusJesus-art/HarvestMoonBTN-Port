# SLUS_011.15: the EXE.
# The source list is in link order: the startup object links before the
# rest of the PSY-Q objects.

psx_exe(SLUS_011.15 BASE 0x80010000 STACK 0x801FFFF0 REGION_TEXT "North America" GP 8
    SOURCES
        main.c
        system.c
        graphics/screen_fade.c
        ui/window_frame.c
        input/pad.c
        cd/afile.c
        graphics/prim.c
        overlay_regions.c
        graphics/anim.c
        graphics/texture_pack.c
        text/text.c
        game/event_flags.c
        ui/loading_screen.c
        input/pad_vibration.c
        geometry/model.c
        graphics/frame_buffers.c
        geometry/vec_length.c
        geometry/ground.c
        geometry/camera.c
        geometry/math_util.c
        graphics/clut.c
        sound/sound_request.c
        sound/sound.c
        sound/cd_audio.c
        game/farm_data_size.c
        debug_stubs.c
        game/farm_state.c
        game/farm_tables.c
        game/cooking_recipes.c
        text/text_bank.c
        game/picture_codes.c
        game/horse_names.c
        game/shop_restock.c
        ui/bg_tile.c
        ui/menu_window.c
        ui/heart.c
        ui/name_entry.c
        ui/ui_icons.c
    SMALL_COMMONS
        main.c
        graphics/screen_fade.c
        input/pad.c
        cd/afile.c
        graphics/anim.c
        text/text.c
        input/pad_vibration.c
        geometry/model.c
        geometry/ground.c
        sound/sound_request.c
        sound/sound.c
        sound/cd_audio.c
        ui/menu_window.c
        ui/ui_icons.c
    SDK_OBJECTS ${CMAKE_SOURCE_DIR}/sdk/objects.txt
    SDK_PATCHES ${CMAKE_SOURCE_DIR}/sdk/patches.txt)
