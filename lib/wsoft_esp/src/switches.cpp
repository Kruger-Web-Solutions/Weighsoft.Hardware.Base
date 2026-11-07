#include "switches.h"

#include "approval_base.h"
#include "pin.h"
#include "scale.h"
#include "weighings.h"

// One request at a time, like the DJB job in wsoft_screen.cpp.
static const int JOB_IDLE = 0;
static const int JOB_QUEUED = 1;
static const int JOB_RUNNING = 2;
static const int JOB_DONE = 3;

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static volatile int job_state = JOB_IDLE;
static WsoftSwitchRequest job;
static WsoftSwitchError job_result = WSOFT_SWITCH_OK;
static uint32_t changes = 0;

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
  scale_lock();
  WsoftSwitchState before = state_now();
  WsoftSwitchError error = wsoft_switch_check(r, before);
  if (error == WSOFT_SWITCH_OK) {
    if (r.which == WSOFT_SWITCH_APPROVAL) {
      approval_set(r.on, r.who, r.where, r.reason);
    } else {
      weighings_set_auto_save(r.on);
      Serial0.printf("SWITCH auto_save %s->%s by=%s where=%s why=%s\n", before.auto_save ? "ON" : "OFF",
                     r.on ? "ON" : "OFF", r.who, wsoft_switch_where_text(r.where), r.reason);
    }
    portENTER_CRITICAL(&lock);
    changes++;
    portEXIT_CRITICAL(&lock);
  }
  scale_unlock();
  if (error != WSOFT_SWITCH_OK) {
    Serial0.printf("SWITCH %s %s refused %s by=%s where=%s\n", wsoft_switch_name(r.which), r.on ? "ON" : "OFF",
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
  if (job_state != JOB_QUEUED) {
    portEXIT_CRITICAL(&lock);
    return;
  }
  r = job;
  job_state = JOB_RUNNING;
  portEXIT_CRITICAL(&lock);

  WsoftSwitchError result = apply(r);

  portENTER_CRITICAL(&lock);
  job_result = result;
  job_state = JOB_DONE;
  portEXIT_CRITICAL(&lock);
}

WsoftSwitchError switches_set(WsoftSwitchWhich which, bool on, const char *who, WsoftSwitchWhere where,
                              const char *reason, uint32_t timeout_ms) {
  WsoftSwitchRequest r;
  WsoftSwitchError error = wsoft_switch_request(which, on, who, where, reason, &r);
  if (error != WSOFT_SWITCH_OK) return error;

  portENTER_CRITICAL(&lock);
  // DONE with nobody waiting is a result whose caller timed out; it must not block for good.
  if (job_state != JOB_IDLE && job_state != JOB_DONE) {
    portEXIT_CRITICAL(&lock);
    return WSOFT_SWITCH_BUSY;
  }
  job = r;
  job_state = JOB_QUEUED;
  portEXIT_CRITICAL(&lock);

  uint32_t start = millis();
  for (;;) {
    portENTER_CRITICAL(&lock);
    int state = job_state;
    if (state == JOB_DONE) {
      WsoftSwitchError result = job_result;
      job_state = JOB_IDLE;
      portEXIT_CRITICAL(&lock);
      return result;
    }
    // Not taken in time: withdraw it, so it is never applied after the caller gave up.
    if (state == JOB_QUEUED && millis() - start > timeout_ms) {
      job_state = JOB_IDLE;
      portEXIT_CRITICAL(&lock);
      return WSOFT_SWITCH_BUSY;
    }
    portEXIT_CRITICAL(&lock);
    // Taken but not finished (a flash write): wait a little longer for the real answer.
    if (millis() - start > timeout_ms + 2000) return WSOFT_SWITCH_BUSY;
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
