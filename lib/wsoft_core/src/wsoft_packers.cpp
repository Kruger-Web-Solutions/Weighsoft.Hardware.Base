#include "wsoft_packers.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "wsoft_json_reader.h"
#include "wsoft_sha256.h"

namespace {

bool eq_ci(const char *a, const char *b) {
  if (a == nullptr || b == nullptr) return false;
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
    a++;
    b++;
  }
  return *a == *b;
}

bool safe_json_str(const char *s) {
  if (s == nullptr) return false;
  for (; *s; s++) {
    if (*s == '"' || *s == '\\' || (unsigned char)*s < 0x20) return false;
  }
  return true;
}

void copy_trunc(char *dst, size_t cap, const char *src) {
  if (dst == nullptr || cap == 0) return;
  if (src == nullptr) {
    dst[0] = 0;
    return;
  }
  size_t n = 0;
  for (; src[n] && n + 1 < cap; n++) dst[n] = src[n];
  dst[n] = 0;
}

}  // namespace

// ---------- 3-4 digit packer/floor-manager PIN + 45 s lock ----------

bool wsoft_packer_pin_format_ok(const char *pin) {
  if (pin == nullptr) return false;
  size_t length = strlen(pin);
  if (length < WSOFT_PACKER_PIN_MIN || length > WSOFT_PACKER_PIN_MAX) return false;
  for (size_t i = 0; i < length; i++) {
    if (pin[i] < '0' || pin[i] > '9') return false;
  }
  return true;
}

void wsoft_packer_pin_hash(const uint8_t salt[WSOFT_PACKER_PIN_SALT_SIZE], const char *pin, uint8_t out[32]) {
  WsoftSha256 ctx;
  wsoft_sha256_begin(&ctx);
  wsoft_sha256_add(&ctx, salt, WSOFT_PACKER_PIN_SALT_SIZE);
  wsoft_sha256_add(&ctx, (const uint8_t *)pin, strlen(pin));
  wsoft_sha256_end(&ctx, out);
}

bool wsoft_packer_pin_matches(const uint8_t salt[WSOFT_PACKER_PIN_SALT_SIZE], const uint8_t hash[32],
                              const char *pin) {
  if (!wsoft_packer_pin_format_ok(pin)) return false;
  uint8_t given[32];
  wsoft_packer_pin_hash(salt, pin, given);
  bool same = wsoft_same_bytes(given, hash, 32);
  memset(given, 0, sizeof given);
  return same;
}

bool wsoft_packer_hex_decode(const char *hex, uint8_t *out, size_t byte_len) {
  if (hex == nullptr || out == nullptr) return false;
  if (strlen(hex) != byte_len * 2) return false;
  for (size_t i = 0; i < byte_len; i++) {
    int hi = WsoftJsonReader::hex_digit(hex[i * 2]);
    int lo = WsoftJsonReader::hex_digit(hex[i * 2 + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = (uint8_t)((hi << 4) | lo);
  }
  return true;
}

WsoftPackerPinGuard::WsoftPackerPinGuard(uint32_t fails, uint32_t now_ms) {
  if (fails >= WSOFT_PACKER_PIN_TRIES) {
    locked_ = true;
    lock_from_ms_ = now_ms;
    fails_ = WSOFT_PACKER_PIN_TRIES;
  } else {
    fails_ = fails;
  }
}

uint32_t WsoftPackerPinGuard::locked_s(uint32_t now_ms) const {
  if (!locked_) return 0;
  uint32_t passed = now_ms - lock_from_ms_;  // unsigned: safe across millis() wrap
  if (passed >= WSOFT_PACKER_PIN_LOCK_MS) return 0;
  return (WSOFT_PACKER_PIN_LOCK_MS - passed + 999) / 1000;
}

void WsoftPackerPinGuard::wrong(uint32_t now_ms) {
  if (locked_ && locked_s(now_ms) == 0) {
    locked_ = false;
    fails_ = 0;
  }
  if (locked_) return;
  fails_++;
  if (fails_ >= WSOFT_PACKER_PIN_TRIES) {
    locked_ = true;
    lock_from_ms_ = now_ms;
  }
}

void WsoftPackerPinGuard::right() {
  fails_ = 0;
  locked_ = false;
}

WsoftPackerPinResult wsoft_packer_pin_try(WsoftPackerPinGuard *guard, bool has_verifier,
                                          const uint8_t salt[WSOFT_PACKER_PIN_SALT_SIZE], const uint8_t hash[32],
                                          const char *entry, uint32_t now_ms) {
  if (!has_verifier) return WSOFT_PACKER_PIN_NOT_SET;
  if (guard->locked_s(now_ms) > 0) return WSOFT_PACKER_PIN_LOCKED;
  if (wsoft_packer_pin_matches(salt, hash, entry)) {
    guard->right();
    return WSOFT_PACKER_PIN_OK;
  }
  guard->wrong(now_ms);
  return guard->locked_s(now_ms) > 0 ? WSOFT_PACKER_PIN_LOCKED : WSOFT_PACKER_PIN_WRONG;
}

// ---------- Roster & strip ----------

bool wsoft_packer_code_ok(const char *code) {
  if (code == nullptr) return false;
  size_t length = strlen(code);
  if (length == 0 || length > WSOFT_PACKER_CODE_MAX) return false;
  for (size_t i = 0; i < length; i++) {
    char c = code[i];
    bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' ||
              c == '-';
    if (!ok) return false;
  }
  return true;
}

void wsoft_packer_roster_reset(WsoftPackerRoster *roster) {
  static const WsoftPackerRoster empty;  // built once, never on a task's stack
  *roster = empty;
}

const WsoftPackerEntry *wsoft_packer_find(const WsoftPackerRoster &roster, const char *code) {
  if (code == nullptr || code[0] == 0) return nullptr;
  for (size_t i = 0; i < roster.count && i < WSOFT_PACKERS_MAX; i++) {
    if (eq_ci(roster.packers[i].code, code)) return &roster.packers[i];
  }
  return nullptr;
}

WsoftPackerEntry *wsoft_packer_find_mut(WsoftPackerRoster *roster, const char *code) {
  if (roster == nullptr || code == nullptr || code[0] == 0) return nullptr;
  for (size_t i = 0; i < roster->count && i < WSOFT_PACKERS_MAX; i++) {
    if (eq_ci(roster->packers[i].code, code)) return &roster->packers[i];
  }
  return nullptr;
}

int WsoftPackerStrip::index_of(const char *code) const {
  if (code == nullptr || code[0] == 0) return -1;
  for (size_t i = 0; i < count_; i++) {
    if (eq_ci(slots_[i].code, code)) return (int)i;
  }
  return -1;
}

void WsoftPackerStrip::remove_at(size_t index) {
  if (index >= count_) return;
  bool was_active = eq_ci(active_code_, slots_[index].code);
  for (size_t j = index + 1; j < count_; j++) slots_[j - 1] = slots_[j];
  count_--;
  slots_[count_] = WsoftPackerStripSlot();
  if (was_active) active_code_[0] = 0;
}

bool WsoftPackerStrip::sign_in(const char *code, uint32_t now_ms) {
  if (!wsoft_packer_code_ok(code)) return false;
  int idx = index_of(code);
  if (idx >= 0) {
    slots_[idx].last_activity_ms = now_ms;
    copy_trunc(active_code_, sizeof active_code_, slots_[idx].code);
    return true;
  }
  if (count_ >= WSOFT_PACKER_STRIP_MAX) return false;
  copy_trunc(slots_[count_].code, sizeof slots_[count_].code, code);
  slots_[count_].last_activity_ms = now_ms;
  count_++;
  copy_trunc(active_code_, sizeof active_code_, code);
  return true;
}

bool WsoftPackerStrip::sign_out(const char *code) {
  int idx = index_of(code);
  if (idx < 0) return false;
  remove_at((size_t)idx);
  return true;
}

bool WsoftPackerStrip::select_code(const char *code, uint32_t now_ms) {
  int idx = index_of(code);
  if (idx < 0) return false;
  slots_[idx].last_activity_ms = now_ms;
  copy_trunc(active_code_, sizeof active_code_, slots_[idx].code);
  return true;
}

void WsoftPackerStrip::touch_active(uint32_t now_ms) {
  int idx = index_of(active_code_);
  if (idx >= 0) slots_[idx].last_activity_ms = now_ms;
}

void WsoftPackerStrip::clear_active() { active_code_[0] = 0; }

void WsoftPackerStrip::clear() {
  count_ = 0;
  active_code_[0] = 0;
  for (size_t i = 0; i < WSOFT_PACKER_STRIP_MAX; i++) slots_[i] = WsoftPackerStripSlot();
}

bool WsoftPackerStrip::contains(const char *code) const { return index_of(code) >= 0; }

size_t WsoftPackerStrip::apply_evicted(const char evicted_codes[][WSOFT_PACKER_CODE_MAX + 1], size_t evicted_count) {
  size_t removed = 0;
  for (size_t i = 0; i < evicted_count; i++) {
    int idx = index_of(evicted_codes[i]);
    if (idx >= 0) {
      remove_at((size_t)idx);
      removed++;
    }
  }
  return removed;
}

size_t WsoftPackerStrip::sync_roster(const WsoftPackerRoster &roster, uint32_t now_ms) {
  size_t removed = apply_evicted(roster.evicted_codes, roster.evicted_count);
  for (size_t i = count_; i > 0; i--) {
    if (wsoft_packer_find(roster, slots_[i - 1].code) == nullptr) {
      remove_at(i - 1);
      removed++;
    }
  }
  if (active_code_[0] != 0 && index_of(active_code_) < 0) active_code_[0] = 0;
  for (size_t i = 0; i < roster.count && i < WSOFT_PACKERS_MAX; i++) {
    const WsoftPackerEntry &p = roster.packers[i];
    if (p.signed_in_here && !p.floor_manager && !contains(p.code) && count_ < WSOFT_PACKER_STRIP_MAX) {
      copy_trunc(slots_[count_].code, sizeof slots_[count_].code, p.code);
      slots_[count_].last_activity_ms = now_ms;
      count_++;
    }
  }
  return removed;
}

size_t WsoftPackerStrip::expire_idle(uint32_t now_ms, uint32_t idle_timeout_ms) {
  if (idle_timeout_ms == 0) return 0;
  size_t removed = 0;
  for (size_t i = count_; i > 0; i--) {
    if ((uint32_t)(now_ms - slots_[i - 1].last_activity_ms) >= idle_timeout_ms) {
      remove_at(i - 1);
      removed++;
    }
  }
  return removed;
}

// ---------- Count & block rules ----------

WsoftSaveResult wsoft_packer_save_hold(const WsoftPackerRoster &roster, const WsoftPackerStrip &strip) {
  if (!roster.require_packer_bags) return WSOFT_SAVE_NONE;
  const char *active = strip.active();
  if (active[0] == 0) return WSOFT_SAVE_NEED_PACKER;
  const WsoftPackerEntry *p = wsoft_packer_find(roster, active);
  if (p == nullptr) return WSOFT_SAVE_NEED_PACKER;
  if (p->remaining <= 0) return WSOFT_SAVE_NO_BAGS;
  return WSOFT_SAVE_NONE;
}

bool wsoft_packer_counts_save(const WsoftHiloStamp *hilo) {
  if (hilo == nullptr || !hilo->on) return true;
  return hilo->state == WSOFT_HILO_PASS || hilo->overridden;
}

bool wsoft_packer_record_save(WsoftPackerRoster *roster, const char *packer_code, const WsoftHiloStamp *hilo) {
  if (roster == nullptr || !wsoft_packer_counts_save(hilo)) return false;
  WsoftPackerEntry *p = wsoft_packer_find_mut(roster, packer_code);
  if (p == nullptr) return false;
  if (p->remaining > 0) p->remaining -= 1;
  return true;
}

bool wsoft_packer_record_void(WsoftPackerRoster *roster, const char *packer_code, const char *batch_id) {
  if (roster == nullptr) return false;
  WsoftPackerEntry *p = wsoft_packer_find_mut(roster, packer_code);
  if (p == nullptr) return false;
  if (batch_id != nullptr && batch_id[0] != 0 && strcmp(p->batch_id, batch_id) != 0) return false;
  p->remaining += 1;
  return true;
}

// ---------- Floor Manager ISSUE / HANDIN & outbox ----------

bool WsoftBagEventOutbox::push(const WsoftBagEvent &ev) {
  if (count_ >= WSOFT_BAG_OUTBOX_MAX) return false;
  items_[count_++] = ev;
  return true;
}

void WsoftBagEventOutbox::remove_by_id(const char *event_id) {
  if (event_id == nullptr || event_id[0] == 0) return;
  for (size_t i = 0; i < count_; i++) {
    if (strcmp(items_[i].event_id, event_id) == 0) {
      for (size_t j = i + 1; j < count_; j++) items_[j - 1] = items_[j];
      count_--;
      items_[count_] = WsoftBagEvent();
      return;
    }
  }
}

size_t WsoftBagEventOutbox::apply_results(const WsoftBagEventResult *results, size_t count,
                                          WsoftPackerRoster *roster) {
  size_t handled = 0;
  for (size_t i = 0; i < count; i++) {
    const WsoftBagEventResult &r = results[i];
    if (roster != nullptr && (r.status == WSOFT_BAG_RES_ACCEPTED || r.status == WSOFT_BAG_RES_DUPLICATE)) {
      for (size_t j = 0; j < count_; j++) {
        if (strcmp(items_[j].event_id, r.event_id) == 0) {
          WsoftPackerEntry *p = wsoft_packer_find_mut(roster, items_[j].packer_code);
          if (p != nullptr) {
            if (r.has_batch_id) {
              copy_trunc(p->batch_id, sizeof p->batch_id, r.batch_id);
              p->has_batch = p->batch_id[0] != 0;
            }
            if (r.has_remaining) p->remaining = r.remaining;
          }
          break;
        }
      }
    }
    size_t before = count_;
    remove_by_id(r.event_id);
    if (count_ < before) handled++;
  }
  return handled;
}

WsoftBagEventError wsoft_packer_floor_issue(WsoftPackerRoster *roster, WsoftBagEventOutbox *outbox,
                                            const char *event_id, const char *actor_code, const char *packer_code,
                                            int32_t quantity, const char *reason_note, const char *occurred_at) {
  if (roster == nullptr || outbox == nullptr) return WSOFT_BAG_ERR_BAD_TEXT;
  if (event_id == nullptr || event_id[0] == 0 || strlen(event_id) > WSOFT_BAG_EVENT_ID_MAX ||
      !safe_json_str(event_id)) {
    return WSOFT_BAG_ERR_BAD_TEXT;
  }
  if (!wsoft_packer_code_ok(actor_code) || !wsoft_packer_code_ok(packer_code)) return WSOFT_BAG_ERR_UNKNOWN_PACKER;
  if (reason_note != nullptr && (!safe_json_str(reason_note) || strlen(reason_note) > WSOFT_BAG_REASON_NOTE_MAX)) {
    return WSOFT_BAG_ERR_BAD_TEXT;
  }
  if (occurred_at != nullptr && (!safe_json_str(occurred_at) || strlen(occurred_at) > WSOFT_BAG_TIME_MAX)) {
    return WSOFT_BAG_ERR_BAD_TEXT;
  }
  const WsoftPackerEntry *actor = wsoft_packer_find(*roster, actor_code);
  if (actor == nullptr || !actor->floor_manager) return WSOFT_BAG_ERR_NOT_FLOOR_MANAGER;
  WsoftPackerEntry *target = wsoft_packer_find_mut(roster, packer_code);
  if (target == nullptr) return WSOFT_BAG_ERR_UNKNOWN_PACKER;
  if (quantity < 1 || quantity > WSOFT_BAG_QTY_MAX) return WSOFT_BAG_ERR_BAD_QTY;
  if (outbox->full()) return WSOFT_BAG_ERR_QUEUE_FULL;

  WsoftBagEvent ev;
  copy_trunc(ev.event_id, sizeof ev.event_id, event_id);
  ev.type = WSOFT_BAG_ISSUE;
  copy_trunc(ev.packer_code, sizeof ev.packer_code, target->code);
  copy_trunc(ev.actor_packer_code, sizeof ev.actor_packer_code, actor->code);
  ev.quantity = quantity;
  if (reason_note != nullptr && reason_note[0] != 0) copy_trunc(ev.reason_note, sizeof ev.reason_note, reason_note);
  if (occurred_at != nullptr && occurred_at[0] != 0) copy_trunc(ev.occurred_at, sizeof ev.occurred_at, occurred_at);
  if (!outbox->push(ev)) return WSOFT_BAG_ERR_QUEUE_FULL;

  target->has_batch = true;
  target->issued += quantity;
  target->remaining += quantity;
  return WSOFT_BAG_OK;
}

WsoftBagEventError wsoft_packer_floor_handin(WsoftPackerRoster *roster, WsoftBagEventOutbox *outbox,
                                             const char *event_id, const char *actor_code, const char *packer_code,
                                             int32_t lost_qty, int32_t returned_qty, const char *reason_code,
                                             const char *reason_note, bool close_batch, const char *occurred_at) {
  if (roster == nullptr || outbox == nullptr) return WSOFT_BAG_ERR_BAD_TEXT;
  if (event_id == nullptr || event_id[0] == 0 || strlen(event_id) > WSOFT_BAG_EVENT_ID_MAX ||
      !safe_json_str(event_id)) {
    return WSOFT_BAG_ERR_BAD_TEXT;
  }
  if (!wsoft_packer_code_ok(actor_code) || !wsoft_packer_code_ok(packer_code)) return WSOFT_BAG_ERR_UNKNOWN_PACKER;
  if (reason_code != nullptr && (!safe_json_str(reason_code) || strlen(reason_code) > WSOFT_BAG_REASON_CODE_MAX)) {
    return WSOFT_BAG_ERR_BAD_TEXT;
  }
  if (reason_note != nullptr && (!safe_json_str(reason_note) || strlen(reason_note) > WSOFT_BAG_REASON_NOTE_MAX)) {
    return WSOFT_BAG_ERR_BAD_TEXT;
  }
  if (occurred_at != nullptr && (!safe_json_str(occurred_at) || strlen(occurred_at) > WSOFT_BAG_TIME_MAX)) {
    return WSOFT_BAG_ERR_BAD_TEXT;
  }
  const WsoftPackerEntry *actor = wsoft_packer_find(*roster, actor_code);
  if (actor == nullptr || !actor->floor_manager) return WSOFT_BAG_ERR_NOT_FLOOR_MANAGER;
  WsoftPackerEntry *target = wsoft_packer_find_mut(roster, packer_code);
  if (target == nullptr) return WSOFT_BAG_ERR_UNKNOWN_PACKER;
  if (lost_qty < 0 || returned_qty < 0 || lost_qty > WSOFT_BAG_QTY_MAX || returned_qty > WSOFT_BAG_QTY_MAX) {
    return WSOFT_BAG_ERR_BAD_QTY;
  }
  if (!close_batch && lost_qty == 0 && returned_qty == 0) return WSOFT_BAG_ERR_BAD_QTY;
  if (lost_qty > 0 && (reason_code == nullptr || reason_code[0] == 0)) return WSOFT_BAG_ERR_REASON_REQUIRED;
  if (lost_qty + returned_qty > target->remaining) return WSOFT_BAG_ERR_EXCEEDS_REMAINING;
  if (outbox->full()) return WSOFT_BAG_ERR_QUEUE_FULL;

  WsoftBagEvent ev;
  copy_trunc(ev.event_id, sizeof ev.event_id, event_id);
  ev.type = WSOFT_BAG_HANDIN;
  copy_trunc(ev.packer_code, sizeof ev.packer_code, target->code);
  copy_trunc(ev.actor_packer_code, sizeof ev.actor_packer_code, actor->code);
  if (target->batch_id[0] != 0) copy_trunc(ev.batch_id, sizeof ev.batch_id, target->batch_id);
  ev.lost_qty = lost_qty;
  ev.returned_qty = returned_qty;
  if (reason_code != nullptr && reason_code[0] != 0) copy_trunc(ev.reason_code, sizeof ev.reason_code, reason_code);
  if (reason_note != nullptr && reason_note[0] != 0) copy_trunc(ev.reason_note, sizeof ev.reason_note, reason_note);
  ev.has_close_batch = true;
  ev.close_batch = close_batch;
  if (occurred_at != nullptr && occurred_at[0] != 0) copy_trunc(ev.occurred_at, sizeof ev.occurred_at, occurred_at);
  if (!outbox->push(ev)) return WSOFT_BAG_ERR_QUEUE_FULL;

  target->remaining -= (lost_qty + returned_qty);
  if (close_batch) {
    target->has_batch = false;
    target->batch_id[0] = 0;
    target->issued = 0;
    target->remaining = 0;
  }
  return WSOFT_BAG_OK;
}

// ---------- Hub JSON builders & strict parsers ----------

bool wsoft_packers_request(const char *const *signed_in_codes, size_t count, char *out, size_t size) {
  if (out == nullptr || size == 0 || count > WSOFT_PACKERS_MAX) return false;
  int n = snprintf(out, size, "{\"signed_in_codes\":[");
  if (n < 0 || (size_t)n >= size) return false;
  size_t used = (size_t)n;
  for (size_t i = 0; i < count; i++) {
    if (!wsoft_packer_code_ok(signed_in_codes[i])) return false;
    n = snprintf(out + used, size - used, "%s\"%s\"", i ? "," : "", signed_in_codes[i]);
    if (n < 0 || (size_t)n >= size - used) return false;
    used += (size_t)n;
  }
  n = snprintf(out + used, size - used, "]}");
  return n > 0 && (size_t)n < size - used;
}

bool wsoft_packers_request(const WsoftPackerStrip &strip, char *out, size_t size) {
  const char *codes[WSOFT_PACKER_STRIP_MAX];
  size_t n = strip.count();
  for (size_t i = 0; i < n; i++) codes[i] = strip.slot_code(i);
  return wsoft_packers_request(codes, n, out, size);
}

bool wsoft_packer_auth_sign_in_request(const char *code, const char *pin, char *out, size_t size) {
  if (out == nullptr || size == 0 || !wsoft_packer_code_ok(code) || !wsoft_packer_pin_format_ok(pin)) return false;
  int n = snprintf(out, size, "{\"action\":\"sign_in\",\"code\":\"%s\",\"pin\":\"%s\"}", code, pin);
  return n > 0 && (size_t)n < size;
}

bool wsoft_packer_auth_sign_out_request(const char *code, char *out, size_t size) {
  if (out == nullptr || size == 0 || !wsoft_packer_code_ok(code)) return false;
  int n = snprintf(out, size, "{\"action\":\"sign_out\",\"code\":\"%s\"}", code);
  return n > 0 && (size_t)n < size;
}

static const char *event_type_str(WsoftBagEventType t) {
  switch (t) {
    case WSOFT_BAG_ISSUE: return "ISSUE";
    case WSOFT_BAG_HANDIN: return "HANDIN";
    case WSOFT_BAG_LOST: return "LOST";
    case WSOFT_BAG_RETURN: return "RETURN";
  }
  return "ISSUE";
}

bool wsoft_bag_events_request(const WsoftBagEvent *events, size_t count, char *out, size_t size) {
  if (out == nullptr || size == 0 || events == nullptr || count == 0 || count > 50) return false;
  int n = snprintf(out, size, "{\"events\":[");
  if (n < 0 || (size_t)n >= size) return false;
  size_t used = (size_t)n;
  for (size_t i = 0; i < count; i++) {
    const WsoftBagEvent &e = events[i];
    if (e.event_id[0] == 0 || !safe_json_str(e.event_id) || !wsoft_packer_code_ok(e.packer_code) ||
        !wsoft_packer_code_ok(e.actor_packer_code)) {
      return false;
    }
    n = snprintf(out + used, size - used,
                 "%s{\"event_id\":\"%s\",\"event_type\":\"%s\",\"packer_code\":\"%s\",\"actor_packer_code\":\"%s\"",
                 i ? "," : "", e.event_id, event_type_str(e.type), e.packer_code, e.actor_packer_code);
    if (n < 0 || (size_t)n >= size - used) return false;
    used += (size_t)n;

    if (e.batch_id[0] != 0) {
      if (!safe_json_str(e.batch_id)) return false;
      n = snprintf(out + used, size - used, ",\"batch_id\":\"%s\"", e.batch_id);
      if (n < 0 || (size_t)n >= size - used) return false;
      used += (size_t)n;
    }
    if (e.type == WSOFT_BAG_ISSUE) {
      if (e.quantity < 1 || e.quantity > WSOFT_BAG_QTY_MAX) return false;
      n = snprintf(out + used, size - used, ",\"quantity\":%ld", (long)e.quantity);
      if (n < 0 || (size_t)n >= size - used) return false;
      used += (size_t)n;
    } else {
      n = snprintf(out + used, size - used, ",\"lost_qty\":%ld,\"returned_qty\":%ld", (long)e.lost_qty,
                   (long)e.returned_qty);
      if (n < 0 || (size_t)n >= size - used) return false;
      used += (size_t)n;
    }
    if (e.reason_code[0] != 0) {
      if (!safe_json_str(e.reason_code)) return false;
      n = snprintf(out + used, size - used, ",\"reason_code\":\"%s\"", e.reason_code);
      if (n < 0 || (size_t)n >= size - used) return false;
      used += (size_t)n;
    }
    if (e.reason_note[0] != 0) {
      if (!safe_json_str(e.reason_note)) return false;
      n = snprintf(out + used, size - used, ",\"reason_note\":\"%s\"", e.reason_note);
      if (n < 0 || (size_t)n >= size - used) return false;
      used += (size_t)n;
    }
    if (e.has_close_batch) {
      n = snprintf(out + used, size - used, ",\"close_batch\":%s", e.close_batch ? "true" : "false");
      if (n < 0 || (size_t)n >= size - used) return false;
      used += (size_t)n;
    }
    if (e.occurred_at[0] != 0) {
      if (!safe_json_str(e.occurred_at)) return false;
      n = snprintf(out + used, size - used, ",\"occurred_at\":\"%s\"", e.occurred_at);
      if (n < 0 || (size_t)n >= size - used) return false;
      used += (size_t)n;
    }
    n = snprintf(out + used, size - used, "}");
    if (n < 0 || (size_t)n >= size - used) return false;
    used += (size_t)n;
  }
  n = snprintf(out + used, size - used, "]}");
  return n > 0 && (size_t)n < size - used;
}

namespace {

typedef WsoftJsonReader Reader;

bool read_nullable_str(Reader &r, char *out, size_t cap, bool *present) {
  if (r.word("null")) {
    out[0] = 0;
    *present = false;
    return true;
  }
  *present = true;
  return r.string(out, cap);
}

bool read_packer(Reader &r, WsoftPackerEntry *p) {
  if (!r.take('{')) return false;
  enum {
    ID = 1,
    CODE = 2,
    NAME = 4,
    FM = 8,
    SALT = 16,
    SHA = 32,
    BATCH = 64,
    ISSUED = 128,
    REM = 256,
    HERE = 512,
    ALL = 1023
  };
  int seen = 0;
  char salt_hex[WSOFT_PACKER_PIN_SALT_SIZE * 2 + 2] = "";
  char sha_hex[66] = "";
  bool has_salt = false, has_sha = false;
  do {
    char key[24];
    if (!r.string(key, sizeof key) || !r.take(':')) return false;
    int bit = 0;
    bool ok = false;
    if (strcmp(key, "packer_id") == 0) {
      bit = ID;
      ok = r.string(p->packer_id, sizeof p->packer_id) && p->packer_id[0] != 0;
    } else if (strcmp(key, "code") == 0) {
      bit = CODE;
      char c[WSOFT_PACKER_CODE_MAX + 2];
      ok = r.string(c, sizeof c) && wsoft_packer_code_ok(c);
      if (ok) strcpy(p->code, c);
    } else if (strcmp(key, "name") == 0) {
      bit = NAME;
      ok = r.string(p->name, sizeof p->name);
    } else if (strcmp(key, "floor_manager") == 0) {
      bit = FM;
      ok = r.boolean(&p->floor_manager);
    } else if (strcmp(key, "pin_salt") == 0) {
      bit = SALT;
      ok = read_nullable_str(r, salt_hex, sizeof salt_hex, &has_salt);
    } else if (strcmp(key, "pin_sha256") == 0) {
      bit = SHA;
      ok = read_nullable_str(r, sha_hex, sizeof sha_hex, &has_sha);
    } else if (strcmp(key, "batch_id") == 0) {
      bit = BATCH;
      ok = read_nullable_str(r, p->batch_id, sizeof p->batch_id, &p->has_batch);
    } else if (strcmp(key, "issued") == 0) {
      bit = ISSUED;
      int64_t v = 0;
      ok = r.whole(&v, 1000000000LL);
      if (ok) p->issued = (int32_t)v;
    } else if (strcmp(key, "remaining") == 0) {
      bit = REM;
      int64_t v = 0;
      ok = r.whole(&v, 1000000000LL);
      if (ok) p->remaining = (int32_t)v;
    } else if (strcmp(key, "signed_in_here") == 0) {
      bit = HERE;
      ok = r.boolean(&p->signed_in_here);
    } else {
      return false;
    }
    if (!ok || (seen & bit)) return false;
    seen |= bit;
  } while (r.take(','));
  if (!r.take('}') || seen != ALL) return false;
  if (has_salt && has_sha) {
    if (!wsoft_packer_hex_decode(salt_hex, p->pin_salt, WSOFT_PACKER_PIN_SALT_SIZE) ||
        !wsoft_packer_hex_decode(sha_hex, p->pin_sha256, 32)) {
      return false;
    }
    p->has_verifier = true;
  } else if (has_salt || has_sha) {
    return false;
  } else {
    p->has_verifier = false;
  }
  return true;
}

bool read_packers_array(Reader &r, WsoftPackerRoster *out) {
  if (!r.take('[')) return false;
  size_t n = 0;
  if (!r.peek(']')) {
    do {
      if (n == WSOFT_PACKERS_MAX) return false;
      out->packers[n] = WsoftPackerEntry();
      if (!read_packer(r, &out->packers[n])) return false;
      n++;
    } while (r.take(','));
  }
  out->count = n;
  return r.take(']');
}

bool read_evicted_array(Reader &r, WsoftPackerRoster *out) {
  if (!r.take('[')) return false;
  size_t n = 0;
  if (!r.peek(']')) {
    do {
      if (n == WSOFT_PACKERS_MAX) return false;
      char c[WSOFT_PACKER_CODE_MAX + 2];
      if (!r.string(c, sizeof c) || !wsoft_packer_code_ok(c)) return false;
      strcpy(out->evicted_codes[n++], c);
    } while (r.take(','));
  }
  out->evicted_count = n;
  return r.take(']');
}

bool read_bag_event_result(Reader &r, WsoftBagEventResult *res) {
  if (!r.take('{')) return false;
  enum { ID = 1, STATUS = 2, BATCH = 4, REM = 8, ERR = 16 };
  int seen = 0;
  do {
    char key[16];
    if (!r.string(key, sizeof key) || !r.take(':')) return false;
    int bit = 0;
    bool ok = false;
    if (strcmp(key, "event_id") == 0) {
      bit = ID;
      ok = r.string(res->event_id, sizeof res->event_id) && res->event_id[0] != 0;
    } else if (strcmp(key, "status") == 0) {
      bit = STATUS;
      char s[16];
      ok = r.string(s, sizeof s);
      if (ok) {
        if (strcmp(s, "accepted") == 0) res->status = WSOFT_BAG_RES_ACCEPTED;
        else if (strcmp(s, "duplicate") == 0) res->status = WSOFT_BAG_RES_DUPLICATE;
        else if (strcmp(s, "rejected") == 0) res->status = WSOFT_BAG_RES_REJECTED;
        else ok = false;
      }
    } else if (strcmp(key, "batch_id") == 0) {
      bit = BATCH;
      ok = read_nullable_str(r, res->batch_id, sizeof res->batch_id, &res->has_batch_id);
    } else if (strcmp(key, "remaining") == 0) {
      bit = REM;
      int64_t v = 0;
      ok = r.whole(&v, 1000000000LL);
      if (ok) {
        res->has_remaining = true;
        res->remaining = (int32_t)v;
      }
    } else if (strcmp(key, "error") == 0) {
      bit = ERR;
      ok = r.string(res->error, sizeof res->error);
    } else {
      return false;
    }
    if (!ok || (seen & bit)) return false;
    seen |= bit;
  } while (r.take(','));
  return r.take('}') && (seen & (ID | STATUS)) == (ID | STATUS);
}

}  // namespace

bool wsoft_packers_parse(const char *body, size_t length, WsoftPackerRoster *out) {
  if (body == nullptr || out == nullptr) return false;
  wsoft_packer_roster_reset(out);
  Reader r = {body, body + length};
  enum { REQ = 1, UNIT = 2, PACKERS = 4, EVICTED = 8, ALL = 15 };
  int seen = 0;
  bool ok = r.take('{');
  while (ok) {
    char key[24];
    if (!r.string(key, sizeof key) || !r.take(':')) {
      ok = false;
      break;
    }
    int bit = 0;
    if (strcmp(key, "require_packer_bags") == 0) {
      bit = REQ;
      ok = r.boolean(&out->require_packer_bags);
    } else if (strcmp(key, "pack_unit_label") == 0) {
      bit = UNIT;
      ok = r.string(out->pack_unit_label, sizeof out->pack_unit_label) && out->pack_unit_label[0] != 0;
    } else if (strcmp(key, "packers") == 0) {
      bit = PACKERS;
      ok = read_packers_array(r, out);
    } else if (strcmp(key, "evicted_codes") == 0) {
      bit = EVICTED;
      ok = read_evicted_array(r, out);
    } else {
      ok = false;
    }
    if (!ok || (seen & bit)) {
      ok = false;
      break;
    }
    seen |= bit;
    if (!r.take(',')) break;
  }
  ok = ok && r.take('}') && r.done() && seen == ALL;
  if (!ok) wsoft_packer_roster_reset(out);
  return ok;
}

bool wsoft_packer_auth_parse(const char *body, size_t length, WsoftPackerAuthReply *out) {
  if (body == nullptr || out == nullptr) return false;
  *out = WsoftPackerAuthReply();
  Reader r = {body, body + length};
  enum {
    OK = 1,
    ACTION = 2,
    REASON = 4,
    ID = 8,
    CODE = 16,
    NAME = 32,
    FM = 64,
    BATCH = 128,
    ISSUED = 256,
    REM = 512,
    SIGNED = 1024,
    XFER = 2048
  };
  int seen = 0;
  bool ok = r.take('{');
  while (ok) {
    char key[32];
    if (!r.string(key, sizeof key) || !r.take(':')) {
      ok = false;
      break;
    }
    int bit = 0;
    if (strcmp(key, "ok") == 0) {
      bit = OK;
      ok = r.boolean(&out->ok);
    } else if (strcmp(key, "action") == 0) {
      bit = ACTION;
      ok = r.string(out->action, sizeof out->action);
    } else if (strcmp(key, "reason") == 0) {
      bit = REASON;
      ok = r.string(out->reason, sizeof out->reason);
    } else if (strcmp(key, "packer_id") == 0) {
      bit = ID;
      ok = r.string(out->packer_id, sizeof out->packer_id);
    } else if (strcmp(key, "code") == 0) {
      bit = CODE;
      char c[WSOFT_PACKER_CODE_MAX + 2];
      ok = r.string(c, sizeof c) && wsoft_packer_code_ok(c);
      if (ok) strcpy(out->code, c);
    } else if (strcmp(key, "name") == 0) {
      bit = NAME;
      ok = r.string(out->name, sizeof out->name);
    } else if (strcmp(key, "floor_manager") == 0) {
      bit = FM;
      ok = r.boolean(&out->floor_manager);
    } else if (strcmp(key, "batch_id") == 0) {
      bit = BATCH;
      ok = read_nullable_str(r, out->batch_id, sizeof out->batch_id, &out->has_batch);
    } else if (strcmp(key, "issued") == 0) {
      bit = ISSUED;
      int64_t v = 0;
      ok = r.whole(&v, 1000000000LL);
      if (ok) out->issued = (int32_t)v;
    } else if (strcmp(key, "remaining") == 0) {
      bit = REM;
      int64_t v = 0;
      ok = r.whole(&v, 1000000000LL);
      if (ok) out->remaining = (int32_t)v;
    } else if (strcmp(key, "signed_in") == 0) {
      bit = SIGNED;
      ok = r.boolean(&out->signed_in);
    } else if (strcmp(key, "transferred_from_device_id") == 0) {
      bit = XFER;
      bool present = false;
      ok = read_nullable_str(r, out->transferred_from_device_id, sizeof out->transferred_from_device_id, &present);
    } else {
      ok = false;
    }
    if (!ok || (seen & bit)) {
      ok = false;
      break;
    }
    seen |= bit;
    if (!r.take(',')) break;
  }
  ok = ok && r.take('}') && r.done() && (seen & OK);
  if (ok) {
    if (!out->ok) {
      ok = (seen & REASON) != 0;
    } else if (strcmp(out->action, "sign_out") == 0) {
      ok = (seen & (ACTION | CODE | SIGNED)) == (ACTION | CODE | SIGNED);
    } else if (strcmp(out->action, "sign_in") == 0) {
      const int req = OK | ACTION | ID | CODE | NAME | FM | BATCH | ISSUED | REM | XFER;
      ok = (seen & req) == req;
    } else {
      ok = false;
    }
  }
  if (!ok) *out = WsoftPackerAuthReply();
  return ok;
}

bool wsoft_bag_events_parse(const char *body, size_t length, WsoftBagEventResult *results, size_t max_results,
                            size_t *count) {
  if (count) *count = 0;
  if (body == nullptr || results == nullptr || count == nullptr) return false;
  Reader r = {body, body + length};
  char key[16];
  if (!r.take('{') || !r.string(key, sizeof key) || strcmp(key, "results") != 0 || !r.take(':') || !r.take('[')) {
    return false;
  }
  size_t n = 0;
  if (!r.peek(']')) {
    do {
      if (n >= max_results) return false;
      results[n] = WsoftBagEventResult();
      if (!read_bag_event_result(r, &results[n])) return false;
      n++;
    } while (r.take(','));
  }
  if (!r.take(']') || !r.take('}') || !r.done()) return false;
  *count = n;
  return true;
}

bool wsoft_hub_packers_verify(const uint8_t key[32], const char *sent_key_id, const char *sent_nonce,
                              const char *key_id, const char *timestamp, const char *nonce, const char *signature,
                              const char *body, size_t length) {
  return wsoft_hub_verify_answer(key, WSOFT_HUB_PACKERS_PREFIX, sent_key_id, sent_nonce, key_id, timestamp, nonce,
                                 signature, body, length);
}

bool wsoft_hub_packer_auth_verify(const uint8_t key[32], const char *sent_key_id, const char *sent_nonce,
                                  const char *key_id, const char *timestamp, const char *nonce, const char *signature,
                                  const char *body, size_t length) {
  return wsoft_hub_verify_answer(key, WSOFT_HUB_PACKER_AUTH_PREFIX, sent_key_id, sent_nonce, key_id, timestamp, nonce,
                                 signature, body, length);
}

bool wsoft_hub_bag_events_verify(const uint8_t key[32], const char *sent_key_id, const char *sent_nonce,
                                 const char *key_id, const char *timestamp, const char *nonce, const char *signature,
                                 const char *body, size_t length) {
  return wsoft_hub_verify_answer(key, WSOFT_HUB_BAG_EVENTS_PREFIX, sent_key_id, sent_nonce, key_id, timestamp, nonce,
                                 signature, body, length);
}
