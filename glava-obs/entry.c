#include <stdlib.h>
#include <obs-module.h>
#include <obs.h>
#include <util/threading.h>
#include <util/platform.h>

OBS_DECLARE_MODULE();

struct mod_state {
	obs_source_t* source;
	os_event_t*   stop_signal;
	pthread_t     thread;
	bool          initialized;
};

static const char* get_name(void* _) {
    UNUSED_PARAMETER(_);
    return "GLava Source";
}

static void destroy(void* data) {
    struct mod_state* s = (struct mod_state*) data;
    if (s) {
        if (s->initialized) {
            os_event_signal(s->stop_signal);
            pthread_join(s->thread, NULL);
        }
        os_event_destroy(s->stop_signal);
        bfree(s);
    }
}

static void* work_thread(void* data) {
    return NULL;
}

static void* create(obs_data_t* settings, obs_source_t* source) {
    struct mod_state* s = bzalloc(sizeof(struct mod_state));
    s->source = source;

    if (os_event_init(&s->stop_signal, OS_EVENT_TYPE_MANUAL) != 0) {
		destroy(s);
		return NULL;
	}

	if (pthread_create(&s->thread, NULL, work_thread, s) != 0) {
		destroy(s);
		return NULL;
	}

    s->initialized = true;
    return s;
}

struct obs_source_info glava_src = {
    .id           = "glava",
    .type         = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_ASYNC_VIDEO,
    .get_name     = get_name,
    .create       = create,
    .destroy      = destroy
};

bool obs_module_load(void) {
    obs_register_source(&glava_src);
    return true;
}
