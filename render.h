
#ifndef RENDER_H
#define RENDER_H

struct gl_data;

typedef struct renderer {
    bool    alive, mirror_input;
    size_t  bufsize_request, rate_request, samplesize_request;
    char*   audio_source_request;
    struct gl_data* gl;
} renderer;

struct renderer* rd_new            (const char** paths,        const char* entry,
                                    const char** requests,     const char* force_backend,
                                    bool         auto_desktop, bool        verbose);
bool             rd_update         (struct renderer*, float* lb, float* rb,
                                    size_t bsz, bool modified);
void             rd_destroy        (struct renderer*);
void             rd_time           (struct renderer*);
void*            rd_get_impl_window(struct renderer*);
struct gl_wcb*   rd_get_wcb        (struct renderer*);

/* gl_wcb - OpenGL Window Creation Backend interface */
struct gl_wcb {
    const char* name;
    void     (*init)           (void);
    void*    (*create_and_bind)(const char* name, const char* class,
                                const char* type, const char** states,
                                size_t states_sz,
                                int w, int h,
                                int x, int y,
                                int version_major, int version_minor,
                                bool clickthrough);
    bool     (*should_close)   (void* ptr);
    bool     (*should_render)  (void* ptr);
    bool     (*bg_changed)     (void* ptr);
    void     (*swap_buffers)   (void* ptr);
    void     (*raise)          (void* ptr);
    void     (*destroy)        (void* ptr);
    void     (*terminate)      (void);
    void     (*get_pos)        (void* ptr, int* x, int* y);
    void     (*get_fbsize)     (void* ptr, int* w, int* h);
    void     (*set_geometry)   (void* ptr, int x, int y, int w, int h);
    void     (*set_swap)       (int interval);
    void     (*set_floating)   (bool floating);
    void     (*set_decorated)  (bool decorated);
    void     (*set_focused)    (bool focused);
    void     (*set_maximized)  (bool maximized);
    void     (*set_transparent)(bool transparent);
    double   (*get_time)       (void* ptr);
    void     (*set_time)       (void* ptr, double time);
    void     (*set_visible)    (void* ptr, bool visible);
    const char* (*get_environment) (void);
    #ifdef GLAVA_RDX11
    Display* (*get_x11_display)(void);
    Window   (*get_x11_window) (void* ptr);
    #else /* define placeholders to ensure equal struct size */
    void* _X11_DISPLAY_PLACEHOLDER;
    void* _X11_WINDOW_PLACEHOLDER;
    #endif
};

#define WCB_FUNC(F)                                 \
    .F = (typeof(((struct gl_wcb*) NULL)->F)) &F

#define WCB_ATTACH(B, N)                        \
    struct gl_wcb N = {                         \
        .name = B,                              \
        WCB_FUNC(init),                         \
        WCB_FUNC(create_and_bind),              \
        WCB_FUNC(should_close),                 \
        WCB_FUNC(should_render),                \
        WCB_FUNC(bg_changed),                   \
        WCB_FUNC(swap_buffers),                 \
        WCB_FUNC(raise),                        \
        WCB_FUNC(destroy),                      \
        WCB_FUNC(terminate),                    \
        WCB_FUNC(set_swap),                     \
        WCB_FUNC(get_pos),                      \
        WCB_FUNC(get_fbsize),                   \
        WCB_FUNC(set_geometry),                 \
        WCB_FUNC(set_floating),                 \
        WCB_FUNC(set_decorated),                \
        WCB_FUNC(set_focused),                  \
        WCB_FUNC(set_maximized),                \
        WCB_FUNC(set_transparent),              \
        WCB_FUNC(set_time),                     \
        WCB_FUNC(get_time),                     \
        WCB_FUNC(set_visible),                  \
        WCB_FUNC(get_environment),              \
        WCB_FUNC(get_x11_display),              \
        WCB_FUNC(get_x11_window)                \
    }

#endif /* RENDER_H */
