#pragma once

// The USB port W-Soft logs on and reads the technician commands (HUB / APPROVAL) from.
// Screen boards: UART0 (Serial0), the CH340 / CH343 USB port.
// Headless S3 (WSOFT_LOG_USB=1): the S3's own USB port (USB-Serial-JTAG, Serial with
// USB_CDC_ON_BOOT=1), so one cable flashes, logs and takes commands.
#include <Arduino.h>

#if defined(WSOFT_LOG_USB) && WSOFT_LOG_USB
#define WSOFT_LOG Serial
#else
#define WSOFT_LOG Serial0
#endif
