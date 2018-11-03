#ifdef GLAVA_JACK_SUPPORT
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <jack/jack.h>

jack_port_t *left_input_port;
jack_port_t *right_input_port = NULL;
jack_client_t *client;

#include "fifo.h"

int process(jack_nframes_t nframes, void *arg) {

    struct audio_data* audio = (struct audio_data*) arg;

    float* bl = (float*) audio->audio_out_l;
    float* br = (float*) audio->audio_out_r;
    size_t fsz = audio->audio_buf_sz;

    pthread_mutex_lock(&audio->mutex);

    memmove(bl, &bl[nframes], (fsz - nframes) * sizeof(float));
    memmove(br, &br[nframes], (fsz - nframes) * sizeof(float));

    jack_default_audio_sample_t *left_in, *right_in;

    left_in = (jack_default_audio_sample_t*) jack_port_get_buffer (left_input_port, nframes);
    if (right_input_port) {
        right_in = (jack_default_audio_sample_t*) jack_port_get_buffer (right_input_port, nframes);
    }

    for (unsigned int i = 0; i < nframes; ++i) {

        size_t idx = (fsz - nframes) + i;
        bl[idx] = left_in[i];

        if (audio->channels == 1) {
            br[idx] = left_in[i];
        }

        /* stereo storing channels in buffer */
        if (audio->channels == 2) {
            br[idx] = right_in[i];
        }
    }
    audio->modified = true;

    pthread_mutex_unlock(&audio->mutex);
    return 0;
}

void jack_shutdown(void *arg) {
    // exit(1);
}

void init_jack_client(struct audio_data* audio) {
    jack_status_t status;

    client = jack_client_open("glava", JackNullOption, &status);
    if (client == NULL) {
        fprintf(stderr, "jack_client_open() failed, "
                "status = 0x%2.0x\n", status);
        if (status & JackServerFailed) {
            fprintf(stderr, "Unable to connect to JACK server\n");
        }
        exit(EXIT_FAILURE);
    }
    if (status & JackServerStarted) {
        fprintf(stderr, "JACK server started\n");
    }

    jack_set_process_callback(client, process, audio);
    jack_on_shutdown(client, jack_shutdown, 0);

    audio->rate = jack_get_sample_rate(client);
    audio->sample_sz = jack_get_buffer_size(client) * 4;

    printf("JACK: sample rate/size was overwritten, new values: %i, %i\n",
           (int) audio->rate, (int) audio->sample_sz);

    if (audio->sample_sz / 4 > audio->audio_buf_sz) {
        printf("ERROR: audio buffer is too small: %i\n", audio->audio_buf_sz);
        exit(EXIT_FAILURE);
    }

    left_input_port = jack_port_register(client, "L",
                                         JACK_DEFAULT_AUDIO_TYPE,
                                         JackPortIsInput, 0);

    if (audio->channels == 2) {
        right_input_port = jack_port_register(client, "R",
                                              JACK_DEFAULT_AUDIO_TYPE,
                                              JackPortIsInput, 0);
    }

    if (jack_activate(client)) {
        fprintf(stderr, "Cannot activate jack client\n");
        exit(EXIT_FAILURE);
    }
}

void close_jack_client() {
    jack_client_close (client);
}
#endif
