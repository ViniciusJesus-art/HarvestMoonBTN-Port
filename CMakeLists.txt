# Harvest Moon: Back to Nature (SLUS_011.15), the EXE and its overlays.
cmake_minimum_required(VERSION 3.20)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/psx-toolchain.cmake)
endif()
project(hmbtn NONE)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
include(cmake/psx.cmake)

# The compiler and linker flags. The C files, in link order, are listed in src/*/CMakeLists.txt.
# Each header sits next to the source it declares and is included by its bare name, so
# every directory with headers is on the include path.
set(PSX_INCLUDE_DIRS
    src/include
    src/main src/main/cd src/main/game src/main/geometry src/main/graphics src/main/input
    src/main/sound src/main/text src/main/ui
    src/overlay/11_GF_FARM src/overlay/11_GF_FARM/animals src/overlay/11_GF_FARM/event
    src/overlay/11_GF_FARM/field src/overlay/11_GF_FARM/menu src/overlay/11_GF_FARM/people
    src/overlay/11_GF_FARM/world
    src/overlay/08_DEBUG src/overlay/10_PdaSamp src/overlay/15_saveload
    src/overlay/17_GF_MG1 src/overlay/19_GF_MG2 src/overlay/21_GF_MG3 src/overlay/23_GF_MG4
    src/overlay/25_GF_MG5 src/overlay/27_GF_TITLE src/overlay/28_GF_STAFF src/overlay/30_GF_swind
    src/overlay/31_gf_mcard
    src)
list(TRANSFORM PSX_INCLUDE_DIRS PREPEND -I OUTPUT_VARIABLE psx_include_flags)
set(PSX_CPP_FLAGS ${psx_include_flags} -I${PSYQ_INCLUDE_DIR} -D_LANGUAGE_C -D__CHAR_UNSIGNED__)
set(PSX_CC1_FLAGS -O2 -gcoff -quiet)
set(PSX_MASPSX_FLAGS --use-comm-section --aspsx-version=2.86)
set(PSX_AS_FLAGS -EL -Isrc/include -march=r3000 -mtune=r3000 -no-pad-sections -G0)
# Each image is one output section of code and data, so its segment is RWX (the PS1 has no
# memory protection).
set(PSX_LD_FLAGS -EL --no-check-sections -nostdlib --no-warn-rwx-segments)
set(PSX_COMPILE_TOOLS
    ${CMAKE_SOURCE_DIR}/tools/psx_cc.py
    ${CMAKE_SOURCE_DIR}/tools/psx_bss.py
    ${CMAKE_SOURCE_DIR}/tools/maspsx/maspsx.py)

# The overlays' load regions: an overlay names its region in its psx_overlay().
psx_overlay_region(low 0x800B7E20)   # where the EXE's .bss ends
psx_overlay_region(high 0x8012DDE8)  # where FARM's image ends: 15_saveload, 30_GF_swind

# Each image declares itself in its CMakeLists.txt (psx_exe, psx_overlay); then the EXE
# and the overlays link in one run, from a linker script generated from them.
add_subdirectory(src/main)
add_subdirectory(src/overlay/08_DEBUG)
add_subdirectory(src/overlay/10_PdaSamp)
add_subdirectory(src/overlay/11_GF_FARM)
add_subdirectory(src/overlay/15_saveload)
add_subdirectory(src/overlay/17_GF_MG1)
add_subdirectory(src/overlay/19_GF_MG2)
add_subdirectory(src/overlay/21_GF_MG3)
add_subdirectory(src/overlay/23_GF_MG4)
add_subdirectory(src/overlay/25_GF_MG5)
add_subdirectory(src/overlay/27_GF_TITLE)
add_subdirectory(src/overlay/28_GF_STAFF)
add_subdirectory(src/overlay/30_GF_swind)
add_subdirectory(src/overlay/31_gf_mcard)
psx_program()

# A_FILE and the disc, from the original disc's data (-DORIGINAL_DISC=).
include(cmake/disc.cmake)

# Every build ends by comparing every binary, and with ORIGINAL_DISC A_FILE and the
# disc, against the original's SHA-1, and fails on a mismatch (also runnable alone as
# `--target check`).
set(check_lists ${CMAKE_SOURCE_DIR}/disc/binaries.sha1)
set(check_targets program)
if(ORIGINAL_DISC)
    list(APPEND check_lists ${CMAKE_SOURCE_DIR}/disc/disc.sha1)
    list(APPEND check_targets afile disc)
endif()
add_custom_target(check ALL
    COMMAND ${CMAKE_COMMAND} "-DLISTS=${check_lists}"
        -DDIR=${CMAKE_BINARY_DIR} -P ${CMAKE_SOURCE_DIR}/cmake/check_sha1.cmake
    DEPENDS ${check_targets}
    VERBATIM)
