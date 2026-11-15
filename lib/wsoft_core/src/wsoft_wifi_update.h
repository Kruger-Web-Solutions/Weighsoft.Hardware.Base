#pragma once

// The "Wi-Fi update" switch: new W-Soft software sent to the board over Wi-Fi, so it does not
// have to be plugged into a laptop (Rhyno, 14 Nov 2026). Pure C++ for host tests
// (test/test_wsoft_wifi_update). lib/wsoft_esp/src/wifi_update.cpp keeps the switch and the
// password in flash and runs the update service; the web page is W-Soft > Settings (admin).
//
// Rules, the same on every W-Soft board, make and model:
// - OFF by default. A board that was never set up cannot be updated over Wi-Fi.
// - Each board has its own password, set on that board. There is no factory or shared
//   password: with no password of its own the switch cannot go ON.
// - The password is 12 to 64 printable characters, no spaces. It is never sent back, shown or
//   logged. It may not be a well-known factory password (the framework's "esp-react").
// - Blocked while Approval is ON, like Calibrate and the scale setup: the switch and the
//   password cannot change, and the update service does not listen. Switch Approval OFF first
//   (that is logged on the audit counter too).
// - A board with room for only one copy of the software (the yellow board, wsoft_cyd28) cannot
//   update over Wi-Fi at all: the switch says so and stays OFF. That board updates by cable.
// - Every change of the switch or the password, and every update that was installed, adds one
//   to the Approval audit counter and writes a line in the Approval log.

#include <stddef.h>
#include <stdint.h>

static const size_t WSOFT_WIFI_UPDATE_PASSWORD_MIN = 12;
static const size_t WSOFT_WIFI_UPDATE_PASSWORD_MAX = 64;
static const int WSOFT_WIFI_UPDATE_WHO_MAX = 64;

enum WsoftWifiUpdateError {
  WSOFT_WIFI_UPDATE_OK = 0,
  WSOFT_WIFI_UPDATE_ONE_COPY,          // no second app slot: cable only (yellow board)
  WSOFT_WIFI_UPDATE_APPROVAL_ON,       // locked while Approval is ON
  WSOFT_WIFI_UPDATE_PASSWORD_FORMAT,   // not 12 to 64 printable characters without spaces
  WSOFT_WIFI_UPDATE_PASSWORD_FACTORY,  // a well-known factory password
  WSOFT_WIFI_UPDATE_PASSWORD_REQUIRED, // ON asked for, and the board has no password of its own
  WSOFT_WIFI_UPDATE_NOTHING,           // the request changes nothing (no switch, no password)
  WSOFT_WIFI_UPDATE_BUSY,              // the board could not take the change now: not applied
};

// "ONE_COPY", "APPROVAL_ON", ... ("" for OK). At most 32 characters, [A-Z_].
const char *wsoft_wifi_update_error_code(WsoftWifiUpdateError error);
// Plain words for the page.
const char *wsoft_wifi_update_error_text(WsoftWifiUpdateError error);

// OK, PASSWORD_FORMAT or PASSWORD_FACTORY.
WsoftWifiUpdateError wsoft_wifi_update_password_check(const char *password);

struct WsoftWifiUpdateState {
  bool two_copies = false;    // the flash has a second app slot (esp_ota_get_next_update_partition)
  bool approval_on = false;
  bool on = false;            // the switch, as stored
  bool password_set = false;  // the board has its own password
};

// One change from the page. A field that is not sent keeps its value.
struct WsoftWifiUpdateRequest {
  bool has_on = false;
  bool on = false;
  const char *password = nullptr;  // nullptr = not sent
};

struct WsoftWifiUpdateChange {
  bool on = false;             // the switch after the change
  bool on_changed = false;
  bool password_changed = false;  // store request.password
};

// Checks one request against the board's state right now. On an error *out must not be used.
// Switching OFF is always allowed, also with Approval ON and on a one-copy board: OFF is the
// safe state. Everything else follows the rules at the top of this file.
WsoftWifiUpdateError wsoft_wifi_update_check(const WsoftWifiUpdateRequest &request,
                                             const WsoftWifiUpdateState &state, WsoftWifiUpdateChange *out);

// True when the update service may listen now: two copies, switch ON, own password, Approval OFF.
bool wsoft_wifi_update_listening(const WsoftWifiUpdateState &state);

// Why the service is not listening, for the page: ONE_COPY, APPROVAL_ON, PASSWORD_REQUIRED,
// or OK (listening, or simply switched off).
WsoftWifiUpdateError wsoft_wifi_update_blocked(const WsoftWifiUpdateState &state);

enum WsoftWifiUpdateEvent {
  WSOFT_WIFI_UPDATE_EVENT_ON = 0,
  WSOFT_WIFI_UPDATE_EVENT_OFF,
  WSOFT_WIFI_UPDATE_EVENT_PASSWORD,
  WSOFT_WIFI_UPDATE_EVENT_ON_PASSWORD,  // switched ON and a new password, in one save
  WSOFT_WIFI_UPDATE_EVENT_INSTALLED,    // an update came in over Wi-Fi and was installed
};

// The event for a checked change. False when the change changes nothing.
bool wsoft_wifi_update_event(const WsoftWifiUpdateChange &change, WsoftWifiUpdateEvent *event);

// "<counter> <time> WIFI-UPDATE <what> by=<who> was=<version>" plus the end byte. Never a password.
static const size_t WSOFT_WIFI_UPDATE_LOG_MAX = 10 + 1 + 20 + 13 + 16 + 4 + WSOFT_WIFI_UPDATE_WHO_MAX + 5 + 16 + 1;

// "9 2026-11-14T18:00:00Z WIFI-UPDATE INSTALLED by=wifi was=0.7.0". who: spaces become '_',
// cut at WSOFT_WIFI_UPDATE_WHO_MAX; empty or null reads "unknown". version is cut at 16.
// False when out is too small (nothing useful is written then).
bool wsoft_wifi_update_log_line(uint32_t counter, const char *when, WsoftWifiUpdateEvent event, const char *who,
                                const char *version, char *out, size_t size);
