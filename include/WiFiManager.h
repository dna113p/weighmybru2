#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <Preferences.h>
#include <ESPmDNS.h>

// Configuration for SuperMini antenna fix
// Set to true to enable maximum power mode for boards with poor antenna design
#define ENABLE_SUPERMINI_ANTENNA_FIX true

// WiFi Power Save Mode Configuration
// WIFI_PS_NONE (0)    - No power save, maximum stability, highest power consumption
// WIFI_PS_MIN_MODEM (1) - Minimum modem sleep, good balance (RECOMMENDED for battery devices)
// WIFI_PS_MAX_MODEM (2) - Maximum modem sleep, best battery, may cause disconnects
//
// IMPORTANT: Your DNA2.4 router has VERY aggressive client timeout settings
// Even with WIFI_PS_NONE, you're getting disconnects due to:
//   - Weak signal (-74 to -81 dBm - move router closer or vice versa)
//   - Router WiFi 6 power save detection (may flag ESP32 as "sleeping")
//   - Strict association timeouts
// 
// RECOMMENDED: Move the scale closer to the router OR use AP mode for configuration only
#define WIFI_POWER_SAVE_MODE WIFI_PS_NONE

void setupWiFi();
void saveWiFiCredentials(const char* ssid, const char* password);
void clearWiFiCredentials(); // Clear stored WiFi credentials
void loadWiFiCredentials(char* ssid, char* password, size_t maxLen);
bool loadWiFiCredentialsFromEEPROM(); // Load and cache WiFi credentials from EEPROM
String getStoredSSID();
String getStoredPassword();
void setupmDNS(); // Setup mDNS for weighmybru.local hostname
void printWiFiStatus(); // Print detailed WiFi status for debugging
void maintainWiFi(); // Periodic WiFi maintenance to ensure AP stability
bool attemptSTAConnection(const char* ssid, const char* password); // Attempt STA connection and switch from AP mode
void switchToAPMode(); // Switch back to AP mode if STA connection fails
void applySuperMiniAntennaFix(); // Apply maximum power settings for problematic SuperMini boards
int getWiFiSignalStrength(); // Get current WiFi signal strength in dBm
String getWiFiSignalQuality(); // Get WiFi signal quality description
String getWiFiConnectionInfo(); // Get detailed WiFi connection information

// WiFi Power Management
bool isWiFiEnabled(); // Check if WiFi is currently enabled
void enableWiFi(); // Enable WiFi and restore previous mode
void disableWiFi(); // Disable WiFi completely to save battery
void toggleWiFi(); // Toggle WiFi on/off
bool loadWiFiEnabledState(); // Load WiFi enabled state from preferences
void saveWiFiEnabledState(bool enabled); // Save WiFi enabled state to preferences

#endif
