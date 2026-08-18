#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "config.h"
#include <HTTPClient.h>
#include "display_functionality.h"
#include "audio_functionality.h"

String query_GroqAPI(String user_message) {
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
    Serial.println("API Response received");
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
  speaker_input(response);
  
  
  start_time = millis();
  
  //display response summary
  set_text(introduction);
  
  return response;
}