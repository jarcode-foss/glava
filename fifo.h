
#include <pthread.h>

struct audio_data {
    volatile float* audio_out_r;
    volatile float* audio_out_l;
    bool modified;
    bool update;
    size_t audio_buf_sz, sample_sz;
    int format;
    unsigned int rate;
    char *source; // pulse source
    int channels;
	int terminate; // shared variable used to terminate audio thread
    pthread_mutex_t mutex;
};

void* input_fifo(void* data);
