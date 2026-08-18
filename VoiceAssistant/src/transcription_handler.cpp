#include <Arduino.h>
#include "mic_functionality.h"
#include <WiFiClientSecure.h>
#include "config.h"


//44 byte wav file header to send to deepgram
void write_WAV_header(uint8_t* header, int dataSize) {
  int new_sample_rate = sample_rate;
  int numChannels = 1;
  int bitsPerSample = 16;
  int byteRate = new_sample_rate * numChannels * bitsPerSample / 8;

  memcpy(header, "RIFF", 4);
  int chunkSize = 36 + dataSize;
  memcpy(header + 4, &chunkSize, 4);
  memcpy(header + 8, "WAVE", 4);
  memcpy(header + 12, "fmt ", 4);
  int subChunk1Size = 16;
  memcpy(header + 16, &subChunk1Size, 4);
  int16_t audioFormat = 1;
  memcpy(header + 20, &audioFormat, 2);
  int16_t channels = numChannels;
  memcpy(header + 22, &channels, 2);
  memcpy(header + 24, &new_sample_rate, 4);
  memcpy(header + 28, &byteRate, 4);
  int16_t blockAlign = numChannels * bitsPerSample / 8;
  memcpy(header + 32, &blockAlign, 2);
  int16_t bitsVal = bitsPerSample;
  memcpy(header + 34, &bitsVal, 2);
  memcpy(header + 36, "data", 4);
  memcpy(header + 40, &dataSize, 4);
}

String transcribe_audio() {
  // STEP 1 - BUILD WAV FILE IN PSRAM
  int wavHeaderSize = 44;  // WAV header is always exactly 44 bytes
  int totalSize = wavHeaderSize + buffer_size;  // total WAV file size
  
  // Allocate memory in PSRAM for the WAV file
  uint8_t* wavBuffer = (uint8_t*)ps_malloc(totalSize);
  if (!wavBuffer) {
    Serial.println("Failed to allocate WAV buffer!");
    return "";
  }
  
  // Write the 44 byte WAV header at the start of the buffer
  write_WAV_header(wavBuffer, buffer_size);
  
  // Copy raw audio samples from audio_buffer into WAV buffer after the header
  memcpy(wavBuffer + wavHeaderSize, audio_buffer, buffer_size);

  
  //CONNECT TO DEEPGRAM
  WiFiClientSecure client;
  client.setInsecure();   // skip SSL certificate verification
  client.setTimeout(30);  

  // Connect using hardcoded IP to bypass DNS issues
  if (!client.connect(IPAddress(208, 184, 56, 200), 443, 30000)) {
    Serial.println("Connection to Deepgram failed!");
    free(wavBuffer);  // free memory before returning
    return "";
  }


  // POST request to Deepgram's transcription endpoint
  client.println("POST /v1/listen?model=nova-2&language=en HTTP/1.1");
  client.println("Host: api.deepgram.com");
  client.println("Authorization: Token " + String(DEEPGRAM_API_KEY));
  client.println("Content-Type: audio/wav");           // sending WAV audio
  client.println("Content-Length: " + String(totalSize)); // total bytes being sent
  client.println("Connection: close");                 // close after response
  client.println();                                    // blank line ends headers

  // Send in 2KB chunks instead of all at once
  // This prevents SSL buffer overflow errors
  int chunkSize = 2048;
  int sent = 0;
  
  while (sent < totalSize) {
    int toSend = min(chunkSize, totalSize - sent); // don't exceed remaining bytes
    client.write(wavBuffer + sent, toSend);         // send chunk
    sent += toSend;                                 // advance position
    delay(20);  
  }
  
  free(wavBuffer);  // free WAV buffer from PSRAM after sending

  
  // Wait up to 60 seconds for Deepgram to respond
  unsigned long timeout = millis();
  while (!client.available()) {
    if (millis() - timeout > 60000) {
      Serial.println("Request timed out!");
      client.stop();
      return "";
    }
    delay(100);  // check every 100ms
  }

  // Read response line by line until we find the transcript
  String response = "";
  while (client.connected() || client.available()) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      response += line;
      // Stop reading once we have the transcript in the response
      if (response.indexOf("\"transcript\"") != -1) break;
    }
    // 15 second timeout for reading response
    if (millis() - timeout > 15000) {
      Serial.println("Read timed out!");
      break;
    }
  }
  client.stop();  // close the connection


  // Deepgram returns JSON like:
  // {"results":{"channels":[{"alternatives":[{"transcript":"text"}]}]}}
  // We find "transcript":" and extract the text after it
  String transcription = "";
  int textIndex = response.indexOf("\"transcript\":\"");
  if (textIndex != -1) {
    int start = textIndex + 14;  
    int end = response.indexOf("\"", start); 
    transcription = response.substring(start, end);  // extract the text
  } else {
    Serial.println("Parse error: " + response);  // print raw response if parsing fails
  }

  return transcription;  // return transcribed text to caller
}