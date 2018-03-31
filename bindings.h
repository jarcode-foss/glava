
#ifndef BINDINGS_H
#define BINDINGS_H
#ifdef GLAVA_UI

struct bindings;

struct bindings* bd_init (struct renderer* r, const char* root, const char* entry);
void             bd_frame(struct bindings* state);

#endif /* GLAVA_UI */
#endif /* BINDINGS_H */
