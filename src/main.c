#include <stdio.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "little8.h"
#include "little8_api_lua.h"

static lua_State *g_L = NULL;

static void frame(void) {
    lua_getglobal(g_L, "running");
    if (!lua_isfunction(g_L, -1)) { lua_pop(g_L, 1); return; }
    if (lua_pcall(g_L, 0, 0, 0) != LUA_OK) {
        fprintf(stderr, "runtime: %s\n", lua_tostring(g_L, -1));
        lua_pop(g_L, 1);
    }
}

static const luaL_Reg l8_libs[] = {
    {LUA_GNAME,       luaopen_base},
    {LUA_COLIBNAME,   luaopen_coroutine},
    {LUA_TABLIBNAME,  luaopen_table},
    {LUA_STRLIBNAME,  luaopen_string},
    {LUA_MATHLIBNAME, luaopen_math},
    {NULL, NULL}
};

int main(int argc, char **argv) {
    const char *cart = (argc > 1) ? argv[1] : "cart.lua";

    g_L = luaL_newstate();
    for (const luaL_Reg *r = l8_libs; r->name; r++) {
        luaL_requiref(g_L, r->name, r->func, 1);
        lua_pop(g_L, 1);
    }

    lua_pushnil(g_L); lua_setglobal(g_L, "dofile");
    lua_pushnil(g_L); lua_setglobal(g_L, "loadfile");

    l8_register_api(g_L);

    if (luaL_dofile(g_L, cart) != LUA_OK) {
        fprintf(stderr, "load: %s\n", lua_tostring(g_L, -1));
        lua_close(g_L);
        return 1;
    }

    little8(frame);
    lua_close(g_L);
    return 0;
}