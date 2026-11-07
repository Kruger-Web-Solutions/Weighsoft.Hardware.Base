#pragma once

// One request at a time from the web or Hub task to the screen task, which applies every
// switch (it is the only flash writer). Each caller gets a ticket and only ever reads, or
// withdraws, its own request, so no caller is handed another caller's answer.
// No locks here: lib/wsoft_esp/src/switches.cpp calls every method under one spinlock.
// Pure C++ for host tests (test/test_wsoft_switch_mailbox).

#include <stdint.h>

#include "wsoft_switch.h"

class WsoftSwitchMailbox {
 public:
  enum Status { QUEUED, RUNNING, DONE, GONE };

  explicit WsoftSwitchMailbox(uint32_t first_ticket = 1) : next_(first_ticket ? first_ticket : 1) {}

  // Caller: 0 while another request is in the box (BUSY), else this request's ticket.
  // A new request is taken only when the box is empty, or holds a result nobody waits for.
  uint32_t post(const WsoftSwitchRequest &request);
  // Screen task: takes the waiting request (QUEUED -> RUNNING).
  bool take(WsoftSwitchRequest *request);
  // Screen task: the answer to the request it took.
  void finish(WsoftSwitchError result);
  // Caller: where its own request is. DONE hands the answer over once and frees the box.
  // GONE: not this ticket's box any more.
  Status check(uint32_t ticket, WsoftSwitchError *result);
  // Caller gives up. True when its request was still queued: withdrawn, never applied.
  // False when it is running (it may still be applied; its answer is dropped) or not its own.
  bool leave(uint32_t ticket);

 private:
  enum State { IDLE, WAITING, TAKEN, FINISHED };
  State state_ = IDLE;
  WsoftSwitchRequest job_;
  uint32_t job_ticket_ = 0;  // the ticket of the request in the box
  uint32_t next_;
  bool waiter_ = false;      // the caller of job_ticket_ still waits for the answer
  WsoftSwitchError result_ = WSOFT_SWITCH_OK;
};
