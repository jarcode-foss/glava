#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "xwincheck.h"

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
