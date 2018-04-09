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

static int lui_utf8_index(lua_State* L) {
    size_t sz;
    const char* c = luaL_checklstring(L, 1, &sz);
    bool ii = lua_isnumber(L, 2);
    int i;
    if (ii) {
        i = lua_tointeger(L, 2);
        if (i >= 1 && i <= sz) {
            size_t char_len = utf8_next(c + (i - 1));
            if (!char_len || char_len + (i - 1) > sz) {
                luaL_error(L, "Tried to index UTF8 string at bad offset `%d` (%d)", i, (int) sz);
            }
            lua_pushlstring(L, c + (i - 1), char_len);
        } else luaL_error(L, "Tried to index UTF8 string at out-of-bounds index `%d` (%d)", i, (int) sz);
    } else {
        lua_getmetatable(L, 1); /* string mt */
        lua_pushvalue(L, 1);    /* key */
        lua_gettable(L, -2);    /* index mt */
    }
    return 1;
}

static int lui_utf8_next(lua_State* L) {
    size_t sz, l, n;
    const char* c = luaL_checklstring(L, 1, &sz);
    int i = lua_isnumber(L, 2) ? lua_tointeger(L, 2) : 0;
    if (i) {
        if (!(l = utf8_next(c + (i - 1)))) goto error;
        if (l + (i - 1) <= sz) {
            if (!(n = utf8_next(c + (i - 1) + l))) goto error;
            if (n + (i - 1) + l <= sz) {
                lua_pushinteger(L, i + l);
                lua_pushlstring(L, c + (i - 1) + l, n);
                return 2;
            }
        }
        lua_pushnil(L);
        return 1;
    } else {
        if (!(l = utf8_next(c))) goto error;
        if (l <= sz) {
            lua_pushinteger(L, l);
            lua_pushlstring(L, c, l);
            return 2;
        } else {
            lua_pushnil(L);
            return 1;
        }
    }
 error:
    luaL_error(L, "Tried to iterate invalid UTF8 string, encountered invalid data after index `%d`", i);
    return 0;
}

static int lui_utf8_pairs(lua_State* L) {
    luaL_checktype(L, 1, LUA_TSTRING);
    lua_pushcfunction(L, lui_utf8_next);
    lua_pushvalue(L, 1);
    return 2;
}

static int lui_utf8_codepoint(lua_State* L) {
    size_t sz, n;
    const char* c = luaL_checklstring(L, 1, &sz);
    n = utf8_next(c);
    if (!n || n > sz)
        luaL_error(L, "Tried to convert bad UTF8 string into codepoint");
    lua_pushinteger(L, utf8_codepoint(c));
    return 1;
}

static int lui_get_advance_for(lua_State* L) {
    uint32_t i = (uint32_t) luaL_checkinteger(L, 1);
    lua_pushinteger(L, (int) ui_get_advance_for(i));
    return 1;
}

#define lua_takenumber(L, name, ...)            \
    do {                                        \
        lua_pushstring(L, name);                \
        lua_rawget(L, -2);                      \
        if (lua_isnumber(L, -1)) {              \
            __VA_ARGS__;                        \
        }                                       \
        lua_pop(L, 1);                          \
    } while (0)

#define lua_taketable(L, name, ...)             \
    do {                                        \
        lua_pushstring(L, name);                \
        lua_rawget(L, -2);                      \
        if (lua_istable(L, -1)) {               \
            __VA_ARGS__;                        \
        }                                       \
        lua_pop(L, 1);                          \
    } while (0)

static int lui_text(lua_State* L) {
    struct text_data* d = lua_newuserdata(L, sizeof(struct text_data));
    *d = (struct text_data) {};
    luaL_getmetatable(L, "ui.text");
    lua_setmetatable(L, -2);
    return 1;
}

static int lui_text_draw(lua_State* L) {
    ui_text_draw(lua_checkuptr(L, 1, "text"));
    return 0;
}

static int lui_text_position(lua_State* L) {
    ui_text_set_pos(lua_checkuptr(L, 1, "text"), (struct position) {
            .x = (uint32_t) luaL_checkinteger(L, 2), .y = (uint32_t) luaL_checkinteger(L, 3) });
    return 0;
}

static int lui_text_contents(lua_State* L) {
    struct text_data* d = lua_checkuptr(L, 1, "text");
    size_t len;
    const char* str = luaL_checklstring(L, 2, &len);
    luaL_checktype(L, 3, LUA_TTABLE);
    struct color color = { .a = 1.0F };
    lua_settop(L, 3);
    lua_takenumber(L, "r", color.r = lua_tonumber(L, -1));
    lua_takenumber(L, "g", color.g = lua_tonumber(L, -1));
    lua_takenumber(L, "b", color.b = lua_tonumber(L, -1));
    lua_takenumber(L, "a", color.a = lua_tonumber(L, -1));
    ui_text_contents(d, str, len, color);
    return 0;
}

static int lui_text_release(lua_State* L) {
    ui_text_release(lua_checkuptr(L, 1, "text"));
    return 0;
}

static void handle_box_properties(lua_State* L, struct box_data* d) {
    lua_takenumber(L, "tex", d->tex = lua_tointeger(L, -1));
    lua_taketable(L, "tex_color", {
            lua_takenumber(L, "r", d->tex_color.r = lua_tonumber(L, -1));
            lua_takenumber(L, "g", d->tex_color.g = lua_tonumber(L, -1));
            lua_takenumber(L, "b", d->tex_color.b = lua_tonumber(L, -1));
            lua_takenumber(L, "a", d->tex_color.a = lua_tonumber(L, -1));
        });
    lua_taketable(L, "tex_pos", {
            lua_takenumber(L, "x", d->tex_pos.x = (int32_t) lua_tointeger(L, -1));
            lua_takenumber(L, "y", d->tex_pos.y = (int32_t) lua_tointeger(L, -1));
        });
    lua_taketable(L, "geometry", {
            lua_takenumber(L, "x", d->geometry.x = (int32_t) lua_tointeger(L, -1));
            lua_takenumber(L, "y", d->geometry.x = (int32_t) lua_tointeger(L, -1));
            lua_takenumber(L, "w", d->geometry.x = (int32_t) lua_tointeger(L, -1));
            lua_takenumber(L, "h", d->geometry.x = (int32_t) lua_tointeger(L, -1));
        });
    
    lua_pop(L, 1); /* pop table */
}

static int lui_box(lua_State* L) {
    struct box_data* d = lua_newuserdata(L, sizeof(struct box_data));
    *d = (struct box_data) {};
    luaL_getmetatable(L, "ui.box");
    lua_setmetatable(L, -2);
    if (lua_istable(L, 1)) {
        lua_pushvalue(L, 1); /* create copy of table arg for top of stack */
        handle_box_properties(L, d);
    }
    ui_box(d);
    return 1;
}

static int lui_box_properties(lua_State* L) {
    struct box_data* d = lua_checkuptr(L, 1, "box");
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_settop(L, 2); /* make sure the table arg is the last/top value in the stack */
    handle_box_properties(L, d);
    ui_box(d);
    return 0;
}

static int lui_box_draw(lua_State* L) {
    ui_box_draw(lua_checkuptr(L, 1, "box"));
    return 0;
}

static int lui_box_release(lua_State* L) {
    ui_box_release(lua_checkuptr(L, 1, "box"));
    return 0;
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
    
static void render_cb(void* udata) {
    lua_State* L = udata;
    lua_pushvalue(L, 2);
    if (lua_isfunction(L, -1)) {
        layer_perrcall(L);
    }
}
    
static void font_cb(void* udata) {
    lua_State* L = udata;
    lua_pushvalue(L, 3);
    if (lua_isfunction(L, -1))
        layer_perrcall(L);
}

static int lui_layer_handlers(lua_State* L) {
    struct layer_data* d = lua_checkuptr(L, 1, "layer");
    luaL_checktype(L, 2, LUA_TFUNCTION);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    
    d->render_cb = render_cb;
    d->font_cb   = font_cb;
    d->udata     = L;
    
    lua_getglobal(L, "__layer_handlers");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);        /* pop nil or other bogus value */
        lua_newtable(L);      /* new handlers table */
        lua_pushvalue(L, -1); /* copy of handlers table */
        lua_newtable(L);      /* metatable */
        lua_pushstring(L, "__mode");
        lua_pushstring(L, "k");
        lua_rawset(L, -3);       /* mt.__mode = v */
        lua_setmetatable(L, -2); /* setmetatable(__layer_handlers, mt) */
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
    ui_layer_draw(lua_checkuptr(L, 1, "layer"));
    return 0;
}

static int lui_layer_position(lua_State* L) {
    struct layer_data* d = lua_checkuptr(L, 1, "layer");
    d->frame.geometry.x = (int32_t) luaL_checkinteger(L, 2);
    d->frame.geometry.y = (int32_t) luaL_checkinteger(L, 3);
    ui_box(&d->frame);
    return 0;
}

static int lui_layer_color(lua_State* L) {
    struct layer_data* d = lua_checkuptr(L, 1, "layer");
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_settop(L, 2);
    lua_takenumber(L, "r", d->frame.tex_color.r = lua_tonumber(L, -1));
    lua_takenumber(L, "g", d->frame.tex_color.g = lua_tonumber(L, -1));
    lua_takenumber(L, "b", d->frame.tex_color.b = lua_tonumber(L, -1));
    lua_takenumber(L, "a", d->frame.tex_color.a = lua_tonumber(L, -1));
    return 0;
}

static int lui_layer_resize(lua_State* L) {
    if (lua_isnumber(L, 4) && lua_isnumber(L, 5)) {
        ui_layer_resize_sep(lua_checkuptr(L, 1, "layer"),
                            (uint32_t) luaL_checkinteger(L, 2),
                            (uint32_t) luaL_checkinteger(L, 3),
                            (uint32_t) luaL_checkinteger(L, 4),
                            (uint32_t) luaL_checkinteger(L, 5));
    } else {
        ui_layer_resize(lua_checkuptr(L, 1, "layer"),
                        (uint32_t) luaL_checkinteger(L, 2),
                        (uint32_t) luaL_checkinteger(L, 3));
    }
    return 0;
}

static int lui_layer_draw_contents(lua_State* L) {
    struct layer_data* d = lua_checkuptr(L, 1, "layer");
    lua_getglobal(L, "__layer_handlers");
    if (lua_istable(L, -1)) {
        lua_pushvalue(L, 1);   /* udata key*/
        lua_rawget(L, -2);     /* index and pop key, return table */
        if (lua_istable(L, -1)) {
            lua_rawgeti(L, -1, 1); /* render callback */
            lua_rawgeti(L, -2, 2); /* font callback */
            lua_remove(L, 2);      /* remove tables from stack */
            lua_remove(L, 2);      /* ^ */
        }
    }
    /* stack layout should be (layer, render function, font function),
       unless the callback functions are NULL */
    ui_layer_draw_contents(d);
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

struct lui_font_wrapper {
    struct char_cache* cache;
};

static int lui_font(lua_State* L) {
    const char* str = luaL_checkstring(L, 1);
    int32_t size = (int32_t) luaL_checkinteger(L, 2);
    struct char_cache* cache;
    if (!(cache = ui_load_font(str, size))) return 0;
    struct lui_font_wrapper* w = lua_newuserdata(L, sizeof(struct lui_font_wrapper));
    w->cache = cache;
    luaL_getmetatable(L, "ui.font");
    lua_setmetatable(L, -2);
    return 1;
}

static int lui_font_select(lua_State* L) {
    ui_select_font(((struct lui_font_wrapper*) lua_checkuptr(L, 1, "font"))->cache);
    return 0;
}

static int lui_font_index(lua_State* L) {
    struct lui_font_wrapper* w = lua_checkuptr(L, 1, "font");
    const char* key = luaL_checkstring(L, 2);
    
    #define SPROP(n) if (!strcmp(key, #n)) lua_pushinteger(L, (int) ui_font_info(w->cache)->n)
    SPROP(ascender);
    else SPROP(descender);
    else SPROP(height);
    else SPROP(width);
    else SPROP(max_advance);
    else SPROP(baseline);
    else {
        luaL_getmetatable(L, "ui.font");
        lua_pushvalue(L, 2);
        lua_rawget(L, -2);
    }
    #undef SPROP
    
    return 1;
}

/* Closure that handles mt.__call(mt, ...) invocations and passes 
   the remaining arguments to the upvalue function */
static int lua_call_new(lua_State* L) {
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_replace(L, 1); /* replace mt argument with function */
    lua_call(L, lua_gettop(L) - 1, LUA_MULTRET);
    return lua_gettop(L);
}

/* Creates a new metatable, with its `__index` set to itself, and with its own
   metatable set to `mt`, which contains a `__call` function that executes the
   `constructor` argument. This allows the following syntax to work:
   
   local value = some_type(arg0, arg1, ...)
   
   where `some_type` is actually the initial metatable. */
static void lua_newtype(lua_State* L, const char* mtname,
                        lua_CFunction constructor, lua_CFunction indexer) {
    luaL_newmetatable(L, mtname); /* new @ */
    lua_pushstring(L, "__index");
    if (!indexer) lua_pushvalue(L, -2);
    else          lua_pushcfunction(L, indexer);
    lua_rawset(L, -3); /* @.__index = @ */
    lua_newtable(L);   /* new mt */
    lua_pushstring(L, "__call");
    lua_pushcfunction(L, constructor); /* closure upvalue */
    lua_pushcclosure(L, lua_call_new, 1);
    lua_rawset(L, -3);       /* mt.__call = lua_call_new */
    lua_setmetatable(L, -2); /* setmetatable(@, mt) */
    /* leave metatable on the stack */
}

struct bindings* bd_init(struct renderer* r, const char* root, const char* entry) {
    
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    
    lua_newtable(L);
    lua_rawset_f(L, "pairs", lui_utf8_pairs);
    lua_rawset_f(L, "index", lui_utf8_index);
    lua_rawset_f(L, "codepoint", lui_utf8_codepoint);
    lua_setglobal(L, "utf8"); /* _G.utf8 = utf8 */
    
    lua_pushstring(L, root);
    lua_setglobal(L, "root_path");
    
    lua_newtable(L); /* `ui` table */
    
    lua_rawset_f(L, "advance", lui_get_advance_for);
    
    lua_pushstring(L, "layer");
    lua_newtype(L,  "ui.layer",      lui_layer, NULL);
    lua_rawset_f(L, "handlers",      lui_layer_handlers);
    lua_rawset_f(L, "draw",          lui_layer_draw);
    lua_rawset_f(L, "draw_contents", lui_layer_draw_contents);
    lua_rawset_f(L, "position",      lui_layer_position);
    lua_rawset_f(L, "resize",        lui_layer_resize);
    lua_rawset_f(L, "color",         lui_layer_color);
    lua_rawset_f(L, "__gc",          lui_layer_release);
    lua_rawset(L, -3); /* ui.layer = layer */

    lua_pushstring(L, "box");
    lua_newtype(L,  "ui.box",     lui_box, NULL);
    lua_rawset_f(L, "draw",       lui_box_draw);
    lua_rawset_f(L, "properties", lui_box_properties);
    lua_rawset_f(L, "__gc",       lui_box_release);
    lua_rawset(L, -3); /* ui.box = box */

    lua_pushstring(L, "text");
    lua_newtype (L, "ui.text",  lui_text, NULL);
    lua_rawset_f(L, "draw",     lui_text_draw);
    lua_rawset_f(L, "position", lui_text_position);
    lua_rawset_f(L, "contents", lui_text_contents);
    lua_rawset_f(L, "__gc",     lui_text_release);
    lua_rawset(L, -3); /* ui.text = text */

    lua_pushstring(L, "font");
    lua_newtype(L,  "ui.font", lui_font, lui_font_index);
    lua_rawset_f(L, "select",  lui_font_select);
    lua_rawset(L, -3);
    
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

void bd_setup(struct bindings* state, const char* module) {
    lua_State* L = state->L;
    lua_getglobal(L, "setup");
    if (!lua_isfunction(L, -1)) {
        fputs(FATAL_PREFIX "`setup` is not a Lua function! Did you forget to set it?\n", stderr);
        exit(EXIT_FAILURE);
    }
    lua_pushstring(L, module);
    switch (lua_pcall(L, 1, 0, 0)) {
    case LUA_ERRRUN:
        fprintf(stderr, FATAL_PREFIX
                "Uncaught error while running Lua setup function:\n%s\n", lua_tostring(L, -1));
        exit(EXIT_FAILURE);
    case LUA_ERRMEM:
        fprintf(stderr, FATAL_PREFIX
                "Memory error while running Lua setup function:\n%s\n", lua_tostring(L, -1));
        exit(EXIT_FAILURE);
    case LUA_ERRERR:
        fprintf(stderr, FATAL_PREFIX
                "Error while running Lua error handler:\n%s\n", lua_tostring(L, -1));
        exit(EXIT_FAILURE);
    }
}

void bd_frame(struct bindings* state, int w, int h) {
    lua_State* L = state->L;
    lua_getglobal(L, "draw");
    if (!lua_isfunction(L, -1)) {
        fputs(FATAL_PREFIX "`draw` is not a Lua function! Did you forget to set it?\n", stderr);
        exit(EXIT_FAILURE);
    }
    lua_pushinteger(L, w);
    lua_pushinteger(L, h);
    switch (lua_pcall(L, 2, 0, 0)) {
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
