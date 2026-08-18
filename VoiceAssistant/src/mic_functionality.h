#ifndef mic_setup_h
#define mic_setup_h
#include <Arduino.h>

void setup_mic();
void stop_mic();
void buffer_allocation();
void record_audio();

extern int buffer_size;
extern int sample_rate;
extern uint8_t* audio_buffer;

#endif // mic_setup_h

