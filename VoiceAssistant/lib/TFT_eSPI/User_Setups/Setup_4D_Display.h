// Setup for 4D Systems 4.3" SPI Display
#define USER_SETUP_INFO "4D Systems 4.3\" SPI Display"

#define ILI9341_DRIVER
#define TFT_WIDTH  480
#define TFT_HEIGHT 272

// ESP32-S3 SPI pins
#define TFT_MISO  -1  // Not used
#define TFT_MOSI  13  // SDA
#define TFT_SCLK  14  // SCL
#define TFT_CS    10  // Chip select
#define TFT_DC    11  // Data/Command
#define TFT_RST   12  // Reset

#define TFT_BL    9   // Backlight control
#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT

#define SPI_FREQUENCY  27000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000