# Prefer a system installation. Downloads, when enabled, stay in the build tree.
option(HMBTN_FETCH_SDL3 "Download pinned SDL3 if no development package is installed" ON)
find_package(SDL3 3.2 CONFIG QUIET)
if(NOT SDL3_FOUND)
    if(NOT HMBTN_FETCH_SDL3)
        message(FATAL_ERROR "SDL3 development files missing. Set SDL3_DIR or enable HMBTN_FETCH_SDL3.")
    endif()
    include(FetchContent)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    # Bundle a consistent static CRT with bundled SDL on MSVC. System packages
    # keep their normal CRT defaults; callers may set the variable explicitly.
    if(MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    message(STATUS "Using pinned SDL3 3.4.18 (download unless FETCHCONTENT_SOURCE_DIR_SDL3 is set)")
    FetchContent_Declare(SDL3
        URL https://www.libsdl.org/release/SDL3-3.4.18.tar.gz
        URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3)
    FetchContent_MakeAvailable(SDL3)
endif()
