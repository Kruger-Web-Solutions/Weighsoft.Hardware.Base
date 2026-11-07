#pragma once

// Hardware.Base only (WSOFT_ON_BASE): weighings saved on the board (Save tap or Auto save)
// and kept in flash (NVS namespace "weigh") until the Hub accepts them. The Auto save switch
// is kept there too (key "auto") and is changed only through switches.h.
// The rules live in wsoft_core (WsoftWeighingSaver, wsoft_weighing_json), host-tested.
//
// Tasks: the screen task calls weighings_update() and is the only writer of flash.
// The web server asks for a save with weighings_save(); the Hub task sends and acks.

#include <Arduino.h>

#include <wsoft_weighing.h>

// rs232 = the weight comes from an RS232 scale line (else the DJB over RS485).
void weighings_begin(bool rs232);
// "rs232" or "rs485", for the weighing payload.
const char *weighings_source();

// Screen task, once per reading. for_trade = trade checks are on right now.
// Returns the result of this pass (WSOFT_SAVE_NONE most of the time).
WsoftSaveResult weighings_update(bool linked, bool fresh, bool alarm, int32_t value, int decimals, int unit,
                                 bool for_trade);

// Any task: a person tapped Save. Waits up to timeout_ms for the screen task's answer.
// On WSOFT_SAVE_SAVED, *saved holds the weighing.
WsoftSaveResult weighings_save(uint32_t timeout_ms, TradeWeight *saved);
// Screen task: a tap on the board's own screen (no waiting, the next pass handles it).
void weighings_request_save();

// Screen task only (it writes flash, NVS "weigh" key "auto"), through switches.cpp.
void weighings_set_auto_save(bool on);

struct WeighingsInfo {
  bool auto_save;
  size_t pending;              // saved, not yet accepted by the Hub
  TradeWeight last;            // last weighing saved since boot (valid = false when none)
  WsoftSaveResult last_result; // last Save tap or auto save result
  uint32_t last_result_ms;     // millis() of last_result, 0 when none
};
void weighings_info(WeighingsInfo *out);

// Hub task: the oldest unsent weighing. False when none wait.
bool weighings_oldest(TradeWeight *w, bool *for_trade);
// Hub task: the Hub accepted (or permanently refused) everything up to seq.
void weighings_delivered(uint32_t seq);
