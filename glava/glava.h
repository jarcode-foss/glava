#ifndef _GLAVA_H
#define _GLAVA_H

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#define GLAVA_REQ_NONE   0
#define GLAVA_REQ_RESIZE 1

struct gl_data;
struct glava_renderer;

/* External API */

typedef struct glava_renderer* volatile glava_handle;
__attribute__((noreturn, visibility("default"))) extern void (*glava_abort)            (void);
__attribute__((noreturn, visibility("default"))) extern void (*glava_return)           (void);
__attribute__((visibility("default")))           void glava_assign_external_ctx (void* ctx);
__attribute__((visibility("default")))           void glava_entry               (int argc, char** argv, glava_handle* ret);
__attribute__((visibility("default")))           void glava_terminate           (glava_handle* ref);
__attribute__((visibility("default")))           void glava_reload              (glava_handle* ref);
__attribute__((visibility("default")))           void glava_sizereq             (glava_handle r, int x, int y, int w, int h);
__attribute__((visibility("default")))           void glava_wait                (glava_handle* ref);
__attribute__((visibility("default")))   unsigned int glava_tex                 (glava_handle r);

#endif /* _GLAVA_H */
