#include "time.h"
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define I2C_SDA 8
#define I2C_SCL 9

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -28800, 60000);  // UTC time, update every 60s
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long start_time;

void display_set(){
  timeClient.update();
  String formattedTime = timeClient.getFormattedTime();
  if (millis() - start_time > 10000) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("Hello Patrick");
    display.println("Time: " + formattedTime);
    display.display();
    }
  
}

void set_text(String text) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println(text);
  display.display();
}

void display_setup() {
  //Time Setup
  timeClient.begin();
  timeClient.update();

// Initialize I2C with specific pins
  Wire.begin(I2C_SDA, I2C_SCL);
  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
}