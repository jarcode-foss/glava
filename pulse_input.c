#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/pulseaudio.h>

#include "fifo.h"

static pa_mainloop* m_pulseaudio_mainloop;

static void cb(__attribute__((unused)) pa_context* pulseaudio_context,
               const pa_server_info* i,
               void* userdata) {

	/* Obtain default sink name */
    struct audio_data* audio = (struct audio_data*) userdata;
	audio->source = malloc(sizeof(char) * 1024);

	strcpy(audio->source,i->default_sink_name);

	/* Append `.monitor` suffix */
	audio->source = strcat(audio->source, ".monitor");

	/* Quiting mainloop */
    pa_context_disconnect(pulseaudio_context);
    pa_context_unref(pulseaudio_context);
	pa_mainloop_quit(m_pulseaudio_mainloop, 0);
    pa_mainloop_free(m_pulseaudio_mainloop);
}


static void pulseaudio_context_state_callback(pa_context* pulseaudio_context, void* userdata) {

	/* Ensure loop is ready	*/
	switch (pa_context_get_state(pulseaudio_context)) {
        case PA_CONTEXT_UNCONNECTED:  break;
        case PA_CONTEXT_CONNECTING:   break;
        case PA_CONTEXT_AUTHORIZING:  break;
        case PA_CONTEXT_SETTING_NAME: break;
        case PA_CONTEXT_READY: /* extract default sink name */
            pa_operation_unref(pa_context_get_server_info(pulseaudio_context, cb, userdata));
            break;
        case PA_CONTEXT_FAILED:
            printf("failed to connect to pulseaudio server\n");
            exit(EXIT_FAILURE);
            break;
        case PA_CONTEXT_TERMINATED:
            pa_mainloop_quit(m_pulseaudio_mainloop, 0);
            break;	  
    }
}


static void init(struct audio_data* audio) {

    if (audio->source) return;
    
	pa_mainloop_api* mainloop_api;
	pa_context* pulseaudio_context;
	int ret;

	/* Create a mainloop API and connection to the default server */
	m_pulseaudio_mainloop = pa_mainloop_new();

	mainloop_api = pa_mainloop_get_api(m_pulseaudio_mainloop);
	pulseaudio_context = pa_context_new(mainloop_api, "glava device list");


	/* Connect to the PA server */
	pa_context_connect(pulseaudio_context, NULL, PA_CONTEXT_NOFLAGS,
                       NULL);

	/* Define a callback so the server will tell us its state */
	pa_context_set_state_callback(pulseaudio_context,
                                  pulseaudio_context_state_callback,
                                  (void*)audio);

	/* Start mainloop to get default sink */

	/* Start with one non blocking iteration in case pulseaudio is not able to run */
	if (!(ret = pa_mainloop_iterate(m_pulseaudio_mainloop, 0, &ret))){
        printf("Could not open pulseaudio mainloop to "
               "find default device name: %d\n"
               "check if pulseaudio is running\n",
               ret);

        exit(EXIT_FAILURE);
    }

	pa_mainloop_run(m_pulseaudio_mainloop, &ret);
	

}

/* Sample format for native 'float' type */
#ifndef __STDC_IEC_559__
#error "IEC 60559 standard unsupported on target system"
#endif

#ifdef __ORDER_LITTLE_ENDIAN__
#define FSAMPLE_FORMAT PA_SAMPLE_FLOAT32LE
#elif __ORDER_BIG_ENDIAN__
#define FSAMPLE_FORMAT PA_SAMPLE_FLOAT32BE
#else
#error "Unsupported float format (requires 32 bit IEEE (little or big endian) floating point support)"
#endif

static void* entry(void* data) {
    struct audio_data* audio = (struct audio_data*) data;
    int i, n;
    size_t ssz = audio->sample_sz;
	float buf[ssz / 2];
    
	const pa_sample_spec ss = {
		.format   = FSAMPLE_FORMAT,
		.rate     = audio->rate,
		.channels = 2
    };
	const pa_buffer_attr pb = {
        .maxlength = (uint32_t) -1,
        .fragsize  = ssz
	};
    
	pa_simple* s = NULL;
	int error;
    
	if (!(s = pa_simple_new(NULL, "glava", PA_STREAM_RECORD,
                            audio->source, "audio for glava",
                            &ss, NULL, &pb, &error))) {
		fprintf(stderr, __FILE__ ": Could not open pulseaudio source: %s, %s. "
                "To find a list of your pulseaudio sources run 'pacmd list-sources'\n",
                audio->source, pa_strerror(error));
		exit(EXIT_FAILURE);
	}
    
	n = 0;
    
    float* bl = (float*) audio->audio_out_l;
    float* br = (float*) audio->audio_out_r;
    size_t fsz = audio->audio_buf_sz;
    
	while (1) {
        
        /* Record some data ... */
        if (pa_simple_read(s, buf, sizeof(buf), &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
        	exit(EXIT_FAILURE);
		}

        pthread_mutex_lock(&audio->mutex);

        /* progressing the audio buffer, making space for new write */

        memmove(bl, &bl[ssz / 4], (fsz - (ssz / 4)) * sizeof(float));
        memmove(br, &br[ssz / 4], (fsz - (ssz / 4)) * sizeof(float)); 

        /* sorting out channels */
        
        for (n = 0, i = 0; i < ssz / 2; i += 2) {

            /* size_t idx = (i / 2) + (at * (BUFSIZE / 2)); */

            int idx = (fsz - (ssz / 4)) + n;

            if (audio->channels == 1) {
                float sample = (buf[i] + buf[i + 1]) / 2;
                bl[idx] = sample;
                br[idx] = sample;
            }

            /* stereo storing channels in buffer */
            if (audio->channels == 2) {
                bl[idx] = buf[i];
                br[idx] = buf[i + 1];
            }
            ++n;
        }
        audio->modified = true;
        
        pthread_mutex_unlock(&audio->mutex);
        
        if (audio->terminate == 1) {
            pa_simple_free(s);
            break;
        }
    }
    
	return 0;
}

AUDIO_ATTACH(pulseaudio);
