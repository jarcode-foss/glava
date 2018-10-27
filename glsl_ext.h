
#define RHANDLER(name, args, ...)                            \
    ({ void _handler(const char* name, void** args) __VA_ARGS__ _handler; })

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
    void (*handler)(const char* name, void** args);
};

struct glsl_ext {
    char*        processed;   /* OUT: null terminated processed source                       */
    size_t       p_len;       /* OUT: length of processed buffer, excluding null char        */
    const char*  source;      /* IN: raw data passed via ext_process                         */
    size_t source_len;        /* IN: raw source len */
    const char*  cd;          /* IN: current directory                                       */
    const char*  cfd;         /* IN: config directory, if NULL it is assumed to cd           */
    const char*  dd;          /* IN: default directory */
    void** destruct;          /* internal */
    size_t       destruct_sz; /* internal */

    /* IN: NULL (where the last element's 'name' member is NULL) terminated
       array of request handlers */
    struct request_handler* handlers;
};

void ext_process(struct glsl_ext* ext, const char* f);
void ext_free   (struct glsl_ext* ext);
bool ext_parse_color(const char* hex, size_t elem_sz, float** results);
