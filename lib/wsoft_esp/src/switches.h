#pragma once

// Hardware.Base only (WSOFT_ON_BASE): the one way to switch Approval and Auto save, from the
// screen, the browser, the Hub or the USB line. The rules are wsoft_switch (wsoft_core,
// host-tested). The board holds the true state; every place shows that state.
//
// Tasks: the screen task is the only flash writer, so it applies every change.
// Other tasks (web server, Hub) call switches_set(), which hands the request to the screen
// task through one mailbox and waits for the answer, like weighings_save().
// Code that already runs on the screen task (its touch pages, the USB line) calls
// switches_apply_here() instead.

#include <Arduino.h>

#include <trade_logic.h>
#include <wsoft_switch.h>

// Screen task, every pass: applies a waiting request.
void switches_poll();

// Any task but the screen task. Waits up to timeout_ms for the screen task to take the
// request; BUSY when it did not, or when another request is still waiting.
WsoftSwitchError switches_set(WsoftSwitchWhich which, bool on, const char *who, WsoftSwitchWhere where,
                              const char *reason, uint32_t timeout_ms);

// Screen task only: checks and applies now.
WsoftSwitchError switches_apply_here(WsoftSwitchWhich which, bool on, const char *who, WsoftSwitchWhere where,
                                     const char *reason);

struct SwitchesInfo {
  bool rs485;                // weight from our weighing card (else an RS232 indicator)
  bool approval_on;
  uint32_t audit_counter;
  TradeCheck trade_check;
  bool auto_save;
  WsoftSwitchError blocked;  // why Approval cannot go ON now (SETUP_INCOMPLETE), or OK
  bool pin_set;
  uint32_t pin_locked_s;
};
void switches_info(SwitchesInfo *out);

// Goes up by one on every change, from anywhere. The Hub task sends a heartbeat when it moves.
uint32_t switches_change_count();

// Added inside the heartbeat's "health" object, starting with a comma:
// ,"auto_save":true|false,"scale_setup_complete":true|false
String switches_health_fields();
