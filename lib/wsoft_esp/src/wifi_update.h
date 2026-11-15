#pragma once

// Hardware.Base only (WSOFT_ON_BASE): the "Wi-Fi update" switch (Rhyno, 14 Nov 2026). New
// software can be sent to the board over Wi-Fi (ArduinoOTA / espota), so the board does not
// have to be plugged into a laptop. The rules are wsoft_wifi_update (wsoft_core, host-tested):
// OFF by default, the board's own password (never a factory or shared one), nothing while
// Approval is ON, and a board with one app slot (the yellow board) stays on the cable.
// The framework's own OTA service and firmware upload page are not built into W-Soft
// (FT_OTA=0, FT_UPLOAD_FIRMWARE=0): this is the only way in over Wi-Fi.
//
// NVS namespace "wifiupd": the switch and the password. The password is never sent back,
// shown or logged. Every change and every installed update goes on the Approval audit
// counter and into the Approval log (approval_audit_event).
//
// Tasks: the web task changes the switch (wifi_update_set); the screen task runs the service
// (wifi_update_poll), writes the audit lines and is where an update comes in.

#include <Arduino.h>

#include <wsoft_wifi_update.h>

// The port espota sends to (the ESP32 default).
static const uint16_t WIFI_UPDATE_PORT = 3232;

// Once at boot, any task, before the first poll. version = this software's version, for the log.
void wifi_update_begin(const char *version);

// Screen task, every pass: starts and stops the service as the rules say, takes an update,
// writes waiting audit lines.
void wifi_update_poll();

// True while an update is coming in: the weigh loop and the Hub task stand still.
bool wifi_update_busy();

struct WifiUpdateInfo {
  bool two_copies;    // this board can hold a second copy of the software
  bool on;            // the switch
  bool password_set;
  bool approval_on;
  bool listening;     // the service is up and waiting for an update right now
  WsoftWifiUpdateError blocked;  // why it is not listening although switched on, or OK
};
void wifi_update_info(WifiUpdateInfo *out);

// Web task (admin): one change from the page. who = the signed-in name, for the log.
// BUSY when another change holds the lock for more than WIFI_UPDATE_SET_WAIT_MS.
static const uint32_t WIFI_UPDATE_SET_WAIT_MS = 500;
WsoftWifiUpdateError wifi_update_set(const WsoftWifiUpdateRequest &request, const char *who);
