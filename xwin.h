
#define XWIN_ALL_DESKTOPS 0xFFFFFFFF

bool xwin_should_render(struct renderer* rd);
void xwin_wait_for_wm(void);
bool xwin_settype(struct gl_wcb* wcb, void* impl, const char* type);
void xwin_setdesktop(struct gl_wcb* wcb, void* impl, unsigned long desktop);
void xwin_addstate(struct gl_wcb* wcb, void* impl, const char* state);
unsigned int xwin_copyglbg(struct renderer* rd, unsigned int texture);
