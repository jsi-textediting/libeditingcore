# libeditingcore: code shared by Lite XL and thither-server.
#
# A static library of Lua bindings with no SDL dependency: process execution,
# PCRE2 regex, directory monitoring and the window-less part of the "system"
# module. The host provides Lua and PCRE2 (targets lua::lua and pcre2-8) and,
# through ec_set_event_hooks(), a way to deliver the dirmonitor event.
#
# Options:
#   EC_FETCH_LUA        fetch Lua 5.5 when no lua::lua target exists   (default OFF)
#   EC_FETCH_PCRE2      fetch PCRE2 when no pcre2-8 target exists      (default OFF)
#   EC_DIRMONITOR_BACKEND  inotify | fsevents | kqueue | win32 | inodewatcher | dummy
#                          (auto-detected when empty)
#   EC_BUILD_TESTS      build tests/ (needs Lua and PCRE2)             (default: top-level only)
cmake_minimum_required(VERSION 3.21)
project(editingcore VERSION 0.1.0 LANGUAGES C)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_ec_top ON)
else()
    set(_ec_top OFF)
endif()

option(EC_FETCH_LUA   "Fetch Lua 5.5 when no lua::lua target exists" OFF)
option(EC_FETCH_PCRE2 "Fetch PCRE2 when no pcre2-8 target exists" OFF)
option(EC_BUILD_TESTS "Build the libeditingcore tests" ${_ec_top})
set(EC_DIRMONITOR_BACKEND "" CACHE STRING
    "dirmonitor backend: inotify | fsevents | kqueue | win32 | inodewatcher | dummy (auto when empty)")

# ── dependencies ────────────────────────────────────────────────────────────
include(FetchContent)

if(NOT TARGET lua::lua)
    if(EC_FETCH_LUA)
        FetchContent_Declare(ec_lua
            URL      https://www.lua.org/ftp/lua-5.5.1.tar.gz
            URL_HASH SHA256=1c4b4068d67061f2a2231ad2b5422e77acea1487ea9890f6320af614f4373dce
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        FetchContent_MakeAvailable(ec_lua)
        file(GLOB _ec_lua_sources "${ec_lua_SOURCE_DIR}/src/*.c")
        list(FILTER _ec_lua_sources EXCLUDE REGEX "/(lua|luac)\\.c$")
        add_library(ec_lua_static STATIC ${_ec_lua_sources})
        target_include_directories(ec_lua_static PUBLIC "${ec_lua_SOURCE_DIR}/src")
        if(UNIX)
            target_compile_definitions(ec_lua_static PRIVATE LUA_USE_POSIX)
            target_link_libraries(ec_lua_static PUBLIC m ${CMAKE_DL_LIBS})
        endif()
        add_library(lua::lua ALIAS ec_lua_static)
    else()
        message(FATAL_ERROR
            "libeditingcore needs a Lua target named lua::lua. Define it in the "
            "host project, or configure with -DEC_FETCH_LUA=ON.")
    endif()
endif()

if(NOT TARGET pcre2-8)
    if(EC_FETCH_PCRE2)
        set(PCRE2_BUILD_PCRE2_8    ON  CACHE BOOL "" FORCE)
        set(PCRE2_BUILD_PCRE2_16   OFF CACHE BOOL "" FORCE)
        set(PCRE2_BUILD_PCRE2_32   OFF CACHE BOOL "" FORCE)
        set(PCRE2_BUILD_PCRE2GREP  OFF CACHE BOOL "" FORCE)
        set(PCRE2_BUILD_TESTS      OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(ec_pcre2
            URL      https://github.com/PCRE2Project/pcre2/releases/download/pcre2-10.44/pcre2-10.44.tar.bz2
            URL_HASH SHA256=d34f02e113cf7193a1ebf2770d3ac527088d485d4e047ed10e5d217c6ef5de96
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            EXCLUDE_FROM_ALL)
        FetchContent_MakeAvailable(ec_pcre2)
    else()
        message(FATAL_ERROR
            "libeditingcore needs a PCRE2 target named pcre2-8. Define it in the "
            "host project, or configure with -DEC_FETCH_PCRE2=ON.")
    endif()
endif()

# ── dirmonitor backend ──────────────────────────────────────────────────────
set(_ec_backend "${EC_DIRMONITOR_BACKEND}")
if(_ec_backend STREQUAL "")
    if(WIN32)
        set(_ec_backend "win32")
    elseif(APPLE)
        include(CheckIncludeFile)
        check_include_file("CoreServices/CoreServices.h" _ec_have_coreservices)
        set(_ec_backend "dummy")
        if(_ec_have_coreservices)
            set(_ec_backend "fsevents")
        endif()
    else()
        include(CheckIncludeFile)
        include(CheckFunctionExists)
        check_include_file("sys/inotify.h" _ec_have_inotify_h)
        if(_ec_have_inotify_h)
            set(_ec_backend "inotify")
        else()
            check_function_exists(kqueue _ec_have_kqueue)
            if(_ec_have_kqueue)
                set(_ec_backend "kqueue")
            else()
                set(_ec_backend "dummy")
                message(WARNING "No suitable dirmonitor backend found; using the dummy backend")
            endif()
        endif()
    endif()
    message(STATUS "editingcore dirmonitor backend: ${_ec_backend} (auto-detected)")
endif()
if(_ec_backend STREQUAL "inodewatcher")
    enable_language(CXX)
    set(_ec_backend_src src/dirmonitor/inodewatcher.cpp)
else()
    set(_ec_backend_src src/dirmonitor/${_ec_backend}.c)
endif()

# ── library ─────────────────────────────────────────────────────────────────
add_library(editingcore STATIC
    src/arena_allocator.c
    src/dirmonitor.c
    src/events.c
    src/platform_posix.c
    src/platform_win32.c
    src/process.c
    src/regex.c
    src/system_core.c
    ${_ec_backend_src}
)
add_library(editingcore::editingcore ALIAS editingcore)
set_target_properties(editingcore PROPERTIES C_STANDARD 11 C_EXTENSIONS ON
    POSITION_INDEPENDENT_CODE ON)
target_include_directories(editingcore
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(editingcore PRIVATE PCRE2_STATIC)
target_link_libraries(editingcore PUBLIC lua::lua PRIVATE pcre2-8)
find_package(Threads REQUIRED)
target_link_libraries(editingcore PUBLIC Threads::Threads)
target_compile_options(editingcore PRIVATE -ffunction-sections -fdata-sections)
if(APPLE)
    target_link_libraries(editingcore PUBLIC "-framework CoreServices" "-framework Foundation")
endif()
if(NOT WIN32)
    target_link_libraries(editingcore PUBLIC ${CMAKE_DL_LIBS})
endif()

if(EC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
