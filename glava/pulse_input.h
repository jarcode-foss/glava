
#ifndef PULSE_INPUT_H
#define PULSE_INPUT_H

#include "fifo.h"

void get_pulse_default_sink(struct audio_data* audio);
void* input_pulse(void* data);

#endif
