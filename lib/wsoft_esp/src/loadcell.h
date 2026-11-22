#pragma once

// Load cell on an HX711 amplifier board (Rhyno, 22 Nov 2026): the board reads the chip itself
// on two pins (DT = data from the chip, SCK = clock to the chip) and keeps the calibration in
// NVS (namespace "loadcell"). The rules are wsoft_loadcell (wsoft_core, host-tested).
//
// Tasks: a small task of its own reads the chip whenever a count is ready, gathers the counts
// of each tenth of a second into one and keeps a moving average of those (the filter); the screen task asks for the weight once per poll; the web task reads the state and
// sets zero, span and the scale setup (unit, decimals, division). Everything shared is under one spinlock.
//
// Timing, from the HX711 datasheet: DT low = a count is ready; 24 clock pulses read it, most
// significant bit first; a 25th pulse picks channel A at gain 128 for the next count. A clock
// pulse stays high at most 50 us (60 us or more puts the chip to sleep), so the 25 pulses run
// with interrupts off.
// Power the HX711 board from 3.3 V: its DT signal is at its supply voltage.
// On the bench 22 Nov 2026: Rhyno's yellow board weighs with an HW-29 HX711 board on CN1.

#include <Arduino.h>

#include <wsoft_loadcell.h>

// Starts the reader task. Call once, from the screen task's setup.
void loadcell_begin(int dt_pin, int sck_pin);

// Screen task, once per poll: the calibrated weight. False when the chip is silent (no count
// for a second), out of range, or the board is not calibrated yet: never a weight then.
bool loadcell_weight(int32_t *value, int *decimals, int *unit);

struct LoadcellInfo {
  bool started;     // loadcell_begin ran (the source is the load cell)
  bool present;     // a count arrived in the last second
  bool have_raw;    // the average below is usable
  int32_t raw;      // the averaged count
  bool calibrated;
  bool have_weight;
  int32_t value;    // the weight in display digits (with cal.decimals and cal.unit)
  uint32_t rate;         // counts a second from the chip, measured; 0 when silent
  int32_t zero_offset;   // counts auto zero has moved the zero by, since the last restart or zero
  bool have_noise;
  int32_t noise_tenths;  // how far the last counts are apart, in tenths of a division (10 = one division)
  WsoftLoadcellCal cal;
};
void loadcell_info(LoadcellInfo *out);

// Web task (admin); zero also from the ZERO tile on the screen. Each uses the averaged count of
// this moment and stores the result.
// who is the signed-in name, for the USB log line.
WsoftLoadcellError loadcell_set_zero(const char *who);
WsoftLoadcellError loadcell_set_span(int32_t value, int decimals, int unit, const char *who);
// The scale setup: digits after the point, unit (1 kg, 3 g), the division in the last digit,
// the filter (WSOFT_LOADCELL_FILTER_OFF .. _HIGH) and auto zero (0 off, 1..3 divisions). All
// of it or none of it.
WsoftLoadcellError loadcell_set_format(int decimals, int unit, int step, int filter, int zero_track,
                                       const char *who);
