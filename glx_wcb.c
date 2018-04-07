
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

#include <dlfcn.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>
#include <X11/Xatom.h>

#include <glad/glad.h>

#include "render.h"
#include "xwin.h"

typedef struct __GLXcontextRec *GLXContext;
typedef XID GLXPixmap;
typedef XID GLXDrawable;

typedef void (*__GLXextFuncPtr)(void);

/* GLX 1.3 and later */
typedef struct __GLXFBConfigRec *GLXFBConfig;
typedef XID GLXFBConfigID;
typedef XID GLXContextID;
typedef XID GLXWindow;
typedef XID GLXPbuffer;

/*
 * Tokens for glXChooseVisual and glXGetConfig:
 */
#define GLX_USE_GL		1
#define GLX_BUFFER_SIZE		2
#define GLX_LEVEL		3
#define GLX_RGBA		4
#define GLX_DOUBLEBUFFER	5
#define GLX_STEREO		6
#define GLX_AUX_BUFFERS		7
#define GLX_RED_SIZE		8
#define GLX_GREEN_SIZE		9
#define GLX_BLUE_SIZE		10
#define GLX_ALPHA_SIZE		11
#define GLX_DEPTH_SIZE		12
#define GLX_STENCIL_SIZE	13
#define GLX_ACCUM_RED_SIZE	14
#define GLX_ACCUM_GREEN_SIZE	15
#define GLX_ACCUM_BLUE_SIZE	16
#define GLX_ACCUM_ALPHA_SIZE	17


/*
 * Error codes returned by glXGetConfig:
 */
#define GLX_BAD_SCREEN		1
#define GLX_BAD_ATTRIBUTE	2
#define GLX_NO_EXTENSION	3
#define GLX_BAD_VISUAL		4
#define GLX_BAD_CONTEXT		5
#define GLX_BAD_VALUE       	6
#define GLX_BAD_ENUM		7

/*
 * GLX 1.1 and later:
 */
#define GLX_VENDOR		1
#define GLX_VERSION		2
#define GLX_EXTENSIONS 		3


/*
 * GLX 1.3 and later:
 */
#define GLX_CONFIG_CAVEAT		0x20
#define GLX_DONT_CARE			0xFFFFFFFF
#define GLX_X_VISUAL_TYPE		0x22
#define GLX_TRANSPARENT_TYPE		0x23
#define GLX_TRANSPARENT_INDEX_VALUE	0x24
#define GLX_TRANSPARENT_RED_VALUE	0x25
#define GLX_TRANSPARENT_GREEN_VALUE	0x26
#define GLX_TRANSPARENT_BLUE_VALUE	0x27
#define GLX_TRANSPARENT_ALPHA_VALUE	0x28
#define GLX_WINDOW_BIT			0x00000001
#define GLX_PIXMAP_BIT			0x00000002
#define GLX_PBUFFER_BIT			0x00000004
#define GLX_AUX_BUFFERS_BIT		0x00000010
#define GLX_FRONT_LEFT_BUFFER_BIT	0x00000001
#define GLX_FRONT_RIGHT_BUFFER_BIT	0x00000002
#define GLX_BACK_LEFT_BUFFER_BIT	0x00000004
#define GLX_BACK_RIGHT_BUFFER_BIT	0x00000008
#define GLX_DEPTH_BUFFER_BIT		0x00000020
#define GLX_STENCIL_BUFFER_BIT		0x00000040
#define GLX_ACCUM_BUFFER_BIT		0x00000080
#define GLX_NONE			0x8000
#define GLX_SLOW_CONFIG			0x8001
#define GLX_TRUE_COLOR			0x8002
#define GLX_DIRECT_COLOR		0x8003
#define GLX_PSEUDO_COLOR		0x8004
#define GLX_STATIC_COLOR		0x8005
#define GLX_GRAY_SCALE			0x8006
#define GLX_STATIC_GRAY			0x8007
#define GLX_TRANSPARENT_RGB		0x8008
#define GLX_TRANSPARENT_INDEX		0x8009
#define GLX_VISUAL_ID			0x800B
#define GLX_SCREEN			0x800C
#define GLX_NON_CONFORMANT_CONFIG	0x800D
#define GLX_DRAWABLE_TYPE		0x8010
#define GLX_RENDER_TYPE			0x8011
#define GLX_X_RENDERABLE		0x8012
#define GLX_FBCONFIG_ID			0x8013
#define GLX_RGBA_TYPE			0x8014
#define GLX_COLOR_INDEX_TYPE		0x8015
#define GLX_MAX_PBUFFER_WIDTH		0x8016
#define GLX_MAX_PBUFFER_HEIGHT		0x8017
#define GLX_MAX_PBUFFER_PIXELS		0x8018
#define GLX_PRESERVED_CONTENTS		0x801B
#define GLX_LARGEST_PBUFFER		0x801C
#define GLX_WIDTH			0x801D
#define GLX_HEIGHT			0x801E
#define GLX_EVENT_MASK			0x801F
#define GLX_DAMAGED			0x8020
#define GLX_SAVED			0x8021
#define GLX_WINDOW			0x8022
#define GLX_PBUFFER			0x8023
#define GLX_PBUFFER_HEIGHT              0x8040
#define GLX_PBUFFER_WIDTH               0x8041
#define GLX_RGBA_BIT			0x00000001
#define GLX_COLOR_INDEX_BIT		0x00000002
#define GLX_PBUFFER_CLOBBER_MASK	0x08000000

/*
 * GLX 1.4 and later:
 */
#define GLX_SAMPLE_BUFFERS              0x186a0 /*100000*/
#define GLX_SAMPLES                     0x186a1 /*100001*/

/* glXCreateContextAttribsARB extension definitions */

#define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092

typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
typedef void       (*glXSwapIntervalEXTProc)        (Display*, GLXDrawable, int);

GLXFBConfig*    (*glXChooseFBConfig)       (Display* dpy, int screen, const int* attribList, int* nitems);
XVisualInfo*    (*glXGetVisualFromFBConfig)(Display* dpy, GLXFBConfig config);
int             (*glXGetFBConfigAttrib)    (Display* dpy, GLXFBConfig config, int attribute, int *value );
Bool            (*glXMakeCurrent)          (Display* dpy, GLXDrawable drawable, GLXContext ctx);
GLXDrawable     (*glXGetCurrentDrawable)   (void);
__GLXextFuncPtr (*glXGetProcAddressARB)    (const GLubyte *);
void            (*glXSwapBuffers)          (Display* dpy, GLXDrawable drawable);
void            (*glXDestroyContext)       (Display* dpy, GLXContext);

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

    void* hgl  = dlopen("libGL.so", RTLD_LAZY);
    void* hglx = dlopen("libGLX.so", RTLD_LAZY);

    if (!hgl && !hglx) {
        fprintf(stderr, "Failed to load GLX functions (libGL and libGLX do not exist!)\n");
        exit(EXIT_FAILURE);
    }

    /* Depending on the graphics driver, the GLX functions that we need may either be in libGL or
       libGLX. */
    void* resolve_f(const char* symbol) {
        void* s = NULL;
        if (hgl)        s = dlsym(hgl,  symbol);
        if (!s && hglx) s = dlsym(hglx, symbol);
        if (!s) {
            fprintf(stderr, "Failed to resolve GLX symbol: `%s`\n", symbol);
            exit(EXIT_FAILURE);
        }
        return s;
    }

    #define resolve(name) do { name = (typeof(name)) resolve_f(#name); } while (0)

    resolve(glXChooseFBConfig);
    resolve(glXGetVisualFromFBConfig);
    resolve(glXGetFBConfigAttrib);
    resolve(glXMakeCurrent);
    resolve(glXGetCurrentDrawable);
    resolve(glXGetProcAddressARB);
    resolve(glXSwapBuffers);
    resolve(glXDestroyContext);
    
    #undef resolve
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
            
            if (best < 0 || (samp_buf && samples > samp)) {
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

    bool desktop = false;
    
    if (type)
        desktop = xwin_settype(&wcb_glx, w, type);

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
    
    // XReparentWindow(display, w->w, DefaultRootWindow(display), 0, 0);
    
    /* Eliminate the window's effective region */
    if (desktop){
        int ignored;
        if (XShapeQueryExtension(display, &ignored, &ignored)) {
            Region region;
            if ((region = XCreateRegion())) {
                XShapeCombineRegion(display, w->w, ShapeInput, 0, 0, region, ShapeSet);
                XDestroyRegion(region);
            }
        }
    }

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

static void destroy(struct glxwin* w) {
    glXDestroyContext(display, w->context);
    XDestroyWindow(display, w->w);
    XCloseDisplay(display);
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
