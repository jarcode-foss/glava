
/* Xlib window creation and GLX context creation backend */

#ifdef GLAVA_GLX

#define GLAVA_RDX11

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <time.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xatom.h>

#include <glad/glad.h>
#include <GL/glx.h>

#include "render.h"
#include "xwin.h"

#define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092
typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
typedef void       (*glXSwapIntervalEXTProc)        (Display*, GLXDrawable, int);

extern struct gl_wcb wcb_glx;

static Display* display;

static int swap;

static bool floating, decorated, focused, maximized, transparent;

struct glxwin {
    Window w;
    GLXContext context;
    double time;
    bool should_close;
};

static void init(void) {
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "XOpenDisplay(): could not establish connection to X11 server\n");
        abort();
    }
    floating    = false;
    decorated   = true;
    focused     = false;
    maximized   = false;
    transparent = false;
}

static void apply_decorations(Window w) {
    if (!decorated) {
        struct {
            unsigned long flags, functions, decorations;
            long input_mode;
            unsigned long status;
        } hints;

        hints.flags       = 2;
        hints.decorations = 0;
        
        Atom motif = XInternAtom(display, "_MOTIF_WM_HINTS", false);

        XChangeProperty(display, w, motif, motif, 32, PropModeReplace,
                        (unsigned char*) &hints, sizeof(hints) / sizeof(long));
    }
}

static void* create_and_bind(const char* name, const char* class,
                             const char* type, const char** states,
                             size_t states_sz,
                             int d, int h,
                             int x, int y,
                             int version_major, int version_minor) {
    struct glxwin* w = malloc(sizeof(struct glxwin));
    w->time         = 0.0D;
    w->should_close = false;

    XVisualInfo* vi;
    XSetWindowAttributes attr;
    GLXFBConfig* fbc;
    int fb_sz, best = -1, samp = -1;

    static int gl_attrs[] = {
        GLX_X_RENDERABLE,  True,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_RENDER_TYPE,   GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_DOUBLEBUFFER,  True,
        GLX_RED_SIZE,      8,
        GLX_GREEN_SIZE,    8,
        GLX_BLUE_SIZE,     8,
        GLX_ALPHA_SIZE,    8,
        None
    };
    
    int context_attrs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, version_major,
        GLX_CONTEXT_MINOR_VERSION_ARB, version_minor,
        // GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
        None
    };
    
    fbc = glXChooseFBConfig(display, DefaultScreen(display), gl_attrs, &fb_sz);
    if (!fbc) {
        fprintf(stderr, "glXChooseFBConfig(): failed\n" );
        abort();
    }
    
    for (int t = 0; t < fb_sz; ++t) {
        XVisualInfo* xvi = glXGetVisualFromFBConfig(display, fbc[t]);
        if (xvi) {
            int samp_buf, samples;
            glXGetFBConfigAttrib(display, fbc[t], GLX_SAMPLE_BUFFERS, &samp_buf);
            glXGetFBConfigAttrib(display, fbc[t], GLX_SAMPLES,        &samples );
            XRenderPictFormat* fmt = XRenderFindVisualFormat(display, xvi->visual);
            
            if (!fmt || (transparent ? fmt->direct.alphaMask == 0 : fmt->direct.alphaMask != 0))
                continue;
            
            if (best < 0 || samp_buf && samples > samp) {
                best = t;
                samp = samples;
            }
            XFree(xvi);
        }
    }

    if (best == -1) {
        fprintf(stderr, "Could not find suitable format for FBConfig\n");
        abort();
    }

    GLXFBConfig config = fbc[best];
    XFree(fbc);
    
    vi = glXGetVisualFromFBConfig(display, config);
    
    attr.colormap          = XCreateColormap(display, DefaultRootWindow(display), vi->visual, AllocNone);
    attr.event_mask        = ExposureMask | KeyPressMask | StructureNotifyMask;
    attr.background_pixmap = None;
    attr.border_pixel      = 0;
    
    if (!(w->w = XCreateWindow(display, DefaultRootWindow(display),
                               x, y, d, h, 0,
                               vi->depth, InputOutput, vi->visual,
                               CWColormap | CWEventMask | CWBackPixmap | CWBorderPixel,
                               &attr))) {
        fprintf(stderr, "XCreateWindow(): failed\n");
        abort();
    }
    
    if (type)
        xwin_settype(&wcb_glx, w, type);

    for (size_t t = 0; t < states_sz; ++t)
        xwin_addstate(&wcb_glx, w, states[t]);

    if (floating) xwin_addstate(&wcb_glx, w, "above");
    if (maximized) {
        xwin_addstate(&wcb_glx, w, "maximized_horz");
        xwin_addstate(&wcb_glx, w, "maximized_vert");
    }

    XSetClassHint(display, w->w, &((XClassHint) { .res_name = (char*) class, .res_class = (char*) class }));

    apply_decorations(w->w);

    XFree(vi);
    
    XStoreName(display, w->w, name);

    Atom dwin = XInternAtom(display, "WM_DELETE_WINDOW", false);
    XSetWMProtocols(display, w->w, &dwin, 1);

    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = NULL;
    glXSwapIntervalEXTProc glXSwapIntervalEXT = NULL;
    glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)
        glXGetProcAddressARB((const GLubyte*) "glXCreateContextAttribsARB");
    glXSwapIntervalEXT = (glXSwapIntervalEXTProc)
        glXGetProcAddressARB((const GLubyte*) "glXSwapIntervalEXT");

    if (!glXCreateContextAttribsARB) {
        fprintf(stderr, "glXGetProcAddressARB(\"glXCreateContextAttribsARB\"): failed\n");
        abort();
    }

    if (!(w->context = glXCreateContextAttribsARB(display, config, 0, True, context_attrs))) {
        fprintf(stderr, "glXCreateContextAttribsARB(): failed\n");
        abort();
    }

    XSync(display, False);
    
    glXMakeCurrent(display, w->w, w->context);
    gladLoadGL();

    GLXDrawable drawable = glXGetCurrentDrawable();
    
    if (glXSwapIntervalEXT) glXSwapIntervalEXT(display, drawable, swap);

    return w;
}

static void set_swap       (int  _swap)        { swap        = _swap;        }
static void set_floating   (bool _floating)    { floating    = _floating;    }
static void set_decorated  (bool _decorated)   { decorated   = _decorated;   }
static void set_focused    (bool _focused)     { focused     = _focused;     }
static void set_maximized  (bool _maximized)   { maximized   = _maximized;   }
static void set_transparent(bool _transparent) { transparent = _transparent; }

static void set_geometry(struct glxwin* w, int x, int y, int d, int h) {
    XMoveResizeWindow(display, w->w, x, y, (unsigned int) d, (unsigned int) h);
}

static void set_visible(struct glxwin* w, bool visible) {
    if (visible) XMapWindow(display, w->w);
    else XUnmapWindow(display, w->w);
}

static bool should_close(struct glxwin* w) {
    return w->should_close;
}

static void swap_buffers(struct glxwin* w) {
    glXSwapBuffers(display, w->w);
    
    while (XPending(display) > 0) {
        XEvent ev;
        XNextEvent(display, &ev);
        switch (ev.type) {
        case ClientMessage:
            if (ev.xclient.message_type  == XInternAtom(display, "WM_PROTOCOLS", 1)
                && ev.xclient.data.l[0]  == XInternAtom(display, "WM_DELETE_WINDOW", 1)) {
                w->should_close = true;
            }
        default: break;
        }
    }
}

static void get_fbsize(struct glxwin* w, int* d, int* h) {
    XWindowAttributes a;
    XGetWindowAttributes(display, w->w, &a);
    *d = a.width;
    *h = a.height;
}

static void get_pos(struct glxwin* w, int* x, int* y) {
    XWindowAttributes a;
    Window _ignored;
    XTranslateCoordinates(display, w->w, DefaultRootWindow(display), 0, 0, x, y, &_ignored);
}

static double get_timert(void) {
    struct timespec tv;
    if (clock_gettime(CLOCK_REALTIME, &tv)) {
        fprintf(stderr, "clock_gettime(CLOCK_REALTIME, ...): %s\n", strerror(errno));
    }
    return (double) tv.tv_sec + ((double) tv.tv_nsec / 1000000000.0D);
}

static double   get_time       (struct glxwin* w)              { return get_timert() - w->time; }
static void     set_time       (struct glxwin* w, double time) { w->time = get_timert() - time; }
static Display* get_x11_display(struct glxwin* w)              { return display; }
static Window   get_x11_window (struct glxwin* w)              { return w->w;    }

WCB_ATTACH("glx", wcb_glx);

#endif /* GLAVA_GLX */
