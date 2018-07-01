
#include <pthread.h>

struct pcm_struct {
	int stateplay; // shared variable used to terminate audio thread
    pthread_mutex_t mutex;
};

void* pcm_main(void* data);
