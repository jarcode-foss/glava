#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <getopt.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "fifo.h"
#include "pulse_input.h"
#include "render.h"
#include "xwin.h"

#ifdef GLAD_DEBUG
#define GLAVA_RELEASE_TYPE_PREFIX "debug, "
#else
#define GLAVA_RELEASE_TYPE_PREFIX "stable, "
#endif
#ifdef GLAVA_STANDALONE
#define GLAVA_RELEASE_TYPE_BUILD "standalone"
#elif GLAVA_UNIX
#define GLAVA_RELEASE_TYPE_BUILD "unix/fhs"
#elif GLAVA_OSX
#define GLAVA_RELEASE_TYPE_BUILD "osx"
#else
#define GLAVA_RELEASE_TYPE_BUILD "?"
#endif
#define GLAVA_RELEASE_TYPE GLAVA_RELEASE_TYPE_PREFIX GLAVA_RELEASE_TYPE_BUILD

#define FORMAT(...)                             \
    ({                                          \
        char* buf = malloc(PATH_MAX);           \
        snprintf(buf, PATH_MAX, __VA_ARGS__);   \
        buf;                                    \
    })

#define ENV(e, ...)                             \
    ({                                          \
        const char* _e = getenv(e);             \
        if (!_e)                                \
            _e = FORMAT(__VA_ARGS__);           \
        _e;                                     \
    })

#ifdef GLAVA_STANDALONE
#define SHADER_INSTALL_PATH "shaders"
#define SHADER_USER_PATH "userconf"
/* FHS compliant systems */
#elif defined(__unix__) || defined(GLAVA_UNIX)
#ifndef SHADER_INSTALL_PATH
#define SHADER_INSTALL_PATH "/etc/xdg/glava"
#endif
#define SHADER_USER_PATH FORMAT("%s/glava", ENV("XDG_CONFIG_HOME", "%s/.config", ENV("HOME", "/home")))
/* OSX */
#elif (defined(__APPLE__) && defined(__MACH__)) || defined(GLAVA_OSX)
#ifndef SHADER_INSTALL_PATH
#define SHADER_INSTALL_PATH "/Library/glava"
#endif
#define SHADER_USER_PATH FORMAT("%s/Library/Preferences/glava", ENV("HOME", "/"))
#else
#error "Unsupported target system"
#endif

#ifndef ACCESSPERMS
#define ACCESSPERMS (S_IRWXU|S_IRWXG|S_IRWXO) /* 0777 */
#endif

/* Copy installed shaders/configuration from the installed location
   (usually /etc/xdg). Modules (folders) will be linked instead of
   copied. */
static void copy_cfg(const char* path, const char* dest, bool verbose) {
    size_t
        sl   = strlen(path),
        tl   = strlen(dest),
        pgsz = (size_t) getpagesize(); /* optimal buffer size */
    DIR* dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "'%s' does not exist\n", path);
        exit(EXIT_FAILURE);
    }

    umask(~(S_IRWXU | S_IRGRP | S_IROTH | S_IXGRP | S_IXOTH));
    if (mkdir(dest, ACCESSPERMS) && errno != EEXIST) {
        fprintf(stderr, "could not create directory '%s': %s\n", dest, strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    struct dirent* d;
    while ((d = readdir(dir)) != NULL) {
        if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
            continue;
        int type = 0;
        size_t
            dl = strlen(d->d_name),
            pl = sl + dl + 2,
            fl = tl + dl + 2;
        char p[pl], f[fl];
        snprintf(p, pl, "%s/%s", path, d->d_name);
        snprintf(f, fl, "%s/%s", dest, d->d_name);
        
        if (d->d_type != DT_UNKNOWN) /* don't bother with stat if we already have the type */
            type = d->d_type == DT_REG ? 1 : (d->d_type == DT_DIR ? 2 : 0);
        else {
            struct stat st;
            if (lstat(p, &st)) {
                fprintf(stderr, "failed to stat '%s': %s\n", p, strerror(errno));
            } else
                type = S_ISREG(st.st_mode) ? 1 : (S_ISDIR(st.st_mode) ? 2 : 0);
        }
        
        switch (type) {
            case 1: {
                int source = -1, dest = -1;
                uint8_t buf[pgsz];
                ssize_t r, t, w, a;
                if ((source = open(p, O_RDONLY)) < 0) {
                    fprintf(stderr, "failed to open (source) '%s': %s\n", p, strerror(errno));
                    goto cleanup;
                }
                if ((dest = open(f, O_TRUNC | O_WRONLY | O_CREAT, ACCESSPERMS)) < 0) {
                    fprintf(stderr, "failed to open (destination) '%s': %s\n", f, strerror(errno));
                    goto cleanup;
                }
                for (t = 0; (r = read(source, buf, pgsz)) > 0; t += r) {
                    for (a = 0; a < r && (w = write(dest, buf + a, r - a)) > 0; a += w);
                    if (w < 0) {
                        fprintf(stderr, "error while writing '%s': %s\n", f, strerror(errno));
                        goto cleanup;
                    }
                }
                if (r < 0) {
                    fprintf(stderr, "error while reading '%s': %s\n", p, strerror(errno));
                    goto cleanup;
                }
                if (verbose)
                    printf("copy '%s' -> '%s'\n", p, f);
            cleanup:
                if (source > 0) close(source);
                if (dest > 0) close(dest);
            }
            break;
            case 2:
                if (symlink(p, f) && errno != EEXIST)
                    fprintf(stderr, "failed to symlink '%s' -> '%s': %s\n", p, f, strerror(errno));
                else if (verbose)
                    printf("symlink '%s' -> '%s'\n", p, f);
                break;
        }
    }
    closedir(dir);
}

#define GLAVA_VERSION_STRING "GLava (glava) " GLAVA_VERSION " (" GLAVA_RELEASE_TYPE ")"

static const char* help_str =
    "Usage: %s [OPTIONS]...\n"
    "Opens a window with an OpenGL context to draw an audio visualizer.\n"
    "\n"
    "Available arguments:\n"
    "-h, --help               show this help and exit\n"
    "-v, --verbose            enables printing of detailed information about execution\n"
    "-d, --desktop            enables running glava as a desktop window by detecting the\n"
    "                           desktop environment and setting the appropriate properties\n"
    "                           automatically. Can override properties in \"rc.glsl\".\n"
    "-r, --request=REQUEST    evaluates the specified request after loading \"rc.glsl\".\n"
    "-m, --force-mod=NAME     forces the specified module to load instead, ignoring any\n"
    "                           `#request mod` instances in the entry point.\n"
    "-e, --entry=FILE         specifies the name of the file to look for when loading shaders,\n"
    "                           by default this is \"rc.glsl\".\n"
    "-C, --copy-config        creates copies and symbolic links in the user configuration\n"
    "                           directory for glava, copying any files in the root directory\n"
    "                           of the installed shader directory, and linking any modules.\n"
    "-b, --backend            specifies a window creation backend to use. By default, the most\n"
    "                           appropriate backend will be used for the underlying windowing\n"
    "                           system.\n"
    "-a, --audio=BACKEND      specifies an audio input backend to use.\n"
    "-p, --pipe[=BIND[:TYPE]] binds value(s) to be read from stdin. The input my be read using\n"
    "                           `@name` or `@name:default` syntax within shader sources.\n"
    "                           A stream of inputs (each overriding the previous) must be\n"
    "                           assigned with the `name = value` syntax and separated by\n"
    "                           newline (\'\\n\') characters.\n"
    "-V, --version            print application version and exit\n"
    "\n"
    "The REQUEST argument is evaluated identically to the \'#request\' preprocessor directive\n"
    "in GLSL files.\n"
    "\n"
    "The DEFINE argument is appended to the associated file before it is processed. It is\n"
    "evaluated identically to the \'#define' preprocessor directive.\n"
    "\n"
    "The FILE argument may be any file path. All specified file paths are relative to the\n"
    "active configuration root (usually ~/.config/glava if present).\n"
    "\n"
    "The BACKEND argument may be any of the following strings (for this particular build):\n"
    "%s"
    "\n"
    "The BIND argument must a valid GLSL identifier."
    "\n"
    "The TYPE argument must be a valid GLSL type. If `--pipe` is used without a \n"
    "type argument, the default type is `vec4` (type used for RGBA colors).\n"
    "\n"
    GLAVA_VERSION_STRING "\n";

static const char* opt_str = "dhvVe:Cm:b:r:a:i::p::";
static struct option p_opts[] = {
    {"help",        no_argument,       0, 'h'},
    {"verbose",     no_argument,       0, 'v'},
    {"desktop",     no_argument,       0, 'd'},
    {"audio",       required_argument, 0, 'a'},
    {"request",     required_argument, 0, 'r'},
    {"entry",       required_argument, 0, 'e'},
    {"force-mod",   required_argument, 0, 'm'},
    {"copy-config", no_argument,       0, 'C'},
    {"backend",     required_argument, 0, 'b'},
    {"pipe",        optional_argument, 0, 'p'},
    {"stdin",       optional_argument, 0, 'i'},
    {"version",     no_argument,       0, 'V'},
    #ifdef GLAVA_DEBUG
    {"run-tests",   no_argument,       0, 'T'},
    #endif
    {0,             0,                 0,  0 }
};

static renderer* rd = NULL;

static void handle_term(int signum) {
    if (rd->alive) {
        puts("\nInterrupt recieved, closing...");
        rd->alive = false;
    }
}

#define append_buf(buf, sz_store, ...)                      \
    ({                                                      \
        buf = realloc(buf, ++(*sz_store) * sizeof(*buf));   \
        buf[*sz_store - 1] = __VA_ARGS__;                   \
    })

int main(int argc, char** argv) {

    /* Evaluate these macros only once, since they allocate */
    const char
        * install_path    = SHADER_INSTALL_PATH,
        * user_path       = SHADER_USER_PATH,
        * entry           = "rc.glsl",
        * force           = NULL,
        * backend         = NULL,
        * audio_impl_name = "pulseaudio";
    const char* system_shader_paths[] = { user_path, install_path, NULL };
    int stdin_type = STDIN_TYPE_NONE;
    
    char**          requests    = malloc(1);
    size_t          requests_sz = 0;
    struct rd_bind* binds       = malloc(1);
    size_t          binds_sz    = 0;
    
    bool verbose = false, copy_mode = false, desktop = false;
    
    int c, idx;
    while ((c = getopt_long(argc, argv, opt_str, p_opts, &idx)) != -1) {
        switch (c) {
            case 'v': verbose   = true;   break;
            case 'C': copy_mode = true;   break;
            case 'd': desktop   = true;   break;
            case 'r': append_buf(requests, &requests_sz, optarg); break;
            case 'e': entry           = optarg; break;
            case 'm': force           = optarg; break;
            case 'b': backend         = optarg; break;
            case 'a': audio_impl_name = optarg; break;
            case '?': exit(EXIT_FAILURE); break;
            case 'V':
                puts(GLAVA_VERSION_STRING);
                exit(EXIT_SUCCESS);
                break;
            default:
            case 'h': {
                char buf[2048];
                size_t bsz = 0;
                for (size_t t = 0; t < audio_impls_idx; ++t)
                    bsz += snprintf(buf + bsz, sizeof(buf) - bsz, "\t\"%s\"%s\n", audio_impls[t]->name,
                                    !strcmp(audio_impls[t]->name, audio_impl_name) ? " (default)" : "");
                printf(help_str, argc > 0 ? argv[0] : "glava", buf);
                exit(EXIT_SUCCESS);
                break;
            }
            case 'p': {
                if (stdin_type != STDIN_TYPE_NONE) goto conflict_error;
                char* parsed_name = NULL;
                const char* parsed_type = NULL;
                if (optarg) {
                    size_t in_sz = strlen(optarg);
                    int sep = -1;
                    for (size_t t = 0; t < in_sz; ++t) {
                        switch (optarg[t]) {
                            case ' ': optarg[t] = '\0';    goto after;
                            case ':': sep       = (int) t; break;
                        }
                    }
                after:
                    if (sep >= 0) {
                        parsed_type = optarg + sep + 1;
                        optarg[sep] = '\0';
                    }
                    parsed_name = optarg;
                } else parsed_name = PIPE_DEFAULT;
                if (*parsed_name == '\0') {
                    fprintf(stderr, "Error: invalid pipe binding name: \"%s\"\n"
                            "Zero length names are not permitted.\n", parsed_name);
                    exit(EXIT_FAILURE);
                }
                for (char* c = parsed_name; *c != '\0'; ++c) {
                    switch (*c) {
                        case '0' ... '9':
                            if (c == parsed_name) {
                                fprintf(stderr, "Error: invalid pipe binding name: \"%s\" ('%c')\n"
                                        "Valid names may not start with a number.\n", parsed_name, *c);
                                exit(EXIT_FAILURE);
                            }
                        case 'a' ... 'z':
                        case 'A' ... 'Z':
                        case '_': continue;
                        default:
                            fprintf(stderr, "Error: invalid pipe binding name: \"%s\" ('%c')\n"
                                    "Valid names may only contain [a..z], [A..Z], [0..9] "
                                    "and '_' characters.\n", parsed_name, *c);
                            exit(EXIT_FAILURE);
                    }
                }
                for (size_t t = 0; t < binds_sz; ++t) {
                    if (!strcmp(binds[t].name, parsed_name)) {
                        fprintf(stderr, "Error: attempted to re-bind pipe argument: \"%s\"\n", parsed_name);
                        exit(EXIT_FAILURE);
                    }
                }
                int type = -1;
                if (parsed_type == NULL || strlen(parsed_type) == 0) {
                    type = STDIN_TYPE_VEC4;
                    parsed_type = bind_types[STDIN_TYPE_VEC4].n;
                } else {
                    for (size_t t = 0 ; bind_types[t].n != NULL; ++t) {
                        if (!strcmp(bind_types[t].n, parsed_type)) {
                            type = bind_types[t].i;
                            parsed_type = bind_types[t].n;
                            break;
                        }
                    }
                }
                if (type == -1) {
                    fprintf(stderr, "Error: Unsupported `--pipe` GLSL type: \"%s\"\n", parsed_type);
                    exit(EXIT_FAILURE);
                }
                struct rd_bind bd = {
                    .name  = parsed_name,
                    .type  = type,
                    .stype = parsed_type
                };
                append_buf(binds, &binds_sz, bd);
                break;
            }
            case 'i': {
                if (binds_sz > 0) goto conflict_error;
                fprintf(stderr, "Warning: `--stdin` is deprecated and will be "
                        "removed in a future release, use `--pipe` instead. \n");
                stdin_type = -1;
                if (optarg == NULL) {
                    stdin_type = STDIN_TYPE_VEC4;
                } else {
                    for (size_t t = 0 ; bind_types[t].n != NULL; ++t) {
                        if (!strcmp(bind_types[t].n, optarg)) {
                            stdin_type = bind_types[t].i;
                            break;
                        }
                    }
                }
                if (stdin_type == -1) {
                    fprintf(stderr, "Error: Unsupported `--stdin` GLSL type: \"%s\"\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            }
            conflict_error:
                fprintf(stderr, "Error: cannot use `--pipe` and `--stdin` together\n");
                exit(EXIT_FAILURE);
                #ifdef GLAVA_DEBUG
            case 'T': {
                entry = "test_rc.glsl";
                rd_enable_test_mode();
            }
                #endif
        }
    }

    if (copy_mode) {
        copy_cfg(install_path, user_path, verbose);
        exit(EXIT_SUCCESS);
    }

    /* Handle `--force` argument as a request override */
    if (force) {
        const size_t bsz = 5 + strlen(force);
        char* force_req_buf = malloc(bsz);
        snprintf(force_req_buf, bsz, "mod %s", force);
        append_buf(requests, &requests_sz, force_req_buf);
    }

    /* Null terminate array arguments */
    append_buf(requests, &requests_sz, NULL);
    append_buf(binds,    &binds_sz,    (struct rd_bind) { .name = NULL });

    rd = rd_new(system_shader_paths, entry, (const char**) requests,
                backend, binds, stdin_type, desktop, verbose);
    
    struct sigaction action = { .sa_handler = handle_term };
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);
    
    float b0[rd->bufsize_request], b1[rd->bufsize_request];
    size_t t;
    for (t = 0; t < rd->bufsize_request; ++t) {
        b0[t] = 0.0F;
        b1[t] = 0.0F;
    }
    
    struct audio_data audio = {
        .source = ({
                char* src = NULL;
                if (rd->audio_source_request && strcmp(rd->audio_source_request, "auto") != 0) {
                    src = strdup(rd->audio_source_request);
                }
                src;
            }),
        .rate         = (unsigned int) rd->rate_request,
        .format       = -1,
        .terminate    = 0,
        .channels     = rd->mirror_input ? 1 : 2,
        .audio_out_r  = b0,
        .audio_out_l  = b1,
        .mutex        = PTHREAD_MUTEX_INITIALIZER,
        .audio_buf_sz = rd->bufsize_request,
        .sample_sz    = rd->samplesize_request,
        .modified     = false
    };

    struct audio_impl* impl = NULL;
    
    for (size_t t = 0; t < audio_impls_idx; ++t) {
        if (!strcmp(audio_impls[t]->name, audio_impl_name)) {
            impl = audio_impls[t];
            break;
        }
    }

    if (!impl) {
        fprintf(stderr, "The specified audio backend (\"%s\") is not available.\n", audio_impl_name);
        exit(EXIT_FAILURE);
    }
    
    impl->init(&audio);
    
    if (verbose) printf("Using audio source: %s\n", audio.source);
    
    pthread_t thread;
    pthread_create(&thread, NULL, impl->entry, (void*) &audio);
    
    float lb[rd->bufsize_request], rb[rd->bufsize_request];
    while (rd->alive) {

        rd_time(rd); /* update timer for this frame */
        
        bool modified; /* if the audio buffer has been updated by the streaming thread */

        /* lock the audio mutex and read our data */
        pthread_mutex_lock(&audio.mutex);
        modified = audio.modified;
        if (modified) {
            /* create our own copies of the audio buffers, so the streaming
               thread can continue to append to it */
            memcpy(lb, (void*) audio.audio_out_l, rd->bufsize_request * sizeof(float));
            memcpy(rb, (void*) audio.audio_out_r, rd->bufsize_request * sizeof(float));
            audio.modified = false; /* set this flag to false until the next time we read */
        }
        pthread_mutex_unlock(&audio.mutex);

        bool ret = rd_update(rd, lb, rb, rd->bufsize_request, modified);
        
        if (!ret) {
            /* Sleep for 50ms and then attempt to render again */
            struct timespec tv = {
                .tv_sec = 0, .tv_nsec = 50 * 1000000
            };
            nanosleep(&tv, NULL);
        }
        #ifdef GLAVA_DEBUG
        if (ret && rd_get_test_mode())
            break;
        #endif
    }

    #ifdef GLAVA_DEBUG
    if (rd_get_test_mode()) {
        if (rd_test_evaluate(rd)) {
            fprintf(stderr, "Test results did not match expected output\n");
            fflush(stderr);
            exit(EXIT_FAILURE);
        }
    }
    #endif

    audio.terminate = 1;
    int return_status;
    if ((return_status = pthread_join(thread, NULL))) {
        fprintf(stderr, "Failed to join with audio thread: %s\n", strerror(return_status));
    }

    free(audio.source);
    rd_destroy(rd);
}
