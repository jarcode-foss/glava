#ifdef GLAVA_UI

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <string.h>

#include "ui.h"
#include "bindings.h"

#ifdef GLAVA_LUAJIT
#include <luajit-2.0/lua.h>
#include <luajit-2.0/lauxlib.h>
#include <luajit-2.0/lualib.h>
#define LUA_OK 0
#define lua_len(...) lua_objlen(__VA_ARGS__)
#endif

#ifdef GLAVA_LUA
#include <lua.h>
#include <luaxlib.h>
#include <lualib.h>
#endif

#define FATAL_PREFIX "[!!]"

struct bindings {
    lua_State* L;
};

#define lua_setglobal_f(L, str, f)              \
    do {                                        \
        lua_pushcfunction(L, f);                \
        lua_setglobal(L, str);                  \
    } while (0)

#define lua_rawset_f(L, str, f)                 \
    do {                                        \
        lua_pushstring(L, str);                 \
        lua_pushcfunction(L, f);                \
        lua_rawset(L, -3);                      \
    } while (0)

static int lua_utf8_next(lua_State* L) {
    char c = (char) luaL_checkinteger(L, 1); /* char */
    lua_pushinteger(L, (int) utf8_next(&c));
    return 1;
}

struct bindings* bd_init(struct renderer* r, const char* root, const char* entry) {
    
    ui_set_font("/usr/share/fonts/TTF/DejaVuSansMono.ttf", 14);
    ui_init(r);
    
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    lua_setglobal_f(L, "utf8_next", lua_utf8_next);
    lua_pushstring(L, root);
    lua_setglobal(L, "root_path");
    switch (luaL_loadfile(L, entry)) {
    case LUA_ERRSYNTAX:
        fprintf(stderr, FATAL_PREFIX
                "Syntax error while compiling Lua entry point:\n%s\n", lua_tostring(L, -1));
        exit(EXIT_FAILURE);
    case LUA_ERRMEM:
        fprintf(stderr, FATAL_PREFIX
                "Memory error while compiling Lua entry point:\n%s\n", lua_tostring(L, -1));
        exit(EXIT_FAILURE);
    case LUA_ERRFILE:
        fprintf(stderr, FATAL_PREFIX
                "Error while reading Lua entry point:\n%s\n", lua_tostring(L, -1));
        exit(EXIT_FAILURE);
    }
    switch (lua_pcall(L, 0, 0, 0)) {
    case LUA_ERRRUN:
        fprintf(stderr, FATAL_PREFIX
                "Uncaught error while running Lua entry point:\n%s\n", lua_tostring(L, -1));
        exit(EXIT_FAILURE);
    case LUA_ERRMEM:
        fprintf(stderr, FATAL_PREFIX
                "Memory error while running Lua entry point:\n%s\n", lua_tostring(L, -1));
        exit(EXIT_FAILURE);
    case LUA_ERRERR:
        fprintf(stderr, FATAL_PREFIX
                "Error while running Lua error handler:\n%s\n", lua_tostring(L, -1));
        exit(EXIT_FAILURE);
    }
    lua_getglobal(L, "draw");
    if (!lua_isfunction(L, -1)) {
        fputs(FATAL_PREFIX "`draw` is not a Lua function! Did you forget to set it?\n", stderr);
        exit(EXIT_FAILURE);
    }
    lua_pop(L, 1);
    struct bindings* ret = malloc(sizeof(struct bindings));
    ret->L = L;
    return ret;
}

void bd_frame(struct bindings* ret) {
    lua_State* L = ret->L;
    lua_getglobal(L, "draw");
    if (!lua_isfunction(L, -1)) {
        fputs(FATAL_PREFIX "`draw` is not a Lua function! Did you forget to set it?\n", stderr);
        exit(EXIT_FAILURE);
    }
    switch (lua_pcall(L, 0, 0, 0)) {
    case LUA_ERRRUN:
        fprintf(stderr, FATAL_PREFIX
                "Uncaught error while running Lua draw function:\n%s\n", lua_tostring(L, -1));
        exit(EXIT_FAILURE);
    case LUA_ERRMEM:
        fprintf(stderr, FATAL_PREFIX
                "Memory error while running Lua draw function:\n%s\n", lua_tostring(L, -1));
        exit(EXIT_FAILURE);
    case LUA_ERRERR:
        fprintf(stderr, FATAL_PREFIX
                "Error while running Lua error handler:\n%s\n", lua_tostring(L, -1));
        exit(EXIT_FAILURE);
    }
}

#endif /* GLAVA_UI */
