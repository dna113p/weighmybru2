#ifndef BATTERYMONITOR_H
#define BATTERYMONITOR_H

#include <Arduino.h>
#include <Preferences.h>

class BatteryMonitor {
public:
    BatteryMonitor(uint8_t batteryPin);
    void begin();
    
    // Battery readings
    float getBatteryVoltage();
    int getBatteryPercentage();
    String getBatteryStatus();  // "Full", "Good", "Low", "Critical"
    
    // Battery state indicators
    bool isCharging();  // Future expansion for charge detection
    bool isLowBattery();
    bool isCriticalBattery();
    
    // Configuration and calibration
    void calibrateVoltage(float actualVoltage);  // For fine-tuning readings
    float getCalibrationOffset() const { return calibrationOffset; }
    float getCalibrationScale() const { return calibrationScale; }
    int getRawAdcReading();
    float getPinVoltage();
    
    // Update method for periodic readings
    void update();
    
    // OLED display helper - returns battery segments (0-3)
    int getBatterySegments();
    
private:
    uint8_t batteryPin;
    Preferences preferences;
    
    // Li-ion voltage thresholds optimized for ESP32 operation (700mAh battery)
    static constexpr float BATTERY_FULL = 4.20f;     // 100% - Treat only near-full cells as full
    static constexpr float BATTERY_GOOD = 4.00f;     // ~75% - Healthy working range
    static constexpr float BATTERY_NOMINAL = 3.80f;  // ~50% - Typical nominal voltage
    static constexpr float BATTERY_LOW = 3.55f;      // ~25% - Consider charging soon
    static constexpr float BATTERY_CRITICAL = 3.40f; // ~5%  - Charge immediately
    static constexpr float BATTERY_EMPTY = 3.25f;    // 0%   - Near protection cutoff under light load
    
    // Hardware configuration
    static constexpr float VOLTAGE_DIVIDER_RATIO = 2.0f;  // 100k + 100k resistors
    static constexpr int ADC_RESOLUTION = 4095;
    
    // Calibration and smoothing
    float calibrationOffset = 0.0f;  // Voltage adjustment for accuracy
    float calibrationScale = 1.0f;   // Multiplicative adjustment for divider/ADC accuracy
    float lastVoltage = 0.0f;        // For smoothing readings
    unsigned long lastUpdate = 0;
    static constexpr unsigned long UPDATE_INTERVAL = 1000; // Update every 1 second
    
    // Internal methods
    float readPinVoltage();
    float readRawVoltage();
    void loadCalibration();
    void saveCalibration();
};

#endif
