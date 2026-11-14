#pragma once

// Hardware.Base only (WSOFT_ON_BASE): weighings saved on the board (Save tap or Auto save)
// and kept in flash (NVS namespace "weigh") until the Hub accepts them. The Auto save switch
// is kept there too (key "auto") and is changed only through switches.h.
// The rules live in wsoft_core (WsoftWeighingSaver, wsoft_weighing_json), host-tested.
//
// Tasks: the screen task calls weighings_update() and is the only writer of flash.
// The web server asks for a save with weighings_save(); the Hub task sends and acks.

#include <Arduino.h>

#include <wsoft_hilo.h>
#include <wsoft_weighing.h>

// rs232 = the weight comes from an RS232 scale line (else the DJB over RS485).
void weighings_begin(bool rs232);
// "rs232" or "rs485", for the weighing payload.
const char *weighings_source();

// Screen task, once per reading. for_trade = trade checks are on right now. tare_value is
// the tare in the reading's decimals and unit (0 = none). hold = why the RS485 setup holds
// this reading back (WsoftWeighingSaver::update). product = the Hi-Lo product in use, or
// nullptr: a saved weighing keeps its Hi-Lo result (judged on the net weight) for the Hub.
// Returns the result of this pass (WSOFT_SAVE_NONE most of the time).
WsoftSaveResult weighings_update(bool linked, bool fresh, bool alarm, int32_t value, int decimals, int unit,
                                 bool for_trade, int32_t tare_value, WsoftSaveResult hold,
                                 const WsoftHiloProduct *product);

// Any task: a person tapped Save. Waits up to timeout_ms for the screen task's answer.
// On WSOFT_SAVE_SAVED, *saved holds the weighing.
WsoftSaveResult weighings_save(uint32_t timeout_ms, TradeWeight *saved);
// Screen task: a tap on the board's own screen (no waiting, the next pass handles it).
void weighings_request_save();

// Writer role (Rhyno, 14 Nov 2026): a Save tap on this board goes to the weigh board, which
// saves it, so each load is still counted once. fn runs in the tapping task and must only set
// a flag. nullptr (the default) = this board saves its own weighings.
void weighings_set_remote_save(void (*fn)());
// Writer role: the weigh board's "what stops a save" and each of its Save results, from its
// feed, shown on this board's screen like its own.
void weighings_remote_status(WsoftSaveResult waiting);
void weighings_remote_result(WsoftSaveResult result);
// Weigh board: Save results (tap or auto) since boot, for the feed; 0 = none yet.
uint32_t weighings_result_count();

// Screen task only (it writes flash, NVS "weigh" key "auto"), through switches.cpp.
void weighings_set_auto_save(bool on);

struct WeighingsInfo {
  bool auto_save;
  size_t pending;              // saved, not yet accepted by the Hub
  TradeWeight last;            // last weighing saved since boot (valid = false when none)
  WsoftSaveResult last_result; // last Save tap or auto save result
  uint32_t last_result_ms;     // millis() of last_result, 0 when none
  WsoftSaveResult waiting;     // what stops a save right now (WsoftWeighingSaver::waiting)
};
void weighings_info(WeighingsInfo *out);

// Hub task: the oldest unsent weighing, its tare (0 = none) and its Hi-Lo result (hilo may be
// nullptr). False when none wait.
bool weighings_oldest(TradeWeight *w, bool *for_trade, int32_t *tare_value, WsoftHiloStamp *hilo);
// Hub task: the Hub accepted (or permanently refused) everything up to seq.
void weighings_delivered(uint32_t seq);
