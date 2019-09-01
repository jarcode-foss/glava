#include <stdlib.h>
#include <obs/obs-module.h>
#include <obs/obs.h>
#include <obs/util/threading.h>
#include <obs/util/platform.h>

#include "../glava/glava.h"
#include <X11/Xlib.h>

#pragma GCC visibility push(default)
OBS_DECLARE_MODULE();
#pragma GCC visibility pop

static glava_handle handle;

/* To access OBS's GL context and internal texture handles we need to define internal 
   OBS structures such that we can access these members. This is not API-stable, but
   these structure layouts rarely change. */

/* OBS INTERNAL DEFS */

typedef struct __GLXcontextRec* GLXContext;
typedef XID GLXPixmap;
typedef XID GLXDrawable;
typedef XID GLXPbuffer;

struct gl_platform_internal {
    Display *display;
    GLXContext context;
    GLXPbuffer pbuffer;
};

struct gs_device_internal {
    struct gl_platform_internal* plat;
    /* trailing members present */
};


struct gs_subsystem_internal {
    void*                      module;
    struct gs_device_internal* device;
    /* trailing members present */
};

struct gs_texture {
    struct gs_device_internal* device;
    int                        type;
    int                        format;
    int                        gl_format;
    int                        gl_target;
    int                        gl_internal_format;
    int                        gl_type;
    unsigned int               texture;
    uint32_t                   levels;
    bool                       is_dynamic;
    bool                       is_render_target;
    bool                       is_dummy;
    bool                       gen_mipmaps;
    void*                      cur_sampler;
    void*                      fbo;
};

struct gs_texture_2d_internal {
    struct gs_texture    base;
    uint32_t             width;
    uint32_t             height;
    /* trailing members present */
};

/* END OBS INTERNAL DEFS */

struct mod_state {
    obs_source_t* source;
    pthread_t     thread;
    bool          initialized;
    gs_texture_t* gs_tex;
    unsigned int  old_tex;
    struct {
        char* opts;
        int w, h;
    } cfg;
};

static const char* get_name(void* _) {
    UNUSED_PARAMETER(_);
    return "GLava Direct Source";
}

static obs_properties_t* get_properties(void* _) {
    UNUSED_PARAMETER(_);

    obs_properties_t* props = obs_properties_create();
    // (obs_properties_t *props, const char *name, const char *description, int min, int max, int step)
    obs_properties_add_int (props, "width",   "Output width",  0, 65535, 1);
    obs_properties_add_int (props, "height",  "Output height", 0, 65535, 1);
    obs_properties_add_text(props, "options", "GLava options", OBS_TEXT_DEFAULT);

    return props;
}

static uint32_t get_width(void* data) {
    struct mod_state* s = (struct mod_state*) data;
    return (uint32_t) s->cfg.w;
}

static uint32_t get_height(void* data) {
    struct mod_state* s = (struct mod_state*) data;
    return (uint32_t) s->cfg.h;
}

static void* work_thread(void* _) {
    UNUSED_PARAMETER(_);
    glava_entry(1, (char**) &"glava", &handle);
    return NULL;
}

static void glava_join(void* data) {
    struct mod_state* s = (struct mod_state*) data;

    glava_terminate(&handle);

    if (s->initialized) {
        if (pthread_join(s->thread, NULL)) {
            blog(LOG_ERROR, "Failed to join GLava thread");
            return;
        }
    }

    s->initialized = false;
    
    if (s->gs_tex != NULL) {
        obs_enter_graphics();
        /* restore old GL texture */
        ((struct gs_texture_2d_internal*) s->gs_tex)->base.texture = s->old_tex;
        gs_texture_destroy(s->gs_tex);
        obs_leave_graphics();
        s->gs_tex = NULL;
    }
}

static void glava_start(void* data) {
    struct mod_state* s = (struct mod_state*) data;

    if (s->initialized) {
        blog(LOG_ERROR, "Already initialized GLava thread");
        return;
    }
    
    if (pthread_create(&s->thread, NULL, work_thread, s) != 0) {
        blog(LOG_ERROR, "Failed to create GLava thread");
        return;
    }
    
    s->initialized = true;
    
    /* Obtain GLava's texture handle */
    blog(LOG_INFO, "Waiting for GLava GL texture...");
    glava_wait(&handle);
    unsigned int g_tex = glava_tex(handle);
    glava_sizereq(handle, 0, 0, s->cfg.w, s->cfg.h);
    obs_enter_graphics();
    /* Create a new high-level texture object */
    s->gs_tex = gs_texture_create(s->cfg.w, s->cfg.h, GS_RGBA, 1, NULL, GS_DYNAMIC);
    /* Re-assign the internal GL texture for the object */
    s->old_tex = ((struct gs_texture_2d_internal*) s->gs_tex)->base.texture;
    ((struct gs_texture_2d_internal*) s->gs_tex)->base.texture = g_tex;
    obs_leave_graphics();
    blog(LOG_INFO, "GLava texture assigned");
}

static void destroy(void* data) {
    struct mod_state* s = (struct mod_state*) data;
    if (s) {
        glava_join(s);
        bfree(s);
    }
}

static void update(void* data, obs_data_t* settings) {
    struct mod_state* s = (struct mod_state*) data;

    s->cfg.w = (int) obs_data_get_int(settings, "width");
    s->cfg.h = (int) obs_data_get_int(settings, "height");
    const char* opts = obs_data_get_string(settings, "options");
    printf("debug: input str '%s', set '%s'\n", opts, s->cfg.opts);
    bool opts_changed = s->cfg.opts == NULL || strcmp(opts, s->cfg.opts);
    if (s->cfg.opts != NULL) {
        free(s->cfg.opts);
    }
    s->cfg.opts = strdup(opts);

    if (opts_changed) {
        blog(LOG_INFO, "Updating GLava state");
        glava_join(s);
        glava_start(s);
    } else {
        glava_sizereq(handle, 0, 0, s->cfg.w, s->cfg.h);
        ((struct gs_texture_2d_internal*) s->gs_tex)->width  = s->cfg.w;
        ((struct gs_texture_2d_internal*) s->gs_tex)->height = s->cfg.h;
    }
}

static void video_render(void* data, gs_effect_t* effect) {
    struct mod_state* s = (struct mod_state*) data;
    if (s->gs_tex == NULL)
        return;

    effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);

    gs_eparam_t* img = gs_effect_get_param_by_name(effect, "image");
    gs_effect_set_texture(img, s->gs_tex);
    while (gs_effect_loop(effect, "Draw"))
        obs_source_draw(s->gs_tex, 0, 0, 0, 0, true);
}

static void* create(obs_data_t* settings, obs_source_t* source) {
    
    blog(LOG_INFO, "Initializing GLava OBS Plugin...");
    
    struct mod_state* s = bzalloc(sizeof(struct mod_state));
    s->source      = source;
    s->cfg         = (typeof(s->cfg)) { .w = 512, .h = 256, .opts = NULL };
    s->gs_tex      = NULL;
    s->initialized = false;
    
    struct obs_video_info ovi;
    if (!obs_get_video_info(&ovi)) {
        blog(LOG_ERROR, "Failed to obtain `obs_video_info`");
        return NULL;
    }
    
    if (strncmp(ovi.graphics_module, "libobs-opengl", 13) != 0) {
        blog(LOG_ERROR, "No GLX rendering context present");
        return NULL;
    }
    
    obs_enter_graphics();
    struct gs_subsystem_internal* sub = (struct gs_subsystem_internal*) gs_get_context();
    glava_assign_external_ctx(sub->device->plat->context);
    obs_leave_graphics();
    
    update(s, settings);
    return s;
}

static struct obs_source_info glava_src = {
    .id             = "glava",
    .type           = OBS_SOURCE_TYPE_INPUT,
    .output_flags   = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_DO_NOT_DUPLICATE,
    .get_name       = get_name,
    .create         = create,
    .destroy        = destroy,
    .update         = update,
    .video_render   = video_render,
    .get_width      = get_width,
    .get_height     = get_height,
    .get_properties = get_properties
};

__attribute__((visibility("default"))) bool obs_module_load(void) {
    obs_register_source(&glava_src);
    return true;
}
