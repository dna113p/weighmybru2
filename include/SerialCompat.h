#ifndef SERIAL_COMPAT_H
#define SERIAL_COMPAT_H

// ESP32-C6 with Arduino core 3.x uses USBSerial instead of Serial
// when ARDUINO_USB_CDC_ON_BOOT is enabled. This header provides
// compatibility by ensuring Serial is properly defined.

#include <Arduino.h>
#include "BoardConfig.h"

// For ESP32-C6, the Arduino core redefines Serial to USBSerial
// but USBSerial may not be available until after USB initialization.
// We need to ensure proper Serial handling for all boards.

#if defined(BOARD_ARCH_ESP32C6)
  // ESP32-C6 uses HWCDC (Hardware USB CDC) as the default Serial
  // The Arduino core should handle this automatically with USB_CDC_ON_BOOT
  // But we may need to explicitly use HWCDC in some cases
  #if !defined(Serial)
    #include <HWCDC.h>
    extern HWCDC Serial;
  #endif
#endif

#endif // SERIAL_COMPAT_H
