// File: src/main.cpp
#include <Arduino.h>
#include <WiFi.h>
#include <driver/i2s.h>
#include "config.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include "time.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "Audio.h"

//LED PIN
#define LED_PIN 6

//button PIN
#define BUTTON_PIN 1

// Microphone pins (INMP441)
#define I2S_MIC_SD  41
#define I2S_MIC_SCK 42
#define I2S_MIC_WS  40

//Screen setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define I2C_SDA 8
#define I2C_SCL 9
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//Amplifier connections
#define I2S_BCLK 47
#define I2S_LRC  21
#define I2S_DOUT 17

unsigned long startTime;

// NTP Settings
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -28800, 60000);  // UTC time, update every 60s
Audio audio;

// LED Control
void blinkLED(int times, int delay_ms = 200) {
for(int i = 0; i < times; i++) {
  digitalWrite(STATUS_LED_PIN, HIGH);
  delay(delay_ms);
  digitalWrite(STATUS_LED_PIN, LOW);
  delay(delay_ms);
  }
}

// Recording settings
#define SAMPLE_RATE    16000
#define RECORD_SECONDS 5
#define BUFFER_SIZE    (SAMPLE_RATE * RECORD_SECONDS * 2)

int16_t* audioBuffer = nullptr;

void stopMic() {
  i2s_driver_uninstall(I2S_NUM_1);
}

//Idle Screen
void displaySet(){
  timeClient.update();
  String formattedTime = timeClient.getFormattedTime();
  if (millis() - startTime > 10000) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("Hello Patrick");
    display.println("Time: " + formattedTime);
    display.display();
    }
  
}

//function for changing display text
void setText(String text) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println(text);
  display.display();
}

//Mic Setup
void setupMic() {
  i2s_config_t mic_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 512,
    .use_apll = false
  };

  i2s_pin_config_t mic_pins = {
    .bck_io_num = I2S_MIC_SCK,
    .ws_io_num = I2S_MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_SD
  };

  i2s_driver_install(I2S_NUM_1, &mic_config, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &mic_pins);
}

//44 byte wav file header to send to deepgram
void writeWAVHeader(uint8_t* header, int dataSize) {
  int sampleRate = SAMPLE_RATE;
  int numChannels = 1;
  int bitsPerSample = 16;
  int byteRate = sampleRate * numChannels * bitsPerSample / 8;

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
  memcpy(header + 24, &sampleRate, 4);
  memcpy(header + 28, &byteRate, 4);
  int16_t blockAlign = numChannels * bitsPerSample / 8;
  memcpy(header + 32, &blockAlign, 2);
  int16_t bitsVal = bitsPerSample;
  memcpy(header + 34, &bitsVal, 2);
  memcpy(header + 36, "data", 4);
  memcpy(header + 40, &dataSize, 4);
}

void recordAudio() {
  // Reinstall mic driver before recording
  setupMic();
  
  Serial.println("Recording... speak now!");
  setText("Recording... speak now!");
  size_t bytesRead = 0;
  size_t totalRead = 0;
  //while the mic hasnt recorded the audio data threshold read more bytes of audio
  while (totalRead < BUFFER_SIZE) {
    i2s_read(I2S_NUM_1, audioBuffer + (totalRead / 2),
             BUFFER_SIZE - totalRead, &bytesRead, portMAX_DELAY);
    totalRead += bytesRead;
  }
  Serial.println("Recording done!");
  setText("Recording done!");
  delay(1000);
  
  setText("Transcribing.....");
  
  // Uninstall mic driver after recording so Audio library can use I2S again
  stopMic();
}

String transcribeAudio() {
  // STEP 1 - BUILD WAV FILE IN PSRAM
  int wavHeaderSize = 44;  // WAV header is always exactly 44 bytes
  int totalSize = wavHeaderSize + BUFFER_SIZE;  // total WAV file size
  
  // Allocate memory in PSRAM for the WAV file
  uint8_t* wavBuffer = (uint8_t*)ps_malloc(totalSize);
  if (!wavBuffer) {
    Serial.println("Failed to allocate WAV buffer!");
    return "";
  }
  
  // Write the 44 byte WAV header at the start of the buffer
  writeWAVHeader(wavBuffer, BUFFER_SIZE);
  
  // Copy raw audio samples from audioBuffer into WAV buffer after the header
  memcpy(wavBuffer + wavHeaderSize, audioBuffer, BUFFER_SIZE);

  
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

// Groq API Function
String queryGroqAPI(String user_message) {
// Check WiFi
  if (WiFi.status() != WL_CONNECTED) {
  Serial.println("WiFi not connected!");
  return "Error: No internet connection";
  }
 // Create HTTP client
  HTTPClient http;
  http.begin(GROQ_ENDPOINT);
  http.addHeader("Authorization", String("Bearer ") + GROQ_API_KEY);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(REQUEST_TIMEOUT_MS);
 // Prepare JSON request (using PSRAM for large documents)
  DynamicJsonDocument requestDoc(2048);  // Request is small
  requestDoc["model"] = GROQ_MODEL;
  JsonArray messages = requestDoc.createNestedArray("messages");
  JsonObject message = messages.createNestedObject();
  message["role"] = "user";
  message["content"] = user_message + " first give me a less than 6 word summary of your response. After this put your response in a newline and your response should be less than 15 words.";
  
  requestDoc["max_tokens"] = 512;      // Limit response length
  requestDoc["temperature"] = 0.7;     // Creativity (0.0 to 1.0)
  requestDoc["top_p"] = 1.0;           // Diversity
  // Serialize request
  String requestBody;
  serializeJson(requestDoc, requestBody);
  Serial.println("Sending request to Groq...");
  Serial.println("Model: " + String(GROQ_MODEL));
  // Send POST request
  int httpCode = http.POST(requestBody);
  String responseText = "";
  // Handle response
  if (httpCode == 200) {
    String response = http.getString();
    Serial.println("✓ API Response received");
    // Parse response using PSRAM for large JSON
    DynamicJsonDocument responseDoc(8192);  // Use PSRAM automatically
    DeserializationError error = deserializeJson(responseDoc, response);
    if (!error) {
      responseText = responseDoc["choices"][0]["message"]["content"].as<String>();
      responseText.trim();
  
      // Print token usage for debugging
      int prompt_tokens = responseDoc["usage"]["prompt_tokens"];
      int completion_tokens = responseDoc["usage"]["completion_tokens"];
      Serial.printf("Tokens used: %d (prompt) + %d (completion) = %d total\n",
                 prompt_tokens, completion_tokens,
                 prompt_tokens + completion_tokens);
    } else {
    responseText = "JSON Parse Error: " + String(error.c_str());
    }
  }
  else if (httpCode == 401) {
  responseText = "Error: Invalid API Key. Check config.h";
  }
  else if (httpCode == 429) {
  responseText = "Error: Rate limited. Wait 1 minute.";
  }
  else {
  responseText = "HTTP Error " + String(httpCode) + ": " + http.getString();
  }
  http.end();
  int newline = responseText.indexOf('\n');
  String introduction = responseText.substring(0, newline);
  String response = responseText.substring(newline, responseText.length());
  
  // Ensure audio is ready and play
  audio.setVolume(15);
  audio.forceMono(true);
  Serial.println(response);
  audio.connecttospeech(response.c_str(), "en");
  
  startTime = millis();
  
  //display response summary
  setText(introduction);
  
  return response;
}


void setup() {
  Serial.begin(115200);
  delay(2000);
  pinMode(LED_PIN, OUTPUT);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  
  
    //Setup Audio and play
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(15);
  audio.forceMono(true);
  audio.connecttospeech("System ready", "en");
  
  //Time Setup
  timeClient.begin();
  timeClient.update();
  
  audioBuffer = (int16_t*)ps_malloc(BUFFER_SIZE);
  if (!audioBuffer) {
    Serial.println("PSRAM allocation failed!");
    return;
  }
  
  // Initialize I2C with specific pins
  Wire.begin(I2C_SDA, I2C_SCL);
  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
}

void loop() {
  
    //when button is pressed
    if (digitalRead(BUTTON_PIN) == LOW){
      Serial.println("LED ON");
      digitalWrite(LED_PIN, HIGH);
      
      recordAudio();
      
      String text = transcribeAudio();
      if (text.length() > 0) {
        Serial.println("You said: " + text);
        setText("You said: " + text);
        String response = queryGroqAPI(text);
      } else {
        Serial.println("Could not transcribe audio.");
      }
      delay(1000);
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED OFF");
  }
  
  
  audio.loop();
  
  
  //time
  displaySet();
  
}


