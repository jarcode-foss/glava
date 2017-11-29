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

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "render.h"
#include "glsl_ext.h"

#define TWOPI 6.28318530718
#define PI 3.14159265359
#define swap(a, b) do { __auto_type tmp = a; a = b; b = tmp; } while (0)

/* Only a single vertex shader is needed for GLava, since all rendering is done in the fragment shader
   over a fullscreen quad */
#define VERTEX_SHADER_SRC \
    "layout(location = 0) in vec3 pos; void main() { gl_Position = vec4(pos.x, pos.y, 0f, 1f); }"

/* load shader file */
static GLuint shaderload(const char*             rpath,
                         GLenum                  type,
                         const char*             shader,
                         struct request_handler* handlers,
                         int                     shader_version,
                         bool                    raw) {

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
    
    static const GLchar* header_fmt = "#version %d\n#define UNIFORM_LIMIT %d\n";
    
    struct glsl_ext ext = {
        .source     = raw ? NULL : map,
        .source_len = raw ? 0 : st.st_size,
        .cd         = shader,
        .handlers   = handlers,
        .processed  = (char*) (raw ? shader : NULL),
        .p_len      = raw ? s_len : 0
    };

    /* If this is raw input, skip processing */
    if (!raw) ext_process(&ext, rpath);
    
    size_t blen = strlen(header_fmt) + 28;
    GLchar* buf = malloc((blen * sizeof(GLchar*)) + ext.p_len);
    int written = snprintf(buf, blen, header_fmt, (int) shader_version, (int) max_uniforms);
    if (written < 0) {
        fprintf(stderr, "snprintf() encoding error while prepending header to shader '%s'\n", path);
        return 0;
    }
    memcpy(buf + written, ext.processed, ext.p_len);
    if (!raw) munmap((void*) map, st.st_size);

    printf("[DEBUG]\n%.*s\n", ext.p_len, ext.processed);
    
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
            GLchar buf[ilen];
            glGetShaderInfoLog(s, ilen, NULL, buf);
            fprintf(stderr, "Shader compilation failed for '%s':\n", path);
            fwrite(buf, sizeof(GLchar), ilen - 1, stderr);
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
#define shaderbuild(shader_path, r, v, ...) shaderbuild_f(shader_path, r, v, (const char*[]) {__VA_ARGS__, 0})
static GLuint shaderbuild_f(const char* shader_path,
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
                                                  shader_path, handlers, shader_version, false))) {
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
    shaders[sz] = shaderload(NULL, GL_VERTEX_SHADER, VERTEX_SHADER_SRC, handlers, shader_version, true);
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
    float buf[w];
    memcpy(buf, data, w * sizeof(float));
    glBindTexture(GL_TEXTURE_1D, tex);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_R16, w, 0, GL_RED, GL_FLOAT, buf);
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

struct gl_sampler_data {
    float* buf;
    size_t sz;
};

/* GLSL uniform bind */

struct gl_bind {
    const char* name;
    GLuint uniform;
    int type;
    int src_type;
    void (**transformations)(struct gl_data*, void**, void* data);
    size_t t_sz;
};

/* setup screen framebuffer object and its texture */

struct gl_sfbo {
    GLuint fbo, tex, shader;
    bool valid;
    const char* name;
    struct gl_bind* binds;
    size_t binds_sz;
};

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

struct overlay_data {
    GLuint vbuf, vao;
};

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

struct gl_data {
    struct gl_sfbo* stages;
    struct overlay_data overlay;
    GLuint audio_tex_r, audio_tex_l;
    size_t stages_sz, bufscale, avg_frames;
    GLFWwindow* w;
    int lww, lwh; /* last window height */
    int rate; /* framerate */
    double tcounter;
    int fcounter, ucounter, kcounter;
    bool print_fps, avg_window, interpolate;
    void** t_data;
    float gravity_step, target_spu, fr, ur;
    float* interpolate_buf[4];
};

#ifdef GLAD_DEBUG

static void glad_debugcb(const char* name, void *funcptr, int len_args, ...) {
    GLenum err = glad_glGetError();

    if (err != GL_NO_ERROR) {
        fprintf(stderr, "glGetError(): %d in %s\n", err, name);
        abort();
    }
}
#endif

#define SHADER_EXT_VERT "vert"
#define SHADER_EXT_FRAG "frag"
#define SHADER_ENTRY "rc.glsl"
    
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
#define ALLOC_ONCE(u, udata, ...)                    \
    if (*udata == NULL) {                       \
        u = malloc(sizeof(*u));                 \
        *u = (typeof(*u)) __VA_ARGS__;          \
        *udata = u;                             \
    } else u = (typeof(u)) *udata;

void transform_gravity(struct gl_data* d, void** udata, void* data) {
    struct gl_sampler_data* s = (struct gl_sampler_data*) data;
    float* b = s->buf;
    size_t sz = s->sz, t;
    
    struct {
        float* applied;
    }* u;
    ALLOC_ONCE(u, udata, { .applied = calloc(sz, sizeof(float)) });

    float g = d->gravity_step * (1.0F / d->ur);
    
    for (t = 0; t < sz; ++t) {
        if (b[t] >= u->applied[t]) {
            u->applied[t] = b[t] - g;
        } else u->applied[t] -= g;
        b[t] = u->applied[t];
    }
}

void transform_average(struct gl_data* d, void** udata, void* data) {
    
    struct gl_sampler_data* s = (struct gl_sampler_data*) data;
    float* b = s->buf;
    size_t sz = s->sz, t, f;
    size_t tsz = sz * d->avg_frames;
    float v;
    bool use_window = d->avg_window;
    
    struct {
        float* bufs;
    }* u;
    ALLOC_ONCE(u, udata, { .bufs = calloc(tsz, sizeof(float)) });
    
    memmove(u->bufs, &u->bufs[sz], (tsz - sz) * sizeof(float));
    memcpy(&u->bufs[tsz - sz], b, sz * sizeof(float));
    
    #define DO_AVG(w)                                   \
        do {                                            \
            for (t = 0; t < sz; ++t) {                  \
                v = 0.0F;                               \
                for (f = 0; f < d->avg_frames; ++f) {   \
                    v += w * u->bufs[(f * sz) + t];     \
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
    float* b = s->buf, w;
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
    }
}

static struct gl_transform transform_functions[] = {
    { .name = "window",  .type = BIND_SAMPLER1D, .apply = transform_window  },
    { .name = "fft",     .type = BIND_SAMPLER1D, .apply = transform_fft     },
    { .name = "wrange",  .type = BIND_SAMPLER1D, .apply = transform_wrange  },
    { .name = "avg",     .type = BIND_SAMPLER1D, .apply = transform_average },
    { .name = "gravity", .type = BIND_SAMPLER1D, .apply = transform_gravity }
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

struct renderer* rd_new(int x, int y, int w, int h, const char* data) {
    
    renderer* r = malloc(sizeof(struct renderer));
    *r = (struct renderer) {
        .alive              = true,
        .gl                 = malloc(sizeof(struct gl_data)),
        .bufsize_request    = 8192,
        .rate_request       = 22000,
        .samplesize_request = 1024
    };
    
    struct gl_data* gl = r->gl;
    *gl = (struct gl_data) {
        .stages       = NULL,
        .rate         = 0,
        .tcounter     = 0.0D,
        .fcounter     = 0,
        .ucounter     = 0,
        .kcounter     = 0,
        .fr           = 1.0F,
        .ur           = 1.0F,
        .print_fps    = true,
        .bufscale     = 1,
        .avg_frames   = 4,
        .avg_window   = true,
        .gravity_step = 0.1,
        .interpolate  = true,
    };
    
    #ifdef GLAD_DEBUG
    printf("Assigning debug callback\n");
    static bool assigned_debug_cb = false;
    if (!assigned_debug_cb) {
        glad_set_post_callback(glad_debugcb);
        assigned_debug_cb = true;
    }
    #endif
    
    if (!glfwInit())
        abort();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_FLOATING, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    #ifdef GLFW_TRANSPARENT_FRAMEBUFFER
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    #elif GLFW_TRANSPARENT
    glfwWindowHint(GLFW_TRANSPARENT, GLFW_TRUE);
    #else
    printf("WARNING: the linked version of GLFW3 does not have transparency support"
           " (GLFW_TRANSPARENT[_FRAMEBUFFER])!\n");
    #endif

    if (!(gl->w = glfwCreateWindow(w, h, "GLava", NULL, NULL))) {
        glfwTerminate();
        abort();
    }

    glfwMakeContextCurrent(gl->w);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_CLAMP);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_MULTISAMPLE);
    glDisable(GL_LINE_SMOOTH);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);


    int shader_version = 330;
    char* module = NULL;
    bool loading_module = true;
    struct gl_sfbo* current = NULL;
    size_t t_count = 0;

    #define WINDOW_HINT(request, attr)                                  \
        { .name = request, .fmt = "b",                                  \
                .handler = RHANDLER(name, args, { glfwWindowHint(attr, *(bool*) args[0]); }) }
    
    struct request_handler handlers[] = {
        {
            .name = "wavetype", .fmt = "s",
            .handler = RHANDLER(name, args, {
                    printf("[STUB] wavetype = '%s'\n", (char*) args[0]);
                })
        },
        {
            .name = "mod", .fmt = "s",
            .handler = RHANDLER(name, args, {
                    if (loading_module) {
                        size_t len = strlen((char*) args[0]);
                        module = malloc(sizeof(char) * (strlen((char*) args[0]) + 1));
                        strncpy(module, (char*) args[0], len + 1);
                    }
                })
        },
        WINDOW_HINT("setfloating",  GLFW_FLOATING),
        WINDOW_HINT("setdecorated", GLFW_DECORATED),
        WINDOW_HINT("setfocused",   GLFW_FOCUSED),
        WINDOW_HINT("setmaximized", GLFW_MAXIMIZED),
        {
            .name = "setversion", .fmt = "ii",
            .handler = RHANDLER(name, args, {
                    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, *(int*) args[0]);
                    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, *(int*) args[1]);
                })
        },
        {   .name = "setshaderversion", .fmt = "i",
            .handler = RHANDLER(name, args, { shader_version = *(int*) args[0]; })           },
        {   .name = "setswap", .fmt = "i",
            .handler = RHANDLER(name, args, { glfwSwapInterval(*(int*) args[0]); })          },
        {   .name = "setframerate", .fmt = "i",
            .handler = RHANDLER(name, args, { gl->rate = *(int*) args[0]; })                 },
        {   .name = "setprintframes", .fmt = "b",
            .handler = RHANDLER(name, args, { gl->print_fps = *(bool*) args[0]; })           },
        {   .name = "settitle", .fmt = "s",
            .handler = RHANDLER(name, args, { glfwSetWindowTitle(gl->w, (char*) args[0]); }) },
        {   .name = "setbufsize", .fmt = "i",
            .handler = RHANDLER(name, args, { r->bufsize_request = *(int*) args[0]; })       },
        {   .name = "setbufscale", .fmt = "i",
            .handler = RHANDLER(name, args, { gl->bufscale = *(int*) args[0]; })             },
        {   .name = "setsamplerate", .fmt = "i",
            .handler = RHANDLER(name, args, { r->rate_request = *(int*) args[0]; })          },
        {   .name = "setsamplesize", .fmt = "i",
            .handler = RHANDLER(name, args, { r->samplesize_request = *(int*) args[0]; })    },
        {   .name = "setavgframes", .fmt = "i",
            .handler = RHANDLER(name, args, { gl->avg_frames = *(int*) args[0]; })           },
        {   .name = "setavgwindow", .fmt = "b",
            .handler = RHANDLER(name, args, { gl->avg_window = *(bool*) args[0]; })          },
        {   .name = "setgravitystep", .fmt = "f",
            .handler = RHANDLER(name, args, { gl->gravity_step = *(float*) args[0]; })       },
        {   .name = "setinterpolate", .fmt = "b",
            .handler = RHANDLER(name, args, { gl->interpolate = *(bool*) args[0]; })       },
        {
            .name = "transform", .fmt = "ss",
            .handler = RHANDLER(name, args, {
                    printf("[DEBUG] setting transform '%s' to '%s'\n",
                           (const char*) args[1], (const char*) args[0]);
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
                    printf("[DEBUG] binding '%s' -> '%s'\n",
                           (const char*) args[0], (const char*) args[1]);
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
                        .t_sz            = 0
                    };
                })
        },
        { .name = NULL }
    };

    #undef WINDOW_WINT
    
    size_t d_len = strlen(data);
        
    {
        size_t se_len = strlen(SHADER_ENTRY);
        size_t bsz = se_len + d_len + 2;
        char se_buf[bsz];
        snprintf(se_buf, bsz, "%s/%s", data, SHADER_ENTRY);

        struct stat st;
        
        int fd = open(se_buf, O_RDONLY);
        if (fd == -1) {
            fprintf(stderr, "failed to load entry '%s': %s\n", se_buf, strerror(errno));
            exit(EXIT_FAILURE);
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

        munmap((void*) map, st.st_size);
    }
    
    if (!module) {
        fprintf(stderr,
                "No module was selected, edit %s/%s to load "
                "a module with `#request mod [name]`\n",
                data, SHADER_ENTRY);
        exit(EXIT_FAILURE);
    }
    
    size_t m_len = strlen(module);
    size_t bsz = d_len + m_len + 2;
    char shaders[bsz]; /* module pack path to use */
    snprintf(shaders, bsz, "%s/%s", data, module);

    printf("Loading module: '%s'\n", module);

    free(module);
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
            abort();
        }
        closedir(dir);
        struct dirent* d;
        size_t idx = 1;
        bool found;
        do {
            found = false;
        
            dir = opendir(shaders);
            while ((d = readdir(dir)) != NULL) {
                if (d->d_type == DT_REG || d->d_type == DT_UNKNOWN) {
                    snprintf(buf, sizeof(buf), "%d." SHADER_EXT_FRAG, idx);
                    if (!strcmp(buf, d->d_name)) {
                        printf("found GLSL stage: '%s'\n", d->d_name);
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
                    snprintf(buf, sizeof(buf), "%d." SHADER_EXT_FRAG, idx);
                    if (!strcmp(buf, d->d_name)) {
                        printf("compiling: '%s'\n", d->d_name);
                        
                        struct gl_sfbo* s = &stages[idx - 1];
                        *s = (struct gl_sfbo) {
                            .name     = strdup(d->d_name),
                            .shader   = 0,
                            .valid    = false,
                            .binds    = malloc(1),
                            .binds_sz = 0
                        };

                        current = s;
                        GLuint id = shaderbuild(shaders, handlers, shader_version, d->d_name);
                        if (!id) {
                            abort();
                        }

                        s->shader = id;

                        /* Only setup a framebuffer and texture if this isn't the final step,
                           as it can rendered directly */
                        if (idx != count)
                            setup_sfbo(&stages[idx - 1], w, h);

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
    
    gl->stages = stages;
    gl->stages_sz = count;
    
    /* target seconds per update */
    gl->target_spu = (float) (r->samplesize_request / 4) / (float) r->rate_request;
    
    gl->audio_tex_r = create_1d_tex();
    gl->audio_tex_l = create_1d_tex();

    if (gl->interpolate) {
        size_t isz = (r->bufsize_request / gl->bufscale) * sizeof(float);
        float* ibuf = malloc(isz * 4);
        gl->interpolate_buf[0] = &ibuf[isz * 0];
        gl->interpolate_buf[1] = &ibuf[isz * 1];
        gl->interpolate_buf[2] = &ibuf[isz * 2];
        gl->interpolate_buf[3] = &ibuf[isz * 3];
    }
    

    {
        gl->t_data = malloc(sizeof(void*) * t_count);
        size_t t;
        for (t = 0; t < t_count; ++t) {
            gl->t_data[t] = NULL;
        }
    }

    overlay(&gl->overlay);

    glfwShowWindow(gl->w);
    
    return r;
}

void rd_time(struct renderer* r) {
    struct gl_data* gl = r->gl;
    
    glfwSetTime(0.0D); /* reset time for measuring this frame */
}

#define IB_START_LEFT 0
#define IB_END_LEFT 1
#define IB_START_RIGHT 2
#define IB_END_RIGHT 3

void rd_update(struct renderer* r, float* lb, float* rb, size_t bsz, bool modified) {
    struct gl_data* gl = r->gl;
    size_t t, a, fbsz = bsz * sizeof(float);
    
    r->alive = !glfwWindowShouldClose(gl->w);
    if (!r->alive)
        return;

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
    float ilb[gl->interpolate ? bsz : 0], irb[gl->interpolate ? bsz : 0];
    if (gl->interpolate) {
        for (t = 0; t < bsz; ++t) {
            /* Obtain start/end values at this index for left & right buffers */
            float
                ilbs = gl->interpolate_buf[IB_START_LEFT ][t],
                ilbe = gl->interpolate_buf[IB_END_LEFT   ][t],
                irbs = gl->interpolate_buf[IB_START_RIGHT][t],
                irbe = gl->interpolate_buf[IB_END_RIGHT  ][t],
                mod  = (gl->ur / gl->fr) * gl->kcounter; /* modifier for this frame */
            if (mod > 1.0F) mod = 1.0F;
            ilb[t] = ilbs + ((ilbe - ilbs) * mod);
            irb[t] = irbs + ((irbe - irbs) * mod);
        }
    }
    
    /* Resize screen textures if needed */
    int ww, wh;
    glfwGetFramebufferSize(gl->w, &ww, &wh);
    if (ww != gl->lww || wh != gl->lwh) {
        for (t = 0; t < gl->stages_sz; ++t) {
            if (gl->stages[t].valid) {
                setup_sfbo(&gl->stages[t], ww, wh);
            }
        }
        gl->lww = ww;
        gl->lwh = wh;
    }
        
    glViewport(0, 0, ww, wh);
        
    struct gl_sfbo* prev;

    /* Iterate through each rendering stage (shader) */
    
    for (t = 0; t < gl->stages_sz; ++t) {

        bool needed[64] = { [ 0 ... 63 ] = false }; /* Load flags for each texture position */
        
        /* Select the program associated with this pass */
        struct gl_sfbo* current = &gl->stages[t];
        glUseProgram(current->shader);
            
        /* Bind framebuffer if this is not the final pass */
        if (current->valid)
            glBindFramebuffer(GL_FRAMEBUFFER, current->fbo);
        
        glClear(GL_COLOR_BUFFER_BIT);

        bool prev_bound = false;
        
        /* Iterate through each uniform binding, transforming and passing the 
           data into the shader. */
        
        size_t b, c = 0;
        for (b = 0; b < current->binds_sz; ++b) {
            struct gl_bind* bind = &current->binds[b];

            /* Handle transformations and bindings for 1D samplers */
            void handle_1d_tex(GLuint tex, float* buf, float* ubuf, size_t sz, int offset) {

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
                
                if (!needed[offset]) {
                    glActiveTexture(GL_TEXTURE0 + offset);
                    update_1d_tex(tex, sz, gl->interpolate ? (ubuf ? ubuf : buf) : buf);
                    glBindTexture(GL_TEXTURE_1D, tex);
                    needed[offset] = true;
                }
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
            case SRC_AUDIO_L:  handle_1d_tex(gl->audio_tex_l, lb, ilb, bsz, 1);         break;
            case SRC_AUDIO_R:  handle_1d_tex(gl->audio_tex_r, rb, irb, bsz, 2);         break;
            case SRC_AUDIO_SZ: glUniform1i(bind->uniform, bsz);                    break;
            case SRC_SCREEN:   glUniform2i(bind->uniform, (GLint) ww, (GLint) wh); break;
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
    glfwSwapBuffers(gl->w);
    glfwPollEvents();

    double duration = glfwGetTime(); /* frame execution time */

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
    }
}

void rd_destroy(struct renderer* r) {
    glfwTerminate();
    free(r->gl);
    free(r);
}
