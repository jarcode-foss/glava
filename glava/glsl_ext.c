
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "glava.h"
#include "render.h"
#include "glsl_ext.h"

#define LINE_START 0
#define GLSL 1
#define MACRO 2
#define REQUEST 3
#define INCLUDE 4
#define COLOR 5
#define DEFINE 6
#define BIND 7
#define EXPAND 8

struct sbuf {
    char* buf;
    size_t at; /* index of final null character */
    size_t bsize; /* actual buffer size */
};

#define append(sbuf, str) n_append(sbuf, strlen(str), str)

static inline void expand_for(struct sbuf* sbuf, size_t len) {
    bool resize = false;
    while (len + 1 > sbuf->bsize - sbuf->at) {
        sbuf->bsize *= 2;
        resize = true;
    }
    if (resize)
        sbuf->buf = realloc(sbuf->buf, sbuf->bsize);
}

/* append 'n' bytes from 'str' to the resizable buffer */
static void n_append(struct sbuf* sbuf, size_t len, const char* str) {
    expand_for(sbuf, len);
    memcpy(sbuf->buf + sbuf->at, str, len);
    sbuf->at += len;
    sbuf->buf[sbuf->at] = '\0';
}

#define s_append(sbuf, fmt, ...) se_append(sbuf, 64, fmt, __VA_ARGS__) 

/* append the formatted string to the resizable buffer, where elen is extra space for formatted chars */
static void se_append(struct sbuf* sbuf, size_t elen, const char* fmt, ...) {
    size_t space = strlen(fmt) + elen;
    expand_for(sbuf, space);
    va_list args;
    va_start(args, fmt);
    int written;
    if ((written = vsnprintf(sbuf->buf + sbuf->at, space, fmt, args)) < 0)
        glava_abort();
    sbuf->at += written;
    va_end(args);
}

#define parse_error(line, f, fmt, ...)                                  \
    do {                                                                \
        fprintf(stderr, "[%s:%d] " fmt "\n", f, (int) line, __VA_ARGS__); \
        glava_abort();                                                  \
    } while (0)

#define parse_error_s(line, f, s)                           \
    do {                                                    \
        fprintf(stderr, "[%s:%d] " s "\n", f, (int) line);  \
        glava_abort();                                      \
    } while (0)

struct schar {
    char* buf;
    size_t sz;
};

bool ext_parse_color(const char* str, size_t elem_sz, float** results) {
    size_t t, len = strlen(str), i = 0, s = 0;
    uint8_t elem_bytes[elem_sz];
    /* Ignore '0x' prefix, if present */
    if (len >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        len -= 2;
        str += 2;
    }
    for (t = 0; t < len && t < 8; ++t) {
        char c = str[t];
        uint8_t b;
        /* obtain value from character */
        switch (c) {
            case 'a' ... 'f': b = (c - 'a') + 10; break;
            case 'A' ... 'F': b = (c - 'A') + 10; break;
            case '0' ... '9': b =  c - '0';       break;
            default: return false;
        }
        elem_bytes[s] = b;
        if (s >= elem_sz - 1) { /* advance to next element */
            uint32_t e = 0; /* component storage */
                            /* mask storage with input data */
            for (size_t v = 0; v < elem_sz; ++v) {
                e |= (uint32_t) elem_bytes[v] << (((elem_sz - 1) - v) * 4);
            }
            /* convert to [0, 1] as floating point value */
            *results[i] = (float) e / (float) ((1 << (elem_sz * 4)) - 1);
            s = 0;
            ++i;
        } else { /* advance character */
            ++s;
        }
    }
    return true;
}

static void free_after(struct glsl_ext* ext, void* ptr) {
    ++ext->destruct_sz;
    ext->destruct = realloc(ext->destruct, sizeof(void*) * ext->destruct_sz);
    ext->destruct[ext->destruct_sz - 1] = ptr;
}

static void inherit(struct glsl_ext* parent, struct glsl_ext* child) {
    free_after(parent, child->processed);
    parent->destruct = realloc(parent->destruct, sizeof(void*) * (parent->destruct_sz + child->destruct_sz));
    memcpy(parent->destruct + parent->destruct_sz, child->destruct, sizeof(void*) * child->destruct_sz);
    parent->destruct_sz += child->destruct_sz;
    free(child->destruct);
}

/* handle raw arguments for #include and #request directives */
static struct schar directive(struct glsl_ext* ext, char** args,
                              size_t args_sz, int state,
                              size_t line, const char* f) {
    switch (state) {
        case DEFINE: {
            /* Workaround for re-defining macros in GLSL. By default this is generally an error in most
               compilers/drivers, but we would prefer to override (non-function) definitions instead.
            
               Due to how this directive is parsed, the macro itself is still emitted afterwards. */
            if (args_sz == 0) {
                parse_error_s(line, f, "No arguments provided to #define directive!");
            }
            size_t bsz       = (strlen(args[0]) * 3) + 64;
            struct schar ret = { .buf = malloc(bsz) };
            int r            = snprintf(ret.buf, bsz, "#ifdef %1$s\n#undef %1$s\n#endif\n", args[0]);
            if (r < 0)
                glava_abort();
            ret.sz = r;
            free_after(ext, ret.buf);
            return ret;
        }
        case INCLUDE: {
            if (args_sz == 0) {
                parse_error_s(line, f, "No arguments provided to #include directive!");
            }
            char* target = args[0];

            /* Handle `:` config specifier */
            size_t tsz = strlen(target);
            if (tsz && target[0] == ':' && ext->cfd) {
                target = &target[1];
                ext->cd = ext->cfd;
            }
            /* Handle `@` default specifier */
            if (tsz && target[0] == '@') {
                if (!ext->dd) {
                    parse_error_s(line, f, "encountered '@' path specifier while no default "
                                "directory is available in the current context");
                }
                target = &target[1];
                ext->cd = ext->dd;
            }
        
            char path[strlen(ext->cd) + tsz + 2];
            snprintf(path, sizeof(path) / sizeof(char), "%s/%s", ext->cd, target);
        
            int fd = open(path, O_RDONLY);
            if (fd == -1)
                parse_error(line, f, "failed to load GLSL shader source "
                            "specified by #include directive '%s': %s\n",
                            path, strerror(errno));
        
            struct stat st;
            fstat(fd, &st);
        
            char* map = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
            if (!map)
                parse_error(line, f, "failed to map GLSL shader source "
                            "specified by #include directive '%s': %s\n",
                            path, strerror(errno));

            struct glsl_ext next = {
                .source      = map,
                .source_len  = st.st_size,
                .cd          = ext->cd,
                .cfd         = ext->cfd,
                .dd          = ext->dd,
                .handlers    = ext->handlers,
                .binds       = ext->binds,
                .ss_lookup   = ext->ss_lookup,
                .ss_len      = ext->ss_len,
                .efuncs      = ext->efuncs
            };

            /* recursively process */
            ext_process(&next, target);
            inherit(ext, &next);
            munmap(map, st.st_size);
            close(fd);

            ext->ss_lookup = next.ss_lookup;
            
            struct schar ret = {
                .buf = next.processed,
                .sz  = next.p_len
            };
        
            return ret;
        }
        case REQUEST: {
            if (args_sz > 0) {
                char* request = args[0];

                struct request_handler* handler;
                bool found = false;
                size_t t;
                for (t = 0; (handler = &ext->handlers[t])->name != NULL; ++t) {
                    if(!strcmp(handler->name, request)) {
                        found = true;
                        void** processed_args = malloc(strlen(handler->fmt) * sizeof(void*));
                    
                        char c;
                        size_t i;
                        for (i = 0; (c = handler->fmt[i]) != '\0'; ++i) {
                            if (args_sz <= 1 + i)
                                parse_error(line, f,
                                            "failed to execute request '%s': expected format '%s'\n",
                                            request, handler->fmt);
                            char* raw = args[1 + i];
                            switch (c) {
                                case 'i': {
                                    int v = (int) strtol(raw, NULL, 0);
                                    processed_args[i] = malloc(sizeof(int));
                                    *(int*) processed_args[i] = v;
                                    break;
                                }
                                case 'f': {
                                    float f = strtof(raw, NULL);
                                    processed_args[i] = malloc(sizeof(float));
                                    *(float*) processed_args[i] = f;
                                    break;
                                }
                                case 's': { *(char**) &processed_args[i] = raw; break; }
                                case 'b': {
                                    bool v;
                                    if (!strcmp(raw, "true")) {
                                        v = true;
                                    } else if (!strcmp(raw, "false")) {
                                        v = false;
                                    } else if (strlen(raw) == 1) {
                                        switch (raw[0]) {
                                            case 't': { v = true; break; }
                                            case 'f': { v = false; break; }
                                            case '1': { v = true; break; }
                                            case '0': { v = false; break; }
                                            default:
                                                parse_error_s(line, f, "tried to parse invalid "
                                                              "raw string into a boolean");
                                        }
                                    } else
                                        parse_error_s(line, f, "tried to parse invalid "
                                                      "raw string into a boolean");
                                    processed_args[i] = malloc(sizeof(bool));
                                    *(bool*) processed_args[i] = v;
                                    break;
                                }
                            }
                        }

                        handler->handler(request, processed_args);
                    
                        for (i = 0; (c = handler->fmt[i]) != '\0'; ++i)
                            if (c != 's')
                                free(processed_args[i]);
                        free(processed_args);
                    }
                }
                if (!found)
                    parse_error(line, f, "unknown request type '%s'", request);
            }
            goto return_empty;
        }
        case EXPAND: {
            if (args_sz >= 2) {
                char*  fmacro    = args[0];
                size_t fmacro_sz = strlen(fmacro);
                char*  arg       = args[1];
                size_t expand_n  = 0;
                bool match       = false;
                if (ext->efuncs) {
                    for (size_t t = 0; ext->efuncs[t].name != NULL; ++t) {
                        if (!strcmp(arg, ext->efuncs[t].name)) {
                            expand_n = ext->efuncs[t].call();
                            match = true;
                            break;
                        }
                    }
                }
                
                if (!match)
                    parse_error(line, f, "#expand directive specified invalid input \"%s\"", arg);

                /* (2 {paren} + 1 {semicolon} + 1 {newline} + 4 {input buf} + macro) * expand + 1 */
                size_t bsz = ((8 + fmacro_sz) * expand_n) + 1;
                struct schar ret = { .buf = malloc(bsz) };
                int r = 0;
                for (size_t t = 0; t < expand_n; ++t) {
                    int sr = snprintf(ret.buf + r, bsz - r, "%s(%d);\n", args[0], (int) t);
                    if (sr >= 0)
                        r += sr;
                    else
                        parse_error(line, f, "internal formatting error (snprintf returned %d)", sr);
                }
                ret.sz = r;
                free_after(ext, ret.buf);
                return ret;
            } else
                parse_error(line, f, "#expand directive missing arguments, "
                            "requires 2 identifiers (got %d)\n", (int) args_sz);
            goto return_empty;
        }
        return_empty:
        default: return (struct schar) { .buf = NULL, .sz  = 0 };
    }
}

/* state machine parser */
void ext_process(struct glsl_ext* ext, const char* f) {
    
    ext->destruct    = malloc(1);
    ext->destruct_sz = 0;

    if (!ext->ss_lookup) {
        ext->ss_lookup = malloc(sizeof(ext->ss_lookup[0]));
        ext->ss_len_s  = 0;
        ext->ss_len    = &ext->ss_len_s;
        ext->ss_own    = true;
    } else ext->ss_own = false;

    ext->ss_lookup = realloc(ext->ss_lookup, sizeof(ext->ss_lookup[0]) * ++(*ext->ss_len));
    int ss_cur = *ext->ss_len - 1;
    ext->ss_lookup[ss_cur] = strdup(f);
    
    struct sbuf sbuf = {
        .buf   = malloc(256),
        .at    = 0,
        .bsize = 256
    };

    size_t source_len = ext->source_len;
    size_t t;
    char at;
    int state = LINE_START;
    size_t macro_start_idx = 0, arg_start_idx = 0, cbuf_idx, bbuf_idx, b_restart;
    size_t line = 1;
    bool quoted = false, arg_start = false, b_sep = false, b_spc = false, b_pre = true;
    int b_br = 0;
    char cbuf[9];
    char bbuf[256];
    char** args = malloc(sizeof(char*));
    size_t args_sz = 0;

    bool prev_slash = false, comment = false, comment_line = false, prev_asterix = false,
        prev_escape = false, string = false, skip_color_start = false;
    
    se_append(&sbuf, 32, "#line 1 %d\n", ss_cur);
    
    for (t = 0; t <= source_len; ++t) {
        at = source_len == t ? '\0' : ext->source[t];
        if (at == '\n')
            ++line;
        switch (state) {
            case LINE_START: { /* processing start of line */
                switch (at) {
                    case '#': {
                        macro_start_idx = t;
                        state = MACRO;
                        continue;
                    }
                    case '\n':
                        if (comment && comment_line) {
                            comment = false;
                            comment_line = false;
                        }
                    case '\t':
                    case ' ':
                        goto copy;
                    default: state = GLSL;
                        /* let execution continue into next state */
                }
            }
            case GLSL: { /* copying GLSL source or unrelated preprocessor syntax */
                switch (at) {
                    case '"':
                        if (!comment && !prev_escape)
                            string = !string;
                        goto normal_char;
                    case '\\':
                        if (!comment) {
                            prev_escape  = !prev_escape;
                            prev_asterix = false;
                            prev_slash   = false;
                            goto copy;
                        } else goto normal_char;
                    case '/':
                        if (!comment) {
                            if (prev_slash) {
                                comment = true;
                                comment_line = true;
                                prev_slash = false;
                            } else prev_slash = true;
                        } else if (!comment_line) {
                            if (prev_asterix) {
                                comment = false;
                                prev_asterix = false;
                            }
                        }
                        prev_escape = false;
                        goto copy;
                    case '*':
                        if (!comment) {
                            if (prev_slash) {
                                comment = true;
                                prev_slash = false;
                            }
                        } else prev_asterix = true;
                        prev_escape = false;
                        goto copy;
                    case '#': {
                        /* handle hex color syntax */
                        if (!comment && !string && !skip_color_start) {
                            if (ext->source[t + 1] == '#') {
                                skip_color_start = true;
                                goto normal_char;
                            }
                            state = COLOR;
                            cbuf_idx = 0;
                            continue;
                        } else {
                            skip_color_start = false;
                            goto normal_char;
                        }
                    }
                    case '@': {
                        /* handle bind syntax */
                        if (!comment && !string && ext->binds != NULL) {
                            state = BIND;
                            b_sep = false;
                            b_spc = false;
                            b_pre = true;
                            b_br  = 0;
                            b_restart = 0;
                            bbuf_idx  = 0;
                            continue;
                        } else goto normal_char;
                    }
                    case '\n':
                        if (comment && comment_line) {
                            comment = false;
                            comment_line = false;
                        }
                        state = LINE_START;
                    normal_char:
                    default:
                        prev_asterix = false;
                        prev_slash   = false;
                        prev_escape  = false;
                        goto copy;
                }
            }
            case COLOR: { /* parse hex color syntax (#ffffffff -> vec4(1.0, 1.0, 1.0, 1.0)) */
                switch (at) {
                    case 'a' ... 'z':
                    case 'A' ... 'Z':
                    case '0' ... '9': {
                        cbuf[cbuf_idx] = at;
                        ++cbuf_idx;
                        if (cbuf_idx >= 8)
                            goto emit_color;
                        else continue;
                    }
                    emit_color:
                    default:
                        cbuf[cbuf_idx] = '\0'; /* null terminate */
                        float r = 0.0F, g = 0.0F, b = 0.0F, a = 1.0F;
                        if (ext_parse_color(cbuf, 2, (float*[]) { &r, &g, &b, &a })) {
                            se_append(&sbuf, 64, " vec4(%.6f, %.6f, %.6f, %.6f) ", r, g, b, a);
                        } else {
                            parse_error(line, f, "Invalid color format '#%s' while "
                                        "parsing GLSL color syntax extension", cbuf);
                        }
                        state = at == '\n' ? LINE_START : GLSL;
                        if (cbuf_idx >= 8)
                            continue;
                        else goto copy; /* copy character if it ended the sequence */
                }
            }
            case BIND: { /* parse bind syntax (@name:default -> __IN_name | default)*/
                switch (at) {
                    default:
                        if (b_br > 0) goto handle_bind; /* store characters in braces */
                        else          goto emit_bind;   /* emit on unexpected char outside braces */
                    case '(':
                        if (b_sep && !b_spc) {
                            ++b_br; goto handle_bind; /* inc. brace level */
                        } else      goto emit_bind;   /* emit if wrong context: `@sym(`, `@(` (no ':') */
                    case ')':
                        /* start emitting on unexpected ')': `@sym:v)`, `@s)` */
                        if (b_br <= 0 || !b_sep) goto emit_bind;
                        else {
                            --b_br;
                            if (b_br <= 0) b_spc = true;
                            goto handle_bind; /* dec. brace level */
                        }
                    case ' ': if (b_br <= 0) b_spc = true;   /* flag a non-braced space */
                    case '#': case '+': case '-':
                    case '!': case '~': case '&':
                        if (b_sep && (b_br > 0 || b_pre))
                            goto handle_bind; /* handle precede syntax only for defaults */
                        else goto emit_bind;  /* if encountered, skip to emit */
                    case ':':
                        if (!b_sep) b_restart = t;
                        b_sep = true;
                    handle_bind: /* use character for binding syntax */
                    case 'a' ... 'z':
                    case 'A' ... 'Z':
                    case '0' ... '9':
                    case '_': {
                        if (b_spc && at != ')')
                            goto emit_bind; /* skip non-braced characters after space: `@sym:vec4 c` */
                        if (b_sep && at != ':')
                            b_pre = false;
                        bbuf[bbuf_idx] = at;
                        ++bbuf_idx;
                        if (bbuf_idx >= sizeof(bbuf) - 1)
                            goto emit_bind; /* start emitting if buffer was filled */
                        else continue;
                    }
                    emit_bind: /* end binding syntax with current char */
                    case '\n':
                    case '\0': {
                        const char* parsed_name    = NULL;
                        const char* parsed_default = NULL;
                        bbuf[bbuf_idx] = '\0'; /* null terminate */
                        int sep = -1;
                        for (size_t p = 0; p < bbuf_idx; ++p)
                            if (bbuf[p] == ':') sep = p;
                        if (sep >= 0) {
                            parsed_default = bbuf + sep + 1;
                            bbuf[sep] = '\0';
                        }
                        parsed_name = bbuf;
                        bool m = false;
                        for (struct rd_bind* bd = ext->binds; bd->name != NULL; ++bd) {
                            if (!strcmp(parsed_name, bd->name)) {
                                se_append(&sbuf, 128, " _IN_%s ", parsed_name);
                                m = true;
                                break;
                            }
                        }
                        if (!m) {
                            if (parsed_default && b_restart > 0) {
                                /* To emit the default, we push back the cursor to where it starts
                                   and simply resume parsing from a normal context. */
                                t = b_restart;
                            } else parse_error(line, f,
                                               "Unexpected `--pipe` binding name '@%s' while parsing GLSL."
                                               " Try assigning a default or binding the value.", parsed_name);
                        }
                        state = GLSL;
                        continue;
                    }
                }
            }
                /* emit contents from start of macro to current index and resume regular parsing*/
                #define skip_macro()                                    \
                    do {                                                \
                        n_append(&sbuf, t - macro_start_idx, &ext->source[macro_start_idx]); \
                        state = at == '\n' ? LINE_START : GLSL;         \
                        goto copy;                                      \
                    } while (0)
            case MACRO: { /* processing start of macro */
                switch (at) {
                    case '\n':
                    case ' ':
                    case '\t':
                    case '\0': { /* end parsing directive */
                        
                        #define DIRECTIVE_CMP(lower, upper)             \
                            (!strncmp("#" lower, &ext->source[macro_start_idx], t - macro_start_idx) \
                                || !strncmp("#" upper, &ext->source[macro_start_idx], t - macro_start_idx))
                        #define DIRECTIVE_CASE(lower, upper)            \
                            ({ if (state == MACRO && DIRECTIVE_CMP(#lower, #upper)) \
                               { state = upper; goto prepare_arg_parse; } })
                        
                        DIRECTIVE_CASE(request, REQUEST);
                        DIRECTIVE_CASE(include, INCLUDE);
                        DIRECTIVE_CASE(define,  DEFINE);
                        DIRECTIVE_CASE(expand,  EXPAND);
                        
                        /* no match */
                        if (state == MACRO) skip_macro();
                        #undef DIRECTIVE_CMP
                        #undef DIRECTIVE_CASE
                    prepare_arg_parse:
                        {
                            arg_start_idx = t + 1;
                            arg_start = true;
                            args_sz = 0;
                            *args = NULL;
                        }
                    }
                    case '0' ... '9':
                        /* digits at the start of an identifier are not legal */
                        if (macro_start_idx == t - 1)
                            goto macro_parse_error;
                    case 'a' ... 'z':
                    case 'A' ... 'Z':
                        continue;
                    default:
                    macro_parse_error:
                        /* invalid char, malformed! */
                        parse_error(line, f, "Unexpected character '%c' while parsing GLSL directive", at);
                }
            }
                /* scope-violating macro to copy the result of the currently parsed argument */
                #define copy_arg(end)                                   \
                    do { if (end - arg_start_idx > 0) {                 \
                            ++args_sz;                                  \
                            args = realloc(args, sizeof(char*) * args_sz); \
                            args[args_sz - 1] = malloc((end - arg_start_idx) + 1); \
                            memcpy(args[args_sz - 1], &ext->source[arg_start_idx], end - arg_start_idx); \
                            args[args_sz - 1][end - arg_start_idx] = '\0'; \
                        } } while (0)
            case REQUEST:
            case INCLUDE:
            case DEFINE:
            case EXPAND: {
                switch (at) {
                    case ' ':
                    case '\t':
                    case '\n':
                    case '\0':
                        if (!quoted) {
                            /* end arg */
                            copy_arg(t);
                            arg_start = true;
                            arg_start_idx = t + 1;
                        } else arg_start = false;
                        
                        if (at == '\n' || at == '\0' || state == DEFINE) {
                            /* end directive */
                            size_t a;
                            struct schar r = directive(ext, args, args_sz, state, line, f);
                            for (a = 0; a < args_sz; ++a) {
                                free(args[a]);
                            }
                            args_sz = 0;
                            /* if something was returned (ie. included file), paste the results */
                            if (r.buf) {
                                n_append(&sbuf, r.sz, r.buf);
                                append(&sbuf, "\n");
                                se_append(&sbuf, 48, "#line %d %d\n", line, ss_cur);
                            }
                            if (state == DEFINE) skip_macro();
                            else state = LINE_START;
                        }
                        break;
                    case '(':
                        if (state != DEFINE || args_sz != 0) goto arg; /* only handle first arg of #define */
                        skip_macro();                                  /* ignore macro functions */
                    case '"':
                        if (state == DEFINE) goto arg; /* do not handle quoting for #define */
                        if (quoted) {
                            /* end arg */
                            copy_arg(t);
                            quoted = false;
                            arg_start = true;
                            arg_start_idx = t + 1;
                        } else if (arg_start) {
                            ++arg_start_idx;
                            quoted = true;
                        } else arg_start = false;
                        break;
                    default: {
                        arg: arg_start = false;
                    }
                }
                
                continue;
            }
                #undef copy_arg
        }
    copy:
        if (at != '\0')
            n_append(&sbuf, 1, &at);
    }
    ext->processed = sbuf.buf;
    ext->p_len = sbuf.at;
    
    if (args) {
        for (t = 0; t < args_sz; ++t) {
            free(args[t]);
        }
        free(args);
    }
}

void ext_free(struct glsl_ext* ext) {
    size_t t;
    free(ext->processed);
    if (ext->ss_own) {
        for (t = 0; t < ext->ss_len_s; ++t)
            free(ext->ss_lookup[t]);
        free(ext->ss_lookup);
    }
    for (t = 0; t < ext->destruct_sz; ++t)
        free(ext->destruct[t]);
    free(ext->destruct);
}
