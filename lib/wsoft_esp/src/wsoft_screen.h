#pragma once

// The W-Soft weigh screen (was main.cpp in Weighsoft.WSoft esp/src @ cfcd012).
// On Hardware.Base, WSoftScreenTask runs wsoft_screen_setup() once and then
// wsoft_screen_loop() forever in its own FreeRTOS task, so the blocking Modbus poll
// and panel draw never stall the web server or MQTT.

#include "wsoft_djb.h"
#include "wsoft_view.h"

// Where the weight comes from:
// RS485: the screen task owns Serial1 and polls the DJB over RS485 (Modbus).
// RS232: the Serial reader service owns Serial1 and reads an RS232 scale's lines; the
//   screen task takes the weight from the line reader below and never opens Serial1.
// WRITER: the weight comes from a weigh board over Wi-Fi (the line reader below); the
//   Writer service owns Serial1 and sends it out. This board saves no weighings.
enum WsoftScreenSource { WSOFT_SCREEN_RS485, WSOFT_SCREEN_RS232, WSOFT_SCREEN_WRITER };
void wsoft_screen_setup(WsoftScreenSource source = WSOFT_SCREEN_RS485);
void wsoft_screen_loop();

// RS232 and Writer: copies the latest weight text and the millis() it arrived, and may set
// the DJB unit code (left at 1 = kg otherwise). Returns false when nothing has arrived yet.
// Called from the screen task, so it must be safe across tasks (a StatefulService read
// lock is).
typedef bool (*WsoftLineReader)(char *weight, size_t size, uint32_t *received_ms, int *unit);
void wsoft_screen_set_line_reader(WsoftLineReader reader);

// Copies the latest snapshot (written by the screen task after every poll).
// Safe to call from any task; it only takes a short spinlock.
void wsoft_screen_snapshot(WSoftSnapshot *out);

// One DJB transaction, run on the screen task (the only task that uses Serial1).
// The caller waits. Returns false when the bus was not used (busy or timeout);
// out->error says which. Returns true when the attempt finished, including a
// refused write while the scale is not linked and a Modbus failure. out->ok is
// the Modbus result. Writes use the slave that last answered the weight poll.
struct WsoftDjbResult {
  bool ok;
  bool sent;
  char error[24];
  uint8_t slave;
  uint16_t doc;
  uint16_t pdu;
  uint8_t command;
  int32_t value;
  char request_hex[48];
  char reply_hex[80];
  uint8_t exception;
  bool alarm_valid;
  uint32_t alarm_raw;
  WsoftDjbAlarmBits alarm;
};

bool wsoft_screen_djb(const WsoftDjbPlan *plan, WsoftDjbResult *out, uint32_t timeout_ms);
