#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <esp_sleep.h>
#ifdef ESP_IDF_VERSION_MAJOR
  #include "esp_wifi.h"
  #include "esp_wifi_types.h"
#endif
#include "WebServer.h"
#include "Scale.h"
#include "WiFiManager.h"
#include "FlowRate.h"
#include "Calibration.h"
#include "BluetoothScale.h"
#include "TouchSensor.h"
#include "Display.h"
#include "PowerManager.h"
#include "BatteryMonitor.h"
#include "BoardConfig.h"
#include "Version.h"

// Board-specific pin configuration
uint8_t dataPin = HX711_DATA_PIN;     // HX711 Data pin
uint8_t clockPin = HX711_CLOCK_PIN;   // HX711 Clock pin  
uint8_t touchPin = TOUCH_TARE_PIN;    // Touch sensor for tare
uint8_t sleepTouchPin = TOUCH_SLEEP_PIN;  // Touch sensor for sleep functionality
uint8_t batteryPin = BATTERY_PIN;     // Battery voltage monitoring
uint8_t sdaPin = I2C_SDA_PIN;         // I2C Data pin for display
uint8_t sclPin = I2C_SCL_PIN;         // I2C Clock pin for display
float calibrationFactor = 4195.712891;
Scale scale(dataPin, clockPin, calibrationFactor);
FlowRate flowRate;
BluetoothScale bluetoothScale;
TouchSensor touchSensor(touchPin, &scale);
Display oledDisplay(sdaPin, sclPin, &scale, &flowRate);
PowerManager powerManager(sleepTouchPin, &oledDisplay);
BatteryMonitor batteryMonitor(batteryPin);

#ifdef BOARD_ARCH_ESP32C6
static const unsigned long USB_CDC_ENUMERATION_TIMEOUT_MS = 250;
#else
static const unsigned long USB_CDC_ENUMERATION_TIMEOUT_MS = 0;
#endif

static const unsigned long FACTORY_RESET_BOOT_DELAY_MS = 250;
static const unsigned long WAKE_MESSAGE_DELAY_MS = 150;
static const unsigned long POST_BLE_INIT_DELAY_MS = 200;
static const unsigned long POST_WIFI_INIT_DELAY_MS = 250;
static const unsigned long PRE_READY_SCREEN_DELAY_MS = 25;

void setup() {
  Serial.begin(115200);
  
#ifdef BOARD_ARCH_ESP32C6
  // Give USB-CDC a short window to enumerate without stalling cold boot for seconds.
  unsigned long usbWaitStart = millis();
  while (!Serial && millis() - usbWaitStart < USB_CDC_ENUMERATION_TIMEOUT_MS) {
    delay(25);
  }
#endif
  
  // Version and board identification
  Serial.println("=================================");
  Serial.printf("WeighMyBru² v%s\n", WEIGHMYBRU_VERSION_STRING);
  Serial.printf("Board: %s\n", WEIGHMYBRU_BOARD_NAME);
  Serial.printf("Build: %s %s\n", WEIGHMYBRU_BUILD_DATE, WEIGHMYBRU_BUILD_TIME);
  Serial.printf("Full Version: %s\n", WEIGHMYBRU_FULL_VERSION);
  Serial.printf("Flash Size: %dMB\n", FLASH_SIZE_MB);
  Serial.println("=================================");
  
  // Link scale and flow rate for tare operation coordination
  scale.setFlowRatePtr(&flowRate);
  
  // Check for factory reset request (hold touch pin during boot)
  pinMode(touchPin, INPUT_PULLDOWN);
  if (digitalRead(touchPin) == HIGH) {
    Serial.println("FACTORY RESET: Touch pin held during boot - clearing WiFi credentials");
    clearWiFiCredentials();
    delay(FACTORY_RESET_BOOT_DELAY_MS);
  }
  
  // CRITICAL: Initialize BLE FIRST before WiFi to prevent radio conflicts
  Serial.println("=== STARTING BLE INITIALIZATION ===");
  Serial.println("Initializing BLE FIRST for GaggiMate compatibility...");
  Serial.printf("Free heap before BLE init: %u bytes\n", ESP.getFreeHeap());
#if HAS_PSRAM
  Serial.printf("Free PSRAM before BLE init: %u bytes\n", ESP.getFreePsram());
#endif
  try {
    Serial.println("Calling bluetoothScale.begin()...");
    bluetoothScale.begin();  // Initialize BLE without scale reference
    Serial.println("BLE initialized successfully - GaggiMate should be able to connect");
    Serial.printf("Free heap after BLE init: %u bytes\n", ESP.getFreeHeap());
#if HAS_PSRAM
    Serial.printf("Free PSRAM after BLE init: %u bytes\n", ESP.getFreePsram());
#endif
  } catch (...) {
    Serial.println("BLE initialization failed - continuing without Bluetooth");
    Serial.printf("Free heap after BLE fail: %u bytes\n", ESP.getFreeHeap());
  }
  Serial.println("=== BLE INITIALIZATION COMPLETE ===");
  
  // Initialize display with error handling - don't block if display fails
  Serial.println("Initializing display...");
  bool displayAvailable = oledDisplay.begin();
  
  if (!displayAvailable) {
    Serial.println("WARNING: Display initialization failed!");
    Serial.println("System will continue in headless mode without display.");
    Serial.println("All functionality remains available via web interface.");
  } else {
    Serial.println("Display initialized - ready for visual feedback");
  }
  
  // Check wake-up reason and show appropriate message
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("Wakeup caused by external signal (touch sensor)");
      delay(WAKE_MESSAGE_DELAY_MS);
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      Serial.println("Wakeup caused by external signal using RTC_CNTL");
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Wakeup caused by timer");
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      Serial.println("Wakeup caused by touchpad");
      break;
    default:
      Serial.println("Wakeup was not caused by deep sleep: " + String(wakeup_reason));
      delay(WAKE_MESSAGE_DELAY_MS);
      break;
  }
  //Wait for BLE to finish intitalizing before starting WiFi
  delay(POST_BLE_INIT_DELAY_MS); 
  
  setupWiFi();
  
  // Configure WiFi power management AFTER WiFi is initialized
  // Use advanced modem sleep for balance between battery life and stability
  Serial.println("=== WiFi Power Management ===");
  
  #ifdef ESP_IDF_VERSION_MAJOR
    esp_err_t ps_result = esp_wifi_set_ps(WIFI_POWER_SAVE_MODE);
    if (ps_result == ESP_OK) {
      const char* mode_name;
      switch(WIFI_POWER_SAVE_MODE) {
        case WIFI_PS_NONE: mode_name = "NONE (max stability, high power)"; break;
        case WIFI_PS_MIN_MODEM: mode_name = "MIN_MODEM (balanced - RECOMMENDED)"; break;
        case WIFI_PS_MAX_MODEM: mode_name = "MAX_MODEM (max battery, may disconnect)"; break;
        default: mode_name = "UNKNOWN"; break;
      }
      Serial.printf("WiFi power save mode: %s\n", mode_name);
      
      // Also set Arduino framework WiFi sleep to match
      bool enable_sleep = (WIFI_POWER_SAVE_MODE != WIFI_PS_NONE);
      WiFi.setSleep(enable_sleep);
      Serial.printf("Arduino WiFi.setSleep(%s) to match ESP-IDF setting\n", enable_sleep ? "true" : "false");
    } else {
      Serial.printf("WARNING: Failed to set WiFi power save mode: %s\n", esp_err_to_name(ps_result));
      // Fallback to Arduino framework - match the configured mode
      bool enable_sleep = (WIFI_POWER_SAVE_MODE != WIFI_PS_NONE);
      WiFi.setSleep(enable_sleep);
      Serial.printf("Using Arduino WiFi.setSleep(%s) fallback\n", enable_sleep ? "true" : "false");
    }
  #else
    // Fallback for non-ESP-IDF builds - match the configured mode
    bool enable_sleep = (WIFI_POWER_SAVE_MODE != WIFI_PS_NONE);
    WiFi.setSleep(enable_sleep);
    Serial.printf("WiFi sleep: %s (Arduino framework)\n", enable_sleep ? "ENABLED" : "DISABLED");
  #endif
  
  Serial.println("============================");

  // Wait for WiFi to fully stabilize after BLE is already running
  delay(POST_WIFI_INIT_DELAY_MS);
  Serial.printf("Version: %s\n", ESP.getSdkVersion());
  // Initialize scale with error handling - don't block web server if HX711 fails
  Serial.println("Initializing scale...");
  if (!scale.begin()) {
    Serial.println("WARNING: Scale (HX711) initialization failed!");
    Serial.println("Web server will continue to run, but scale readings will not be available.");
    Serial.println("Check HX711 wiring and connections.");
  } else {
    Serial.println("Scale initialized successfully");
    // Now that scale is ready, set the reference in BluetoothScale
    bluetoothScale.setScale(&scale);
  }
  
  // BLE was initialized earlier - no need to initialize again
  // bluetoothScale.begin(&scale);
  
  // Set bluetooth reference in display for status indicator (if display available)
  if (oledDisplay.isConnected()) {
    oledDisplay.setBluetoothScale(&bluetoothScale);
  }
  
  // Set display reference in bluetooth for timer control
  bluetoothScale.setDisplay(&oledDisplay);
  
  // Set power manager reference in display for timer state synchronization (if display available)
  if (oledDisplay.isConnected()) {
    oledDisplay.setPowerManager(&powerManager);
  }
  
  // Set battery monitor reference in display for battery status (if display available)
  if (oledDisplay.isConnected()) {
    oledDisplay.setBatteryMonitor(&batteryMonitor);
  }

  // Initialize touch sensor
  touchSensor.begin();

  // Initialize power manager
  powerManager.begin();

  // Initialize battery monitor
  batteryMonitor.begin();

  // Show IP addresses and welcome message if display is available
  delay(PRE_READY_SCREEN_DELAY_MS);
  if (oledDisplay.isConnected()) {
    oledDisplay.showIPAddresses();
  }

  // Link display to touch sensor for tare feedback (if display available)
  if (oledDisplay.isConnected()) {
    touchSensor.setDisplay(&oledDisplay);
  }
  
  // Link flow rate to touch sensor for averaging reset on tare
  touchSensor.setFlowRate(&flowRate);

  setupWebServer(scale, flowRate, bluetoothScale, oledDisplay, batteryMonitor);
}

void loop() {
  static unsigned long lastWeightUpdate = 0;
  static unsigned long lastWiFiCheck = 0;
  
  // Update weight at optimal frequency for brewing accuracy
  if (millis() - lastWeightUpdate >= 20) { // Update every 20ms (50Hz) - still very responsive
    float weight = scale.getWeight();
    flowRate.update(weight);
    lastWeightUpdate = millis();
  }
  
  static unsigned long lastBLEUpdate = 0;
  
  // Check WiFi status every 30 seconds for debugging
  if (millis() - lastWiFiCheck >= 30000) {
    printWiFiStatus();
    lastWiFiCheck = millis();
  }
  
  // Maintain WiFi AP stability
  maintainWiFi();
  
  // Update Bluetooth less frequently to reduce BLE interference
  if (millis() - lastBLEUpdate >= 50) { // Update every 50ms (20Hz) - sufficient for app responsiveness
    bluetoothScale.update();
    lastBLEUpdate = millis();
  }
  
  // Update touch sensor
  touchSensor.update();
  
  // Update power manager
  powerManager.update();
  
  // Update battery monitor
  batteryMonitor.update();
  
  // Update display
  oledDisplay.update();
  
  // Balanced delay for responsive readings without system overload
  delay(25); // Increased from 5ms to 25ms to reduce BLE interference and system load
}
