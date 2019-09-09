
#define XWIN_ALL_DESKTOPS 0xFFFFFFFF

#ifndef XWIN_H
#define XWIN_H

#include <stdbool.h>
#include "render.h"

typedef unsigned long int Window;

void xwin_assign_icon_bmp(struct gl_wcb* wcb, void* impl, const char* path);
bool xwin_should_render(struct gl_wcb* wcb, void* impl);
void xwin_wait_for_wm(void);
bool xwin_settype(struct gl_wcb* wcb, void* impl, const char* type);
void xwin_setdesktop(struct gl_wcb* wcb, void* impl, unsigned long desktop);
void xwin_addstate(struct gl_wcb* wcb, void* impl, const char* state);
unsigned int xwin_copyglbg(struct glava_renderer* rd, unsigned int texture);
Window* xwin_get_desktop_layer(struct gl_wcb* wcb);
const char* xwin_detect_wm(struct gl_wcb* wcb);

#endif
