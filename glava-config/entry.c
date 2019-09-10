
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define GLAVA_LUA_ENTRY "glava-config.main"
#define GLAVA_LUA_ENTRY_FUNC "entry"

#ifndef LUA_OK
#define LUA_OK 0
#endif

/* Should be already defined by Meson */
#ifndef GLAVA_RESOURCE_PATH
#define GLAVA_RESOURCE_PATH "../resources"
#endif
#ifndef SHADER_INSTALL_PATH
#ifndef GLAVA_STANDALONE
#define SHADER_INSTALL_PATH "/etc/xdg/glava"
#else
#define SHADER_INSTALL_PATH "../shaders/glava"
#endif
#endif

static int traceback(lua_State *L) {
    if (!lua_isstring(L, 1))
        return 1;
    lua_getglobal(L, "debug");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return 1;
    }
    lua_getfield(L, -1, "traceback");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return 1;
    }
    lua_pushvalue(L, 1);
    lua_pushinteger(L, 2);
    lua_call(L, 2, 1);
    return 1;
}

int main(int argc, char** argv) {

    puts("WARNING: `glava-config` is in an incomplete state. Do not use this tool outside of development purposes.");
    fflush(stdout);
    
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    lua_pushcfunction(L, traceback);

    #ifdef GLAVA_STANDALONE
    /* Local path environment for standalone execution */
    lua_getglobal(L, "package");
    lua_pushstring(L, "path");
    lua_gettable(L, -2);
    lua_pushstring(L, "./glava-env/?.lua;./glava-env/?/init.lua;");
    lua_insert(L, -2);
    lua_concat(L, 2);
    lua_pushstring(L, "path");
    lua_insert(L, -2);
    lua_settable(L, -3);
    lua_pop(L, 1);
    #endif

    /* GLava compilation settings */
    lua_newtable(L);
    lua_pushstring(L, "resource_path");
    lua_pushstring(L, GLAVA_RESOURCE_PATH);
    lua_rawset(L, -3);
    lua_pushstring(L, "system_shader_path");
    lua_pushstring(L, SHADER_INSTALL_PATH);
    lua_rawset(L, -3);
    lua_setglobal(L, "glava");
    
    lua_getglobal(L, "require");
    lua_pushstring(L, GLAVA_LUA_ENTRY);
    lua_call(L, 1, 1);
    lua_pushstring(L, GLAVA_LUA_ENTRY_FUNC);
    lua_gettable(L, -2);
    if (!lua_isfunction(L, -1)) {
        fprintf(stderr, "FATAL: no `" GLAVA_LUA_ENTRY_FUNC "` function in entry module\n");
        exit(EXIT_FAILURE);
    }
    for (int t = 0; t < argc; ++t)
        lua_pushstring(L, argv[t]);
    int result = EXIT_FAILURE;
    switch (lua_pcall(L, argc, 1, 1)) {
        case LUA_OK:
            if (lua_isnumber(L, -1))
                result = lua_tonumber(L, -1);
            break;
        case LUA_ERRRUN:
            fprintf(stderr, "FATAL: error in `" GLAVA_LUA_ENTRY
                    "." GLAVA_LUA_ENTRY_FUNC "`: %s\n", lua_tostring(L, -1));
            break;
        default:
            fprintf(stderr, "FATAL: unhandled error from lua_pcall\n");
            break;
    }
    lua_close(L);
    return result;
}
