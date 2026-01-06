# Fixes for Seeed XIAO ESP32-C6 Board Support

## Issues Fixed

### 1. Display Pointer: Null Error
**Problem:** The Display class was allocating the display object with `new` in the constructor, but if memory allocation failed or the display was uninitialized, accessing the display pointer caused crashes.

**Fixes:**
- Added proper initialization of `displayConnected` to `false` in Display.h header
- Added null check and try-catch in Display constructor for memory allocation
- Added destructor to properly clean up display pointer
- Added null checks to all methods that access the display pointer:
  - All display methods now check both `displayConnected` and `display != nullptr` before use
  - Added 100ms delay after Wire.begin() for I2C stability (from brew-mate)
  - Added explicit null pointer check in `begin()` method

### 2. Bluetooth Disabled on C6 Board
**Problem:** The C6 board build had `DISABLE_BLUETOOTH=1` flag and was using an old Arduino-ESP32 framework (3.0.0) that didn't have full C6 BLE support.

**Fixes:**
- Changed platform from standard espressif32 to Seeed's platform: `https://github.com/Seeed-Studio/platform-seeedboards.git`
- Removed `DISABLE_BLUETOOTH=1` build flag
- Removed pinned Arduino-ESP32 3.0.0 version (now uses latest from Seeed platform)
- Added NimBLE library to common dependencies
- Platform now has proper BLE support for ESP32-C6

## Files Modified

### platformio.ini
- Updated `esp32c6-xiao` environment to use Seeed platform
- Removed DISABLE_BLUETOOTH flag
- Removed platform_packages and extra_scripts
- Added board configuration matching brew-mate's working setup

### include/Display.h
- Added destructor declaration
- Initialized `displayConnected` to `false` in class definition

### src/Display.cpp
- Added destructor for cleanup
- Added null check in constructor with try-catch
- Added null checks to all methods accessing display pointer (20+ methods)
- Added 100ms delay after Wire.begin() for I2C stability

## Board Pin Configuration (C6)
The pin configuration matches the working brew-mate project:
- HX711_DATA_PIN: 22 (D4/SDA)
- HX711_CLOCK_PIN: 23 (D5/SCL)
- TOUCH_TARE_PIN: 21 (D3)
- TOUCH_SLEEP_PIN: 19 (D8/SCK)
- I2C_SDA_PIN: 20 (D9/MISO)
- I2C_SCL_PIN: 18 (D10/MOSI)
- BATTERY_PIN: 0 (A0)
- VBUS_PIN: 2
- BATTERY_SWITCH_PIN: 1

## Testing
Build and upload to the XIAO ESP32-C6 board with:
```bash
pio run -t upload -e esp32c6-xiao
```

Monitor serial output:
```bash
pio device monitor -e esp32c6-xiao
```

Both display and Bluetooth should now work correctly on the ESP32-C6 board.
