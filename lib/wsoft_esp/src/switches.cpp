#include "wsoft_log.h"
#include "switches.h"

#include "approval_base.h"
#include "pin.h"
#include "scale.h"
#include "weighings.h"

#include <wsoft_switch_mailbox.h>

// One request at a time, each with its own ticket (wsoft_switch_mailbox). Every call on
// the box is under this spinlock.
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static WsoftSwitchMailbox box;
static uint32_t changes = 0;
// Once the screen task has a request, the caller waits for its answer: one flash write.
// This cap is only for a stuck screen task; then the answer is UNCONFIRMED, never BUSY.
static const uint32_t RUNNING_MAX_MS = 5000;

static WsoftSwitchState state_now() {
  WeighingsInfo weigh;
  weighings_info(&weigh);
  WsoftSwitchState s;
  s.approval_on = approval_on();
  s.auto_save = weigh.auto_save;
  s.rs485 = scale_rs485();
  s.setup_complete = scale_setup_complete();
  return s;
}

// Screen task. The scale setup lock is held from the check to the write, so a setup save
// cannot slip in between "setup complete" and "Approval ON".
static WsoftSwitchError apply(const WsoftSwitchRequest &r) {
  scale_lock(SCALE_LOCK_FOREVER);
  WsoftSwitchState before = state_now();
  WsoftSwitchError error = wsoft_switch_check(r, before);
  if (error == WSOFT_SWITCH_OK) {
    if (r.which == WSOFT_SWITCH_APPROVAL) {
      approval_set(r.on, r.who, r.where, r.reason);
    } else {
      weighings_set_auto_save(r.on);
      WSOFT_LOG.printf("SWITCH auto_save %s->%s by=%s where=%s why=%s\n", before.auto_save ? "ON" : "OFF",
                     r.on ? "ON" : "OFF", r.who, wsoft_switch_where_text(r.where), r.reason);
    }
    portENTER_CRITICAL(&lock);
    changes++;
    portEXIT_CRITICAL(&lock);
  }
  scale_unlock();
  if (error != WSOFT_SWITCH_OK) {
    WSOFT_LOG.printf("SWITCH %s %s refused %s by=%s where=%s\n", wsoft_switch_name(r.which), r.on ? "ON" : "OFF",
                   wsoft_switch_error_code(error), r.who, wsoft_switch_where_text(r.where));
  }
  return error;
}

WsoftSwitchError switches_apply_here(WsoftSwitchWhich which, bool on, const char *who, WsoftSwitchWhere where,
                                     const char *reason) {
  WsoftSwitchRequest r;
  WsoftSwitchError error = wsoft_switch_request(which, on, who, where, reason, &r);
  if (error != WSOFT_SWITCH_OK) return error;
  return apply(r);
}

void switches_poll() {
  WsoftSwitchRequest r;
  portENTER_CRITICAL(&lock);
  bool have = box.take(&r);
  portEXIT_CRITICAL(&lock);
  if (!have) return;

  WsoftSwitchError result = apply(r);

  portENTER_CRITICAL(&lock);
  box.finish(result);
  portEXIT_CRITICAL(&lock);
}

WsoftSwitchError switches_set(WsoftSwitchWhich which, bool on, const char *who, WsoftSwitchWhere where,
                              const char *reason, uint32_t timeout_ms) {
  WsoftSwitchRequest r;
  WsoftSwitchError error = wsoft_switch_request(which, on, who, where, reason, &r);
  if (error != WSOFT_SWITCH_OK) return error;

  portENTER_CRITICAL(&lock);
  uint32_t ticket = box.post(r);
  portEXIT_CRITICAL(&lock);
  if (ticket == 0) return WSOFT_SWITCH_BUSY;  // another request is in the box

  uint32_t start = millis();
  for (;;) {
    WsoftSwitchError result = WSOFT_SWITCH_BUSY;
    uint32_t waited = millis() - start;
    portENTER_CRITICAL(&lock);
    WsoftSwitchMailbox::Status status = box.check(ticket, &result);
    if (status == WsoftSwitchMailbox::QUEUED && waited > timeout_ms) {
      box.leave(ticket);  // still queued: withdrawn, so it is never applied
      portEXIT_CRITICAL(&lock);
      return WSOFT_SWITCH_BUSY;
    }
    if (status == WsoftSwitchMailbox::RUNNING && waited > timeout_ms + RUNNING_MAX_MS) {
      box.leave(ticket);  // its answer is dropped when it comes
      portEXIT_CRITICAL(&lock);
      return WSOFT_SWITCH_UNCONFIRMED;
    }
    portEXIT_CRITICAL(&lock);
    if (status == WsoftSwitchMailbox::DONE) return result;
    if (status == WsoftSwitchMailbox::GONE) return WSOFT_SWITCH_UNCONFIRMED;  // cannot happen: only we leave
    delay(10);
  }
}

void switches_info(SwitchesInfo *out) {
  WsoftSwitchState s = state_now();
  out->rs485 = s.rs485;
  out->approval_on = s.approval_on;
  out->audit_counter = approval_audit_counter();
  out->trade_check = approval_check();
  out->auto_save = s.auto_save;
  out->blocked = wsoft_switch_blocked(s);
  out->pin_set = pin_is_set();
  out->pin_locked_s = pin_locked_s();
}

uint32_t switches_change_count() {
  portENTER_CRITICAL(&lock);
  uint32_t value = changes;
  portEXIT_CRITICAL(&lock);
  return value;
}

String switches_health_fields() {
  WeighingsInfo weigh;
  weighings_info(&weigh);
  // RS232: the indicator owns the setup, so there is nothing left to set on the board.
  bool complete = !scale_rs485() || scale_setup_complete();
  String out = ",\"auto_save\":";
  out += weigh.auto_save ? "true" : "false";
  out += ",\"scale_setup_complete\":";
  out += complete ? "true" : "false";
  return out;
}
