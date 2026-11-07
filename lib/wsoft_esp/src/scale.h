#pragma once

// Hardware.Base only (WSOFT_ON_BASE): the RS485 scale setup kept on the board (NVS namespace
// "scale"), the card's e / decimals / unit as last read, and the setup audit counter.
// The rules are wsoft_scale_setup (wsoft_core, host-tested). RS232: nothing here applies,
// the indicator owns its setup.
//
// Tasks: the web task saves the setup; the screen task reads the card and counts card writes.
// scale_lock() is a mutex held across "check, then write" so a setup save and an Approval
// switch can never cross.

#include <Arduino.h>

#include <wsoft_scale_setup.h>

// Screen task, at boot. rs485 = the weight comes from our weighing card.
void scale_begin(bool rs485);
bool scale_rs485();

void scale_lock();
void scale_unlock();

bool scale_setup_complete();

// Screen task: the values just read from the card.
void scale_set_card(int32_t division, int decimal, int unit);
// Screen task: a card register write went through (setup audit + 1).
void scale_card_written();

struct ScaleInfo {
  bool rs485;
  WsoftScaleSetup setup;
  WsoftCardFormat card;
  uint32_t audit;
};
void scale_info(ScaleInfo *out);

// Web task (admin): checks and saves an A5 body. LOCKED while Approval is ON or on RS232.
// who is the signed-in name, for the USB log line.
WsoftSetupError scale_save(const WsoftScaleSetupInput &in, const char *who);
