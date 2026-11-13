#pragma once

// From the facts to the words on the screen. Pure C++ (no Arduino), host-tested
// (test/test_wsoft_ui). The board code reads the facts from its services into a WsoftUiInput
// every poll; wsoft_ui_model() turns them into the WsoftUiModel that wsoft_ui_paint draws.
// Every rule about what the screen says lives here, so it can be tested without a board.

#include <stddef.h>
#include <stdint.h>

#include "trade_logic.h"
#include "wsoft_hilo.h"
#include "wsoft_relay.h"
#include "wsoft_scale_setup.h"
#include "wsoft_ui_paint.h"
#include "wsoft_weighing.h"

struct WsoftUiInput {
  bool dark = true;
  int page = WSOFT_UI_PAGE_LIVE;

  // Wi-Fi and Hub
  bool wifi_up = false;
  int rssi = 0;               // dBm, while up
  const char *ip = "";        // the board's address on the Wi-Fi, while up
  bool wifi_alert = false;    // Wi-Fi is down, the alert is due and nobody dismissed it
  bool hub_online = false;
  bool hub_setup = false;     // not paired with a Hub yet: not a fault
  const char *hub_text = "";  // the Hub state in words (hub_state_text)

  // the scale
  const char *source = "RS485";  // RS485, RS232 or WRITER
  bool linked = false;
  bool alarm = false;
  bool stable = false;
  int32_t gross = 0;          // in decimals and unit
  int32_t net = 0;            // gross less the tare (the gross when no tare is on)
  bool tared = false;
  int decimals = 3;
  int unit = 1;               // 1 kg, 2 t, 3 g
  WsoftScaleHold hold = WSOFT_HOLD_NONE;

  // trade
  TradeCheck check = TRADE_OFF;
  uint32_t trade_seq = 0;     // the last trade weight
  uint32_t audit = 0;
  const char *plate = "";     // wsoft_ui_plate_text, "" when there is none

  // Hi-Lo
  WsoftRelayZone zone = WSOFT_RELAY_NONE;       // the zone the relays are driven with
  const WsoftHiloProduct *product = nullptr;    // the product in use, or nullptr
  size_t product_count = 0;                     // products the Hub gave this board

  // saving
  bool writer = false;        // Writer mode: this board never saves
  bool auto_save = false;
  size_t pending = 0;         // saved, not yet taken by the Hub
  WsoftSaveResult recent = WSOFT_SAVE_NONE;     // the last Save result while under 3 s old
  uint32_t saved_seq = 0;                       // the number of the last saved weighing
  WsoftSaveResult waiting = WSOFT_SAVE_NONE;    // what stops a save right now

  // the menu
  bool approval_on = false;
  bool rs485 = false;         // our weighing card: TARE is there
  bool tare_on = false;
  int32_t tare_value = 0;     // in decimals and unit
  bool preset_on = false;     // the setup has a preset tare
  const char *preset_text = "";  // "2.5 kg"

  // the pages
  const char *message = "";   // the page's top-right line
  int pin_count = 0;          // digits typed
  const char *address = "";   // to open in a browser: the no-PIN note and the Wi-Fi page
  const char *ap_name = "";   // the board's own hotspot
};

// Fills *model (every byte of it) from the facts.
void wsoft_ui_model(const WsoftUiInput &in, WsoftUiModel *model);

// Wi-Fi bars from the signal in dBm: 4 from -55, 3 from -67, 2 from -75, else 1.
int wsoft_ui_wifi_bars(int rssi);

// The plate line: "SA 1234/56   MAX 150 MIN 1 E 0.05 KG". "" when the setup has neither an SA
// number nor a Max. What is not known shows as '-' (e: '?').
void wsoft_ui_plate_text(const WsoftScaleSetup &setup, const WsoftCardFormat &card, char *out, size_t size);
