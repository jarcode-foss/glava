/* X11 specific code and features */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/XShm.h>

#include <glad/glad.h>

#define GLAVA_RDX11
#include "render.h"
#include "xwin.h"

static Window find_desktop(struct renderer* r) {
    static Window desktop;
    static bool searched = false;
    if (!searched) {
        Display* d = rd_get_wcb(r)->get_x11_display();
        desktop = DefaultRootWindow(d);
        Window _ignored, * children;
        unsigned int nret;
        XQueryTree(d, desktop, &_ignored, &_ignored, &children, &nret);
        if (children) {
            for (unsigned int t = 0; t < nret; ++t) {
                char* name;
                XFetchName(d, children[t], &name);
                if (name) {
                    /* Mutter-based window managers */
                    if (!strcmp(name, "mutter guard window")) {
                        printf("Using mutter guard window instead of root window\n");
                        // desktop = children[t];
                        t = nret; /* break after */
                    }
                    XFree(name);
                }
            }
            XFree(children);
        }
        searched = true;
    }
    return desktop;
}

bool xwin_should_render(struct renderer* rd) {
    bool ret = true, should_close = false;
    Display* d = rd_get_wcb(rd)->get_x11_display();
    if (!d) {
        d = XOpenDisplay(0);
        should_close = true;
    }

    Atom prop       = XInternAtom(d, "_NET_ACTIVE_WINDOW", true);
    Atom fullscreen = XInternAtom(d, "_NET_WM_STATE_FULLSCREEN", true);
    
    Atom actual_type;
    int actual_format, t;
    unsigned long nitems, bytes_after;
    unsigned char* data;

    int handler(Display* d, XErrorEvent* e) { return 0; }
    
    XSetErrorHandler(handler); /* dummy error handler */
          
    if (Success != XGetWindowProperty(d, DefaultRootWindow(d), prop, 0, 1, false, AnyPropertyType,
                                     &actual_type, &actual_format, &nitems, &bytes_after, &data)) {
        goto close; /* if an error occurs here, the WM probably isn't EWMH compliant */
    }

    if (!nitems)
        goto close;
    
    Window active = ((Window*) data)[0];

    prop = XInternAtom(d, "_NET_WM_STATE", true);

    if (Success != XGetWindowProperty(d, active, prop, 0, LONG_MAX, false, AnyPropertyType,
                                      &actual_type, &actual_format, &nitems, &bytes_after, &data)) {
        goto close; /* some WMs are a little slow on creating _NET_WM_STATE, so errors may occur here */
    }
    for (t = 0; t < nitems; ++t) {
        if (fullscreen == ((Atom*) data)[t]) {
            ret = false;
        }
    }
 close:
    if (should_close)
        XCloseDisplay(d);
    return ret;
}

/* Set window types defined by the EWMH standard, possible values:
   -> "desktop", "dock", "toolbar", "menu", "utility", "splash", "dialog", "normal" */
static void xwin_changeatom(struct gl_wcb* wcb, void* impl, const char* type,
                            const char* atom, const char* fmt, int mode, struct renderer* rd) {
    Window w = wcb->get_x11_window(impl);
    Display* d = wcb->get_x11_display();
    Atom wtype = XInternAtom(d, atom, false);
    size_t len = strlen(type), t;
    char formatted[len + 1];
    for (t = 0; t < len + 1; ++t) {
        char c = type[t];
        switch (c) {
        case 'a' ... 'z': c -= 'a' - 'A';
        default:          formatted[t] = c;
        }
    }
    char buf[256];
    snprintf(buf, sizeof(buf), fmt, formatted);
    Atom desk = XInternAtom(d, buf, false);
    XChangeProperty(d, w, wtype, XA_ATOM, 32, mode, (unsigned char*) &desk, 1);
    
    if (strcmp(formatted, "DESKTOP"))
    {
        XReparentWindow(d, w, find_desktop(rd), 0,0);
    }
    Region region;
    region = XCreateRegion();
    if (region) {
        XShapeCombineRegion(d, w, ShapeInput, 0, 0, region,
                            ShapeSet);
        XDestroyRegion(region);
    }
}

void xwin_settype(struct gl_wcb* wcb, void* impl, const char* type, struct renderer* r) {
    xwin_changeatom(wcb, impl, type, "_NET_WM_WINDOW_TYPE", "_NET_WM_WINDOW_TYPE_%s", PropModeReplace, r);
}

void xwin_addstate(struct gl_wcb* wcb, void* impl, const char* state, struct renderer* r) {
    xwin_changeatom(wcb, impl, state, "_NET_WM_STATE", "_NET_WM_STATE_%s", PropModeAppend, r);
}

static Drawable get_drawable(Display* d, Window w) {
    Drawable p;
    Atom act_type;
    int act_format;
    unsigned long nitems, bytes_after;
    unsigned char *data = NULL;
    Atom id;

    id = XInternAtom(d, "_XROOTPMAP_ID", False);
    
    if (XGetWindowProperty(d, w, id, 0, 1, False, XA_PIXMAP,
                           &act_type, &act_format, &nitems, &bytes_after,
                           &data) == Success && data) {
        p = *((Pixmap *) data);
        XFree(data);
    } else {
        p = w;
    }

    return p;
}

unsigned int xwin_copyglbg(struct renderer* rd, unsigned int tex) {
    GLuint texture = (GLuint) tex;
    if (!texture)
        glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    bool use_shm = true;
    
    int x, y, w, h;
    rd_get_wcb(rd)->get_fbsize(rd_get_impl_window(rd), &w, &h);
    rd_get_wcb(rd)->get_pos(rd_get_impl_window(rd), &x, &y);
    XColor c;
    Display* d = rd_get_wcb(rd)->get_x11_display();
    Drawable src = get_drawable(d, find_desktop(rd));

    /* Obtain section of root pixmap */
    
    XShmSegmentInfo shminfo;
    Visual* visual = DefaultVisual(d, DefaultScreen(d));
    XVisualInfo match = { .visualid = XVisualIDFromVisual(visual) };
    int nret;
    XVisualInfo* info = XGetVisualInfo(d, VisualIDMask, &match, &nret);
    XImage* image;
    if (use_shm) {
        image = XShmCreateImage(d, visual, info->depth, ZPixmap, NULL,
                                &shminfo, (unsigned int) w, (unsigned int) h);
        if ((shminfo.shmid = shmget(IPC_PRIVATE, image->bytes_per_line * image->height,
                                    IPC_CREAT | 0777)) == -1) {
            fprintf(stderr, "shmget() failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        shminfo.shmaddr = image->data = shmat(shminfo.shmid, 0, 0);
        shminfo.readOnly = false;
        XShmAttach(d, &shminfo);
        XShmGetImage(d, src, image, x, y, AllPlanes);
    } else {
        image = XGetImage(d, src, x, y, (unsigned int) w, (unsigned int) h,
                  ZPixmap, AllPlanes);
    }

    /* Try to convert pixel bit depth to OpenGL storage format. The following formats\
       will need intermediate conversion before OpenGL can accept the data:
       
       - 8-bit pixel formats (retro displays, low-bandwidth virtual displays)
       - 36-bit pixel formats (rare deep color displays) */

    if (image) {
        bool invalid = false, aligned = false;
        GLenum type;
        switch (image->bits_per_pixel) {
        case 16:
            switch (image->depth) {
            case 12: type = GL_UNSIGNED_SHORT_4_4_4_4; break; /* 12-bit (rare)    */
            case 15: type = GL_UNSIGNED_SHORT_5_5_5_1; break; /* 15-bit, hi-color */
            case 16:                                          /* 16-bit, hi-color */
                type    = GL_UNSIGNED_SHORT_5_6_5;
                aligned = true;
                break;
            }
            break;
        case 32:
            switch (image->depth) {
            case 24: type = GL_UNSIGNED_BYTE;           break; /* 24-bit, true color */
            case 30: type = GL_UNSIGNED_INT_10_10_10_2; break; /* 30-bit, deep color */
            }
            break;
        case 64: 
            if (image->depth == 48) /* 48-bit deep color */
                type = GL_UNSIGNED_SHORT;
            else goto invalid;
            break;
            /* >64-bit formats */
        case 128:
            if (image->depth == 96)
                type = GL_UNSIGNED_INT;
            else goto invalid;
            break;
        default:
        invalid: invalid = true;
        }
    
        uint8_t* buf;
        if (invalid) {
            abort();
            /* Manual reformat (slow) */
            buf = malloc(4 * w * h);
            int xi, yi;
            Colormap map = DefaultColormap(d, DefaultScreen(d));
            for (yi = 0; yi < h; ++yi) {
                for (xi = 0; xi < w; ++xi) {
                    c.pixel = XGetPixel(image, xi, yi);
                    XQueryColor(d, map, &c);
                    size_t base = (xi + (yi * w)) * 4;
                    buf[base + 0] = c.red   / 256;
                    buf[base + 1] = c.green / 256;
                    buf[base + 2] = c.blue  / 256;
                    buf[base + 3] = 255;
                }
            }
    
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf);
            free(buf);
        } else {
            /* Use image data directly. The alpha value is garbage/unassigned data, but
               we need to read it because X11 keeps pixel data aligned */
            buf = (uint8_t*) image->data;
            /* Data could be 2, 4, or 8 byte aligned, the RGBA format and type (depth)
               already ensures reads will be properly aligned across scanlines */
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            GLenum format = image->bitmap_bit_order == LSBFirst ?
                (!aligned ? GL_BGRA : GL_BGR) :
                (!aligned ? GL_RGBA : GL_RGB);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, format, type, buf);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4); /* restore default */
        }
    }
    if (use_shm) {
        XShmDetach(d, &shminfo);
        shmdt(shminfo.shmaddr);
        shmctl(shminfo.shmid, IPC_RMID, NULL);
    }

    if (image) XDestroyImage(image);
    
    return texture;
}
