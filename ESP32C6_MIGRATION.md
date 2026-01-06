# ESP32C6 Support - Migration Guide

## Overview

This document outlines the changes made to support the **Seeed Studio XIAO ESP32C6** board in the WeighMyBru2 project.

## What Was Changed

### 1. Build Configuration (`platformio.ini`)

Added new build environment for ESP32C6:

```ini
[env:esp32c6-xiao]
board = seeed_xiao_esp32c6
board_upload.flash_size = 4MB
upload_flags = 
  --chip=esp32c6
  --before=default_reset
  --after=hard_reset
build_flags = 
  ${env.build_flags}
  -DBOARD_XIAO_C6
```

**Note:** Removed `-DBOARD_HAS_PSRAM` flag since ESP32-C6 has no PSRAM.

### 2. Pin Mapping (`include/BoardConfig.h`)

Updated pin definitions for XIAO ESP32C6:

| Function | ESP32-S3 Pin | ESP32-C6 Pin | Notes |
|----------|-------------|--------------|-------|
| HX711 Data | GPIO5 | **GPIO22** | Load cell data |
| HX711 Clock | GPIO6 | **GPIO23** | Load cell clock |
| Tare Touch | GPIO4 | **GPIO21** | Tare button |
| Sleep/Timer Touch | GPIO3 | **GPIO19** | Power/timer control |
| Battery ADC | GPIO7 | **GPIO0** | ADC1_CH0 |
| I2C SDA | GPIO8 | **GPIO20** | Display data |
| I2C SCL | GPIO9 | **GPIO18** | Display clock |
| **VBUS Detect** | N/A | **GPIO2** | USB power detection (NEW) |
| **Battery Switch** | N/A | **GPIO1** | Battery circuit control (optional) |

### 3. Board Identification (`include/Version.h`)

Added board name string for C6:

```cpp
#elif defined(BOARD_XIAO_C6)
  #define WEIGHMYBRU_BOARD_NAME "XIAO ESP32C6"
```

### 4. WiFi Antenna Fix (`include/WiFiManager.h`)

Made the SuperMini antenna fix conditional - only applies to SUPERMINI board:

```cpp
#ifdef BOARD_SUPERMINI
  #define ENABLE_SUPERMINI_ANTENNA_FIX true
#else
  #define ENABLE_SUPERMINI_ANTENNA_FIX false
#endif
```

The XIAO ESP32C6 has a proper antenna design and doesn't need the power boost workaround.

### 5. Deep Sleep Wake-up (`src/PowerManager.cpp`)

Updated wake-up mechanism for ESP32-C6 compatibility:

- **ESP32-S3**: Uses `esp_sleep_enable_ext0_wakeup()` (EXT0 wake-up)
- **ESP32-C6**: Uses `esp_deep_sleep_enable_gpio_wakeup()` (GPIO wake-up)

The C6 doesn't support EXT0/EXT1 wake-up sources, so we use the newer GPIO wake-up API.

### 6. USB Power Detection (`src/BatteryMonitor.cpp`)

Added VBUS detection for ESP32-C6:

```cpp
bool BatteryMonitor::isCharging() {
    #ifdef VBUS_PIN
        return digitalRead(VBUS_PIN) == HIGH;
    #else
        return false;
    #endif
}
```

When USB is connected to the C6, GPIO2 (VBUS_PIN) goes HIGH, allowing the firmware to detect charging status.

## Key Differences: ESP32-S3 vs ESP32-C6

| Feature | ESP32-S3 | ESP32-C6 |
|---------|----------|----------|
| Architecture | Xtensa LX7 (dual-core) | RISC-V (single-core) |
| WiFi | 2.4 GHz | 2.4 GHz + WiFi 6 |
| Bluetooth | BLE 5.0 | BLE 5.0 + Zigbee/Thread |
| PSRAM | Yes | **No** |
| Flash | 4-8MB | 4MB |
| Hardware Touch | Yes | **No** |
| Wake-up Sources | EXT0/EXT1 | GPIO wake-up |
| USB | USB-OTG | USB Serial/JTAG |

## Building the Project

### For XIAO ESP32C6:
```bash
platformio run -e esp32c6-xiao
```

### For XIAO ESP32S3:
```bash
platformio run -e esp32s3-xiao
```

### For ESP32-S3 SuperMini:
```bash
platformio run -e esp32s3-supermini
```

## Testing Checklist

Before deploying to hardware, verify:

- [ ] **Compilation**: Project builds without errors for `esp32c6-xiao` environment
- [ ] **WiFi AP Mode**: Device creates access point successfully
- [ ] **WiFi STA Mode**: Device connects to WiFi network
- [ ] **BLE**: Bluetooth advertising works (appears as "WeighMyBru")
- [ ] **BLE Connections**: GaggiMate and Bean Conqueror apps can connect
- [ ] **HX711 Scale**: Weight readings are accurate
- [ ] **OLED Display**: Display shows weight, timer, flow rate
- [ ] **Touch Sensors**: Tare and sleep/timer buttons work
- [ ] **Battery Monitoring**: ADC reads battery voltage correctly
- [ ] **VBUS Detection**: `isCharging()` returns true when USB connected
- [ ] **Deep Sleep**: Device enters sleep mode
- [ ] **Wake-up**: Device wakes from sleep on touch sensor press

## Potential Issues & Solutions

### 1. PSRAM Dependency

**Issue**: ESP32-C6 has no PSRAM. If the application uses PSRAM, it will fail.

**Solution**: The code already has conditional compilation for PSRAM. Monitor heap usage during runtime. If memory issues occur, consider:
- Reducing web server buffer sizes
- Optimizing display buffer usage
- Reducing BLE connection parameters

### 2. AsyncTCP Library Compatibility

**Issue**: The `AsyncTCP` library may not be fully compatible with ESP32-C6.

**Solution**: If you encounter compilation or runtime errors with AsyncTCP:
1. Check for ESP32-C6 compatible fork
2. Update to latest ESP-IDF 5.x compatible version
3. Consider using alternative async libraries

### 3. ADC Calibration

**Issue**: ESP32-C6 ADC characteristics may differ from S3.

**Solution**: After first build, verify battery voltage readings:
```cpp
Serial.printf("Battery: %.2fV (%d%%)\n", 
    batteryMonitor.getBatteryVoltage(), 
    batteryMonitor.getBatteryPercentage());
```

Compare with multimeter reading and use `batteryMonitor.calibrateVoltage(actualVoltage)` if needed.

### 4. GPIO Wake-up Sensitivity

**Issue**: GPIO wake-up behavior may differ from EXT0.

**Solution**: Test deep sleep/wake thoroughly. If spurious wake-ups occur, add additional debouncing or adjust pull-down resistor values.

## Hardware Wiring

### XIAO ESP32C6 Connections

```
┌─────────────────────┐
│   XIAO ESP32C6      │
├─────────────────────┤
│ GPIO22 ──> HX711 DT │  Load cell data
│ GPIO23 ──> HX711 SCK│  Load cell clock
│ GPIO21 ──> Touch Tare│  Tare button
│ GPIO19 ──> Touch Pwr│  Sleep/timer button
│ GPIO0  ──> Battery  │  Battery voltage (via divider)
│ GPIO20 ──> SDA      │  OLED display data
│ GPIO18 ──> SCL      │  OLED display clock
│ GPIO2  ──> VBUS     │  USB power detection (optional)
│ GPIO1  ──> Bat Sw   │  Battery switch (optional)
└─────────────────────┘
```

### Battery Voltage Divider

The battery monitoring uses a 2:1 voltage divider (100kΩ + 100kΩ):

```
Battery+ ──[100kΩ]──┬──[100kΩ]── GND
                    │
                  GPIO0 (ADC)
```

This divides the 4.2V Li-ion voltage to 2.1V max, well within the ESP32-C6's 3.3V ADC range.

## Feature Support Matrix

| Feature | SuperMini | XIAO S3 | XIAO C6 |
|---------|-----------|---------|---------|
| WiFi AP/STA | ✅ | ✅ | ✅ |
| BLE Scale | ✅ | ✅ | ✅ |
| Web Interface | ✅ | ✅ | ✅ |
| OLED Display | ✅ | ✅ | ✅ |
| HX711 Scale | ✅ | ✅ | ✅ |
| Touch Sensors | ✅ | ✅ | ✅ |
| Battery Monitor | ✅ | ✅ | ✅ |
| VBUS Detection | ❌ | ❌ | ✅ |
| Deep Sleep | ✅ | ✅ | ✅ |
| PSRAM | ✅ | ✅ | ❌ |
| Antenna Fix | ✅ | ❌ | ❌ |

## Next Steps

1. **Build the firmware**: Use PlatformIO to compile for `esp32c6-xiao`
2. **Upload to board**: Flash the firmware to your XIAO ESP32C6
3. **Test basic functions**: Verify WiFi, BLE, display, scale readings
4. **Calibrate sensors**: 
   - Calibrate HX711 scale with known weights
   - Verify battery voltage readings
5. **Test power management**: Verify deep sleep and wake-up work correctly
6. **Integration testing**: Test with GaggiMate or Bean Conqueror apps

## Support

If you encounter issues specific to ESP32-C6:

1. Check serial monitor output for error messages
2. Verify pin connections match the new pin mapping
3. Ensure you're using the correct build environment (`esp32c6-xiao`)
4. Check ESP-IDF and Arduino core versions are compatible with C6

## Additional Notes

- The ESP32-C6 is a newer chip with RISC-V architecture, so some legacy ESP32 code patterns may need updates
- WiFi 6 features are available but not currently used by this project
- Zigbee/Thread capabilities are present but not utilized
- Single-core architecture should be sufficient for this application (scale + display + web server)
