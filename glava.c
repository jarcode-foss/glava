#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>

#include "fifo.h"
#include "pulse_input.h"
#include "render.h"
#include "xwin.h"


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
#define SHADER_INSTALL_PATH FORMAT("%s/glava", ENV("XDG_CONFIG_DIRS", "/etc/xdg"))
#define SHADER_USER_PATH FORMAT("%s/glava", ENV("XDG_CONFIG_HOME", "%s/.config", ENV("HOME", "/home")))
/* OSX */
#elif (defined(__APPLE__) && defined(__MACH__)) || defined(GLAVA_OSX)
#define SHADER_INSTALL_PATH "/Library/glava"
#define SHADER_USER_PATH FORMAT("%s/Library/Preferences/glava", ENV("HOME", "/"))
#else
#error "Unsupported target system"
#endif

int main(int argc, char** argv) {

    const char* entry = "rc.glsl";
    const char* system_shader_paths[] = { SHADER_INSTALL_PATH, SHADER_USER_PATH, NULL };

    renderer* r = rd_new(system_shader_paths, entry);

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
