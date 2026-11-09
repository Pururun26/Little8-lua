cmake_minimum_required(VERSION 3.16)
project(Little8_lua C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Опции
# ---------------------------------------------------------------------------
option(L8_RAYLIB_DLL "Link against raylib.dll (import lib) instead of static lib" OFF)

# ---------------------------------------------------------------------------
# Раскладка проекта
# ---------------------------------------------------------------------------
set(L8_ROOT   ${CMAKE_CURRENT_SOURCE_DIR})
set(L8_SRC    ${L8_ROOT}/src)
set(L8_LUA    ${L8_ROOT}/lua-5.4.9)
set(L8_RAYLIB ${L8_ROOT}/raylib/raylib-6.0_win64_mingw-w64)

set(LUA_DIR ${L8_LUA})

if(NOT EXISTS ${LUA_DIR}/lapi.c)
    message(FATAL_ERROR "Lua sources not found in ${LUA_DIR}")
endif()

# ---------------------------------------------------------------------------
# Lua 5.4 — статическая либа, только нужное.
# Без lua.c/luac.c (standalone), без linit.c (тянет luaopen_* отовсюду),
# без loslib/lutf8lib/liolib/ldblib/loadlib.
# ---------------------------------------------------------------------------
set(LUA_SOURCES
    # core
    ${LUA_DIR}/lapi.c
    ${LUA_DIR}/lcode.c
    ${LUA_DIR}/lctype.c
    ${LUA_DIR}/ldebug.c
    ${LUA_DIR}/ldo.c
    ${LUA_DIR}/ldump.c
    ${LUA_DIR}/lfunc.c
    ${LUA_DIR}/lgc.c
    ${LUA_DIR}/llex.c
    ${LUA_DIR}/lmem.c
    ${LUA_DIR}/lobject.c
    ${LUA_DIR}/lopcodes.c
    ${LUA_DIR}/lparser.c
    ${LUA_DIR}/lstate.c
    ${LUA_DIR}/lstring.c
    ${LUA_DIR}/ltable.c
    ${LUA_DIR}/ltm.c
    ${LUA_DIR}/lundump.c
    ${LUA_DIR}/lvm.c
    ${LUA_DIR}/lzio.c
    # aux
    ${LUA_DIR}/lauxlib.c
    # stdlibs we actually open
    ${LUA_DIR}/lbaselib.c
    ${LUA_DIR}/lcorolib.c
    ${LUA_DIR}/lmathlib.c
    ${LUA_DIR}/lstrlib.c
    ${LUA_DIR}/ltablib.c
)

add_library(lua54 STATIC ${LUA_SOURCES})
target_include_directories(lua54 PUBLIC ${LUA_DIR})

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lua54 PRIVATE -w)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(lua54 PUBLIC m)
endif()

# ---------------------------------------------------------------------------
# Raylib (prebuilt)
# ---------------------------------------------------------------------------
set(RAYLIB_INCLUDE_DIR ${L8_RAYLIB}/include)

if(L8_RAYLIB_DLL)
    set(RAYLIB_LIBRARY ${L8_RAYLIB}/lib/libraylibdll.a)
else()
    set(RAYLIB_LIBRARY ${L8_RAYLIB}/lib/libraylib.a)
endif()

foreach(_f ${RAYLIB_INCLUDE_DIR}/raylib.h ${RAYLIB_LIBRARY})
    if(NOT EXISTS ${_f})
        message(FATAL_ERROR "raylib file not found: ${_f}")
    endif()
endforeach()

message(STATUS "raylib mode:  ${L8_RAYLIB_DLL}")
message(STATUS "raylib lib:   ${RAYLIB_LIBRARY}")

# ---------------------------------------------------------------------------
# Host
# ---------------------------------------------------------------------------
set(L8_HOST_SOURCES
    ${L8_SRC}/main.c
    ${L8_SRC}/little8_api.c
    ${L8_SRC}/little8_audio.c
    ${L8_SRC}/little8_save.c
)

file(SIZE ${L8_SRC}/little8_api_lua.c _lua_c_size)
if(_lua_c_size GREATER 0)
    list(APPEND L8_HOST_SOURCES ${L8_SRC}/little8_api_lua.c)
endif()

add_executable(little8-lua ${L8_HOST_SOURCES})

target_include_directories(little8-lua PRIVATE
    ${L8_SRC}
    ${RAYLIB_INCLUDE_DIR}
)

# Порядок важен: raylib → lua54 → системные. Для GNU ld и статических
# архивов библиотека-поставщик символов должна идти ПОСЛЕ потребителя.
target_link_libraries(little8-lua PRIVATE
    ${RAYLIB_LIBRARY}
    lua54
)

if(WIN32)
    # opengl32 — GL-контекст raylib
    # gdi32    — GetDC/ChoosePixelFormat (обычно уже в дефолтах, но на всякий)
    # winmm    — timeBeginPeriod/timeEndPeriod (rcore.c)
    target_link_libraries(little8-lua PRIVATE
        opengl32
        gdi32
        winmm
    )
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(little8-lua PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# ---------------------------------------------------------------------------
# Копировать raylib.dll рядом с exe, если линкуемся через import lib
# ---------------------------------------------------------------------------
if(WIN32 AND L8_RAYLIB_DLL)
    set(RAYLIB_DLL ${L8_RAYLIB}/lib/raylib.dll)
    if(EXISTS ${RAYLIB_DLL})
        add_custom_command(TARGET little8-lua POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${RAYLIB_DLL}
                $<TARGET_FILE_DIR:little8-lua>
            COMMENT "Copying raylib.dll next to little8-lua.exe"
        )
    else()
        message(WARNING "L8_RAYLIB_DLL=ON, but ${RAYLIB_DLL} not found")
    endif()
endif()