#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
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

#define GLAVA_VERSION "1.0"

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
#define SHADER_INSTALL_PATH "/etc/xdg/glava"
#define SHADER_USER_PATH FORMAT("%s/glava", ENV("XDG_CONFIG_HOME", "%s/.config", ENV("HOME", "/home")))
/* OSX */
#elif (defined(__APPLE__) && defined(__MACH__)) || defined(GLAVA_OSX)
#define SHADER_INSTALL_PATH "/Library/glava"
#define SHADER_USER_PATH FORMAT("%s/Library/Preferences/glava", ENV("HOME", "/"))
#else
#error "Unsupported target system"
#endif

/* Copy installed shaders/configuration from the installed location
   (usually /etx/xdg). Modules (folders) will be linked instead of
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
        case 1:
            {
                int source = -1, dest = -1;
                uint8_t buf[pgsz];
                ssize_t r, t, w, a;
                if ((source = open(p, O_RDONLY)) < 0) {
                    fprintf(stderr, "failed to open (source) '%s': %s\n", p, strerror(errno));
                    goto cleanup;
                }
                if ((dest = open(f, O_WRONLY | O_CREAT, ACCESSPERMS)) < 0) {
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
            if (symlink(p, f))
                fprintf(stderr, "failed to symlink '%s' -> '%s': %s\n", p, f, strerror(errno));
            else if (verbose)
                printf("symlink '%s' -> '%s'\n", p, f);
            break;
        }
    }
    closedir(dir);
}

static const char* help_str =
    "Usage: %s [OPTIONS]...\n"
    "Opens a window with an OpenGL context to draw an audio visualizer.\n"
    "\n"
    "Available arguments:\n"
    "-h, --help              show this help and exit\n"
    "-v, --verbose           enables printing of detailed information about execution\n"
    "-m, --force-mod=NAME    forces the specified module to load instead, ignoring any\n"
    "                          `#request mod` instances in the entry point.\n"
    "-e, --entry=NAME        specifies the name of the file to look for when loading shaders,\n"
    "                          by default this is \"rc.glsl\".\n"
    "-C, --copy-config       creates copies and symbolic links in the user configuration\n"
    "                          directory for glava, copying any files in the root directory\n"
    "                          of the installed shader directory, and linking any modules.\n"
    "\n"
    "GLava (glava) " GLAVA_VERSION "\n"
    " -- Copyright (C) 2017 Levi Webb\n";

static const char* opt_str = "hve:Cm:";
static struct option p_opts[] = {
    {"help",        no_argument,       0, 'h'},
    {"verbose",     no_argument,       0, 'v'},
    {"entry",       required_argument, 0, 'e'},
    {"force-mod",   required_argument, 0, 'm'},
    {"copy-config", no_argument,       0, 'C'},
    {0,             0,                 0,  0 }
};

int main(int argc, char** argv) {

    /* Evaluate these macros only once, since they allocate */
    const char* install_path = SHADER_INSTALL_PATH;
    const char* user_path    = SHADER_USER_PATH;
    const char* entry        = "rc.glsl";
    const char* force        = NULL;
    const char* system_shader_paths[] = { user_path, install_path, NULL };
    bool verbose = false;
    bool copy_mode = false;
    
    int c, idx, n = 0;
    while ((c = getopt_long(argc, argv, opt_str, p_opts, &idx)) != -1) {
        switch (c) {
        case 'v': verbose   = true;   break;
        case 'C': copy_mode = true;   break;
        case 'e': entry     = optarg; break;
        case 'm': force     = optarg; break;
        case '?': exit(EXIT_FAILURE); break;
        default:
        case 'h':
            printf(help_str, argc > 0 ? argv[0] : "glava");
            exit(EXIT_SUCCESS);
        }
    }

    if (copy_mode) {
        copy_cfg(install_path, user_path, verbose);
        exit(EXIT_SUCCESS);
    }

    renderer* r = rd_new(system_shader_paths, entry, force);

    float b0[r->bufsize_request], b1[r->bufsize_request];
    size_t t;
    for (t = 0; t < r->bufsize_request; ++t) {
        b0[t] = 0.0F;
        b1[t] = 0.0F;
    }
    
    struct audio_data audio = {
        .source = ({
                char* src = NULL;
                if (r->audio_source_request && strcmp(r->audio_source_request, "auto") != 0) {
                    src = strdup(r->audio_source_request);
                }
                src;
            }),
        .rate         = (unsigned int) r->rate_request,
        .format       = -1,
        .terminate    = 0,
        .channels     = 2,
        .audio_out_r  = b0,
        .audio_out_l  = b1,
        .mutex        = PTHREAD_MUTEX_INITIALIZER,
        .audio_buf_sz = r->bufsize_request,
        .sample_sz    = r->samplesize_request,
        .modified     = false
    };
    if (!audio.source) {
        get_pulse_default_sink(&audio);
        printf("Using default PulseAudio sink: %s\n", audio.source);
    }
    
    pthread_t thread;
    int thread_id = pthread_create(&thread, NULL, input_pulse, (void*) &audio);
    
    float lb[r->bufsize_request], rb[r->bufsize_request];
    while (r->alive) {

        rd_time(r); /* update timer for this frame */
        
        bool modified; /* if the audio buffer has been updated by the streaming thread */

        /* lock the audio mutex and read our data */
        pthread_mutex_lock(&audio.mutex);
        modified = audio.modified;
        if (modified) {
            /* create our own copies of the audio buffers, so the streaming thread can continue to append to it */
            memcpy(lb, (void*) audio.audio_out_l, r->bufsize_request * sizeof(float));
            memcpy(rb, (void*) audio.audio_out_r, r->bufsize_request * sizeof(float));
            audio.modified = false; /* set this flag to false until the next time we read */
        }
        pthread_mutex_unlock(&audio.mutex);
        
        /* Only render if needed (ie. stop rendering when fullscreen windows are focused) */
        if (xwin_should_render()) {
            rd_update(r, lb, rb, r->bufsize_request, modified);
        } else {
            /* Sleep for 50ms and then attempt to render again */
            struct timespec tv = {
                .tv_sec = 0, .tv_nsec = 50 * 1000000
            };
            nanosleep(&tv, NULL);
        }
    }

    rd_destroy(r);
}
