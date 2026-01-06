#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// Board identification and pin configuration

#ifdef BOARD_SUPERMINI
  #define BOARD_NAME "ESP32-S3-DevKitC-1 (SuperMini)"
  #define BOARD_TYPE_SUPERMINI
  
#elif defined(BOARD_XIAO)
  #define BOARD_NAME "XIAO ESP32S3" 
  #define BOARD_TYPE_XIAO
  
#elif defined(BOARD_XIAO_C6)
  #define BOARD_NAME "XIAO ESP32C6"
  #define BOARD_TYPE_XIAO_C6
  
#else
  #define BOARD_NAME "ESP32-S3 (Unknown)"
  #define BOARD_TYPE_SUPERMINI  // Default fallback
  
#endif

// Pin definitions - vary by board
#ifdef BOARD_TYPE_XIAO_C6
  // XIAO ESP32C6 pin assignments  
  #define HX711_DATA_PIN      22  // GPIO22 (D4/SDA) - HX711 Data pin
  #define HX711_CLOCK_PIN     23  // GPIO23 (D5/SCL) - HX711 Clock pin  
  #define TOUCH_TARE_PIN      21  // GPIO21 (D3) - Touch sensor for tare
  #define TOUCH_SLEEP_PIN     19  // GPIO19 (D8/SCK) - Touch sensor for sleep/timer functionality
  #define BATTERY_PIN         0   // GPIO0 (A0) - Battery voltage monitoring (ADC1_CH0)
  #define I2C_SDA_PIN         20  // GPIO20 (D9/MISO) - I2C Data pin for display
  #define I2C_SCL_PIN         18  // GPIO18 (D10/MOSI) - I2C Clock pin for display
  #define VBUS_PIN            2   // GPIO2 - USB power detection (optional)
  #define BATTERY_SWITCH_PIN  1   // GPIO1 - Battery circuit control (optional)
#else
  // ESP32-S3 boards (SuperMini and XIAO ESP32S3) - identical pin assignments
  #define HX711_DATA_PIN      5   // GPIO5 - HX711 Data pin
  #define HX711_CLOCK_PIN     6   // GPIO6 - HX711 Clock pin  
  #define TOUCH_TARE_PIN      4   // GPIO4 - Touch sensor for tare (T0)
  #define TOUCH_SLEEP_PIN     3   // GPIO3 - Touch sensor for sleep functionality
  #define BATTERY_PIN         7   // GPIO7 - Battery voltage monitoring (ADC1_CH6)
  #define I2C_SDA_PIN         8   // GPIO8 - I2C Data pin for display
  #define I2C_SCL_PIN         9   // GPIO9 - I2C Clock pin for display
#endif

// Board-specific configurations
#ifdef BOARD_TYPE_SUPERMINI
  #define FLASH_SIZE_MB       4
  #define BOARD_DESCRIPTION   "ESP32-S3 SuperMini with 4MB Flash"
  
#elif defined(BOARD_TYPE_XIAO)
  #define FLASH_SIZE_MB       8
  #define BOARD_DESCRIPTION   "XIAO ESP32S3 with 8MB Flash"
  
#elif defined(BOARD_TYPE_XIAO_C6)
  #define FLASH_SIZE_MB       4
  #define BOARD_DESCRIPTION   "XIAO ESP32C6 with 4MB Flash"
  
#endif

// Board feature flags
#ifdef BOARD_TYPE_XIAO_C6
  // ESP32-C6 features
  #define HAS_WIFI            true
  #define HAS_BLUETOOTH       true
  #define HAS_PSRAM           false   // C6 has no PSRAM
  #define HAS_TOUCH_SENSOR    false   // C6 has no hardware touch, using digital touch modules
  #define ADC_RESOLUTION      12      // 12-bit ADC
  #define PWM_RESOLUTION      8       // 8-bit PWM
  #define HAS_VBUS_DETECT     true    // USB power detection available
#else
  // ESP32-S3 features (common to SuperMini and XIAO ESP32S3)
  #define HAS_WIFI            true
  #define HAS_BLUETOOTH       true
  #define HAS_PSRAM           true
  #define HAS_TOUCH_SENSOR    true    // Using digital touch modules (not ESP32 capacitive)
  #define ADC_RESOLUTION      12      // 12-bit ADC
  #define PWM_RESOLUTION      8       // 8-bit PWM
  #define HAS_VBUS_DETECT     false   // Not available on S3 boards
#endif

#endif // BOARD_CONFIG_H