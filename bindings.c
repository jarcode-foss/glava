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

#define FATAL_PREFIX "[!!] "

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

#define lua_checkuptr(L, idx, name)                                  \
    ({                                                              \
        void* _ret = luaL_checkudata(L, idx, "ui." name);           \
        luaL_argcheck(L, _ret != NULL, idx, "`" name "` expected"); \
        _ret;                                                       \
    })

static int lua_utf8_next(lua_State* L) {
    char c = (char) luaL_checkinteger(L, 1); /* char */
    lua_pushinteger(L, (int) utf8_next(&c));
    return 1;
}

static int lui_get_advance_for(lua_State* L) {
    uint32_t i = (uint32_t) luaL_checkinteger(L, 1);
    lua_pushinteger(L, (int) ui_get_advance_for(i));
    return 1;
}

static int lui_layer(lua_State* L) {
    uint32_t
        w = (uint32_t) luaL_checkinteger(L, 1),
        h = (uint32_t) luaL_checkinteger(L, 2);
    struct layer_data* d = lua_newuserdata(L, sizeof(struct layer_data));
    *d = (struct layer_data) {}; /* empty init */
    luaL_getmetatable(L, "ui.layer");
    lua_setmetatable(L, -2);
    ui_layer(d, w, h);
    return 1;
}

static void layer_perrcall(lua_State* L) {
    switch (lua_pcall(L, 0, 0, 0)) {
    case LUA_ERRRUN:
        fprintf(stderr, FATAL_PREFIX
                "Uncaught error while running Lua `layer` handler function:\n%s\n", lua_tostring(L, -1));
    case LUA_ERRMEM:
        fprintf(stderr, FATAL_PREFIX
                "Memory error while running Lua `layer` handler function:\n%s\n", lua_tostring(L, -1));
        exit(EXIT_FAILURE);
    case LUA_ERRERR:
        fprintf(stderr, FATAL_PREFIX
                "Error while running Lua error handler:\n%s\n", lua_tostring(L, -1));
    }
}

static int lui_layer_handlers(lua_State* L) {
    struct layer_data* d = lua_checkuptr(L, 1, "layer");
    luaL_checktype(L, 2, LUA_TFUNCTION);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    
    void render_cb(void* udata) {
        lua_State* L = udata;
        lua_pushvalue(L, 2);
        layer_perrcall(L);
    }
    
    void font_cb(void* udata) {
        lua_State* L = udata;
        lua_pushvalue(L, 3);
        layer_perrcall(L);
    }

    d->render_cb = render_cb;
    d->font_cb   = font_cb;

    lua_getglobal(L, "__layer_handlers");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);        /* pop nil or other bogus value */
        lua_newtable(L);      /* new handlers table */
        lua_pushvalue(L, -1); /* copy of handlers table */
        lua_setglobal(L, "__layer_handlers");
    }
    lua_pushvalue(L, 1);   /* udata key */
    lua_newtable(L);       /* table value for containing callbacks */
    lua_pushvalue(L, 2);   /* copy of render callback */
    lua_rawseti(L, -2, 1); /* set and pop */
    lua_pushvalue(L, 3);   /* copy of font callback */
    lua_rawseti(L, -2, 2); /* set and pop */
    lua_rawset(L, -3);     /* set callback table */
    
    return 0;
}
static int lui_layer_draw(lua_State* L) {
    struct layer_data* d = lua_checkuptr(L, 1, "layer");
    lua_getglobal(L, "__layer_handlers");
    if (lua_istable(L, -1)) {
        lua_pushvalue(L, 1);   /* udata key*/
        lua_rawget(L, -1);     /* index and pop key, return table */
        if (lua_istable(L, -1)) {
            lua_rawgeti(L, -1, 1); /* render callback */
            lua_rawgeti(L, -2, 2); /* font callback */
            lua_remove(L, 2);      /* remove tables from stack */
            lua_remove(L, 3);      /* ^ */
        }
    }
    /* stack layout should be (layer, render function, font function),
       unless the callback functions are NULL */
    ui_layer_draw(d);
    return 0;
}

static int lui_layer_position(lua_State* L) {
    struct layer_data* d = lua_checkuptr(L, 1, "layer");
    d->frame.geometry.x = (int32_t) luaL_checkinteger(L, 2);
    d->frame.geometry.y = (int32_t) luaL_checkinteger(L, 3);
    ui_box(&d->frame);
    return 0;
}

static int lui_layer_resize(lua_State* L) {
    ui_layer_resize(lua_checkuptr(L, 1, "layer"),
                    (uint32_t) luaL_checkinteger(L, 2),
                    (uint32_t) luaL_checkinteger(L, 3));
    return 0;
}

static int lui_layer_draw_contents(lua_State* L) {
    ui_layer_draw_contents(lua_checkuptr(L, 1, "layer"));
    return 0;
}
static int lui_layer_release(lua_State* L) {
    ui_layer_release(lua_checkuptr(L, 1, "layer"));
    return 0;
}

void bd_request(struct bindings* state, const char* request, const char** args) {
    lua_State* L = state->L;
    lua_getglobal(L, "request");
    if (!lua_isfunction(L, -1)) {
        fputs(FATAL_PREFIX "`request` is not a Lua function! Did you forget to set it?\n", stderr);
        exit(EXIT_FAILURE);
    }
    size_t nargs = 1;
    lua_pushstring(L, request);
    for (const char** arg = args; *arg != NULL; ++arg) {
        lua_pushstring(L, *arg);
        ++nargs;
    }
    switch (lua_pcall(L, nargs, 0, 0)) {
    case LUA_ERRRUN:
        fprintf(stderr, FATAL_PREFIX
                "Uncaught error while running Lua request function:\n%s\n", lua_tostring(L, -1));
        exit(EXIT_FAILURE);
    case LUA_ERRMEM:
        fprintf(stderr, FATAL_PREFIX
                "Memory error while running Lua request function:\n%s\n", lua_tostring(L, -1));
        exit(EXIT_FAILURE);
    case LUA_ERRERR:
        fprintf(stderr, FATAL_PREFIX
                "Error while running Lua error handler:\n%s\n", lua_tostring(L, -1));
        exit(EXIT_FAILURE);
    }
}

/* Closure that handles mt.__call(mt, ...) invocations and passes 
   the remaining arguments to the upvalue function */
static int lua_call_new(lua_State* L) {
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_replace(L, 1); /* replace mt argument with function */
    lua_call(L, lua_gettop(L), LUA_MULTRET);
    return lua_gettop(L);
}

/* Creates a new metatable, with its `__index` set to itself, and with its own
   metatable set to `mt`, which contains a `__call` function that executes the
   `constructor` argument. This allows the following syntax to work:
   
   local value = some_type(arg0, arg1, ...)
   
   where `some_type` is actually the initial metatable. */
static void lua_newtype(lua_State* L, const char* mtname, lua_CFunction constructor) {
    luaL_newmetatable(L, mtname); /* new @ */
    lua_pushstring(L, "__index");
    lua_pushvalue(L, -2);
    lua_rawset(L, -3); /* @.__index = @ */
    lua_pushstring(L, "mt");
    lua_newtable(L);   /* new mt */
    lua_pushstring(L, "__call");
    lua_pushcfunction(L, constructor); /* closure upvalue */
    lua_pushcclosure(L, lua_call_new, 1);
    lua_rawset(L, -3); /* mt.__call = lua_call_new */
    lua_rawset(L, -3); /* @.mt = mt */
    /* leave metatable on the stack */
}

struct bindings* bd_init(struct renderer* r, const char* root, const char* entry) {
    
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    lua_setglobal_f(L, "utf8_next", lua_utf8_next);
    
    lua_pushstring(L, root);
    lua_setglobal(L, "root_path");
    
    lua_newtable(L); /* `ui` table */
    
    lua_rawset_f(L, "advance", lui_get_advance_for);
    
    lua_pushstring(L, "layer");
    lua_newtype(L,  "ui.layer",      lui_layer);
    lua_rawset_f(L, "handlers",      lui_layer_handlers);
    lua_rawset_f(L, "draw",          lui_layer_draw);
    lua_rawset_f(L, "draw_contents", lui_layer_draw_contents);
    lua_rawset_f(L, "position",      lui_layer_position);
    lua_rawset_f(L, "resize",        lui_layer_resize);
    lua_rawset_f(L, "__gc",          lui_layer_release);
    lua_rawset(L, -3); /* ui.layer = layer */
    
    lua_setglobal(L, "ui"); /* _G.ui = ui */
    
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
