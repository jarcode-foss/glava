
bool xwin_should_render(struct renderer* rd);
void xwin_settype(struct gl_wcb* wcb, void* impl, const char* type, struct renderer*);
void xwin_addstate(struct gl_wcb* wcb, void* impl, const char* state, struct renderer*);
unsigned int xwin_copyglbg(struct renderer* rd, unsigned int texture);
