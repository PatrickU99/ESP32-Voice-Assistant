#include <Arduino.h>
#include "Audio.h"
#include "audio_functionality.h"
Audio audio;

//Amplifier connections
#define I2S_BCLK 47
#define I2S_LRC  21
#define I2S_DOUT 17

void audio_setup() {
//Setup Audio and play
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(15);
  audio.forceMono(true);
  audio.connecttospeech("System ready", "en");
}

void speaker_input(String text) {
  audio.setVolume(15);
  audio.forceMono(true);
  audio.connecttospeech(text.c_str(), "en");
} 

void wait_for_audio() {
    delay(500); // give audio time to start playing
  while (audio.isRunning()) {
    audio.loop();
    delay(1);
  }
}