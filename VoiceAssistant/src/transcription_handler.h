#ifndef transcription_handler_h
#define transcription_handler_h

void write_WAV_header(uint8_t* header, int dataSize);
String transcribe_audio();

#endif // TRANSCRIPTION_HANDLER_H
