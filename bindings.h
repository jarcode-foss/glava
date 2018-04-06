
#ifndef BINDINGS_H
#define BINDINGS_H
#ifdef GLAVA_UI

struct bindings;

struct bindings* bd_init   (struct renderer* r, const char* root, const char* entry);
void             bd_setup  (struct bindings* state);
void             bd_request(struct bindings* state, const char* request, const char** args);
void             bd_frame  (struct bindings* state);

#endif /* GLAVA_UI */
#endif /* BINDINGS_H */
