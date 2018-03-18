
bool xwin_should_render(struct renderer* rd);
bool xwin_settype(struct gl_wcb* wcb, void* impl, const char* type);
void xwin_addstate(struct gl_wcb* wcb, void* impl, const char* state);
unsigned int xwin_copyglbg(struct renderer* rd, unsigned int texture);
