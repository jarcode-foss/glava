
#include <pthread.h>

void* pcm_main(void* data);

struct pcm_struct {
    int stateplay;
    pthread_mutex_t mutex;
};