#include "little8_api_lua.h"
#include "little8.h"

#include <lua.h>
#include <lauxlib.h>

#include <stdint.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------
// Хелперы
// ---------------------------------------------------------------------------
#define L_CHECK_U8(L, i)  ((uint8_t)luaL_checkinteger((L), (i)))

// ---------------------------------------------------------------------------
// Экран
// ---------------------------------------------------------------------------
static int l_cls(lua_State *L) {
    cls(L_CHECK_U8(L, 1));
    return 0;
}

static int l_pset(lua_State *L) {
    pset(luaL_checkinteger(L, 1),
         luaL_checkinteger(L, 2),
         L_CHECK_U8(L, 3));
    return 0;
}

static int l_pget(lua_State *L) {
    lua_pushinteger(L,
        pget(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2)));
    return 1;
}

// ---------------------------------------------------------------------------
// Рисование
// ---------------------------------------------------------------------------
static int l_print(lua_State *L) {
    // ВНИМАНИЕ: перекрывает базовый Lua print. См. примечание ниже.
    print(luaL_checkstring(L, 1),
          luaL_checkinteger(L, 2),
          luaL_checkinteger(L, 3),
          L_CHECK_U8(L, 4));
    return 0;
}

static int l_line(lua_State *L) {
    line(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2),
         luaL_checkinteger(L, 3), luaL_checkinteger(L, 4),
         L_CHECK_U8(L, 5));
    return 0;
}

static int l_rect(lua_State *L) {
    rect(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2),
         luaL_checkinteger(L, 3), luaL_checkinteger(L, 4),
         L_CHECK_U8(L, 5), lua_toboolean(L, 6));
    return 0;
}

static int l_oval(lua_State *L) {
    oval(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2),
         luaL_checkinteger(L, 3), luaL_checkinteger(L, 4),
         L_CHECK_U8(L, 5), lua_toboolean(L, 6));
    return 0;
}

// ---------------------------------------------------------------------------
// Ввод
// ---------------------------------------------------------------------------
static int l_btn(lua_State *L) {
    lua_pushboolean(L,
        btn(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2)));
    return 1;
}

static int l_btnp(lua_State *L) {
    lua_pushboolean(L,
        btnp(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2)));
    return 1;
}

// ---------------------------------------------------------------------------
// Спрайты
// ---------------------------------------------------------------------------
static int l_spr(lua_State *L) {
    spr(luaL_checkinteger(L, 1),                       // id
        luaL_checkinteger(L, 2),                       // x
        luaL_checkinteger(L, 3),                       // y
        lua_toboolean(L, 4),                           // flip_x  (default false)
        lua_toboolean(L, 5),                           // flip_y  (default false)
        (int)luaL_optinteger(L, 6, -1));               // colorkey (default -1 = solid)
    return 0;
}

static int l_sprdata(lua_State *L) {
    int id = luaL_checkinteger(L, 1);
    if (id < 0 || id > 255)
        return luaL_error(L, "sprdata: id must be 0..255 (got %d)", id);

    luaL_checktype(L, 2, LUA_TSTRING);          // только строка
    int w = luaL_checkinteger(L, 3);
    int h = luaL_checkinteger(L, 4);
    if (w <= 0 || h <= 0 || w > 128 || h > 128)
        return luaL_error(L, "sprdata: bad size %dx%d (1..128)", w, h);

    size_t n = (size_t)w * (size_t)h;

    size_t len;
    const char *s = lua_tolstring(L, 2, &len);
    const char *p = s;
    const char *end = s + len;

    uint8_t *buf = (uint8_t *)malloc(n);
    if (!buf) return luaL_error(L, "sprdata: out of memory");

    size_t count = 0;
    while (count < n) {
        // пропускаем разделители: запятые, пробелы, \n, всё нецифровое
        while (p < end && (*p < '0' || *p > '9')) p++;
        if (p >= end) break;

        int v = 0;
        while (p < end && *p >= '0' && *p <= '9') {
            v = v * 10 + (*p - '0');
            if (v > 15) {
                free(buf);
                return luaL_error(L,
                    "sprdata: value %d at #%d out of range 0..15",
                    v, (int)(count + 1));
            }
            p++;
        }
        buf[count++] = (uint8_t)v;
    }

    if (count < n) {
        int got = (int)count, need = (int)n;
        free(buf);
        return luaL_error(L,
            "sprdata: too few values (got %d, need %d for %dx%d)",
            got, need, w, h);
    }

    sprdata(id, buf, w, h);
    free(buf);
    return 0;
}

// ---------------------------------------------------------------------------
// Звук
// ---------------------------------------------------------------------------
static int l_tone(lua_State *L) {
    tone(luaL_checkinteger(L, 1),   // frequency
         luaL_checkinteger(L, 2),   // duration
         luaL_checkinteger(L, 3),   // volume
         luaL_checkinteger(L, 4));  // channel
    return 0;
}

// ---------------------------------------------------------------------------
// Сохранения
// ---------------------------------------------------------------------------
static int l_dset(lua_State *L) {
    int slot = luaL_checkinteger(L, 1);
    int64_t value = (int64_t)luaL_checkinteger(L, 2);
    dset(slot, value);
    return 0;
}

static int l_dget(lua_State *L) {
    int slot = luaL_checkinteger(L, 1);
    lua_pushinteger(L, (lua_Integer)dget(slot));
    return 1;
}

// ---------------------------------------------------------------------------
// Время
// ---------------------------------------------------------------------------
static int l_time(lua_State *L) {
    lua_pushinteger(L, (lua_Integer)l8_time());
    return 1;
}

static int l_utime(lua_State *L) {
    lua_pushinteger(L, (lua_Integer)l8_utime());
    return 1;
}

// ---------------------------------------------------------------------------
// Регистрация
// ---------------------------------------------------------------------------
static const luaL_Reg l8_funcs[] = {
    // screen
    {"cls",     l_cls},
    {"pset",    l_pset},
    {"pget",    l_pget},
    // drawing
    {"print",   l_print},
    {"line",    l_line},
    {"rect",    l_rect},
    {"oval",    l_oval},
    // input
    {"btn",     l_btn},
    {"btnp",    l_btnp},
    // sprites
    {"spr",     l_spr},
    {"sprdata", l_sprdata},
    // audio
    {"tone",    l_tone},
    // save
    {"dset",    l_dset},
    {"dget",    l_dget},
    // time
    {"time",  l_time},
    {"utime", l_utime},
    {NULL, NULL}
};

void l8_register_api(lua_State *L) {
    for (const luaL_Reg *r = l8_funcs; r->name; r++) {
        lua_pushcfunction(L, r->func);
        lua_setglobal(L, r->name);
    }
}