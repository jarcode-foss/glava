/* X11 specific code and features */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "render.h"
#include "xwin.h"

bool xwin_should_render(void) {
    bool ret = true;
    Display* d = XOpenDisplay(0);

    Atom prop       = XInternAtom(d, "_NET_ACTIVE_WINDOW", true);
    Atom fullscreen = XInternAtom(d, "_NET_WM_STATE_FULLSCREEN", true);
    
    Atom actual_type;
    int actual_format, t;
    unsigned long nitems, bytes_after;
    unsigned char* data;

    int handler(Display* d, XErrorEvent* e) {}
    
    XSetErrorHandler(handler); /* dummy error handler */
          
    if (Success != XGetWindowProperty(d, RootWindow(d, 0), prop, 0, 1, false, AnyPropertyType,
                                     &actual_type, &actual_format, &nitems, &bytes_after, &data)) {
        goto close; /* if an error occurs here, the WM probably isn't EWMH compliant */
    }

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
    XCloseDisplay(d);
    return ret;
}

/* Set window types defined by the EWMH standard, possible values:
   -> "desktop", "dock", "toolbar", "menu", "utility", "splash", "dialog", "normal" */
void xwin_settype(struct renderer* rd, const char* type) {
    Window w = glfwGetX11Window((GLFWwindow*) rd_get_impl_window(rd));
    Display* d = XOpenDisplay(0);
    Atom wtype = XInternAtom(d, "_NET_WM_WINDOW_TYPE", false);
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
    snprintf(buf, sizeof(buf), "_NET_WM_WINDOW_TYPE_%s", formatted);
    Atom desk = XInternAtom(d, buf, false);
    XChangeProperty(d, w, wtype, XA_ATOM, 32, PropModeReplace, (unsigned char*) &desk, 1);
    XCloseDisplay(d);
}
