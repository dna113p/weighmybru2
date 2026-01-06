#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "Scale.h"
#include "FlowRate.h"
#ifndef DISABLE_BLUETOOTH
#include "BluetoothScale.h"
#endif
#include "Display.h"
#include "BatteryMonitor.h"

extern float calibrationFactor;

#ifndef DISABLE_BLUETOOTH
void setupWebServer(Scale &scale, FlowRate &flowRate, BluetoothScale &bluetoothScale, Display &display, BatteryMonitor &battery);
#else
void setupWebServer(Scale &scale, FlowRate &flowRate, Display &display, BatteryMonitor &battery);
#endif
void startWebServer();
void stopWebServer();

#endif
