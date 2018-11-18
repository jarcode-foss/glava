
#include <pthread.h>

struct audio_data {
    volatile float* audio_out_r;
    volatile float* audio_out_l;
    bool modified;
    size_t audio_buf_sz, sample_sz;
    int format;
    unsigned int rate;
    char *source; // pulse source
    int channels;
	int terminate; // shared variable used to terminate audio thread
    pthread_mutex_t mutex;
};

struct audio_impl {
    const char* name;
    void  (*init)(struct audio_data* data);
    void* (*entry)(void* data);
};

#define AUDIO_FUNC(F)                                   \
    .F = (typeof(((struct audio_impl*) NULL)->F)) &F

extern struct audio_impl* audio_impls[4];
extern size_t audio_impls_idx;

static inline void register_audio_impl(struct audio_impl* impl) { audio_impls[audio_impls_idx++] = impl; }

#define AUDIO_ATTACH(N)                                         \
    static struct audio_impl N##_var = {                        \
        .name = #N,                                             \
        AUDIO_FUNC(init),                                       \
        AUDIO_FUNC(entry),                                      \
    };                                                          \
    void __attribute__((constructor)) _##N##_construct(void) { \
        register_audio_impl(&N##_var);                          \
    }
