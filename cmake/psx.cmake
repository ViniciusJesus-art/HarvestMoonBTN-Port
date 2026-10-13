# Build rules for the game's images.
#
# Each image declares itself in its own CMakeLists.txt: the EXE with psx_exe, an
# overlay with psx_overlay, its sources in link order. An object is built from its
# source by tools/psx_cc.py with every flag spelled out here. psx_program links the EXE
# and the overlays in one GNU ld run, from a linker script it generates into the build
# directory (<build>/program.ld, from cmake/program.ld.in, exe.ld.in and overlay.ld.in):
# the objects go to ld in link order and the script places them by wildcard. Objects
# go to the build tree at the source's path (src/main/main.c -> <build>/src/main/main.o),
# and the link runs in the build directory, so ld sees them by that relative path.

# _psx_object(<var> <file.c>)
#
# Set <var> to the object of a C source of the current source directory, relative to
# the build directory.
function(_psx_object out_var source)
    cmake_path(ABSOLUTE_PATH source BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
               NORMALIZE OUTPUT_VARIABLE absolute)
    cmake_path(RELATIVE_PATH absolute BASE_DIRECTORY ${CMAKE_SOURCE_DIR}
               OUTPUT_VARIABLE relative)
    string(REGEX REPLACE "\\.c$" ".o" object ${relative})
    set(${out_var} ${object} PARENT_SCOPE)
endfunction()

# psx_c_objects(<var> GP <bytes> [OWN_COMMONS] SOURCES <file.c>... [SMALL_COMMONS <file.c>...])
#
# Compile C sources (relative to the current source directory) and set <var> to
# their objects. GP is cc1's and maspsx's -G: the largest object reached through $gp
# (8 for the EXE, 0 for an overlay). OWN_COMMONS: the image was linked by PSYLINK,
# so each file allocates the commons it defines itself, at the tail of its .bss, and
# ends its .bss on a doubleword (tools/psx_bss.py); SMALL_COMMONS names the files that
# also own small (.sbss) ones.
function(psx_c_objects out_var)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "OWN_COMMONS" "GP" "SOURCES;SMALL_COMMONS")
    set(objects)
    foreach(source IN LISTS ARG_SOURCES)
        cmake_path(ABSOLUTE_PATH source BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   NORMALIZE OUTPUT_VARIABLE absolute)
        cmake_path(RELATIVE_PATH absolute BASE_DIRECTORY ${CMAKE_SOURCE_DIR}
                   OUTPUT_VARIABLE relative)
        _psx_object(object ${source})
        set(object ${CMAKE_BINARY_DIR}/${object})
        set(bss_flags)
        if(ARG_OWN_COMMONS)
            list(APPEND bss_flags --own-commons)
            if(source IN_LIST ARG_SMALL_COMMONS)
                list(APPEND bss_flags --small-commons)
            endif()
        endif()
        add_custom_command(
            OUTPUT ${object}
            COMMAND ${Python3_EXECUTABLE} tools/psx_cc.py ${relative} -o ${object} -MF ${object}.d
                --cpp ${PSX_CPP} ${PSX_CPP_FLAGS}
                --cc1 ${PSX_CC1} ${PSX_CC1_FLAGS} -G${ARG_GP}
                --maspsx ${Python3_EXECUTABLE} tools/maspsx/maspsx.py ${PSX_MASPSX_FLAGS} -G${ARG_GP}
                --bss ${Python3_EXECUTABLE} tools/psx_bss.py ${bss_flags} ${relative}
                --as ${PSX_AS} ${PSX_AS_FLAGS}
                --objcopy ${PSX_OBJCOPY} --set-section-alignment .text=1
            DEPENDS ${absolute} ${PSX_COMPILE_TOOLS}
            DEPFILE ${object}.d
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "CC ${relative}"
            VERBATIM)
        list(APPEND objects ${object})
    endforeach()
    set(${out_var} ${objects} PARENT_SCOPE)
endfunction()

# psx_sdk_objects(<var> LIST <objects.txt> PATCHES <patches.txt>)
#
# Convert the PSY-Q library objects the EXE links into the
# current build directory's sdk/, and set <var> to them.
function(psx_sdk_objects out_var)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "LIST;PATCHES" "")
    file(STRINGS ${ARG_LIST} rows REGEX "^[^#]")
    set(objects)
    foreach(row IN LISTS rows)
        string(REGEX MATCH "^[^ \t]+" object ${row})
        list(APPEND objects ${CMAKE_CURRENT_BINARY_DIR}/sdk/${object})
    endforeach()
    add_custom_command(
        OUTPUT ${objects}
        COMMAND ${Python3_EXECUTABLE} tools/psyq_objects.py --sdk ${PSYQ_SDK_DIR}
            --objects ${ARG_LIST} --patches ${ARG_PATCHES} --out ${CMAKE_CURRENT_BINARY_DIR}/sdk
        DEPENDS ${ARG_LIST} ${ARG_PATCHES} ${CMAKE_SOURCE_DIR}/tools/psyq_objects.py
            ${CMAKE_SOURCE_DIR}/tools/psyq_elf.py ${CMAKE_SOURCE_DIR}/tools/psyq_lnk.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Converting the PSY-Q objects the EXE links"
        VERBATIM)
    set(${out_var} ${objects} PARENT_SCOPE)
endfunction()

set(PSX_LD_TEMPLATES ${CMAKE_CURRENT_LIST_DIR})
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${PSX_LD_TEMPLATES}/exe.ld.in ${PSX_LD_TEMPLATES}/overlay.ld.in)

# psx_overlay_region(<name> <address>)
#
# Declare an overlay load region: the overlays of REGION <name> load at <address>,
# which the linker script names overlay_<name> (the EXE refers to it by that name).
function(psx_overlay_region name address)
    set_property(GLOBAL APPEND PROPERTY PSX_REGIONS ${name})
    set_property(GLOBAL PROPERTY PSX_REGION_${name} ${address})
endfunction()

# _psx_add_image(<name> <block> <obj>...)
#
# Add an image to the program link: its section block of the linker script and its
# objects, in link order (ld takes them in the order given and the script's wildcards
# keep it). Its directory gets a target, <name>_objects, that builds them: a custom
# command's output has a rule only where a target of its own directory needs it.
function(_psx_add_image name block)
    add_custom_target(${name}_objects DEPENDS ${ARGN})
    set_property(GLOBAL APPEND PROPERTY PSX_PROGRAM_OBJECTS ${ARGN})
    set_property(GLOBAL APPEND PROPERTY PSX_PROGRAM_TARGETS ${name}_objects)
    set_property(GLOBAL APPEND_STRING PROPERTY PSX_SECTIONS "${block}")
endfunction()

# _psx_list_file(<var>)
#
# Set <var> to the current CMakeLists.txt, relative to the source root.
function(_psx_list_file out_var)
    cmake_path(RELATIVE_PATH CMAKE_CURRENT_LIST_FILE BASE_DIRECTORY ${CMAKE_SOURCE_DIR}
               OUTPUT_VARIABLE list_file)
    set(${out_var} ${list_file} PARENT_SCOPE)
endfunction()

# psx_exe(<name> BASE <address> STACK <sp> REGION_TEXT <text> GP <bytes>
#         SOURCES <file.c>... [SMALL_COMMONS <file.c>...]
#         SDK_OBJECTS <objects.txt> SDK_PATCHES <patches.txt>)
#
# Declare the EXE: compile its sources (linked by PSYLINK, so OWN_COMMONS, see
# psx_c_objects), convert the PSY-Q objects it links (psx_sdk_objects), and add it to
# the program link at BASE. Link order: the startup object (the SDK list's first), the
# sources, then the rest of the SDK list. psx_program cuts <build>/<name> out of the
# link: the payload with the PS-X EXE header CPE2X wrote in front of it, given the
# initial stack pointer STACK and the header's region text REGION_TEXT.
function(psx_exe name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG ""
        "BASE;STACK;REGION_TEXT;GP;SDK_OBJECTS;SDK_PATCHES" "SOURCES;SMALL_COMMONS")
    psx_c_objects(objects GP ${ARG_GP} OWN_COMMONS SOURCES ${ARG_SOURCES}
                  SMALL_COMMONS ${ARG_SMALL_COMMONS})
    psx_sdk_objects(sdk_objects LIST ${ARG_SDK_OBJECTS} PATCHES ${ARG_SDK_PATCHES})
    list(POP_FRONT sdk_objects startup)

    set(NAME ${name})
    set(BASE ${ARG_BASE})
    cmake_path(RELATIVE_PATH CMAKE_CURRENT_SOURCE_DIR BASE_DIRECTORY ${CMAKE_SOURCE_DIR}
               OUTPUT_VARIABLE DIR)
    _psx_list_file(LIST_FILE)
    file(READ ${PSX_LD_TEMPLATES}/exe.ld.in template)
    string(CONFIGURE "${template}" block @ONLY)
    _psx_add_image(${name} "${block}" ${startup} ${objects} ${sdk_objects})
    set_property(GLOBAL PROPERTY PSX_EXE ${name} ${ARG_STACK} ${ARG_REGION_TEXT})
endfunction()

# psx_overlay(<name> FILE <file> REGION <region> [OWN_COMMONS]
#             SOURCES <file.c>... [GAP <file.c> <section> <byte>...]...)
#
# Declare an overlay: compile its sources (GP 0; OWN_COMMONS as in psx_c_objects) and
# add it to the program link, at its region's address (psx_overlay_region). The first
# source is its header, whose .data (the header word) comes first; then every .rodata,
# .text, .data and .bss in link order. Each GAP is a linker gap: bytes the original
# link left after <file.c>'s <section> that no object holds. ld
# cannot put a byte inside a wildcard, so a section with a gap names its inputs.
# psx_program cuts the overlay's section out of the link as <build>/<file>, the raw
# file it is in A_FILE.
function(psx_overlay name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "OWN_COMMONS" "FILE;REGION" "SOURCES;GAP")
    get_property(regions GLOBAL PROPERTY PSX_REGIONS)
    if(NOT ARG_REGION IN_LIST regions)
        message(FATAL_ERROR "psx_overlay(${name}): no region ${ARG_REGION} (psx_overlay_region)")
    endif()
    set(own_commons)
    if(ARG_OWN_COMMONS)
        set(own_commons OWN_COMMONS)
    endif()
    psx_c_objects(objects GP 0 ${own_commons} SOURCES ${ARG_SOURCES})

    # GAP <file.c> <section> <byte>...: the gap after gap_keys[i] ("<file.c> <section>")
    # is gap_bytes[i].
    set(gap_keys)
    set(gap_sections)
    set(gap_bytes)
    set(expect SOURCE)
    foreach(word IN LISTS ARG_GAP)
        if(word MATCHES "\\.c$")
            if(NOT word IN_LIST ARG_SOURCES)
                message(FATAL_ERROR "psx_overlay(${name}): GAP ${word} is not one of its sources")
            endif()
            set(source ${word})
            set(expect SECTION)
        elseif(expect STREQUAL "SECTION")
            list(APPEND gap_keys "${source} ${word}")
            list(APPEND gap_sections ${word})
            list(APPEND gap_bytes "")
            set(expect BYTES)
        elseif(expect STREQUAL "BYTES")
            list(POP_BACK gap_bytes bytes)
            string(STRIP "${bytes} ${word}" bytes)
            list(APPEND gap_bytes "${bytes}")
        else()
            message(FATAL_ERROR "psx_overlay(${name}): GAP expects <file.c> <section> <byte>...")
        endif()
    endforeach()

    # Each section's inputs: a wildcard, or with a gap, every object's by name.
    cmake_path(RELATIVE_PATH CMAKE_CURRENT_SOURCE_DIR BASE_DIRECTORY ${CMAKE_SOURCE_DIR}
               OUTPUT_VARIABLE dir)
    foreach(section IN ITEMS .rodata .text .data .bss)
        string(TOUPPER ${section} var)
        string(SUBSTRING ${var} 1 -1 var)
        if(NOT section IN_LIST gap_sections)
            set(${var} "    ${dir}/*(${section});")
            continue()
        endif()
        set(lines)
        foreach(source IN LISTS ARG_SOURCES)
            _psx_object(object ${source})
            string(APPEND lines "    ${object}(${section});\n")
            list(FIND gap_keys "${source} ${section}" gap)
            if(gap GREATER -1)
                list(GET gap_bytes ${gap} bytes)
                string(REGEX REPLACE "([^ ]+)" "BYTE(\\1);" bytes "${bytes}")
                cmake_path(GET object FILENAME file)
                string(APPEND lines "    ${bytes}  /* the linker-gap bytes after ${file}'s ${section} */\n")
            endif()
        endforeach()
        string(STRIP "${lines}" lines)
        set(${var} "    ${lines}")
    endforeach()
    if(ARG_OWN_COMMONS)
        set(BSS "    . = ALIGN(0x8);  /* the first .bss on a doubleword; each ends on one (tools/psx_bss.py) */\n${BSS}")
    endif()

    list(GET ARG_SOURCES 0 header)
    _psx_object(HEADER ${header})
    set(NAME ${name})
    set(REGION ${ARG_REGION})
    _psx_list_file(LIST_FILE)
    file(READ ${PSX_LD_TEMPLATES}/overlay.ld.in template)
    string(CONFIGURE "${template}" block @ONLY)
    _psx_add_image(${name} "${block}" ${objects})
    set_property(GLOBAL APPEND PROPERTY PSX_OVERLAYS ${name})
    set_property(GLOBAL PROPERTY PSX_OVERLAY_${name} ${ARG_REGION} ${ARG_FILE})
endfunction()

# psx_program()
#
# Generate the linker script, <build>/program.ld (cmake/program.ld.in: the regions,
# NOCROSSREFS for the overlays of each region, every image's block), link the EXE and
# the overlays in one run, as PSYLINK did, and cut each binary out of
# the link: the EXE's payload (objcopy -O binary of .main) with the PS-X EXE header in
# front of it as PSY-Q's CPE2X wrote it (tools/ps_x_exe.py), and each overlay's
# section as the raw file it is in A_FILE. The target is `program`.
function(psx_program)
    get_property(exe GLOBAL PROPERTY PSX_EXE)
    list(GET exe 0 PSX_EXE)
    list(GET exe 1 stack)
    list(GET exe 2 region_text)
    get_property(regions GLOBAL PROPERTY PSX_REGIONS)
    get_property(overlays GLOBAL PROPERTY PSX_OVERLAYS)
    get_property(PSX_SECTIONS GLOBAL PROPERTY PSX_SECTIONS)

    set(PSX_REGIONS)
    set(PSX_NOCROSSREFS)
    foreach(region IN LISTS regions)
        get_property(address GLOBAL PROPERTY PSX_REGION_${region})
        string(APPEND PSX_REGIONS "overlay_${region} = ${address};\n")
        set(members)
        foreach(overlay IN LISTS overlays)
            get_property(overlay_region GLOBAL PROPERTY PSX_OVERLAY_${overlay})
            list(GET overlay_region 0 overlay_region)
            if(overlay_region STREQUAL region)
                list(APPEND members .${overlay})
            endif()
        endforeach()
        list(LENGTH members count)
        if(count GREATER 1)
            list(JOIN members " " members)
            string(APPEND PSX_NOCROSSREFS "NOCROSSREFS(${members})\n")
        endif()
    endforeach()
    set(script ${CMAKE_BINARY_DIR}/program.ld)
    configure_file(${PSX_LD_TEMPLATES}/program.ld.in ${script} @ONLY)

    get_property(objects GLOBAL PROPERTY PSX_PROGRAM_OBJECTS)
    set(link_order)
    foreach(object IN LISTS objects)
        cmake_path(RELATIVE_PATH object BASE_DIRECTORY ${CMAKE_BINARY_DIR}
                   OUTPUT_VARIABLE relative)
        list(APPEND link_order ${relative})
    endforeach()
    set(exe_file ${CMAKE_BINARY_DIR}/${PSX_EXE})
    set(outputs ${exe_file})
    set(cut)
    foreach(overlay IN LISTS overlays)
        get_property(overlay_file GLOBAL PROPERTY PSX_OVERLAY_${overlay})
        list(GET overlay_file 1 overlay_file)
        list(APPEND outputs ${CMAKE_BINARY_DIR}/${overlay_file})
        list(APPEND cut COMMAND ${PSX_OBJCOPY} -O binary --only-section=.${overlay}
            program.elf ${overlay_file})
    endforeach()
    add_custom_command(
        OUTPUT ${outputs}
        COMMAND ${PSX_LD} ${PSX_LD_FLAGS} -T ${script} -Map program.map -o program.elf
            ${link_order}
        COMMAND ${PSX_OBJCOPY} -O binary --only-section=.main program.elf ${PSX_EXE}.payload
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/ps_x_exe.py program.elf
            ${PSX_EXE}.payload -o ${exe_file} --stack ${stack} --region ${region_text}
        ${cut}
        DEPENDS ${script} ${objects} ${CMAKE_SOURCE_DIR}/tools/ps_x_exe.py
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "LD ${PSX_EXE} and its overlays"
        VERBATIM)
    add_custom_target(program ALL DEPENDS ${outputs})
    get_property(targets GLOBAL PROPERTY PSX_PROGRAM_TARGETS)
    add_dependencies(program ${targets})
endfunction()
