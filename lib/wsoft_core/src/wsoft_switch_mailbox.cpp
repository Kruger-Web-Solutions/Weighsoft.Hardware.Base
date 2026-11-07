#include "wsoft_switch_mailbox.h"

uint32_t WsoftSwitchMailbox::post(const WsoftSwitchRequest &request) {
  bool free = state_ == IDLE || (state_ == FINISHED && !waiter_);
  if (!free) return 0;
  job_ = request;
  job_ticket_ = next_;
  next_ = next_ == 0xFFFFFFFFu ? 1 : next_ + 1;
  waiter_ = true;
  state_ = WAITING;
  return job_ticket_;
}

bool WsoftSwitchMailbox::take(WsoftSwitchRequest *request) {
  if (state_ != WAITING) return false;
  *request = job_;
  state_ = TAKEN;
  return true;
}

void WsoftSwitchMailbox::finish(WsoftSwitchError result) {
  if (state_ != TAKEN) return;
  result_ = result;
  // Nobody waits any more: drop the answer and free the box.
  state_ = waiter_ ? FINISHED : IDLE;
}

WsoftSwitchMailbox::Status WsoftSwitchMailbox::check(uint32_t ticket, WsoftSwitchError *result) {
  if (ticket == 0 || ticket != job_ticket_ || !waiter_) return GONE;
  switch (state_) {
    case WAITING: return QUEUED;
    case TAKEN: return RUNNING;
    case FINISHED:
      *result = result_;
      waiter_ = false;
      state_ = IDLE;
      return DONE;
    case IDLE: break;
  }
  return GONE;
}

bool WsoftSwitchMailbox::leave(uint32_t ticket) {
  if (ticket == 0 || ticket != job_ticket_ || !waiter_) return false;
  waiter_ = false;
  if (state_ == WAITING) {
    state_ = IDLE;
    return true;
  }
  if (state_ == FINISHED) state_ = IDLE;
  return false;
}
