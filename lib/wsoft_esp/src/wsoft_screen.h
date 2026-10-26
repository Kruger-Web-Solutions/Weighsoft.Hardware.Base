#pragma once

// The W-Soft weigh screen (was main.cpp in Weighsoft.WSoft esp/src @ cfcd012).
// On Hardware.Base, WSoftScreenTask runs wsoft_screen_setup() once and then
// wsoft_screen_loop() forever in its own FreeRTOS task, so the blocking Modbus poll
// and panel draw never stall the web server or MQTT.

#include "wsoft_view.h"

void wsoft_screen_setup();
void wsoft_screen_loop();

// Copies the latest snapshot (written by the screen task after every poll).
// Safe to call from any task; it only takes a short spinlock.
void wsoft_screen_snapshot(WSoftSnapshot *out);
