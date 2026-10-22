# Optional PS1 disc reconstruction from an installation made by game_data.py.
# The original ORIGINAL_DISC flow remains in cmake/disc.cmake.
if(ORIGINAL_DISC)
    message(FATAL_ERROR "Choose HMBTN_EXTRACTED_DATA_DIR or ORIGINAL_DISC, not both")
endif()
get_filename_component(game_data "${HMBTN_EXTRACTED_DATA_DIR}" ABSOLUTE)
set(importer "${CMAKE_SOURCE_DIR}/port/tools/game_data.py")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${importer}" verify "${game_data}"
    RESULT_VARIABLE data_result OUTPUT_VARIABLE data_output ERROR_VARIABLE data_error)
if(NOT data_result EQUAL 0)
    message(FATAL_ERROR "Invalid extracted data: ${data_output}${data_error}")
endif()
message(STATUS "${data_output}")

# Use every manifest file as a dependency: edits trigger integrity validation again.
file(READ "${game_data}/manifest.json" data_manifest)
string(JSON file_count LENGTH "${data_manifest}" files)
math(EXPR last_file "${file_count} - 1")
set(data_files "${game_data}/manifest.json")
foreach(index RANGE ${last_file})
    string(JSON name MEMBER "${data_manifest}" files ${index})
    list(APPEND data_files "${game_data}/${name}")
endforeach()
set(data_stamp "${CMAKE_BINARY_DIR}/extracted-data.stamp")
add_custom_command(OUTPUT "${data_stamp}"
    COMMAND "${Python3_EXECUTABLE}" "${importer}" verify "${game_data}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${data_stamp}"
    DEPENDS ${data_files} "${importer}" "${CMAKE_SOURCE_DIR}/disc/a_file.txt"
        "${CMAKE_SOURCE_DIR}/disc/disc.xml"
    VERBATIM)

set(members "${CMAKE_SOURCE_DIR}/disc/a_file.txt")
file(STRINGS "${members}" rows REGEX "^ *[0-9]+ +[^ ]+ +built ")
set(built)
foreach(row IN LISTS rows)
    string(REGEX MATCH "^ *[0-9]+ +([^ ]+)" _ "${row}")
    list(APPEND built "${CMAKE_BINARY_DIR}/${CMAKE_MATCH_1}")
endforeach()
set(afile_bin "${CMAKE_BINARY_DIR}/A_FILE.BIN")
set(afile_hdt "${CMAKE_BINARY_DIR}/A_FILE.HDT")
add_custom_command(OUTPUT "${afile_bin}" "${afile_hdt}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/afile.py" pack
        --members "${members}" --original "${game_data}/A_FILE"
        --built "${CMAKE_BINARY_DIR}" --bin "${afile_bin}" --hdt "${afile_hdt}"
    DEPENDS "${data_stamp}" "${members}" ${built} "${CMAKE_SOURCE_DIR}/tools/afile.py"
    VERBATIM)
add_custom_target(afile ALL DEPENDS "${afile_bin}" "${afile_hdt}")
add_dependencies(afile program)

set(layout "${CMAKE_SOURCE_DIR}/disc/disc.xml")
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/disc.bin" "${CMAKE_BINARY_DIR}/disc.cue"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/psx_disc.py" build
        "${layout}" --files "${game_data}/disc"
        --replace "SLUS_011.15=${CMAKE_BINARY_DIR}/SLUS_011.15"
        --replace "A_FILE.BIN=${afile_bin}" --replace "A_FILE.HDT=${afile_hdt}"
        -o "${CMAKE_BINARY_DIR}/disc.bin" --cue "${CMAKE_BINARY_DIR}/disc.cue"
    DEPENDS "${layout}" "${data_stamp}" "${CMAKE_BINARY_DIR}/SLUS_011.15"
        "${afile_bin}" "${afile_hdt}" "${CMAKE_SOURCE_DIR}/tools/psx_disc.py"
    VERBATIM)
add_custom_target(disc ALL DEPENDS "${CMAKE_BINARY_DIR}/disc.bin")
add_dependencies(disc afile program)
