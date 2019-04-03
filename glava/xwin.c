/* X11 specific code and features */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include <time.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/XShm.h>

#include "glad.h"

#define GLAVA_RDX11
#include "render.h"
#include "xwin.h"

/* Note: currently unused */
Window* __attribute__ ((unused)) xwin_get_desktop_layer(struct gl_wcb* wcb) {
    static Window desktop;
    static bool searched = false;
    if (!searched) {
        Display* d = wcb->get_x11_display();
        Atom class = XInternAtom(d, "WM_CLASS", false);
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
                        printf("Reparenting to mutter guard window instead of root window\n");
                        desktop = children[t];
                        t = nret; /* break after */
                    }
                    XFree(name);
                }
                unsigned long bytes;
                XTextProperty text = {};
                char** list;
                int list_sz;
                /* Get WM_CLASS property */
                if (Success == XGetWindowProperty(d, children[t], class, 0, 512, false, AnyPropertyType,
                                                  &text.encoding, &text.format, &text.nitems, &bytes,
                                                  &text.value)) {
                    /* decode string array */
                    if (Success == XmbTextPropertyToTextList(d, &text, &list, &list_sz)) {
                        if (list_sz >= 1 && !strcmp(list[0], "plasmashell")) {
                            desktop = children[t];
                            t = nret;
                        }
                        XFreeStringList(list);
                    }
                    XFree(text.value);
                }
            }
            XFree(children);
        }
        searched = true;
    }
    return &desktop;
}

void xwin_wait_for_wm(void) {
    Display* d = XOpenDisplay(0);

    Atom check = None;
    bool exists = false;
    struct timespec tv = { .tv_sec = 0, .tv_nsec = 50 * 1000000 };
    
    do {
        if (check == None) {
            check = XInternAtom(d, "_NET_SUPPORTING_WM_CHECK", true);
        }
        if (check) {
            int num_prop, idx;
            Atom* props = XListProperties(d, DefaultRootWindow(d), &num_prop);
            for (idx = 0; idx < num_prop; ++idx) {
                if (props[idx] == check) {
                    exists = true;
                    break;
                }
            }
            XFree(props);
        }
        if (!exists) nanosleep(&tv, NULL);
    } while (!exists);

    XCloseDisplay(d);
}

const char* xwin_detect_wm(struct gl_wcb* wcb) {
    Display* d = wcb->get_x11_display();
    Atom check = XInternAtom(d, "_NET_SUPPORTING_WM_CHECK", false);
    Atom name = XInternAtom(d, "_NET_WM_NAME", false);
    Atom type = XInternAtom(d, "UTF8_STRING", false);
    union {
        Atom a;
        int i;
        long unsigned int lui;
    } ignored;
    
    unsigned long nitems = 0;
    unsigned char* wm_name = NULL;
    Window* wm_check;
    if (Success != XGetWindowProperty(d, DefaultRootWindow(d), check, 0, 1024, false, XA_WINDOW,
                                      &ignored.a, &ignored.i, &nitems, &ignored.lui, (unsigned char**) &wm_check)) {
        return NULL;
    }
    
    if (nitems > 0 && Success == XGetWindowProperty(d, *wm_check, name, 0, 1024, false, type,
                                                    &ignored.a, &ignored.i, &nitems, &ignored.lui, &wm_name)) {
        if (nitems > 0) {
            static const char* wm_name_store = NULL;
            if (wm_name_store) XFree((unsigned char*) wm_name_store);
            wm_name_store = (const char*) wm_name;
        } else {
            XFree(wm_name);
            wm_name = NULL;
        }
    }
    
    XFree(wm_check);
    
    return (const char*) wm_name;
    
}

bool xwin_should_render(struct gl_wcb* wcb, void* impl) {
    bool ret = true, should_close = false;
    Display* d = wcb->get_x11_display();
    if (!d) {
        d = XOpenDisplay(0);
        should_close = true;
    }
    
    Atom prop       = XInternAtom(d, "_NET_ACTIVE_WINDOW", true);
    Atom fullscreen = XInternAtom(d, "_NET_WM_STATE_FULLSCREEN", true);
    
    Atom actual_type;
    int actual_format, t;
    unsigned long nitems, bytes_after;
    unsigned char* data = NULL;

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

    if (data) {
        XFree(data);
        data = NULL;
    }

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
    if (data)
        XFree(data);
    if (should_close)
        XCloseDisplay(d);
    return ret;
}

/* Create string copy on stack with upcase chars */
#define S_UPPER(in, out) char out[strlen(in) + 1];                  \
    do {                                                            \
        for (size_t t = 0; t < sizeof(out) / sizeof(char); ++t) {   \
            char c = in[t];                                         \
            switch (c) {                                            \
                case 'a' ... 'z': c -= 'a' - 'A';                   \
                default:          out[t] = c;                       \
            }                                                       \
        }                                                           \
    } while (0)

static void xwin_changeatom(struct gl_wcb* wcb, void* impl, const char* type,
                            const char* atom, const char* fmt, int mode) {
    Window w = wcb->get_x11_window(impl);
    Display* d = wcb->get_x11_display();
    Atom wtype = XInternAtom(d, atom, false);
    char buf[256];
    snprintf(buf, sizeof(buf), fmt, type);
    Atom desk = XInternAtom(d, buf, false);
    XChangeProperty(d, w, wtype, XA_ATOM, 32, mode, (unsigned char*) &desk, 1);
}

/* Set window types defined by the EWMH standard, possible values:
   -> "desktop", "dock", "toolbar", "menu", "utility", "splash", "dialog", "normal" */
bool xwin_settype(struct gl_wcb* wcb, void* impl, const char* rtype) {
    S_UPPER(rtype, type);
    if (type[0] != '!') {
        xwin_changeatom(wcb, impl, type, "_NET_WM_WINDOW_TYPE",
                        "_NET_WM_WINDOW_TYPE_%s", PropModeReplace);
    }
    return !strcmp(type, "DESKTOP");
}

void xwin_addstate(struct gl_wcb* wcb, void* impl, const char* rstate) {
    S_UPPER(rstate, state);
    if (strcmp(state, "PINNED"))
        xwin_changeatom(wcb, impl, state, "_NET_WM_STATE", "_NET_WM_STATE_%s", PropModeAppend);
    else
        xwin_setdesktop(wcb, impl, XWIN_ALL_DESKTOPS);
}

void xwin_setdesktop(struct gl_wcb* wcb, void* impl, unsigned long desktop) {
    Window w = wcb->get_x11_window(impl);
    Display* d = wcb->get_x11_display();
    Atom wtype = XInternAtom(d, "_NET_WM_DESKTOP", false);
    XChangeProperty(d, w, wtype, XA_CARDINAL, 32, PropModeReplace, (unsigned char*) &desktop, 1);
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
    
    int x, y, w, h;
    rd_get_wcb(rd)->get_fbsize(rd_get_impl_window(rd), &w, &h);
    rd_get_wcb(rd)->get_pos(rd_get_impl_window(rd), &x, &y);
    XColor c;
    Display* d = rd_get_wcb(rd)->get_x11_display();
    Drawable src = get_drawable(d, DefaultRootWindow(d));
    bool use_shm = XShmQueryExtension(d);
    
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
                          AllPlanes, ZPixmap);
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
    XFree(info);
    
    return texture;
}
