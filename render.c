#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <math.h>
#include <time.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "glad.h"

#include "render.h"
#include "xwin.h"
#include "glsl_ext.h"

#define TWOPI 6.28318530718
#define PI 3.14159265359
#define swap(a, b) do { __auto_type tmp = a; a = b; b = tmp; } while (0)

#define IB_START_LEFT 0
#define IB_END_LEFT 1
#define IB_START_RIGHT 2
#define IB_END_RIGHT 3
#define IB_WORK_LEFT 4
#define IB_WORK_RIGHT 5

/* Only a single vertex shader is needed for GLava, since all rendering is done in the fragment shader
   over a fullscreen quad */
#define VERTEX_SHADER_SRC                                               \
    "layout(location = 0) in vec3 pos; void main() { gl_Position = vec4(pos.x, pos.y, 0.0F, 1.0F); }"

struct gl_wcb* wcbs[2] = {};
static size_t wcbs_idx = 0;

static inline void register_wcb(struct gl_wcb* wcb) { wcbs[wcbs_idx++] = wcb; }

#define DECL_WCB(N)                             \
    do {                                        \
        extern struct gl_wcb N;                 \
        register_wcb(&N);                       \
    } while (0)

/* GLSL bind source */
 
struct gl_bind_src {
    const char* name;
    int type;
    int src_type;
};

/* function that can be applied to uniform binds */

struct gl_transform {
    const char* name;
    int type;
    void (*apply)(struct gl_data*, void**, void* data);
};

/* data for sampler1D */

struct gl_sampler_data {
    float* buf;
    size_t sz;
};

/* per-bind data containing the framebuffer and 1D texture to render for smoothing. */

struct sm_fb {
    GLuint fbo, tex;
};

/* GLSL uniform bind */

struct gl_bind {
    const char* name;
    GLuint uniform;
    int type;
    int src_type;
    void (**transformations)(struct gl_data*, void**, void* data);
    size_t t_sz;
    struct sm_fb sm;
};

/* GL screen framebuffer object */

struct gl_sfbo {
    GLuint fbo, tex, shader;
    bool valid, nativeonly;
    const char* name;
    struct gl_bind* binds;
    size_t binds_sz;
};

/* data for screen-space overlay (quad) */

struct overlay_data {
    GLuint vbuf, vao;
};

struct gl_data {
    struct gl_sfbo* stages;
    struct overlay_data overlay;
    GLuint audio_tex_r, audio_tex_l, bg_tex, sm_prog;
    size_t stages_sz, bufscale, avg_frames;
    void* w;
    struct gl_wcb* wcb;
    int lww, lwh, lwx, lwy; /* last window dimensions */
    int rate; /* framerate */
    double tcounter;
    int fcounter, ucounter, kcounter;
    bool print_fps, avg_window, interpolate, force_geometry, force_raised,
        copy_desktop, smooth_pass, premultiply_alpha, check_fullscreen,
        clickthrough;
    void** t_data;
    size_t t_count;
    float gravity_step, target_spu, fr, ur, smooth_distance, smooth_ratio,
        smooth_factor, fft_scale, fft_cutoff;
    struct {
        float r, g, b, a;
    } clear_color;
    float* interpolate_buf[6];
    int geometry[4];
};



/* load shader file */
static GLuint shaderload(const char*             rpath,
                         GLenum                  type,
                         const char*             shader,
                         const char*             config,
                         const char*             defaults,
                         struct request_handler* handlers,
                         int                     shader_version,
                         bool                    raw,
                         struct gl_data*         gl) {

    size_t s_len = strlen(shader);

    /* Path buffer, used for output and */
    char path[raw ? 2 : strlen(rpath) + s_len + 2];
    if (raw) {
        path[0] = '*';
        path[1] = '\0';
    }
    struct stat st;
    int fd = -1;
    
    if (!raw) {
        snprintf(path, sizeof(path) / sizeof(char), "%s/%s", shader, rpath);
    
        fd = open(path, O_RDONLY);
        if (fd == -1) {
            fprintf(stderr, "failed to load shader '%s': %s\n", path, strerror(errno));
            return 0;
        }
        fstat(fd, &st);
    }

    /* open and create a copy with prepended header */
    GLint max_uniforms;
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &max_uniforms);
    
    const GLchar* map = raw ? shader : mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    
    static const GLchar* header_fmt =
        "#version %d\n"
        "#define UNIFORM_LIMIT %d\n"
        "#define PRE_SMOOTHED_AUDIO %d\n"
        "#define SMOOTH_FACTOR %.6f\n"
        "#define USE_ALPHA %d\n"
        "#define PREMULTIPLY_ALPHA %d\n";
    
    struct glsl_ext ext = {
        .source     = raw ? NULL : map,
        .source_len = raw ? 0 : st.st_size,
        .cd         = shader,
        .cfd        = config,
        .dd         = defaults,
        .handlers   = handlers,
        .processed  = (char*) (raw ? shader : NULL),
        .p_len      = raw ? s_len : 0
    };

    /* If this is raw input, skip processing */
    if (!raw) ext_process(&ext, rpath);
    
    size_t blen = strlen(header_fmt) + 64;
    GLchar* buf = malloc((blen * sizeof(GLchar*)) + ext.p_len);
    int written = snprintf(buf, blen, header_fmt, (int) shader_version, (int) max_uniforms,
                           gl->smooth_pass ? 1 : 0, (double) gl->smooth_factor,
                           1, gl->premultiply_alpha ? 1 : 0);
    if (written < 0) {
        fprintf(stderr, "snprintf() encoding error while prepending header to shader '%s'\n", path);
        return 0;
    }
    memcpy(buf + written, ext.processed, ext.p_len);
    if (!raw) munmap((void*) map, st.st_size);
    
    GLuint s = glCreateShader(type);
    GLint sl = (GLint) (ext.p_len + written);
    glShaderSource(s, 1, (const GLchar* const*) &buf, &sl);
    switch (glGetError()) {
        case GL_INVALID_VALUE:
        case GL_INVALID_OPERATION:
            fprintf(stderr, "invalid operation while loading shader source\n");
            return 0;
    }
    glCompileShader(s);
    GLint ret, ilen;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ret);
    if (ret == GL_FALSE) {
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &ilen);
        if (ilen) {
            GLchar ebuf[ilen];
            glGetShaderInfoLog(s, ilen, NULL, ebuf);
            fprintf(stderr, "Shader compilation failed for '%s':\n", path);
            fwrite(ebuf, sizeof(GLchar), ilen - 1, stderr);
            #ifdef GLAVA_DEBUG
            fprintf(stderr, "Processed shader source for '%s':\n", path);
            fwrite(buf, sizeof(GLchar), sl, stderr);
            #endif
            return 0;
        } else {
            fprintf(stderr, "Shader compilation failed for '%s', but no info was available\n", path);
            return 0;
        }
    }
    
    if (!raw) ext_free(&ext);
    free(buf);
    close(fd);
    return s;
}

/* link shaders */
#define shaderlink(...) shaderlink_f((GLuint[]) {__VA_ARGS__, 0})
static GLuint shaderlink_f(GLuint* arr) {
    GLuint f, p;
    int i = 0;

    if ((p = glCreateProgram()) == 0) {
        fprintf(stderr, "failed to create program\n");
        abort();
    }
    
    while ((f = arr[i++]) != 0) {
        glAttachShader(p, f);
        switch (glGetError()) {
            case GL_INVALID_VALUE:
                fprintf(stderr, "tried to pass invalid value to glAttachShader\n");
                return 0;
            case GL_INVALID_OPERATION:
                fprintf(stderr, "shader is already attached, or argument types "
                        "were invalid when calling glAttachShader\n");
                return 0;
        }
    }
    glLinkProgram(p);
    GLint ret, ilen;
    glGetProgramiv(p, GL_LINK_STATUS, &ret);
    if (ret == GL_FALSE) {
        glGetProgramiv(p, GL_INFO_LOG_LENGTH, &ilen);
        if (ilen) {
            GLchar buf[ilen];
            glGetProgramInfoLog(p, ilen, NULL, buf);
            fprintf(stderr, "Shader linking failed for program %d:\n", (int) p);
            fwrite(buf, sizeof(GLchar), ilen - 1, stderr);
            return 0;
        } else {
            fprintf(stderr, "Shader linking failed for program %d, but no info was available\n", (int) p);
            return 0;
        }
    }
    return p;
}

/* load shaders */
#define shaderbuild(gl, shader_path, c, d, r, v, ...)                    \
    shaderbuild_f(gl, shader_path, c, d, r, v, (const char*[]) {__VA_ARGS__, 0})
static GLuint shaderbuild_f(struct gl_data* gl,
                            const char* shader_path,
                            const char* config, const char* defaults,
                            struct request_handler* handlers,
                            int shader_version,
                            const char** arr) {
    const char* str;
    int i = 0, sz = 0, t;
    while ((str = arr[i++]) != NULL) ++sz;
    GLuint shaders[sz + 2];
    shaders[sz + 1] = 0;
    for (i = 0; i < sz; ++i) {
        const char* path = arr[i];
        size_t len = strlen(path);
        for (t = len - 2; t >= 0; --t) {
            if (path[t] == '.') {
                if (!strcmp(path + t + 1, "frag") || !strcmp(path + t + 1, "glsl")) {
                    if (!(shaders[i] = shaderload(path, GL_FRAGMENT_SHADER,
                                                  shader_path, config, defaults, handlers,
                                                  shader_version, false, gl))) {
                        return 0;
                    }
                } else if (!strcmp(path + t + 1, "vert")) {
                    /*
                      if (!(shaders[i] = shaderload(path, GL_VERTEX_SHADER, shader_path))) {
                      return 0;
                      }
                    */
                    fprintf(stderr, "shaderbuild(): vertex shaders not allowed: %s\n", path);
                    abort();
                } else {
                    fprintf(stderr, "shaderbuild(): invalid file extension: %s\n", path);
                    abort();
                }
                break;
            }
        }
    }
    /* load builtin vertex shader */
    shaders[sz] = shaderload(NULL, GL_VERTEX_SHADER, VERTEX_SHADER_SRC, NULL, NULL, handlers, shader_version, true, gl);
    fflush(stdout);
    return shaderlink_f(shaders);
}

static GLuint create_1d_tex() {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_1D, tex);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    return tex;
}

static void update_1d_tex(GLuint tex, size_t w, float* data) {
    glBindTexture(GL_TEXTURE_1D, tex);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_R16, w, 0, GL_RED, GL_FLOAT, data);
}

#define BIND_VEC2 0
#define BIND_VEC3 1
#define BIND_VEC4 2
#define BIND_IVEC2 3
#define BIND_IVEC3 4
#define BIND_IVEC4 5
#define BIND_INT 6
#define BIND_FLOAT 7
#define BIND_SAMPLER1D 8
#define BIND_SAMPLER2D 9

/* setup screen framebuffer object and its texture */

static void setup_sfbo(struct gl_sfbo* s, int w, int h) {
    GLuint tex = s->valid ? s->tex : ({ glGenTextures(1, &s->tex); s->tex; });
    GLuint fbo = s->valid ? s->fbo : ({ glGenFramebuffers(1, &s->fbo); s->fbo; });
    s->valid = true;
    /* bind texture and setup space */
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
    
    /* setup and bind framebuffer to texture */
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    switch (glCheckFramebufferStatus(GL_FRAMEBUFFER)) {
        case GL_FRAMEBUFFER_COMPLETE: break;
        default:
            fprintf(stderr, "error in frambuffer state\n");
            abort();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void overlay(struct overlay_data* d) {
    GLfloat buf[18];
    buf[0] = -1.0f; buf[1] = -1.0f; buf[2] = 0.0f;
    buf[3] =  1.0f; buf[4] = -1.0f; buf[5] = 0.0f;
    buf[6] = -1.0f; buf[7] =  1.0f; buf[8] = 0.0f;
    
    buf[9]  =  1.0f; buf[10] =  1.0f; buf[11] = 0.0f;
    buf[12] =  1.0f; buf[13] = -1.0f; buf[14] = 0.0f;
    buf[15] = -1.0f; buf[16] =  1.0f; buf[17] = 0.0f;
    
    glGenBuffers(1, &d->vbuf);
    glBindBuffer(GL_ARRAY_BUFFER, d->vbuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 18, buf, GL_STATIC_DRAW);
    
    glGenVertexArrays(1, &d->vao);
    glBindVertexArray(d->vao);
    
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, d->vbuf);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*) 0);
    glDisableVertexAttribArray(0);

    glBindVertexArray(0);
}

static void drawoverlay(const struct overlay_data* d) {
    glBindVertexArray(d->vao);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0);
    glBindVertexArray(0);
}

#define TRANSFORM_NONE 0
#define TRANSFORM_FFT 1
#define TRANSFORM_WINDOW 2

#ifdef GLAD_DEBUG

struct err_msg {
    GLenum code;
    const char* msg;
    const char* cname;
};

#define CODE(c) .code = c, .cname = #c

static const struct err_msg err_lookup[] = {
    { CODE(GL_INVALID_ENUM), .msg = "Invalid enum parameter" },
    { CODE(GL_INVALID_VALUE), .msg = "Invalid value parameter" },
    { CODE(GL_INVALID_OPERATION), .msg = "Invalid operation" },
    { CODE(GL_STACK_OVERFLOW), .msg = "Stack overflow" },
    { CODE(GL_STACK_UNDERFLOW), .msg = "Stack underflow" },
    { CODE(GL_OUT_OF_MEMORY), .msg = "Out of memory" },
    { CODE(GL_INVALID_FRAMEBUFFER_OPERATION), .msg = "Out of memory" },
    #ifdef GL_CONTEXT_LOSS
    { CODE(GL_CONTEXT_LOSS), .msg = "Context loss (graphics device or driver reset?)" }
    #endif
};

#undef CODE

static void glad_debugcb(const char* name, void *funcptr, int len_args, ...) {
    GLenum err = glad_glGetError();

    if (err != GL_NO_ERROR) {
        const char* cname = "?", * msg = "Unknown error code";
        size_t t;
        for (t = 0; t < sizeof(err_lookup) / sizeof(struct err_msg); ++t) {
            if (err_lookup[t].code == err) {
                cname = err_lookup[t].cname;
                msg   = err_lookup[t].msg;
                break;
            }
        }
        fprintf(stderr, "glGetError(): %d (%s) in %s: '%s'\n",
                (int) err, cname, name, msg);
        abort();
    }
}
#endif

#define SHADER_EXT_VERT "vert"
#define SHADER_EXT_FRAG "frag"
    
static struct gl_bind_src bind_sources[] = {
    #define SRC_PREV 0
    { .name = "prev", .type = BIND_SAMPLER2D, .src_type = SRC_PREV },
    #define SRC_AUDIO_L 1
    { .name = "audio_l", .type = BIND_SAMPLER1D, .src_type = SRC_AUDIO_L },
    #define SRC_AUDIO_R 2
    { .name = "audio_r", .type = BIND_SAMPLER1D, .src_type = SRC_AUDIO_R },
    #define SRC_AUDIO_SZ 3
    { .name = "audio_sz", .type = BIND_INT, .src_type = SRC_AUDIO_SZ },
    #define SRC_SCREEN 4
    { .name = "screen", .type = BIND_IVEC2, .src_type = SRC_SCREEN }
};

#define window(t, sz) (0.53836 - (0.46164 * cos(TWOPI * (double) t  / (double)(sz - 1))))
#define ALLOC_ONCE(u, udata, sz)                \
    if (*udata == NULL) {                       \
        u = calloc(sz, sizeof(typeof(*u)));     \
        *udata = u;                             \
    } else u = (typeof(u)) *udata;

/* type generic clamp/min/max, like in GLSL */

#define clamp(v, min, max)                      \
    ({                                          \
        __auto_type _v = v;                     \
        if (_v < min) _v = min;                 \
        else if (_v > max) _v = max;            \
        _v;                                     \
    })

#define min(a0, b0)                             \
    ({                                          \
        __auto_type _a = a0;                    \
        __auto_type _b = b0;                    \
        _a < _b ? _a : _b;                      \
    })

#define max(a0, b0)                             \
    ({                                          \
        __auto_type _a = a0;                    \
        __auto_type _b = b0;                    \
        _a > _b ? _a : _b;                      \
    })

#define E 2.7182818284590452353

void transform_smooth(struct gl_data* d, void** udaa, void* data) {
    struct gl_sampler_data* s = (struct gl_sampler_data*) data;
    float* b = s->buf;
    size_t
        sz  = s->sz,
        asz = (size_t) ceil(s->sz / d->smooth_ratio);
    for (int t = 0; t < asz; ++t) {
        float
            db  = log(t), /* buffer index on log scale */
            avg = 0;      /* adj value averages (weighted) */
        /* Calculate real indexes for sampling at this position, since the
           distance is specified in scalar values */
        int smin = (int) floor(powf(E, max(db - d->smooth_distance, 0)));
        int smax = min((int) ceil(powf(E, db + d->smooth_distance)), sz - 1);
        int count = 0;
        for (int s = smin; s <= smax; ++s) {
            if (b[s]) {
                avg += b[s] /* / abs(powf(10, db + (t - s))) */;
                count++;
            }
        }
        avg /= count;
        b[t] = avg;
    }
}

void transform_gravity(struct gl_data* d, void** udata, void* data) {
    struct gl_sampler_data* s = (struct gl_sampler_data*) data;
    float* b = s->buf;
    size_t sz = s->sz, t;
    
    float* applied;
    ALLOC_ONCE(applied, udata, sz);

    float g = d->gravity_step * (1.0F / d->ur);
    
    for (t = 0; t < sz; ++t) {
        if (b[t] >= applied[t]) {
            applied[t] = b[t] - g;
        } else applied[t] -= g;
        b[t] = applied[t];
    }
}

void transform_average(struct gl_data* d, void** udata, void* data) {
    
    struct gl_sampler_data* s = (struct gl_sampler_data*) data;
    float* b = s->buf;
    size_t sz = s->sz, t, f;
    size_t tsz = sz * d->avg_frames;
    float v;
    bool use_window = d->avg_window;
    
    float* bufs;
    ALLOC_ONCE(bufs, udata, tsz);
    
    memmove(bufs, &bufs[sz], (tsz - sz) * sizeof(float));
    memcpy(&bufs[tsz - sz], b, sz * sizeof(float));
    
    #define DO_AVG(w)                                   \
        do {                                            \
            for (t = 0; t < sz; ++t) {                  \
                v = 0.0F;                               \
                for (f = 0; f < d->avg_frames; ++f) {   \
                    v += w * bufs[(f * sz) + t];        \
                }                                       \
                b[t] = v / d->avg_frames;               \
            }                                           \
        } while (0)
    
    if (use_window)
        DO_AVG(window(f, d->avg_frames));
    else
        DO_AVG(1);

    #undef DO_AVG
}

void transform_wrange(struct gl_data* d, void** _, void* data) {
    struct gl_sampler_data* s = (struct gl_sampler_data*) data;
    float* b = s->buf;
    size_t sz = s->sz, t;
    for (t = 0; t < sz; ++t) {
        b[t] += 1.0F;
        b[t] /= 2.0F;
    }
}

void transform_window(struct gl_data* d, void** _, void* data) {
    struct gl_sampler_data* s = (struct gl_sampler_data*) data;
    float* b = s->buf;
    size_t sz = s->sz, t;
    
    for (t = 0; t < sz; ++t) {
        b[t] *= window(t, sz);
    }
}

void transform_fft(struct gl_data* d, void** _, void* in) {
    struct gl_sampler_data* s = (struct gl_sampler_data*) in;
    float* data = s->buf;
    unsigned long nn = (unsigned long) (s->sz / 2);
    
    unsigned long n, mmax, m, j, istep, i;
    float wtemp, wr, wpr, wpi, wi, theta;
    float tempr, tempi;
 
    // reverse-binary reindexing
    n = nn<<1;
    j=1;
    for (i=1; i<n; i+=2) {
        if (j>i) {
            swap(data[j-1], data[i-1]);
            swap(data[j], data[i]);
        }
        m = nn;
        while (m>=2 && j>m) {
            j -= m;
            m >>= 1;
        }
        j += m;
    };
 
    // here begins the Danielson-Lanczos section
    mmax=2;
    while (n>mmax) {
        istep = mmax<<1;
        theta = -(2*M_PI/mmax);
        wtemp = sin(0.5*theta);
        wpr = -2.0*wtemp*wtemp;
        wpi = sin(theta);
        wr = 1.0;
        wi = 0.0;
        for (m=1; m < mmax; m += 2) {
            for (i=m; i <= n; i += istep) {
                j=i+mmax;
                tempr = wr*data[j-1] - wi*data[j];
                tempi = wr * data[j] + wi*data[j-1];
 
                data[j-1] = data[i-1] - tempr;
                data[j] = data[i] - tempi;
                data[i-1] += tempr;
                data[i] += tempi;
            }
            wtemp=wr;
            wr += wr*wpr - wi*wpi;
            wi += wi*wpr + wtemp*wpi;
        }
        mmax=istep;
    }
    
    /* abs and log scale */
    for (n = 0; n < s->sz; ++n) {
        if (data[n] < 0.0F) data[n] = -data[n];
        data[n] = log(data[n] + 1) / 3;
        data[n] *= max((((float) n / (float) s->sz) * d->fft_scale) + (1.0F - d->fft_cutoff), 1.0F);
    }
}

static struct gl_transform transform_functions[] = {
    { .name = "window",  .type = BIND_SAMPLER1D, .apply = transform_window  },
    { .name = "fft",     .type = BIND_SAMPLER1D, .apply = transform_fft     },
    { .name = "wrange",  .type = BIND_SAMPLER1D, .apply = transform_wrange  },
    { .name = "avg",     .type = BIND_SAMPLER1D, .apply = transform_average },
    { .name = "gravity", .type = BIND_SAMPLER1D, .apply = transform_gravity },
    { .name = "smooth",  .type = BIND_SAMPLER1D, .apply = transform_smooth  }
};

static struct gl_bind_src* lookup_bind_src(const char* str) {
    size_t t;
    for (t = 0; t < sizeof(bind_sources) / sizeof(struct gl_bind_src); ++t) {
        if (!strcmp(bind_sources[t].name, str)) {
            return &bind_sources[t];
        }
    }
    return NULL;
}

struct renderer* rd_new(const char** paths,        const char* entry,
                        const char** requests,     const char* force_backend,
                        bool         auto_desktop, bool        verbose) {

    xwin_wait_for_wm();
    
    renderer* r = malloc(sizeof(struct renderer));
    *r = (struct renderer) {
        .alive                = true,
        .mirror_input         = false,
        .gl                   = malloc(sizeof(struct gl_data)),
        .bufsize_request      = 8192,
        .rate_request         = 22000,
        .samplesize_request   = 1024,
        .audio_source_request = NULL
    };
    
    struct gl_data* gl = r->gl;
    *gl = (struct gl_data) {
        .w                 = NULL,
        .wcb               = NULL,
        .stages            = NULL,
        .rate              = 0,
        .tcounter          = 0.0D,
        .fcounter          = 0,
        .ucounter          = 0,
        .kcounter          = 0,
        .fr                = 1.0F,
        .ur                = 1.0F,
        .print_fps         = true,
        .bufscale          = 1,
        .avg_frames        = 6,
        .avg_window        = true,
        .gravity_step      = 4.2,
        .interpolate       = true,
        .force_geometry    = false,
        .force_raised      = false,
        .smooth_factor     = 0.025,
        .smooth_distance   = 0.01,
        .smooth_ratio      = 4,
        .bg_tex            = 0,
        .sm_prog           = 0,
        .copy_desktop      = true,
        .premultiply_alpha = true,
        .check_fullscreen  = false,
        .smooth_pass       = true,
        .fft_scale         = 10.2F,
        .fft_cutoff        = 0.3F,
        .geometry          = { 0, 0, 500, 400 },
        .clear_color       = { 0.0F, 0.0F, 0.0F, 0.0F },
        .clickthrough      = false
    };

    bool forced = force_backend != NULL;
    const char* backend = force_backend;

    /* Window creation backend interfaces */
    #ifdef GLAVA_GLFW
    DECL_WCB(wcb_glfw);
    if (!forced) backend = "glfw";
    #endif

    #ifdef GLAVA_GLX
    DECL_WCB(wcb_glx);
    if (!forced && getenv("DISPLAY")) {
        backend = "glx";
    }
    #endif

    if (!backend) {
        fprintf(stderr, "No backend available for the active windowing system\n");
        if (wcbs_idx == 0) {
            fprintf(stderr, "None have been compiled into this build.\n");
        } else {
            fprintf(stderr, "Available backends:\n");
            for (size_t t = 0; t < wcbs_idx; ++t) {
                fprintf(stderr, "\t\"%s\"\n", wcbs[t]->name);
            }
        }
        exit(EXIT_FAILURE);
    }

    if (verbose) printf("Using backend: '%s'\n", backend);

    for (size_t t = 0; t < wcbs_idx; ++t) {
        if (wcbs[t]->name && !strcmp(wcbs[t]->name, backend)) {
            gl->wcb = wcbs[t];
            break;
        }
    };

    if (!gl->wcb) {
        fprintf(stderr, "Invalid window creation backend selected: '%s'\n", backend);
        abort();
    }
    
    #ifdef GLAD_DEBUG
    if (verbose) printf("Assigning debug callback\n");
    static bool assigned_debug_cb = false;
    if (!assigned_debug_cb) {
        glad_set_post_callback(glad_debugcb);
        assigned_debug_cb = true;
    }
    #endif
    
    gl->wcb->init();

    int shader_version        = 330,
        context_version_major = 3,
        context_version_minor = 3;
    const char* module = NULL;
    const char* wintitle_default = "GLava";
    char* xwintype = NULL, * wintitle = (char*) wintitle_default;
    char** xwinstates = malloc(1);
    size_t xwinstates_sz = 0;
    bool loading_module = true, loading_smooth_pass = false, loading_presets = false;;
    struct gl_sfbo* current = NULL;
    size_t t_count = 0;

    #define WINDOW_HINT(request)                                        \
        { .name = "set" #request, .fmt = "b",                           \
                .handler = RHANDLER(name, args, { gl->wcb->set_##request(*(bool*) args[0]); }) }
    
    struct request_handler handlers[] = {
        {
            .name = "setopacity", .fmt = "s",
            .handler = RHANDLER(name, args, {

                    bool native_opacity = !strcmp("native", (char*) args[0]);
                    
                    gl->premultiply_alpha = native_opacity;

                    gl->wcb->set_transparent(native_opacity);

                    if (!strcmp("xroot", (char*) args[0]))
                        gl->copy_desktop = true;
                    else
                        gl->copy_desktop = false;

                    if (!gl->copy_desktop && !native_opacity && strcmp("none", (char*) args[0])) {
                        fprintf(stderr, "Invalid opacity option: '%s'\n", (char*) args[0]);
                        exit(EXIT_FAILURE);
                    }
                })
        },
        {
            .name = "setmirror", .fmt = "b",
            .handler = RHANDLER(name, args, { r->mirror_input = *(bool*) args[0]; })
        },
        {
            .name = "setfullscreencheck", .fmt = "b",
            .handler = RHANDLER(name, args, { gl->check_fullscreen = *(bool*) args[0]; })
        },
        {
            .name = "setbg", .fmt = "s",
            .handler = RHANDLER(name, args, {
                    float* results[] = {
                        &gl->clear_color.r,
                        &gl->clear_color.g,
                        &gl->clear_color.b,
                        &gl->clear_color.a
                    };
                    if (!ext_parse_color((char*) args[0], 2, results)) {
                        fprintf(stderr, "Invalid value for `setbg` request: '%s'\n", (char*) args[0]);
                        exit(EXIT_FAILURE);
                    }
                })
        },
        {
            .name = "setbgf", .fmt = "ffff",
            .handler = RHANDLER(name, args, {
                    gl->clear_color.r = *(float*) args[0];
                    gl->clear_color.g = *(float*) args[1];
                    gl->clear_color.b = *(float*) args[2];
                    gl->clear_color.a = *(float*) args[3];
                })
        },
        {
            .name = "mod", .fmt = "s",
            .handler = RHANDLER(name, args, {
                    if (loading_module) {
                        if (module != NULL) free((char*) module);
                        size_t len = strlen((char*) args[0]);
                        char* str = malloc(sizeof(char) * (len + 1));
                        strncpy(str, (char*) args[0], len + 1);
                        module = str;
                    }
                })
        },
        {
            .name = "nativeonly", .fmt = "b",
            .handler = RHANDLER(name, args, {
                    if (current)
                        current->nativeonly = *(bool*) args[0];
                    else {
                        fprintf(stderr, "`nativeonly` request needs module context\n");
                        exit(EXIT_FAILURE);
                    }
                })
        },
        WINDOW_HINT(floating),
        WINDOW_HINT(decorated),
        WINDOW_HINT(focused),
        WINDOW_HINT(maximized),
        {
            .name = "setversion", .fmt = "ii",
            .handler = RHANDLER(name, args, {
                    context_version_major = *(int*) args[0];
                    context_version_minor = *(int*) args[1];
                })
        },
        {
            .name = "setgeometry", .fmt = "iiii",
            .handler = RHANDLER(name, args, {
                    gl->geometry[0] = *(int*) args[0];
                    gl->geometry[1] = *(int*) args[1];
                    gl->geometry[2] = *(int*) args[2];
                    gl->geometry[3] = *(int*) args[3];
                })
        },
        {
            .name = "addxwinstate", .fmt = "s",
            .handler = RHANDLER(name, args, {
                    if (!auto_desktop || loading_presets) {
                        ++xwinstates_sz;
                        xwinstates = realloc(xwinstates, sizeof(*xwinstates) * xwinstates_sz);
                        xwinstates[xwinstates_sz - 1] = strdup((char*) args[0]);
                    }
                })
        },
        {   .name = "setsource", .fmt = "s",
            .handler = RHANDLER(name, args, {
                    if (r->audio_source_request) free(r->audio_source_request);
                    r->audio_source_request = strdup((char*) args[0]); })                    },
        {   .name = "setclickthrough", .fmt = "b",
            .handler = RHANDLER(name, args, { gl->clickthrough = *(bool*) args[0]; })        },
        {   .name = "setforcegeometry", .fmt = "b",
            .handler = RHANDLER(name, args, { gl->force_geometry = *(bool*) args[0]; })      },
        {   .name = "setforceraised", .fmt = "b",
            .handler = RHANDLER(name, args, { gl->force_raised = *(bool*) args[0]; })        },
        {   .name = "setxwintype", .fmt = "s",
            .handler = RHANDLER(name, args, {
                    if (xwintype) free(xwintype);
                    xwintype = strdup((char*) args[0]); })                                   },
        {   .name = "setshaderversion", .fmt = "i",
            .handler = RHANDLER(name, args, { shader_version = *(int*) args[0]; })           },
        {   .name = "setswap", .fmt = "i",
            .handler = RHANDLER(name, args, { gl->wcb->set_swap(*(int*) args[0]); })         },
        {   .name = "setframerate", .fmt = "i",
            .handler = RHANDLER(name, args, { gl->rate = *(int*) args[0]; })                 },
        {   .name = "setprintframes", .fmt = "b",
            .handler = RHANDLER(name, args, { gl->print_fps = *(bool*) args[0]; })           },
        {   .name = "settitle", .fmt = "s",
            .handler = RHANDLER(name, args, {
                    if (wintitle && wintitle != wintitle_default) free((char*) wintitle);
                    wintitle = strdup((char*) args[0]); })                                   },
        {   .name = "setbufsize", .fmt = "i",
            .handler = RHANDLER(name, args, { r->bufsize_request = *(int*) args[0]; })       },
        {   .name = "setbufscale", .fmt = "i",
            .handler = RHANDLER(name, args, { gl->bufscale = *(int*) args[0]; })             },
        {   .name = "setsamplerate", .fmt = "i",
            .handler = RHANDLER(name, args, { r->rate_request = *(int*) args[0]; })          },
        {   .name = "setsamplesize", .fmt = "i",
            .handler = RHANDLER(name, args, { r->samplesize_request = *(int*) args[0]; })    },
        {   .name = "setavgframes", .fmt = "i",
            .handler = RHANDLER(name, args, {
                    if (!loading_smooth_pass) gl->avg_frames = *(int*) args[0]; })           },
        {   .name = "setavgwindow", .fmt = "b",
            .handler = RHANDLER(name, args, {
                    if (!loading_smooth_pass) gl->avg_window = *(bool*) args[0]; })          },
        {   .name = "setgravitystep", .fmt = "f",
            .handler = RHANDLER(name, args, {
                    if (!loading_smooth_pass) gl->gravity_step = *(float*) args[0]; })       },
        {   .name = "setsmoothpass", .fmt = "b",
            .handler = RHANDLER(name, args, {
                    if (!loading_smooth_pass) gl->smooth_pass = *(bool*) args[0]; })         },
        {   .name = "setsmoothfactor", .fmt = "f",
            .handler = RHANDLER(name, args, {
                    if (!loading_smooth_pass) gl->smooth_factor = *(float*) args[0]; })      },
        {   .name = "setsmooth", .fmt = "f",
            .handler = RHANDLER(name, args, {
                    if (!loading_smooth_pass) gl->smooth_distance = *(float*) args[0]; })    },
        {   .name = "setsmoothratio", .fmt = "f",
            .handler = RHANDLER(name, args, {
                    if (!loading_smooth_pass) gl->smooth_ratio = *(float*) args[0]; })       },
        {   .name = "setinterpolate", .fmt = "b",
            .handler = RHANDLER(name, args, {
                    if (!loading_smooth_pass) gl->interpolate = *(bool*) args[0]; })         },
        {   .name = "setfftscale", .fmt = "f",
            .handler = RHANDLER(name, args, {
                    if (!loading_smooth_pass) gl->fft_scale = *(float*) args[0];})           },
        {   .name = "setfftcutoff", .fmt = "f",
            .handler = RHANDLER(name, args, {
                    if (!loading_smooth_pass) gl->fft_cutoff = *(float*) args[0];})          },
        {
            .name = "transform", .fmt = "ss",
            .handler = RHANDLER(name, args, {
                    size_t t;
                    struct gl_bind* bind = NULL;
                    for (t = 0; t < current->binds_sz; ++t) {
                        if (!strcmp(current->binds[t].name, (const char*) args[0])) {
                            bind = &current->binds[t];
                            break;
                        }
                    }
                    if (!bind) {
                        fprintf(stderr, "Cannot add transformation to uniform '%s':"
                                " uniform does not exist! (%d present in this unit)\n",
                                (const char*) args[0], (int) current->binds_sz);
                        exit(EXIT_FAILURE);
                    }
                    struct gl_transform* tran = NULL;
                    for (t = 0; t < sizeof(transform_functions) / sizeof(struct gl_transform); ++t) {
                        if (!strcmp(transform_functions[t].name, (const char*) args[1])) {
                            tran = &transform_functions[t];
                            break;
                        }
                    }
                    if (!tran) {
                        fprintf(stderr, "Cannot add transformation '%s' to uniform '%s':"
                                " transform function does not exist!\n",
                                (const char*) args[1], (const char*) args[0]);
                        exit(EXIT_FAILURE);
                    }
                    if (tran->type != bind->type) {
                        fprintf(stderr, "Cannot apply '%s' to uniform '%s': mismatching types\n",
                                (const char*) args[1], (const char*) args[0]);
                        exit(EXIT_FAILURE);
                    }
                    ++bind->t_sz;
                    bind->transformations =
                        realloc(bind->transformations, bind->t_sz * sizeof(void (*)(void*)));
                    bind->transformations[bind->t_sz - 1] = tran->apply;
                    ++t_count;
                })
        },
        {
            .name = "uniform", .fmt = "ss",
            .handler = RHANDLER(name, args, {
                    if (!current) {
                        fprintf(stderr, "Cannot bind uniform '%s' outside of a context"
                                " (load a module first!)\n", (const char*) args[0]);
                        exit(EXIT_FAILURE);
                    }
                    struct gl_bind_src* src = lookup_bind_src((const char*) args[0]);
                    if (!src) {
                        fprintf(stderr, "Cannot bind uniform '%s': bind type does not exist!\n",
                                (const char*) args[0]);
                        exit(EXIT_FAILURE);
                    }
                    ++current->binds_sz;
                    current->binds = realloc(current->binds, current->binds_sz * sizeof(struct gl_bind));
                    current->binds[current->binds_sz - 1] = (struct gl_bind) {
                        .name            = strdup((const char*) args[1]), 
                        .type            = src->type,
                        .src_type        = src->src_type,
                        .transformations = malloc(1),
                        .t_sz            = 0,
                        .sm              = {}
                    };
                })
        },
        { .name = NULL }
    };
    
    #undef WINDOW_WINT
    
    /* Find entry point in data directory list. The first entry point found will indicate
       the path to use for future shader files and modules. Generally, user configuration
       directories will be populated with symlinks to the installed modules. */
    
    const char* data;
    const char* dd; /* defaults dir (system) */
    const char* env = gl->wcb->get_environment();
    size_t d_len, e_len;

    for (const char** i = paths; (data = *i) != NULL; ++i) dd = data;
    for (const char** i = paths; (data = *i) != NULL; ++i) {
        d_len = strlen(data);
        e_len = env ? strlen(env) : 0;
        size_t se_len = strlen(entry);
        /* '/' + \0 + "env_" + ".glsl" = 11 char padding, min 7 for "default" */
        size_t bsz = se_len + 11;
        if (d_len > e_len && d_len >= 7) bsz += d_len;
        else if (e_len >= 7) bsz += e_len;
        else bsz += 7;
        char se_buf[bsz];
        snprintf(se_buf, bsz, "%s/%s", data, entry);

        struct stat st;
        
        int fd = open(se_buf, O_RDONLY);
        if (fd == -1) {
            /* If the file exists but there was an error opening it, complain and exit */
            if (errno != ENOENT  &&
                errno != ENOTDIR &&
                errno != ELOOP     ) {
                fprintf(stderr, "Failed to load entry '%s': %s\n", se_buf, strerror(errno));
                exit(EXIT_FAILURE);
            } else continue;
        }
        fstat(fd, &st);

        const char* map = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);

        struct glsl_ext ext = {
            .source     = map,
            .source_len = st.st_size,
            .cd         = data,
            .handlers   = handlers
        };
        
        ext_process(&ext, se_buf);
        ext_free(&ext);
        
        munmap((void*) map, st.st_size);

        if (auto_desktop) {
            if (env) {
                snprintf(se_buf, bsz, "%s/env_%s.glsl", data, env);
                fd = open(se_buf, O_RDONLY);
                if (fd == -1) {
                    if (errno != ENOENT  &&
                        errno != ENOTDIR &&
                        errno != ELOOP) {
                        fprintf(stderr, "Failed to load desktop environment specific presets "
                                "at '%s': %s\n", se_buf, strerror(errno));
                        exit(EXIT_FAILURE);
                    } else {
                        printf("No presets for current desktop environment (\"%s\"), "
                               "using default presets for embedding\n", env);
                        snprintf(se_buf, bsz, "%s/env_default.glsl", data);
                        fd = open(se_buf, O_RDONLY);
                        if (fd == -1) {
                            fprintf(stderr, "Failed to load default presets at '%s': %s\n",
                                    se_buf, strerror(errno));
                            exit(EXIT_FAILURE);
                        }
                    }
                }
                fstat(fd, &st);
                map = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);

                ext.source     = map;
                ext.source_len = st.st_size;

                loading_presets = true;
                ext_process(&ext, se_buf);
                ext_free(&ext);
                loading_presets = false;

                munmap((void*) map, st.st_size);
            } else {
                fprintf(stderr, "Failed to detect the desktop environment! "
                        "Is the window manager EWMH compliant?");
            }
        }
        
        break;
    }

    {
        struct glsl_ext ext = {
            .cd         = data,
            .handlers   = handlers
        };
        
        const char* req;
        char fbuf[64];
        int idx = 1;
        for (const char** i = requests; (req = *i) != NULL; ++i) {
            size_t rlen = strlen(req) + 16;
            char* rbuf = malloc(rlen);
            rlen = snprintf(rbuf, rlen, "#request %s", req);
            snprintf(fbuf, sizeof(fbuf), "[request arg %d]", idx);
            ext.source     = rbuf;
            ext.source_len = rlen;
            ext_process(&ext, fbuf);
            ext_free(&ext);
            ++idx;
        }
    }
    
    if (!module) {
        fprintf(stderr,
                "No module was selected, edit '%s' to load "
                "a module with `#request mod [name]`\n",
                entry);
        exit(EXIT_FAILURE);
    }
    
    gl->w = gl->wcb->create_and_bind(wintitle, "GLava", xwintype, (const char**) xwinstates, xwinstates_sz,
                                     gl->geometry[2], gl->geometry[3], gl->geometry[0], gl->geometry[1],
                                     context_version_major, context_version_minor, gl->clickthrough);
    if (!gl->w) abort();

    for (size_t t = 0; t < xwinstates_sz; ++t)
        free(xwinstates[t]);
    
    if (xwintype)   free(xwintype);
    if (xwinstates) free(xwinstates);
    if (wintitle && wintitle != wintitle_default) free(wintitle);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_CLAMP);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_MULTISAMPLE);
    glDisable(GL_LINE_SMOOTH);

    if (!gl->premultiply_alpha) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    
    size_t m_len = strlen(module);
    size_t bsz = d_len + m_len + 2;
    char shaders[bsz]; /* module pack path to use */
    snprintf(shaders, bsz, "%s/%s", data, module);

    if (verbose) printf("Loading module: '%s'\n", module);

    free((void*) module);
    loading_module = false;

    /* Iterate through shader passes in the shader directory and build textures, framebuffers, and
       shader programs with each fragment shader. */
    
    struct gl_sfbo* stages;
    size_t count = 0;
    
    {
        char buf[32];
        DIR* dir = opendir(shaders);
        if (dir == NULL) {
            fprintf(stderr, "shaders folder '%s' does not exist!", shaders);
        } else {
            closedir(dir);
            struct dirent* d;
            size_t idx = 1;
            bool found;
            do {
                found = false;
        
                dir = opendir(shaders);
                while ((d = readdir(dir)) != NULL) {
                    if (d->d_type == DT_REG || d->d_type == DT_UNKNOWN) {
                        snprintf(buf, sizeof(buf), "%d." SHADER_EXT_FRAG, (int) idx);
                        if (!strcmp(buf, d->d_name)) {
                            if (verbose) printf("found GLSL stage: '%s'\n", d->d_name);
                            ++count;
                            found = true;
                        }
                    }
                }
                closedir(dir);
                ++idx;
            } while (found);
        
            stages = malloc(sizeof(struct gl_sfbo) * count);
        
            idx = 1;
            do {
                found = false;
            
                dir = opendir(shaders);
                while ((d = readdir(dir)) != NULL) {
                    if (d->d_type == DT_REG || d->d_type == DT_UNKNOWN) {
                        snprintf(buf, sizeof(buf), "%d." SHADER_EXT_FRAG, (int) idx);
                        if (!strcmp(buf, d->d_name)) {
                            if (verbose) printf("compiling: '%s'\n", d->d_name);
                        
                            struct gl_sfbo* s = &stages[idx - 1];
                            *s = (struct gl_sfbo) {
                                .name       = strdup(d->d_name),
                                .shader     = 0,
                                .valid      = false,
                                .nativeonly = false,
                                .binds      = malloc(1),
                                .binds_sz   = 0
                            };

                            current = s;
                            GLuint id = shaderbuild(gl, shaders, data, dd, handlers, shader_version, d->d_name);
                            if (!id) {
                                abort();
                            }

                            s->shader = id;

                            /* Only setup a framebuffer and texture if this isn't the final step,
                               as it can rendered directly */
                            if (idx != count) {
                                int w, h;
                                gl->wcb->get_fbsize(gl->w, &w, &h);
                                setup_sfbo(&stages[idx - 1], w, h);
                            }

                            glUseProgram(id);
                        
                            /* Setup uniform bindings */
                            size_t b;
                            for (b = 0; b < s->binds_sz; ++b) {
                                s->binds[b].uniform = glGetUniformLocation(id, s->binds[b].name);
                            }
                            glBindFragDataLocation(id, 1, "fragment");
                            glUseProgram(0);
                        
                            found = true;
                        }
                    }
                }
                closedir(dir);
                ++idx;
            } while (found);
        }
    }
    
    gl->stages = stages;
    gl->stages_sz = count;
    
    {
        struct gl_sfbo* final = NULL;
        if (!gl->premultiply_alpha) {
            for (size_t t = 0; t < gl->stages_sz; ++t) {
                if (!gl->stages[t].nativeonly) {
                    final = &gl->stages[t];
                }
            }
        }
        /* Invalidate framebuffer and use direct rendering if it was instantiated 
           due to a following `nativeonly` shader pass. */
        if (final) final->valid = false;
    }

    /* Compile smooth pass shader */
    
    {
        const char* util_folder = "util";
        size_t u_len = strlen(util_folder);
        size_t usz = d_len + u_len + 2;
        char util[usz]; /* module pack path to use */
        snprintf(util, usz, "%s/%s", data, util_folder);
        loading_smooth_pass = true;
        if (!(gl->sm_prog = shaderbuild(gl, util, data, dd, handlers, shader_version, "smooth_pass.frag")))
            abort();
        loading_smooth_pass = false;
    }
    
    /* target seconds per update */
    gl->target_spu = (float) (r->samplesize_request / 4) / (float) r->rate_request;
    
    gl->audio_tex_r = create_1d_tex();
    gl->audio_tex_l = create_1d_tex();
    
    if (gl->interpolate) {
        /* Allocate six buffers at once */
        size_t isz = (r->bufsize_request / gl->bufscale);
        float* ibuf = malloc(isz * 6 * sizeof(float));
        
        gl->interpolate_buf[IB_START_LEFT ] = &ibuf[isz * IB_START_LEFT ]; /* left channel keyframe start  */
        gl->interpolate_buf[IB_END_LEFT   ] = &ibuf[isz * IB_END_LEFT   ]; /* left channel keyframe end    */
        gl->interpolate_buf[IB_START_RIGHT] = &ibuf[isz * IB_START_RIGHT]; /* right channel keyframe start */
        gl->interpolate_buf[IB_END_RIGHT  ] = &ibuf[isz * IB_END_RIGHT  ]; /* right channel keyframe end   */
        gl->interpolate_buf[IB_WORK_LEFT  ] = &ibuf[isz * IB_WORK_LEFT  ]; /* left interpolation results   */
        gl->interpolate_buf[IB_WORK_RIGHT ] = &ibuf[isz * IB_WORK_RIGHT ]; /* right interpolation results  */
    }

    {
        gl->t_data  = malloc(sizeof(void*) * t_count);
        gl->t_count = t_count;
        size_t t;
        for (t = 0; t < t_count; ++t) {
            gl->t_data[t] = NULL;
        }
    }

    overlay(&gl->overlay);
    
    glClearColor(gl->clear_color.r, gl->clear_color.g, gl->clear_color.b, gl->clear_color.a);
    
    gl->wcb->set_visible(gl->w, true);
    
    return r;
}

void rd_time(struct renderer* r) {
    struct gl_data* gl = r->gl;
    
    gl->wcb->set_time(gl->w, 0.0D); /* reset time for measuring this frame */
}

bool rd_update(struct renderer* r, float* lb, float* rb, size_t bsz, bool modified) {
    struct gl_data* gl = r->gl;
    size_t t, a, fbsz = bsz * sizeof(float);
    
    if (gl->wcb->should_close(gl->w)) {
        r->alive = false;
        return true;
    }

    /* Stop rendering if the backend has some reason not to render (minimized, obscured) */
    if (!gl->wcb->should_render(gl->w))
        return false;
    
    /* Stop rendering when fullscreen windows are focused */
    if (gl->check_fullscreen && !xwin_should_render(gl->wcb, gl->w))
        return false;

    /* Force disable interpolation if the update rate is close to or higher than the frame rate */
    float uratio = (gl->ur / gl->fr); /* update : framerate ratio */
    bool old_interpolate = gl->interpolate;
    gl->interpolate = uratio <= 0.9F ? old_interpolate : false;

    /* Perform buffer scaling */
    size_t nsz = gl->bufscale > 1 ? (bsz / gl->bufscale) : 0;
    float nlb[nsz], nrb[nsz];
    if (gl->bufscale > 1) {
        float accum;
        for (t = 0; t < nsz; ++t) {
            accum = 0.0F;
            for (a = 0; a < gl->bufscale; ++a) {
                accum += lb[(t * gl->bufscale) + a];
            }
            accum /= (float) gl->bufscale;
            nlb[t] = accum;
        }
        for (t = 0; t < nsz; ++t) {
            accum = 0.0F;
            for (a = 0; a < gl->bufscale; ++a) {
                accum += rb[(t * gl->bufscale) + a];
            }
            accum /= (float) gl->bufscale;
            nrb[t] = accum;
        }
        lb = nlb;
        rb = nrb;
        bsz = nsz;
        fbsz = bsz * sizeof(float);
    }

    /* Linear interpolation */
    float
        * ilb = gl->interpolate_buf[IB_WORK_LEFT ],
        * irb = gl->interpolate_buf[IB_WORK_RIGHT];
    if (gl->interpolate) {
        for (t = 0; t < bsz; ++t) {
            /* Obtain start/end values at this index for left & right buffers */
            float
                ilbs = gl->interpolate_buf[IB_START_LEFT ][t],
                ilbe = gl->interpolate_buf[IB_END_LEFT   ][t],
                irbs = gl->interpolate_buf[IB_START_RIGHT][t],
                irbe = gl->interpolate_buf[IB_END_RIGHT  ][t],
                mod  = uratio * gl->kcounter; /* modifier for this frame */
            if (mod > 1.0F) mod = 1.0F;
            ilb[t] = ilbs + ((ilbe - ilbs) * mod);
            irb[t] = irbs + ((irbe - irbs) * mod);
        }
    }
    int ww, wh, wx, wy;
    gl->wcb->get_fbsize(gl->w, &ww, &wh);
    gl->wcb->get_pos(gl->w, &wx, &wy);
    
    /* Resize screen textures if needed */
    if (ww != gl->lww || wh != gl->lwh) {
        for (t = 0; t < gl->stages_sz; ++t) {
            if (gl->stages[t].valid) {
                setup_sfbo(&gl->stages[t], ww, wh);
            }
        }
    }

    /* Resize and grab new background data if needed */
    if (gl->copy_desktop && (gl->wcb->bg_changed(gl->w) || ww != gl->lww || wh != gl->lwh || wx != gl->lwx || wy != gl->lwy)) {
        gl->bg_tex = xwin_copyglbg(r, gl->bg_tex);
    }

    gl->lwx = wx;
    gl->lwy = wy;
    gl->lww = ww;
    gl->lwh = wh;
    
    glViewport(0, 0, ww, wh);
        
    struct gl_sfbo* prev;

    /* Iterate through each rendering stage (shader) */
    
    for (t = 0; t < gl->stages_sz; ++t) {

        bool load_flags[64] = { [ 0 ... 63 ] = false }; /* Load flags for each texture position */
        
        /* Current shader program */
        struct gl_sfbo* current = &gl->stages[t];

        if (current->nativeonly && !gl->premultiply_alpha)
            continue;
        
        /* Bind framebuffer if this is not the final pass */
        if (current->valid)
            glBindFramebuffer(GL_FRAMEBUFFER, current->fbo);
        
        glClear(GL_COLOR_BUFFER_BIT);
        
        if (!current->valid && gl->copy_desktop) {
            /* Self-contained code for drawing background image */
            static GLuint bg_prog, bg_utex, bg_screen;
            static bool setup = false;
            /* Shader to flip texture and override alpha channel */
            static const char* frag_shader =
                "uniform sampler2D tex;"                                                             "\n"
                "uniform ivec2 screen;"                                                              "\n"
                "out vec4 fragment;"                                                                 "\n"
                "in vec4 gl_FragCoord;"                                                              "\n"
                "void main() {"                                                                      "\n"
                "    fragment = texelFetch(tex, ivec2(gl_FragCoord.x, "                              "\n"
                "                       screen.y - gl_FragCoord.y), 0);"                             "\n"
                "    fragment.a = 1.0F;"                                                             "\n"
                "}"                                                                                  "\n";
            if (!setup) {
                bg_prog = shaderlink(shaderload(NULL, GL_VERTEX_SHADER, VERTEX_SHADER_SRC,
                                                NULL, NULL, NULL, 330, true, gl),
                                     shaderload(NULL, GL_FRAGMENT_SHADER, frag_shader,
                                                NULL, NULL, NULL, 330, true, gl));
                bg_utex   = glGetUniformLocation(bg_prog, "tex");
                bg_screen = glGetUniformLocation(bg_prog, "screen");
                glBindFragDataLocation(bg_prog, 1, "fragment");
                setup = true;
            }
            glUseProgram(bg_prog);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, gl->bg_tex);
            glUniform2i(bg_screen, (GLint) ww, (GLint) wh);
            glUniform1i(bg_utex, 0);
            /* We need to disable blending, we might read in bogus alpha values due
               to how we obtain the background texture (format is four byte `rgb_`, 
               where the last value is skipped) */
            if (!gl->premultiply_alpha) glDisable(GL_BLEND);
            drawoverlay(&gl->overlay);
            if (!gl->premultiply_alpha) glEnable(GL_BLEND);
            glUseProgram(0);
        }
        
        /* Select the program associated with this pass */
        glUseProgram(current->shader);

        bool prev_bound = false;
        
        /* Iterate through each uniform binding, transforming and passing the 
           data into the shader. */
        
        size_t b, c = 0;
        for (b = 0; b < current->binds_sz; ++b) {
            struct gl_bind* bind = &current->binds[b];

            /* Handle transformations and bindings for 1D samplers */
            void handle_1d_tex(GLuint tex, float* buf, float* ubuf, size_t sz, int offset, bool audio) {
                
                if (load_flags[offset])
                    goto bind_uniform;
                load_flags[offset] = true;

                /* Only apply transformations if the buffers we
                   were given are newly copied from PA */
                if (modified) {
                    size_t t;
                    struct gl_sampler_data d = {
                        .buf = buf, .sz = sz
                    };
                    
                    for (t = 0; t < bind->t_sz; ++t) {
                        bind->transformations[t](gl, &gl->t_data[c], &d);
                        ++c; /* Index for transformation data (note: change if new
                                transform types are added) */
                    }
                }
                glActiveTexture(GL_TEXTURE0 + offset);

                /* Update texture with our data */
                update_1d_tex(tex, sz, gl->interpolate ? (ubuf ? ubuf : buf) : buf);

                /* Apply pre-smoothing shader pass if configured */
                if (audio && gl->smooth_pass) {
                    static bool setup = false;
                    static GLuint sm_utex, sm_usz, sm_uw;
                    
                    /* Compile preprocess shader and handle uniform locations */
                    if (!setup) {
                        sm_utex = glGetUniformLocation(gl->sm_prog, "tex");
                        sm_usz  = glGetUniformLocation(gl->sm_prog, "sz");
                        sm_uw   = glGetUniformLocation(gl->sm_prog, "w");
                        glBindFragDataLocation(gl->sm_prog, 1, "fragment");
                        setup = true;
                    }

                    /* Allocate and setup our per-bind data, if needed */

                    struct sm_fb* sm = &bind->sm;
                    if (sm->tex == 0) {
                        glGenTextures(1, &sm->tex);
                        glGenFramebuffers(1, &sm->fbo);

                        /* 1D texture parameters */
                        glBindTexture(GL_TEXTURE_1D, sm->tex);
                        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                        glTexImage1D(GL_TEXTURE_1D, 0, GL_R16, sz, 0, GL_RED, GL_FLOAT, NULL);
    
                        /* setup and bind framebuffer to texture */
                        glBindFramebuffer(GL_FRAMEBUFFER, sm->fbo);
                        glFramebufferTexture1D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,\
                                               GL_TEXTURE_1D, sm->tex, 0);
                        
                        switch (glCheckFramebufferStatus(GL_FRAMEBUFFER)) {
                            case GL_FRAMEBUFFER_COMPLETE: break;
                            default:
                                fprintf(stderr, "error in frambuffer state\n");
                                abort();
                        }
                    } else {
                        /* Just bind our data if it was already allocated and setup */
                        glBindFramebuffer(GL_FRAMEBUFFER, sm->fbo);
                        glBindTexture(GL_TEXTURE_1D, sm->tex);
                    }
                    
                    glUseProgram(gl->sm_prog);
                    glActiveTexture(GL_TEXTURE0 + offset);
                    glBindTexture(GL_TEXTURE_1D, tex);
                    glUniform1i(sm_uw, sz);  /* target texture width */
                    glUniform1i(sm_usz, sz); /* source texture width */
                    glUniform1i(sm_utex, offset);
                    if (!gl->premultiply_alpha) glDisable(GL_BLEND);
                    glViewport(0, 0, sz, 1);
                    drawoverlay(&gl->overlay);
                    glViewport(0, 0, ww, wh);
                    if (!gl->premultiply_alpha) glEnable(GL_BLEND);

                    /* Return state */
                    glUseProgram(current->shader);
                    if (current->valid)
                        glBindFramebuffer(GL_FRAMEBUFFER, current->fbo);
                    else
                        glBindFramebuffer(GL_FRAMEBUFFER, 0);

                    tex = sm->tex; /* replace input texture with our processed one */
                }
                
                glActiveTexture(GL_TEXTURE0 + offset);
                glBindTexture(GL_TEXTURE_1D, tex);
            bind_uniform:
                glUniform1i(bind->uniform, offset);
            }

            /* Handle each binding source; only bother to handle transformations
               for 1D samplers, since that's the only transformation type that
               (currently) exists. */
            
            switch (bind->src_type) {
                case SRC_PREV:
                    /* bind texture and pass it to the shader uniform if we need to pass
                       the sampler from the previous pass */
                    if (!prev_bound && prev != NULL) {
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, prev->tex);
                        prev_bound = true;
                    }
                    glUniform1i(bind->uniform, 0);
                    break;
                case SRC_AUDIO_L:  handle_1d_tex(gl->audio_tex_l, lb, ilb, bsz, 1, true); break;
                case SRC_AUDIO_R:  handle_1d_tex(gl->audio_tex_r, rb, irb, bsz, 2, true); break;
                case SRC_AUDIO_SZ: glUniform1i(bind->uniform, bsz);                       break;
                case SRC_SCREEN:   glUniform2i(bind->uniform, (GLint) ww, (GLint) wh);    break;
            }
        }
        
        drawoverlay(&gl->overlay); /* Fullscreen quad (actually just two triangles) */

        /* Reset some state */
        if (current->valid)
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glUseProgram(0);

        prev = current;
    }

    /* Push and copy buffer if we need to interpolate from it later */
    if (gl->interpolate && modified) {
        memcpy(gl->interpolate_buf[IB_START_LEFT ], gl->interpolate_buf[IB_END_LEFT ], fbsz);
        memcpy(gl->interpolate_buf[IB_START_RIGHT], gl->interpolate_buf[IB_END_RIGHT], fbsz);
        memcpy(gl->interpolate_buf[IB_END_LEFT   ], lb, fbsz);
        memcpy(gl->interpolate_buf[IB_END_RIGHT  ], rb, fbsz);
    }

    /* Swap buffers, handle events, etc. (vsync is potentially included here, too) */
    gl->wcb->swap_buffers(gl->w);

    double duration = gl->wcb->get_time(gl->w); /* frame execution time */

    /* Handling sleeping (to meet target framerate) */
    if (gl->rate > 0) {
        double target = 1.0D / (double) gl->rate; /* 1 / freq = time per frame */
        if (duration < target) {
            double sleep = target - duration;
            struct timespec tv = {
                .tv_sec = (time_t) floor(sleep),
                .tv_nsec = (long) (double) ((sleep - floor(sleep)) * 1000000000.0D)
            };
            nanosleep(&tv, NULL);
            duration = target; /* update duration to reflect our sleep time */
        }
    }

    /* Handle counters and print FPS counter (if needed) */
    
    ++gl->fcounter;           /* increment frame counter                          */
    if (modified) {           /* if this is an update/key frame                   */
        ++gl->ucounter;       /*   increment update frame counter                 */
        gl->kcounter = 0;     /*   reset keyframe counter (for interpolation)     */
    } else ++gl->kcounter;    /* increment keyframe counter otherwise             */
    gl->tcounter += duration; /* timer counter, measuring when a >1s has occurred */
    if (gl->tcounter >= 1.0D) {
        gl->fr = gl->fcounter / gl->tcounter; /* frame rate (FPS)     */
        gl->ur = gl->ucounter / gl->tcounter; /* update rate (UPS)    */
        if (gl->print_fps)                    /* print FPS            */
            printf("FPS: %.2f, UPS: %.2f\n",
                   (double) gl->fr, (double) gl->ur);
        gl->tcounter = 0;                     /* reset timer          */
        gl->fcounter = 0;                     /* reset frame counter  */
        gl->ucounter = 0;                     /* reset update counter */
        
        /* Refresh window position and size if we are forcing it */
        if (gl->force_geometry) {
            gl->wcb->set_geometry(gl->w,
                                  gl->geometry[0], gl->geometry[1],
                                  gl->geometry[2], gl->geometry[3]);
        }
        
        if (gl->force_raised) {
            gl->wcb->raise(gl->w);
        }
    }

    /* Restore interpolation settings */
    gl->interpolate = old_interpolate;
    
    return true;
}

void*          rd_get_impl_window (struct renderer* r)  { return r->gl->w;   }
struct gl_wcb* rd_get_wcb         (struct renderer* r)  { return r->gl->wcb; }

void rd_destroy(struct renderer* r) {
    r->gl->wcb->destroy(r->gl->w);
    if (r->gl->interpolate) free(r->gl->interpolate_buf[0]);
    size_t t, b;
    if (r->gl->t_data) {
        for (t = 0; t < r->gl->t_count; ++t) {
            if (r->gl->t_data[t])
                free(r->gl->t_data[t]);
        }
        free(r->gl->t_data);
    }
    for (t = 0; t < r->gl->stages_sz; ++t) {
        struct gl_sfbo* stage = &r->gl->stages[t];
        for (b = 0; b < stage->binds_sz; ++b) {
            struct gl_bind* bind = &stage->binds[b];
            free(bind->transformations);
            free((char*) bind->name); /* strdup */
        }
        free(stage->binds);
        free((char*) stage->name); /* strdup */
    }
    free(r->gl->stages);
    r->gl->wcb->terminate();
    free(r->gl);
    if (r->audio_source_request)
        free(r->audio_source_request);
    free(r);
}
