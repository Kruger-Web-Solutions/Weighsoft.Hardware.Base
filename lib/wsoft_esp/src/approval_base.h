#pragma once

// Hardware.Base only (WSOFT_ON_BASE): the extra Approval calls the switches and the scale
// setup need. Defined in approval.cpp inside #ifdef WSOFT_ON_BASE, and kept out of
// approval.h so that file stays identical to the Weighsoft.WSoft original.

#include <wsoft_scale_setup.h>
#include <wsoft_switch.h>

#include "approval.h"

// Writes the switch: log line (with where=), audit counter + 1, engine reset.
// Screen task only, and only through switches.cpp, which checks the rules first.
void approval_set(bool on, const char *who, WsoftSwitchWhere where, const char *reason);

// The last approval log lines, oldest first (at most max, and at most 8). Any task.
int approval_log_lines(char lines[][WSOFT_SWITCH_LOG_MAX], int max);

// approval_update() with the scale setup's hold for this reading (see approval.cpp).
TradeCheck approval_update_held(bool linked, bool fresh, bool alarm, int32_t value, int decimals, int unit,
                                WsoftScaleHold hold);
