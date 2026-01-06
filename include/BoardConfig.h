#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// Board identification and pin configuration

#ifdef BOARD_SUPERMINI
  #define BOARD_NAME "ESP32-S3-DevKitC-1 (SuperMini)"
  #define BOARD_TYPE_SUPERMINI
  #define BOARD_ARCH_ESP32S3
  
#elif defined(BOARD_XIAO)
  #define BOARD_NAME "XIAO ESP32S3" 
  #define BOARD_TYPE_XIAO
  #define BOARD_ARCH_ESP32S3

#elif defined(BOARD_XIAO_ESP32C6)
  #define BOARD_NAME "XIAO ESP32C6"
  #define BOARD_TYPE_XIAO_C6
  #define BOARD_ARCH_ESP32C6
  
#else
  #define BOARD_NAME "ESP32-S3 (Unknown)"
  #define BOARD_TYPE_SUPERMINI  // Default fallback
  #define BOARD_ARCH_ESP32S3
  
#endif

// Pin definitions - board specific
#if defined(BOARD_XIAO_ESP32C6)
  // XIAO ESP32C6 pin definitions (from brew-mate reference)
  #define HX711_DATA_PIN      22  // GPIO22 - HX711 Data pin
  #define HX711_CLOCK_PIN     23  // GPIO23 - HX711 Clock pin  
  #define TOUCH_TARE_PIN      21  // GPIO21 - Touch sensor for tare
  #define TOUCH_SLEEP_PIN     2   // GPIO2 - Touch sensor for sleep/power (deep sleep wake capable on C6)
  #define BATTERY_PIN         0   // GPIO0 - Battery voltage monitoring (ADC)
  #define I2C_SDA_PIN         20  // GPIO20 - I2C Data pin for display
  #define I2C_SCL_PIN         18  // GPIO18 - I2C Clock pin for display
#else
  // ESP32-S3 pin definitions (SuperMini and XIAO S3)
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

// Architecture-specific features
#if defined(BOARD_ARCH_ESP32S3)
  // ESP32-S3 features
  #define HAS_WIFI            true
  #define HAS_BLUETOOTH       true
  #define HAS_PSRAM           true
  #define HAS_NATIVE_USB      true
  #define ADC_BITS            12    // 12-bit ADC
  #define PWM_RESOLUTION      8     // 8-bit PWM

#elif defined(BOARD_ARCH_ESP32C6)
  // ESP32-C6 features (RISC-V based, no PSRAM)
  #define HAS_WIFI            true
  #define HAS_BLUETOOTH       true
  #define HAS_PSRAM           false
  #define HAS_NATIVE_USB      true
  #define ADC_BITS            12    // 12-bit ADC
  #define PWM_RESOLUTION      8     // 8-bit PWM
#endif

// Common feature flag for touch sensor (all boards use digital TTP223 modules)
#define HAS_TOUCH_SENSOR    true

#endif // BOARD_CONFIG_H