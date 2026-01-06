#include "WiFiManager.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include "WebServer.h"  // For web server control

// ESP-IDF includes for advanced WiFi power management
#ifdef ESP_IDF_VERSION_MAJOR
    #include "esp_wifi.h"
    #include "esp_wifi_types.h"
    #include "esp_err.h"
    #include "lwip/icmp.h"
    #include "lwip/inet_chksum.h"
    #include "lwip/raw.h"
    #include "lwip/ip_addr.h"
    #include "ping/ping_sock.h"
#endif

Preferences wifiPrefs;

// Station credentials
char stored_ssid[33] = {0};
char stored_password[65] = {0};

// Cache for WiFi credentials to avoid repeated slow EEPROM reads
static String cachedSSID = "";
static String cachedPassword = "";
static bool credentialsCached = false;
static unsigned long lastCacheTime = 0;
const unsigned long CACHE_TIMEOUT = 300000; // 5 minutes cache timeout

// Filesystem status tracking
static bool filesystemAvailable = false;
static bool filesystemChecked = false;
static unsigned long lastFilesystemError = 0;
const unsigned long FILESYSTEM_ERROR_COOLDOWN = 30000; // Show error message every 30 seconds max

// AP credentials
const char* ap_ssid = "WeighMyBru-AP";
const char* ap_password = "";

// WiFi Power Management State
static bool wifiEnabled = true; // WiFi enabled by default
static bool wifiEnabledCached = false;
static wifi_mode_t previousWiFiMode = WIFI_OFF; // Store previous mode when disabling WiFi

unsigned long startAttemptTime = 0;
const unsigned long timeout = 10000; // 10 seconds

void checkFilesystemStatus() {
    if (filesystemChecked) {
        return; // Already checked
    }
    
    // Test if filesystem/NVS is available
    Preferences testPrefs;
    if (testPrefs.begin("test", false)) {
        testPrefs.end();
        filesystemAvailable = true;
        Serial.println("✓ Filesystem/NVS is available");
    } else {
        filesystemAvailable = false;
        Serial.println("=================================");
        Serial.println("⚠️  FILESYSTEM NOT AVAILABLE");
        Serial.println("=================================");
        Serial.println("The device filesystem has not been");
        Serial.println("uploaded to the ESP32.");
        Serial.println("");
        Serial.println("To fix this, run:");
        Serial.println("pio run -t uploadfs");
        Serial.println("or upload filesystem via PlatformIO");
        Serial.println("");
        Serial.println("Device will work in AP mode until");
        Serial.println("filesystem is uploaded.");
        Serial.println("=================================");
    }
    filesystemChecked = true;
}

void showFilesystemErrorIfNeeded() {
    if (filesystemAvailable) {
        return; // No error to show
    }
    
    unsigned long now = millis();
    if (now - lastFilesystemError > FILESYSTEM_ERROR_COOLDOWN) {
        Serial.println("⚠️  Filesystem not available - run 'pio run -t uploadfs'");
        lastFilesystemError = now;
    }
}

void saveWiFiCredentials(const char* ssid, const char* password) {
    Serial.println("Saving WiFi credentials...");
    unsigned long startTime = millis();
    
    checkFilesystemStatus();
    
    if (!filesystemAvailable) {
        // Update cache even if we can't save to NVS
        cachedSSID = String(ssid);
        cachedPassword = String(password);
        credentialsCached = true;
        lastCacheTime = millis();
        
        Serial.println("INFO: WiFi credentials cached (filesystem unavailable for permanent storage)");
        return;
    }
    
    if (wifiPrefs.begin("wifi", false)) {
        wifiPrefs.putString("ssid", ssid);
        wifiPrefs.putString("password", password);
        wifiPrefs.end();
        
        // Update cache immediately
        cachedSSID = String(ssid);
        cachedPassword = String(password);
        credentialsCached = true;
        lastCacheTime = millis();
        
        Serial.printf("WiFi credentials saved in %lu ms\n", millis() - startTime);
    } else {
        showFilesystemErrorIfNeeded();
        // Still update cache for this session
        cachedSSID = String(ssid);
        cachedPassword = String(password);
        credentialsCached = true;
        lastCacheTime = millis();
    }
}

void clearWiFiCredentials() {
    Serial.println("Clearing WiFi credentials...");
    if (wifiPrefs.begin("wifi", false)) {
        wifiPrefs.clear();
        wifiPrefs.end();
        
        // Clear cache
        cachedSSID = "";
        cachedPassword = "";
        credentialsCached = true;
        lastCacheTime = millis();
        
        Serial.println("WiFi credentials cleared");
    } else {
        Serial.println("ERROR: Failed to open WiFi preferences for clearing");
    }
}

bool loadWiFiCredentialsFromEEPROM() {
    // Check if cache is still valid first (no serial output for speed)
    if (credentialsCached && (millis() - lastCacheTime < CACHE_TIMEOUT)) {
        return true;
    }
    
    checkFilesystemStatus();
    
    if (!filesystemAvailable) {
        // Use empty defaults if filesystem unavailable
        cachedSSID = "";
        cachedPassword = "";
        credentialsCached = true;
        lastCacheTime = millis();
        return false;
    }
    
    unsigned long startTime = millis();
    
    // Add timeout protection
    const unsigned long EEPROM_TIMEOUT = 5000; // 5 second timeout
    bool success = false;
    
    unsigned long attemptStart = millis();
    if (wifiPrefs.begin("wifi", true)) {
        // Check if operation is taking too long
        if (millis() - attemptStart > EEPROM_TIMEOUT) {
            wifiPrefs.end();
            cachedSSID = "";
            cachedPassword = "";
        } else {
            cachedSSID = wifiPrefs.getString("ssid", "");
            cachedPassword = wifiPrefs.getString("password", "");
            success = true;
        }
        wifiPrefs.end();
        
        credentialsCached = true;
        lastCacheTime = millis();
        
        // Minimal serial output to reduce blocking
        Serial.printf("WiFi: %s in %lums\n", success ? "OK" : "TIMEOUT", millis() - startTime);
        return success;
    } else {
        showFilesystemErrorIfNeeded();
        // Use empty defaults if EEPROM fails
        cachedSSID = "";
        cachedPassword = "";
        credentialsCached = true;
        lastCacheTime = millis();
        return false;
    }
}

void loadWiFiCredentials(char* ssid, char* password, size_t maxLen) {
    loadWiFiCredentialsFromEEPROM();
    strncpy(ssid, cachedSSID.c_str(), maxLen - 1);
    strncpy(password, cachedPassword.c_str(), maxLen - 1);
    ssid[maxLen - 1] = '\0';
    password[maxLen - 1] = '\0';
}

String getStoredSSID() {
    // Fast path - return immediately if already cached and recent
    if (credentialsCached && (millis() - lastCacheTime < CACHE_TIMEOUT)) {
        return cachedSSID;
    }
    
    // Only do EEPROM read if cache is invalid
    loadWiFiCredentialsFromEEPROM();
    return cachedSSID;
}

String getStoredPassword() {
    // Fast path - return immediately if already cached and recent
    if (credentialsCached && (millis() - lastCacheTime < CACHE_TIMEOUT)) {
        return cachedPassword;
    }
    
    // Only do EEPROM read if cache is invalid
    loadWiFiCredentialsFromEEPROM();
    return cachedPassword;
}

void setupWiFi() {
    // Check if WiFi should be enabled
    if (!loadWiFiEnabledState()) {
        Serial.println("WiFi is disabled - skipping WiFi setup for battery saving");
        WiFi.mode(WIFI_OFF);
        return;
    }
    
    char ssid[33] = {0};
    char password[65] = {0};
    loadWiFiCredentials(ssid, password, sizeof(ssid));
    
    // Ensure WiFi is completely reset first
    Serial.println("=== WIFI ANTENNA OPTIMIZATION ===");
    Serial.println("Resetting WiFi subsystem...");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(500); // Longer delay for complete reset
    
    // Enable WiFi persistent mode for automatic reconnection
    WiFi.persistent(true);
    WiFi.setAutoReconnect(true);
    Serial.println("WiFi auto-reconnect enabled for connection stability");
    
    // Configure WiFi for battery-optimized stability
    #ifdef ESP_IDF_VERSION_MAJOR
        // Listen interval: how many beacons to skip before waking (lower = more stable, higher = more battery)
        // 3 beacons = good balance for battery devices (default is often 1-3)
        wifi_config_t conf;
        esp_wifi_get_config(WIFI_IF_STA, &conf);
        conf.sta.listen_interval = 3; // Wake every 3 beacons for router keep-alive
        esp_wifi_set_config(WIFI_IF_STA, &conf);
        Serial.println("WiFi listen interval: 3 beacons (battery-optimized stability)");
    #endif
    
    // Apply SuperMini antenna fix for boards with poor antenna design
    applySuperMiniAntennaFix();
    
    // Check if we have stored credentials - prioritize STA connection
    if (strlen(ssid) > 0) {
        Serial.println("=== ATTEMPTING STA CONNECTION ===");
        Serial.println("Found stored credentials for: " + String(ssid));
        Serial.println("Trying STA mode first...");
        
        // Try STA mode first for lower power consumption
        WiFi.mode(WIFI_STA);
        delay(500); // Brief delay for mode switch
        
        #ifdef ESP_IDF_VERSION_MAJOR
            // Set WiFi protocol to include long-range mode for better weak-signal performance
            // Use 11b/g/n + LR for maximum compatibility and range
            esp_err_t proto_result = esp_wifi_set_protocol(WIFI_IF_STA, 
                WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR);
            if (proto_result == ESP_OK) {
                Serial.println("WiFi Long Range (LR) mode enabled for weak signal areas");
            } else {
                // LR mode not available on all chips, fall back to standard protocols
                esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
                Serial.println("Using standard WiFi protocols (11b/g/n)");
            }
        #endif
        
        delay(500); // Additional delay for protocol change
        
        // ANTENNA FIX: Reapply power settings after mode switch
        // Mode switch can reset power levels, so reapply the fix
        if (ENABLE_SUPERMINI_ANTENNA_FIX) {
            applySuperMiniAntennaFix();
        }
        
        startAttemptTime = millis();
        WiFi.begin(ssid, password);
        
        // Wait for connection with reasonable timeout
        int connectionAttempts = 0;
        const int maxAttempts = 20; // 10 seconds total - reduced to prevent watchdog timeout
        
        Serial.print("Connecting");
        while (WiFi.status() != WL_CONNECTED && connectionAttempts < maxAttempts) {
            delay(100);  // Shorter delay to check more frequently
            if (connectionAttempts % 5 == 0) {
                Serial.print(".");
                yield(); // Feed the watchdog
            }
            connectionAttempts++;
            
            // Check for immediate connection failures
            if (WiFi.status() == WL_NO_SSID_AVAIL) {
                Serial.println("\nNetwork '" + String(ssid) + "' not found");
                break;
            }
            if (WiFi.status() == WL_CONNECT_FAILED) {
                Serial.println("\nConnection failed - likely incorrect password");
                break;
            }
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nSTA CONNECTION SUCCESSFUL!");
            Serial.println("===========================");
            Serial.println("Connected to: " + String(ssid));
            Serial.println("IP Address: " + WiFi.localIP().toString());
            Serial.println("Gateway: " + WiFi.gatewayIP().toString());
            Serial.println("DNS: " + WiFi.dnsIP().toString());
            Serial.println("Signal: " + String(WiFi.RSSI()) + " dBm");
            Serial.println("AP mode disabled - optimized for low power");
            Serial.println("Will auto-fallback to AP if connection lost");
            Serial.println("===========================");
            
            // Setup mDNS for STA mode
            setupmDNS();
            
            return; // Exit early - we're connected via STA, no need for AP
        } else {
            Serial.println("\nSTA CONNECTION FAILED");
            Serial.println("Status code: " + String(WiFi.status()));
            Serial.println("Falling back to AP mode for configuration...");
        }
    } else {
        Serial.println("=== NO STORED CREDENTIALS ===");
        Serial.println("No WiFi credentials found - starting AP mode for initial setup");
    }
    
    // Fallback to AP mode if STA failed or no credentials exist
    Serial.println("Starting AP mode...");
    WiFi.mode(WIFI_AP);
    delay(1000); // Ensure mode switch is stable
    
    // Configure AP with optimized settings for maximum visibility
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    
    // Start AP with maximum power and visibility settings
    bool apStarted = false;
    Serial.println("Starting AP for credential configuration...");
    
    // Try channel 6 first (most common and widely supported)
    apStarted = WiFi.softAP(ap_ssid, ap_password, 6, false, 4); // Channel 6, broadcast SSID, max 4 clients
    
    if (apStarted) {
        Serial.println("AP started successfully on channel 6");
    } else {
        Serial.println("Channel 6 failed, trying channel 1...");
        apStarted = WiFi.softAP(ap_ssid, ap_password, 1, false, 4); // Channel 1, broadcast SSID
        
        if (apStarted) {
            Serial.println("AP started successfully on channel 1");
        } else {
            Serial.println("Channel 1 failed, trying default settings...");
            apStarted = WiFi.softAP(ap_ssid); // Simplest possible configuration
            if (apStarted) {
                Serial.println("AP started with default settings");
            }
        }
    }
    
    if (apStarted) {
        Serial.println("=== AP MODE ACTIVE ===");
        Serial.println("AP SSID: " + String(ap_ssid));
        Serial.println("AP IP: " + WiFi.softAPIP().toString());
        Serial.println("AP MAC: " + WiFi.softAPmacAddress());
        Serial.printf("AP Channel: %d\n", WiFi.channel());
        Serial.printf("WiFi TX Power: %d dBm\n", WiFi.getTxPower());
        Serial.println("Connect to 'WeighMyBru-AP' to configure WiFi");
        Serial.println("Access: http://192.168.4.1 or http://weighmybru.local");
        Serial.println("=====================");
        
        // Setup mDNS for AP mode
        setupmDNS();
    } else {
        Serial.println("ERROR: AP failed to start - hardware or RF issue suspected");
    }
}

void setupmDNS() {
    // Start mDNS service with hostname "weighmybru"
    if (MDNS.begin("weighmybru")) {
        Serial.println("mDNS responder started/updated");
        Serial.println("Access the scale at: http://weighmybru.local");
        
        // Add service to MDNS-SD
        MDNS.addService("http", "tcp", 80);
        MDNS.addService("websocket", "tcp", 81);
        
        // Add some useful service properties
        MDNS.addServiceTxt("http", "tcp", "device", "WeighMyBru Coffee Scale");
        MDNS.addServiceTxt("http", "tcp", "version", "2.0");
        
    } else {
        Serial.println("Error starting mDNS responder");
    }
}

void printWiFiStatus() {
    Serial.println("=== WiFi Status ===");
    Serial.println("WiFi Mode: " + String(WiFi.getMode()));
    Serial.println("AP Status: " + String(WiFi.softAPgetStationNum()) + " clients connected");
    Serial.println("AP IP: " + WiFi.softAPIP().toString());
    Serial.println("AP SSID: " + String(ap_ssid));
    Serial.println("STA Status: " + String(WiFi.status()));
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("STA IP: " + WiFi.localIP().toString());
        Serial.println("STA RSSI: " + String(WiFi.RSSI()) + " dBm");
    }
    Serial.println("WiFi Sleep: " + String(WiFi.getSleep() ? "ON" : "OFF"));
    Serial.println("==================");
}

void maintainWiFi() {
    // Skip maintenance if WiFi is disabled
    if (!isWiFiEnabled()) {
        return;
    }
    
    static unsigned long lastMaintenance = 0;
    static unsigned long lastDisconnectTime = 0;
    static int consecutiveDisconnects = 0;
    const unsigned long maintenanceInterval = 15000; // Every 15 seconds
    const unsigned long disconnectCooldown = 60000; // 1 minute cooldown after repeated failures
    
    if (millis() - lastMaintenance >= maintenanceInterval) {
        lastMaintenance = millis();
        
        // Check current WiFi mode and connection health
        wifi_mode_t currentMode = WiFi.getMode();
        
        if (currentMode == WIFI_STA) {
            // We're in STA mode - check if connection is still healthy
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("WARNING: STA connection lost! Attempting reconnection...");
                
                // Track disconnect frequency to prevent reconnection loops
                consecutiveDisconnects++;
                lastDisconnectTime = millis();
                
                // If too many consecutive disconnects, wait longer before trying AP fallback
                if (consecutiveDisconnects >= 3) {
                    Serial.printf("Multiple disconnects detected (%d). Giving auto-reconnect more time...\n", consecutiveDisconnects);
                }
                
                // Try to reconnect to saved credentials
                char ssid[33] = {0};
                char password[65] = {0};
                loadWiFiCredentials(ssid, password, sizeof(ssid));
                
                if (strlen(ssid) > 0) {
                    Serial.println("Attempting to reconnect to: " + String(ssid));
                    
                    // Don't call WiFi.begin() if auto-reconnect is already trying
                    // Just wait and let the ESP32's built-in reconnect mechanism work
                    if (WiFi.getAutoReconnect()) {
                        Serial.println("Auto-reconnect active - waiting for ESP32 to reconnect...");
                    } else {
                        WiFi.begin(ssid, password);
                    }
                    
                    // Wait for reconnection - longer timeout for repeated failures
                    int maxAttempts = (consecutiveDisconnects >= 3) ? 40 : 20; // 20s for repeated failures
                    int attempts = 0;
                    while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
                        delay(500);
                        Serial.print(".");
                        attempts++;
                        
                        // Check if reconnection succeeded early
                        if (WiFi.status() == WL_CONNECTED) {
                            break;
                        }
                    }
                    
                    if (WiFi.status() == WL_CONNECTED) {
                        Serial.println("\nSTA reconnection successful!");
                        Serial.println("IP: " + WiFi.localIP().toString());
                        consecutiveDisconnects = 0; // Reset counter on success
                    } else {
                        Serial.printf("\nSTA reconnection failed after %d attempts\n", attempts);
                        
                        // Only switch to AP mode if we've had repeated failures
                        if (consecutiveDisconnects >= 5) {
                            Serial.println("Too many failed reconnects - switching to AP mode");
                            switchToAPMode();
                            consecutiveDisconnects = 0; // Reset counter
                        } else {
                            Serial.println("Will retry on next maintenance cycle...");
                        }
                    }
                } else {
                    Serial.println("No stored credentials - switching to AP mode");
                    switchToAPMode();
                }
            } else {
                // Connection is healthy - reset disconnect counter
                if (consecutiveDisconnects > 0) {
                    Serial.println("Connection restored after previous disconnects");
                    consecutiveDisconnects = 0;
                }
                Serial.println("STA mode healthy - connection maintained");
                Serial.println("Connected to: " + WiFi.SSID() + " | IP: " + WiFi.localIP().toString() + " | RSSI: " + String(WiFi.RSSI()) + "dBm");
            }
        } else if (currentMode == WIFI_AP) {
            // We're in AP mode - just ensure it's still running properly
            if (WiFi.softAPgetStationNum() == 0) {
                Serial.println("AP mode active - 'WeighMyBru-AP' ready for configuration");
            } else {
                Serial.println("AP mode active - " + String(WiFi.softAPgetStationNum()) + " clients connected");
            }
        } else if (currentMode == WIFI_OFF) {
            Serial.println("CRITICAL: WiFi is OFF! This should not happen - restarting AP mode");
            switchToAPMode();
        }
        
        // Monitor WiFi power save mode (check periodically but don't spam logs)
        static unsigned long lastPowerCheck = 0;
        if (millis() - lastPowerCheck >= 300000) { // Check every 5 minutes
            #ifdef ESP_IDF_VERSION_MAJOR
                wifi_ps_type_t current_ps;
                esp_wifi_get_ps(&current_ps);
                if (current_ps != WIFI_POWER_SAVE_MODE) {
                    Serial.println("WARNING: WiFi power save mode changed! Restoring...");
                    esp_wifi_set_ps(WIFI_POWER_SAVE_MODE);
                }
            #endif
            lastPowerCheck = millis();
        }
        
        // Print status for debugging
        Serial.println("WiFi maintenance check completed");
    }
}

// Function to attempt STA connection with new credentials and switch from AP mode
bool attemptSTAConnection(const char* ssid, const char* password) {
    Serial.println("=== ATTEMPTING STA CONNECTION ===");
    Serial.println("SSID: " + String(ssid));
    Serial.println("Switching from AP mode to STA mode...");
    
    // Disconnect from AP mode but keep WiFi on
    WiFi.mode(WIFI_STA);
    delay(1000); // Allow mode switch to stabilize
    
    // ANTENNA FIX: Reapply power settings after mode switch for SuperMini boards
    if (ENABLE_SUPERMINI_ANTENNA_FIX) {
        Serial.println("Reapplying SuperMini antenna fix after mode switch...");
        applySuperMiniAntennaFix();
    }
    
    // Attempt connection with new credentials
    startAttemptTime = millis();
    WiFi.begin(ssid, password);
    
    // Wait for connection with reasonable timeout
    int connectionAttempts = 0;
    const int maxAttempts = 20; // 10 seconds total - reduced to prevent watchdog timeout
    
    Serial.print("Connecting");
    while (WiFi.status() != WL_CONNECTED && connectionAttempts < maxAttempts) {
        delay(100);  // Shorter delay to check more frequently
        if (connectionAttempts % 5 == 0) {
            Serial.print(".");
            yield(); // Feed the watchdog
        }
        connectionAttempts++;
        
        // Check for immediate connection failures
        if (WiFi.status() == WL_NO_SSID_AVAIL) {
            Serial.println("\nSSID not found");
            return false;
        }
        if (WiFi.status() == WL_CONNECT_FAILED) {
            Serial.println("\nConnection failed - likely wrong password");
            return false;
        }
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nSTA CONNECTION SUCCESSFUL!");
        Serial.println("Connected to: " + String(ssid));
        Serial.println("IP Address: " + WiFi.localIP().toString());
        Serial.println("Gateway: " + WiFi.gatewayIP().toString());
        Serial.println("RSSI: " + String(WiFi.RSSI()) + " dBm");
        Serial.println("AP mode disabled - power consumption optimized");
        
        // Setup mDNS for the new STA connection
        setupmDNS();
        
        return true;
    } else {
        Serial.println("\nSTA connection failed or timed out");
        Serial.println("Status code: " + String(WiFi.status()));
        return false;
    }
}

// Function to switch back to AP mode if STA connection fails
void switchToAPMode() {
    Serial.println("=== SWITCHING TO AP MODE ===");
    Serial.println("Disconnecting from STA mode...");
    WiFi.disconnect(true);
    delay(500);
    
    Serial.println("Setting AP mode...");
    WiFi.mode(WIFI_AP);
    delay(1000); // Allow mode switch to stabilize
    
    // Restart AP with same settings as setupWiFi()
    Serial.println("Starting AP broadcast...");
    bool apStarted = WiFi.softAP(ap_ssid, ap_password, 6, false, 4);
    
    if (apStarted) {
        Serial.println("AP MODE RESTORED");
        Serial.println("==================");
        Serial.println("SSID: " + String(ap_ssid));
        Serial.println("IP: " + WiFi.softAPIP().toString());
        Serial.println("Config URL: http://192.168.4.1");
        Serial.println("mDNS: http://weighmybru.local");
        Serial.println("==================");
        
        // Setup mDNS for AP mode
        setupmDNS();
    } else {
        Serial.println("CRITICAL: Failed to restart AP mode!");
        Serial.println("Retrying with minimal settings...");
        // Try with minimal settings as fallback
        if (WiFi.softAP(ap_ssid)) {
            Serial.println("AP started with minimal settings");
            setupmDNS();
        } else {
            Serial.println("FATAL: Cannot start AP mode - WiFi hardware issue?");
        }
    }
}

// Apply antenna and power optimization for better WiFi performance
void applySuperMiniAntennaFix() {
    if (!ENABLE_SUPERMINI_ANTENNA_FIX) {
        Serial.println("Antenna optimization disabled in configuration");
        return;
    }
    
    Serial.println("Applying antenna and power optimization...");
    
    #ifdef ESP_IDF_VERSION_MAJOR
        // Set maximum TX power for better range (84 = 21dBm, max allowed)
        esp_err_t pwr_result = esp_wifi_set_max_tx_power(84);
        if (pwr_result == ESP_OK) {
            Serial.println("ESP-IDF TX power set to maximum (21dBm)");
        } else if (pwr_result == ESP_ERR_WIFI_NOT_INIT) {
            Serial.println("TX power setting deferred - WiFi not initialized yet");
        } else {
            Serial.printf("ESP-IDF TX power setting: %s\n", esp_err_to_name(pwr_result));
        }
        
        // For ESP32-C6: Use esp_phy API if available for antenna config
        // The internal antenna should be default, but we ensure max power is set
        #if defined(CONFIG_IDF_TARGET_ESP32C6)
            Serial.println("ESP32-C6 detected - internal antenna is default");
        #endif
    #endif
    
    // Arduino framework maximum power (works as backup and confirmation)
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    Serial.println("Arduino framework TX power: 19.5dBm (max)");
    
    Serial.println("Antenna optimization complete");
}

// Get current WiFi signal strength in dBm
int getWiFiSignalStrength() {
    if (WiFi.status() != WL_CONNECTED) {
        return -100; // Return very poor signal if not connected
    }
    return WiFi.RSSI();
}

// Get WiFi signal quality description
String getWiFiSignalQuality() {
    if (WiFi.status() != WL_CONNECTED) {
        return "Disconnected";
    }
    
    int rssi = WiFi.RSSI();
    
    if (rssi >= -30) {
        return "Excellent";
    } else if (rssi >= -50) {
        return "Very Good";
    } else if (rssi >= -60) {
        return "Good";
    } else if (rssi >= -70) {
        return "Fair";
    } else if (rssi >= -80) {
        return "Weak";
    } else {
        return "Very Weak";
    }
}

// Get detailed WiFi connection information
String getWiFiConnectionInfo() {
    String info = "{";
    
    if (WiFi.status() == WL_CONNECTED) {
        info += "\"connected\":true,";
        info += "\"mode\":\"STA\",";
        info += "\"ssid\":\"" + WiFi.SSID() + "\",";
        info += "\"signal_strength\":" + String(WiFi.RSSI()) + ",";
        info += "\"signal_quality\":\"" + getWiFiSignalQuality() + "\",";
        info += "\"channel\":" + String(WiFi.channel()) + ",";
        info += "\"tx_power\":" + String(WiFi.getTxPower()) + ",";
        info += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
        info += "\"gateway\":\"" + WiFi.gatewayIP().toString() + "\",";
        info += "\"dns\":\"" + WiFi.dnsIP().toString() + "\",";
        info += "\"mac\":\"" + WiFi.macAddress() + "\"";
    } else {
        info += "\"connected\":false,";
        info += "\"mode\":\"AP\",";
        info += "\"ssid\":\"" + String(ap_ssid) + "\",";
        info += "\"signal_strength\":null,";
        info += "\"signal_quality\":\"N/A - AP Mode\",";
        info += "\"channel\":" + String(WiFi.channel()) + ",";
        info += "\"tx_power\":" + String(WiFi.getTxPower()) + ",";
        info += "\"ip\":\"" + WiFi.softAPIP().toString() + "\",";
        info += "\"gateway\":\"N/A\",";
        info += "\"dns\":\"N/A\",";
        info += "\"mac\":\"" + WiFi.macAddress() + "\",";
        info += "\"connected_clients\":" + String(WiFi.softAPgetStationNum());
    }
    
    info += "}";
    return info;
}

// WiFi Power Management Functions

bool loadWiFiEnabledState() {
    if (wifiEnabledCached) {
        return wifiEnabled;
    }
    
    // Check filesystem status first
    checkFilesystemStatus();
    
    if (!filesystemAvailable) {
        showFilesystemErrorIfNeeded();
        wifiEnabled = true; // Default to enabled when filesystem unavailable
        wifiEnabledCached = true;
        return wifiEnabled;
    }
    
    if (wifiPrefs.begin("wifi", true)) {
        wifiEnabled = wifiPrefs.getBool("enabled", true); // Default to enabled
        wifiPrefs.end();
        wifiEnabledCached = true;
        Serial.printf("WiFi enabled state loaded: %s\n", wifiEnabled ? "ON" : "OFF");
    } else {
        showFilesystemErrorIfNeeded();
        wifiEnabled = true; // Default to enabled on error
        wifiEnabledCached = true;
    }
    
    return wifiEnabled;
}

void saveWiFiEnabledState(bool enabled) {
    checkFilesystemStatus();
    
    if (!filesystemAvailable) {
        // Can't save when filesystem unavailable, but update in-memory state
        wifiEnabled = enabled;
        wifiEnabledCached = true;
        return;
    }
    
    if (wifiPrefs.begin("wifi", false)) {
        wifiPrefs.putBool("enabled", enabled);
        wifiPrefs.end();
        wifiEnabled = enabled;
        wifiEnabledCached = true;
        Serial.printf("WiFi enabled state saved: %s\n", enabled ? "ON" : "OFF");
    } else {
        showFilesystemErrorIfNeeded();
        // Still update in-memory state
        wifiEnabled = enabled;
        wifiEnabledCached = true;
    }
}

bool isWiFiEnabled() {
    loadWiFiEnabledState();
    return wifiEnabled;
}

void enableWiFi() {
    Serial.println("Enabling WiFi...");
    
    // Save the enabled state
    saveWiFiEnabledState(true);
    
    // If WiFi was previously off, restore it
    if (WiFi.getMode() == WIFI_OFF) {
        // Try to restore to STA mode first if we have credentials
        if (loadWiFiCredentialsFromEEPROM() && !cachedSSID.isEmpty()) {
            Serial.println("Attempting to reconnect to saved network...");
            if (attemptSTAConnection(cachedSSID.c_str(), cachedPassword.c_str())) {
                Serial.println("WiFi reconnected to STA mode");
                startWebServer(); // Start web server when WiFi is enabled
                return;
            }
        }
        
        // Fall back to AP mode if STA connection fails
        Serial.println("Starting WiFi in AP mode...");
        switchToAPMode();
        startWebServer(); // Start web server when WiFi is enabled
    }
    
    Serial.println("WiFi enabled");
}

void disableWiFi() {
    Serial.println("Disabling WiFi to save battery...");
    
    // Stop web server first to prevent TCP/IP stack issues
    stopWebServer();
    
    // Save current mode before disabling
    previousWiFiMode = WiFi.getMode();
    
    // Save the disabled state
    saveWiFiEnabledState(false);
    
    // Gracefully close active connections before disabling WiFi
    Serial.println("Closing active connections...");
    
    // Give time for current HTTP responses to complete
    delay(100);
    
    // Properly disconnect based on current mode
    if (previousWiFiMode == WIFI_STA || previousWiFiMode == WIFI_AP_STA) {
        Serial.println("Disconnecting from STA...");
        WiFi.disconnect(true);
    }
    
    if (previousWiFiMode == WIFI_AP || previousWiFiMode == WIFI_AP_STA) {
        Serial.println("Stopping AP mode...");
        WiFi.softAPdisconnect(true);
    }
    
    // Additional delay to ensure cleanup
    delay(200);
    
    // Now safely turn off WiFi
    WiFi.mode(WIFI_OFF);
    
    Serial.println("WiFi disabled - battery saving mode active");
}

void toggleWiFi() {
    if (isWiFiEnabled() && WiFi.getMode() != WIFI_OFF) {
        disableWiFi();
    } else {
        enableWiFi();
    }
}
