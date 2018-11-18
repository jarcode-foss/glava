#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <poll.h>

#include "fifo.h"

/* Implementation struct storage */

typeof(*audio_impls) audio_impls[sizeof(audio_impls) / sizeof(struct audio_impl*)] = {};
size_t audio_impls_idx = 0;

/* FIFO backend */

static void init(struct audio_data* audio) {
    if (!audio->source) {
        audio->source = strdup("/tmp/mpd.fifo");
    }
}

static void* entry(void* data) {
    struct audio_data* audio = (struct audio_data *) data;
    
    float* bl = (float*) audio->audio_out_l;
    float* br = (float*) audio->audio_out_r;
    size_t fsz = audio->audio_buf_sz;
    size_t ssz = audio->sample_sz;
    
    int fd;
    int16_t buf[ssz / 2];
    size_t q;
    int timeout = 50;
    
    struct timespec tv_last = {}, tv;
    bool measured = false;
    
    if ((fd = open(audio->source, O_RDONLY)) == -1) {
        fprintf(stderr, "failed to open FIFO audio source \"%s\": %s\n", audio->source, strerror(errno));
        exit(EXIT_FAILURE);
    }

    struct pollfd pfd = {
        .fd     = fd,
        .events = POLLIN
    };

    size_t buffer_offset = (fsz - (ssz / 4));
    
    while (true) {

        /* The poll timeout is set to accommodate an approximate UPS, but has little purpose except
           for effectively setting the rate of empty samples in the event of the FIFO descriptor
           blocking for long periods of time. */
        
        switch (poll(&pfd, 1, timeout)) {
            case -1:
                fprintf(stderr, "FIFO backend: poll() failed (%s)\n", strerror(errno));
                exit(EXIT_FAILURE);
            case 0:
                pthread_mutex_lock(&audio->mutex);
                
                memmove(bl, &bl[ssz / 4], buffer_offset * sizeof(float));
                memmove(br, &br[ssz / 4], buffer_offset * sizeof(float));
                
                for (q = 0; q < (ssz / 4); ++q) bl[buffer_offset + q] = 0;
                for (q = 0; q < (ssz / 4); ++q) br[buffer_offset + q] = 0;
            
                audio->modified = true;
                
                pthread_mutex_unlock(&audio->mutex);
                break;
            default: {
                read(fd, buf, sizeof(buf));
                clock_gettime(CLOCK_REALTIME, measured ? &tv : &tv_last);
                if (measured) {
                    /* Set the timeout slightly higher than the delay between samples to prevent empty writes */
                    timeout = (((tv.tv_sec - tv_last.tv_sec) * 1000) + ((tv.tv_nsec - tv_last.tv_nsec) / 1000000)) + 1;
                    tv_last = tv;
                } else measured = true;
            
                pthread_mutex_lock(&audio->mutex);
                
                memmove(bl, &bl[ssz / 4], buffer_offset * sizeof(float));
                memmove(br, &br[ssz / 4], buffer_offset * sizeof(float));
            
                for (size_t n = 0, q = 0; q < (ssz / 2); q += 2) {

                    size_t idx = (fsz - (ssz / 4)) + n;
                
                    if (audio->channels == 1) {
                        float sample = ((buf[q] + buf[q + 1]) / 2) / (float) 65535;
                        bl[idx] = sample;
                        br[idx] = sample;
                    }
                
                    if (audio->channels == 2) {
                        bl[idx] = buf[q] / (float) 65535;
                        br[idx] = buf[q + 1] / (float) 65535;
                    }
                
                    n++;
                }
            
                audio->modified = true;
            
                pthread_mutex_unlock(&audio->mutex);
                break;
            }
        }
        
        if (audio->terminate == 1) {
            close(fd);
            break;
        }
        
    }
    
    return 0;
}

AUDIO_ATTACH(fifo);
