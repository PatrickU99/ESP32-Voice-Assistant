# ESP32-S3 Voice Assistant

A voice assistant built on the ESP32-S3 that records audio, 
transcribes it using Deepgram, sends it to Groq AI for a response, 
and plays it back through a speaker.

## Hardware
- ESP32-S3 N16R8
- INMP441 Microphone
- MAX98357A Amplifier
- SSD1306 OLED Display
- PIR Motion Sensor

## Features
- Speech to text via Deepgram
- AI responses via Groq
- Text to speech playback
- OLED display output

## Setup
1. Copy `include/config_template.h` to `include/config.h`
2. Fill in your WiFi and API credentials
3. Upload via PlatformIO