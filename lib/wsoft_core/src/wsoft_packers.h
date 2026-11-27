#pragma once

// Packer sign-in, shared-scale name strip, 3-4 digit offline PIN pad, hard stop at 0 bags,
// Floor Manager ISSUE/HANDIN bag events, and signed Hub sync (ADR 0011, Sprint 2).
// Pure C++ (no Arduino or NVS), host-tested in test/test_wsoft_packers.
//
// Deliberately separate from wsoft_pin.h (the Approval switch PIN: 4-6 digits, 300 s lock).
// require_packer_bags defaults to false (OFF) so existing boards and Control Room CRCs are
// unchanged until switched on.

#include <stddef.h>
#include <stdint.h>

#include "wsoft_hilo.h"
#include "wsoft_hub_commands.h"
#include "wsoft_weighing.h"

// Hub signed response prefixes for the 3 Sprint 2 endpoints.
static const char *const WSOFT_HUB_PACKERS_PREFIX = "scale-models-hub-packers-v1";
static const char *const WSOFT_HUB_PACKER_AUTH_PREFIX = "scale-models-hub-packer-auth-v1";
static const char *const WSOFT_HUB_BAG_EVENTS_PREFIX = "scale-models-hub-bag-events-v1";

// Separate 3-4 digit packer/floor-manager PIN rules and lighter 45 s lockout.
static const size_t WSOFT_PACKER_PIN_MIN = 3;
static const size_t WSOFT_PACKER_PIN_MAX = 4;
static const size_t WSOFT_PACKER_PIN_SALT_SIZE = 16;
static const uint32_t WSOFT_PACKER_PIN_TRIES = 5;
static const uint32_t WSOFT_PACKER_PIN_LOCK_MS = 45000;

// Roster, strip, and outbox bounds.
static const size_t WSOFT_PACKERS_MAX = 32;
static const size_t WSOFT_PACKER_STRIP_MAX = 8;
static const size_t WSOFT_PACKER_ID_MAX = 64;
static const size_t WSOFT_PACKER_CODE_MAX = 32;
static const size_t WSOFT_PACKER_NAME_MAX = 48;
static const size_t WSOFT_PACKER_BATCH_ID_MAX = 64;
static const size_t WSOFT_PACKER_UNIT_MAX = 16;
static const size_t WSOFT_BAG_EVENT_ID_MAX = 64;
static const size_t WSOFT_BAG_REASON_CODE_MAX = 32;
static const size_t WSOFT_BAG_REASON_NOTE_MAX = 200;
static const size_t WSOFT_BAG_TIME_MAX = 64;
static const size_t WSOFT_BAG_OUTBOX_MAX = 16;
static const int32_t WSOFT_BAG_QTY_MAX = 100000;
static const uint32_t WSOFT_PACKER_IDLE_DEFAULT_MS = 300000;

static const size_t WSOFT_PACKERS_REQUEST_MAX = 64 + WSOFT_PACKER_STRIP_MAX * (WSOFT_PACKER_CODE_MAX + 4);
static const size_t WSOFT_PACKER_AUTH_REQUEST_MAX = 128;
static const size_t WSOFT_BAG_EVENTS_REQUEST_MAX = 32 + WSOFT_BAG_OUTBOX_MAX * 512;

enum WsoftPackerPinResult {
  WSOFT_PACKER_PIN_OK = 0,
  WSOFT_PACKER_PIN_WRONG,
  WSOFT_PACKER_PIN_LOCKED,
  WSOFT_PACKER_PIN_NOT_SET,
};

// 3 or 4 ASCII digits ('0'..'9') only.
bool wsoft_packer_pin_format_ok(const char *pin);
// sha256(salt || pin).
void wsoft_packer_pin_hash(const uint8_t salt[WSOFT_PACKER_PIN_SALT_SIZE], const char *pin, uint8_t out[32]);
// Constant-time compare against stored hash. False for bad format.
bool wsoft_packer_pin_matches(const uint8_t salt[WSOFT_PACKER_PIN_SALT_SIZE], const uint8_t hash[32], const char *pin);
// Decodes exact byte_len * 2 hex characters (case-insensitive) into out.
bool wsoft_packer_hex_decode(const char *hex, uint8_t *out, size_t byte_len);

// Lighter 45 s lockout after 5 wrong tries.
class WsoftPackerPinGuard {
 public:
  explicit WsoftPackerPinGuard(uint32_t fails = 0, uint32_t now_ms = 0);

  uint32_t locked_s(uint32_t now_ms) const;
  int tries_left() const { return (int)(WSOFT_PACKER_PIN_TRIES - fails_); }
  uint32_t fails() const { return fails_; }

  void wrong(uint32_t now_ms);
  void right();

 private:
  uint32_t fails_ = 0;
  bool locked_ = false;
  uint32_t lock_from_ms_ = 0;
};

WsoftPackerPinResult wsoft_packer_pin_try(WsoftPackerPinGuard *guard, bool has_verifier,
                                          const uint8_t salt[WSOFT_PACKER_PIN_SALT_SIZE], const uint8_t hash[32],
                                          const char *entry, uint32_t now_ms);

// Packer code accepted by the Hub: [A-Za-z0-9_.-]{1,32}.
bool wsoft_packer_code_ok(const char *code);

struct WsoftPackerEntry {
  char packer_id[WSOFT_PACKER_ID_MAX + 1] = "";
  char code[WSOFT_PACKER_CODE_MAX + 1] = "";
  char name[WSOFT_PACKER_NAME_MAX + 1] = "";
  bool floor_manager = false;
  bool has_verifier = false;
  uint8_t pin_salt[WSOFT_PACKER_PIN_SALT_SIZE] = {};
  uint8_t pin_sha256[32] = {};
  bool has_batch = false;
  char batch_id[WSOFT_PACKER_BATCH_ID_MAX + 1] = "";
  int32_t issued = 0;
  int32_t remaining = 0;
  bool signed_in_here = false;
};

struct WsoftPackerRoster {
  bool require_packer_bags = false;  // OFF by default!
  char pack_unit_label[WSOFT_PACKER_UNIT_MAX + 1] = "bags";
  WsoftPackerEntry packers[WSOFT_PACKERS_MAX];
  size_t count = 0;
  char evicted_codes[WSOFT_PACKERS_MAX][WSOFT_PACKER_CODE_MAX + 1] = {};
  size_t evicted_count = 0;
};

// Empties a roster in place. A roster is about 10 KB: `*r = WsoftPackerRoster()` builds a
// temporary on the stack, which overflowed the ESP32 loop task at boot (27 Nov 2026).
void wsoft_packer_roster_reset(WsoftPackerRoster *roster);

const WsoftPackerEntry *wsoft_packer_find(const WsoftPackerRoster &roster, const char *code);
WsoftPackerEntry *wsoft_packer_find_mut(WsoftPackerRoster *roster, const char *code);

// Shared-scale signed-in name strip (up to 8 slots, 1-tap select, idle timeout, eviction).
struct WsoftPackerStripSlot {
  char code[WSOFT_PACKER_CODE_MAX + 1] = "";
  uint32_t last_activity_ms = 0;
};

class WsoftPackerStrip {
 public:
  bool sign_in(const char *code, uint32_t now_ms);
  bool sign_out(const char *code);
  bool select_code(const char *code, uint32_t now_ms);
  void touch_active(uint32_t now_ms);
  void clear_active();
  void clear();

  // Removes any evicted codes from the strip (and clears active if evicted). Returns count removed.
  size_t apply_evicted(const char evicted_codes[][WSOFT_PACKER_CODE_MAX + 1], size_t evicted_count);
  // Applies evicted_codes, drops codes missing from roster, and adds signed_in_here entries if room.
  size_t sync_roster(const WsoftPackerRoster &roster, uint32_t now_ms);
  // Logs out slots idle for >= idle_timeout_ms (0 = disabled). Returns count removed.
  size_t expire_idle(uint32_t now_ms, uint32_t idle_timeout_ms);

  size_t count() const { return count_; }
  const char *slot_code(size_t i) const { return i < count_ ? slots_[i].code : ""; }
  const char *active() const { return active_code_; }
  bool contains(const char *code) const;

 private:
  int index_of(const char *code) const;
  void remove_at(size_t index);

  WsoftPackerStripSlot slots_[WSOFT_PACKER_STRIP_MAX];
  size_t count_ = 0;
  char active_code_[WSOFT_PACKER_CODE_MAX + 1] = "";
};

// Count & block rules (when require_packer_bags == true).
// Returns WSOFT_SAVE_NONE when require_packer_bags is false or the active packer has remaining > 0.
WsoftSaveResult wsoft_packer_save_hold(const WsoftPackerRoster &roster, const WsoftPackerStrip &strip);
// True when a saved weighing counts against the batch: PASS, operator_override, or plain save without Hi-Lo.
bool wsoft_packer_counts_save(const WsoftHiloStamp *hilo);
// Decrements packer's remaining by 1 (clamped at 0) when wsoft_packer_counts_save(hilo) is true.
bool wsoft_packer_record_save(WsoftPackerRoster *roster, const char *packer_code, const WsoftHiloStamp *hilo);
// Increments packer's remaining by 1 on void (when batch_id is null/empty or matches open batch_id).
bool wsoft_packer_record_void(WsoftPackerRoster *roster, const char *packer_code, const char *batch_id = nullptr);

// Floor Manager ISSUE / HANDIN and idempotent bag-events outbox.
enum WsoftBagEventType { WSOFT_BAG_ISSUE = 0, WSOFT_BAG_HANDIN, WSOFT_BAG_LOST, WSOFT_BAG_RETURN };

enum WsoftBagEventError {
  WSOFT_BAG_OK = 0,
  WSOFT_BAG_ERR_NOT_FLOOR_MANAGER,
  WSOFT_BAG_ERR_UNKNOWN_PACKER,
  WSOFT_BAG_ERR_BAD_QTY,
  WSOFT_BAG_ERR_REASON_REQUIRED,
  WSOFT_BAG_ERR_EXCEEDS_REMAINING,
  WSOFT_BAG_ERR_QUEUE_FULL,
  WSOFT_BAG_ERR_BAD_TEXT,
};

struct WsoftBagEvent {
  char event_id[WSOFT_BAG_EVENT_ID_MAX + 1] = "";
  WsoftBagEventType type = WSOFT_BAG_ISSUE;
  char packer_code[WSOFT_PACKER_CODE_MAX + 1] = "";
  char actor_packer_code[WSOFT_PACKER_CODE_MAX + 1] = "";
  char batch_id[WSOFT_PACKER_BATCH_ID_MAX + 1] = "";
  int32_t quantity = 0;
  int32_t lost_qty = 0;
  int32_t returned_qty = 0;
  char reason_code[WSOFT_BAG_REASON_CODE_MAX + 1] = "";
  char reason_note[WSOFT_BAG_REASON_NOTE_MAX + 1] = "";
  bool has_close_batch = false;
  bool close_batch = false;
  char occurred_at[WSOFT_BAG_TIME_MAX + 1] = "";
};

enum WsoftBagEventResultStatus { WSOFT_BAG_RES_ACCEPTED = 0, WSOFT_BAG_RES_DUPLICATE, WSOFT_BAG_RES_REJECTED };

struct WsoftBagEventResult {
  char event_id[WSOFT_BAG_EVENT_ID_MAX + 1] = "";
  WsoftBagEventResultStatus status = WSOFT_BAG_RES_REJECTED;
  bool has_batch_id = false;
  char batch_id[WSOFT_PACKER_BATCH_ID_MAX + 1] = "";
  bool has_remaining = false;
  int32_t remaining = 0;
  char error[WSOFT_CMD_ERROR_MAX + 1] = "";
};

class WsoftBagEventOutbox {
 public:
  size_t size() const { return count_; }
  bool full() const { return count_ >= WSOFT_BAG_OUTBOX_MAX; }
  const WsoftBagEvent &at(size_t i) const { return items_[i]; }
  bool push(const WsoftBagEvent &ev);
  void remove_by_id(const char *event_id);
  void clear() { count_ = 0; }
  // Removes delivered/acknowledged events and updates matching roster entry if accepted/duplicate.
  size_t apply_results(const WsoftBagEventResult *results, size_t count, WsoftPackerRoster *roster);

 private:
  WsoftBagEvent items_[WSOFT_BAG_OUTBOX_MAX];
  size_t count_ = 0;
};

WsoftBagEventError wsoft_packer_floor_issue(WsoftPackerRoster *roster, WsoftBagEventOutbox *outbox,
                                            const char *event_id, const char *actor_code, const char *packer_code,
                                            int32_t quantity, const char *reason_note = nullptr,
                                            const char *occurred_at = nullptr);

WsoftBagEventError wsoft_packer_floor_handin(WsoftPackerRoster *roster, WsoftBagEventOutbox *outbox,
                                             const char *event_id, const char *actor_code, const char *packer_code,
                                             int32_t lost_qty, int32_t returned_qty, const char *reason_code,
                                             const char *reason_note = nullptr, bool close_batch = true,
                                             const char *occurred_at = nullptr);

// Sprint 2 Hub JSON builders, strict parsers, and response signature verifiers.
struct WsoftPackerAuthReply {
  bool ok = false;
  char action[16] = "";  // "sign_in", "sign_out", or ""
  char reason[32] = "";  // e.g. "INVALID_PIN" when ok == false
  char packer_id[WSOFT_PACKER_ID_MAX + 1] = "";
  char code[WSOFT_PACKER_CODE_MAX + 1] = "";
  char name[WSOFT_PACKER_NAME_MAX + 1] = "";
  bool floor_manager = false;
  bool has_batch = false;
  char batch_id[WSOFT_PACKER_BATCH_ID_MAX + 1] = "";
  int32_t issued = 0;
  int32_t remaining = 0;
  bool signed_in = false;
  char transferred_from_device_id[WSOFT_PACKER_ID_MAX + 1] = "";
};

bool wsoft_packers_request(const char *const *signed_in_codes, size_t count, char *out, size_t size);
bool wsoft_packers_request(const WsoftPackerStrip &strip, char *out, size_t size);
bool wsoft_packers_parse(const char *body, size_t length, WsoftPackerRoster *out);

bool wsoft_packer_auth_sign_in_request(const char *code, const char *pin, char *out, size_t size);
bool wsoft_packer_auth_sign_out_request(const char *code, char *out, size_t size);
bool wsoft_packer_auth_parse(const char *body, size_t length, WsoftPackerAuthReply *out);

bool wsoft_bag_events_request(const WsoftBagEvent *events, size_t count, char *out, size_t size);
bool wsoft_bag_events_parse(const char *body, size_t length, WsoftBagEventResult *results, size_t max_results,
                            size_t *count);

bool wsoft_hub_packers_verify(const uint8_t key[32], const char *sent_key_id, const char *sent_nonce,
                              const char *key_id, const char *timestamp, const char *nonce, const char *signature,
                              const char *body, size_t length);
bool wsoft_hub_packer_auth_verify(const uint8_t key[32], const char *sent_key_id, const char *sent_nonce,
                                  const char *key_id, const char *timestamp, const char *nonce, const char *signature,
                                  const char *body, size_t length);
bool wsoft_hub_bag_events_verify(const uint8_t key[32], const char *sent_key_id, const char *sent_nonce,
                                 const char *key_id, const char *timestamp, const char *nonce, const char *signature,
                                 const char *body, size_t length);
