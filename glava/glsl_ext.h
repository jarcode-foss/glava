#ifndef GLSL_EXT_H
#define GLSL_EXT_H

#include <stdlib.h>
#include <stdbool.h>

struct request_handler {
    const char* name;
    /*
      handler format:
      'i' - signed integer (void* -> int*)
      'f' - float (void* -> float*)
      's' - string (void* -> const char*)
      'b' - bool (void* -> bool*)
      
      example:

      .fmt = "sii" // takes a string, and then two integers
      .fmt = "ffb" // takes two floats, then a boolean
     */
    const char* fmt;
    #if defined(__clang__)
    void (^handler)(const char* name, void** args);
    #elif defined(__GNUC__) || defined(__GNUG__)
    void (*handler)(const char* name, void** args);
    #else
    #error "no nested function/block syntax available"
    #endif
};

struct glsl_ext_efunc {
    char* name;
    #if defined(__clang__)
    size_t (^call)(void);
    #elif defined(__GNUC__) || defined(__GNUG__)
    size_t (*call)(void);
    #else
    #error "no nested function/block syntax available"
    #endif
};

struct glsl_ext {
    char*        processed;        /* OUT: null terminated processed source                */
    size_t       p_len;            /* OUT: length of processed buffer, excluding null char */
    const char*  source;           /* IN: raw data passed via ext_process                  */
    size_t       source_len;       /* IN: raw source len                                   */
    const char*  cd;               /* IN: current directory                                */
    const char*  cfd;              /* IN: config directory, if NULL it is assumed to cd    */
    const char*  dd;               /* IN: default directory                                */
    struct rd_bind* binds;         /* OPT IN: --pipe binds                                 */
    struct glsl_ext_efunc* efuncs; /* OPT IN: `#expand` binds                              */
    void**       destruct;         /* internal                                             */
    size_t       destruct_sz;      /* internal                                             */
    char**       ss_lookup;        /* source-string lookup table                           */
    size_t*      ss_len;
    size_t       ss_len_s;
    bool         ss_own;

    /* IN: NULL (where the last element's 'name' member is NULL) terminated
       array of request handlers */
    struct request_handler* handlers;
};

void ext_process(struct glsl_ext* ext, const char* f);
void ext_free   (struct glsl_ext* ext);
bool ext_parse_color(const char* hex, size_t elem_sz, float** results);

#endif
