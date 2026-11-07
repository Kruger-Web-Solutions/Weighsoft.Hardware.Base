#pragma once

// The two board switches, Approval and Auto save, and the one set of rules every place
// uses to change them: the screen, the browser, the Hub and the USB line.
// Pure C++ for host tests (test/test_wsoft_switch). lib/wsoft_esp/src/switches.cpp
// stores the result; approval.cpp writes the Approval log line and the audit counter.
//
// Rules:
// - Approval needs a reason (1 to 80 printable characters, trimmed). Auto save does not.
// - A switch to the state it is already in changes nothing (ALREADY).
// - On RS485 Approval can only go ON once the scale setup is complete. RS232: the
//   indicator owns the setup, so there is no such check.
// - Last change wins: each request is checked against the state the one before it left.

#include <stddef.h>
#include <stdint.h>

enum WsoftSwitchWhich { WSOFT_SWITCH_APPROVAL = 0, WSOFT_SWITCH_AUTO_SAVE = 1 };

enum WsoftSwitchWhere { WSOFT_WHERE_SCREEN = 0, WSOFT_WHERE_BROWSER, WSOFT_WHERE_HUB, WSOFT_WHERE_USB };

enum WsoftSwitchError {
  WSOFT_SWITCH_OK = 0,
  WSOFT_SWITCH_ALREADY,
  WSOFT_SWITCH_SETUP_INCOMPLETE,
  WSOFT_SWITCH_REASON_REQUIRED,
  WSOFT_SWITCH_REASON_TOO_LONG,
  WSOFT_SWITCH_BUSY,          // the screen task did not take the request in time
  WSOFT_SWITCH_WHO_REQUIRED,  // no name to log (should not happen: every caller has one)
};

static const int WSOFT_SWITCH_WHO_MAX = 64;
static const int WSOFT_SWITCH_REASON_MAX = 80;
// "<counter> <time> OFF->ON by=<who> where=<where> why=<reason>" plus the end byte.
static const size_t WSOFT_SWITCH_LOG_MAX = 10 + 1 + 20 + 9 + 4 + WSOFT_SWITCH_WHO_MAX + 7 + 7 + 5 +
                                           WSOFT_SWITCH_REASON_MAX + 1;

struct WsoftSwitchRequest {
  WsoftSwitchWhich which = WSOFT_SWITCH_APPROVAL;
  bool on = false;
  char who[WSOFT_SWITCH_WHO_MAX + 1] = "";
  WsoftSwitchWhere where = WSOFT_WHERE_SCREEN;
  char reason[WSOFT_SWITCH_REASON_MAX + 1] = "";
};

struct WsoftSwitchState {
  bool approval_on = false;
  bool auto_save = false;
  bool rs485 = true;           // the weight comes from our weighing card
  bool setup_complete = false;  // the RS485 scale setup is complete (wsoft_scale_setup)
};

// "approval" / "auto_save" (the JSON and Hub names). False for anything else.
bool wsoft_switch_parse(const char *text, WsoftSwitchWhich *which);
const char *wsoft_switch_name(WsoftSwitchWhich which);
const char *wsoft_switch_where_text(WsoftSwitchWhere where);
// "ALREADY", "SETUP_INCOMPLETE", ... ("" for OK). At most 32 characters, [A-Z_].
const char *wsoft_switch_error_code(WsoftSwitchError error);
// Plain words for the page and the screen.
const char *wsoft_switch_error_text(WsoftSwitchError error);

// Cleans and checks one request. The reason follows wsoft_reason_check (required for
// Approval). who is trimmed, spaces become '_' (one word in the log). A who over
// WSOFT_SWITCH_WHO_MAX is cut and ends in '~', so the log shows that it was cut.
// On an error *out is left half filled and must not be used.
WsoftSwitchError wsoft_switch_request(WsoftSwitchWhich which, bool on, const char *who, WsoftSwitchWhere where,
                                      const char *reason, WsoftSwitchRequest *out);

// The reason rule shared with the scale setup save: printable ASCII (0x20..0x7e) only,
// trimmed, at most 80. required = an empty reason is REASON_REQUIRED. out gets the trimmed text.
WsoftSwitchError wsoft_reason_check(const char *reason, bool required, char out[WSOFT_SWITCH_REASON_MAX + 1]);

// Checks a clean request against the board's state right now.
WsoftSwitchError wsoft_switch_check(const WsoftSwitchRequest &request, const WsoftSwitchState &state);

// Why Approval cannot go ON right now (SETUP_INCOMPLETE), or OK.
WsoftSwitchError wsoft_switch_blocked(const WsoftSwitchState &state);

// "4 2026-11-07T21:00:00Z OFF->ON by=rhyno where=browser why=bench test".
// False when out is too small (nothing useful is written then).
bool wsoft_switch_log_line(uint32_t counter, const char *when, bool from_on, bool to_on, const char *who,
                           WsoftSwitchWhere where, const char *reason, char *out, size_t size);
