// File: src/main.cpp
#include <Arduino.h>
#include <WiFi.h>
#include <driver/i2s.h>
#include "config.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "Audio.h"
#include "mic_functionality.h"
#include "display_functionality.h"
#include "query_handler.h"
#include "audio_functionality.h"
#include "transcription_handler.h"

//LED PIN
#define LED_PIN 6

//button PIN
#define BUTTON_PIN 1

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
  
  audio_setup();
  buffer_allocation();
  display_setup();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
    //when button is pressed
    if (digitalRead(BUTTON_PIN) == LOW){
      Serial.println("LED ON");
      digitalWrite(LED_PIN, HIGH);
      
      record_audio();
      
      String text = transcribe_audio();
      if (text.length() > 0) {
        Serial.println("You said: " + text);
        set_text("You said: " + text);
        String response = query_GroqAPI(text);
        wait_for_audio();
      } else {
        Serial.println("Could not transcribe audio.");
      }
      delay(1000);
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED OFF");
  }
  
  //Return to Home Screen
  display_set();
}


