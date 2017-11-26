#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>

#include "fifo.h"
#include "pulse_input.h"
#include "render.h"
#include "xwincheck.h"

int main(int argc, char** argv) {
    const char* audio_source = argc >= 2 ? argv[1] : NULL; //TODO: change

    renderer* r = rd_new(0, 0, 400, 500, "shaders");

    float b0[r->bufsize_request], b1[r->bufsize_request];
    size_t t;
    for (t = 0; t < r->bufsize_request; ++t) {
        b0[t] = 0.0F;
        b1[t] = 0.0F;
    }
    
    struct audio_data audio = {
        .source = ({
                char* src = NULL;
                if (audio_source && strcmp(audio_source, "auto") != 0) {
                    src = malloc(1 + strlen(audio_source));
                    strcpy(src, audio_source);
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
