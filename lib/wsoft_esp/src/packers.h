#pragma once

// Hardware.Base only (WSOFT_ON_BASE): cached packer roster, shared-scale signed-in strip,
// offline 3-4 digit PIN guard (45 s lock), bag-event outbox, and signed Hub sync.
// Rules live in wsoft_packers (wsoft_core, host-tested).
//
// require_packer_bags defaults to false (OFF) so existing bench boards and Control Room
// golden CRCs are unchanged until enabled on the Hub.

#include <Arduino.h>

#include <wsoft_hilo.h>
#include <wsoft_packers.h>
#include <wsoft_weighing.h>

void packers_begin();

// True when the Hub has switched require_packer_bags ON for this board (false by default).
bool packers_required();

// Save guard for weighings_update(): returns WSOFT_SAVE_NONE whenever !packers_required()
// or the active packer has remaining > 0; otherwise WSOFT_SAVE_NEED_PACKER / WSOFT_SAVE_NO_BAGS.
WsoftSaveResult packers_save_hold();

// Copies the active packer's code, name, and open batch_id when packers_required() and an
// active packer is selected on the strip.
bool packers_active_meta(char *code, size_t code_size, char *name, size_t name_size, char *batch_id,
                         size_t batch_size);

// Updates remaining count after a saved weighing or void, and refreshes strip activity.
void packers_on_saved(const char *packer_code, const WsoftHiloStamp *hilo);
void packers_on_void(const char *packer_code, const char *batch_id = nullptr);

// Shared-scale strip & offline/online auth.
WsoftPackerPinResult packers_sign_in_offline(const char *code, const char *pin, WsoftPackerEntry *out_entry = nullptr);
void packers_apply_auth(const WsoftPackerAuthReply &reply);
bool packers_select(const char *code);
bool packers_sign_out(const char *code);
void packers_tick(uint32_t now_ms);

// Floor Manager ISSUE / HANDIN queued locally and synced via /api/v1/ingest/bag-events.
WsoftBagEventError packers_floor_issue(const char *event_id, const char *actor_code, const char *packer_code,
                                       int32_t quantity, const char *reason_note = nullptr,
                                       const char *occurred_at = nullptr);
WsoftBagEventError packers_floor_handin(const char *event_id, const char *actor_code, const char *packer_code,
                                        int32_t lost_qty, int32_t returned_qty, const char *reason_code,
                                        const char *reason_note = nullptr, bool close_batch = true,
                                        const char *occurred_at = nullptr);

// Hub task sync hooks.
bool packers_pull_due();
bool packers_request(char *out, size_t size);
void packers_take(const WsoftPackerRoster &fresh);
void packers_failed();

bool packers_bag_events_due();
bool packers_bag_events_request(char *out, size_t size, size_t *count);
void packers_bag_events_take(const WsoftBagEventResult *results, size_t count);
void packers_bag_events_failed();

struct PackersInfo {
  bool required;
  char pack_unit_label[WSOFT_PACKER_UNIT_MAX + 1];
  size_t roster_count;
  size_t strip_count;
  char active_code[WSOFT_PACKER_CODE_MAX + 1];
  int32_t active_remaining;
  size_t outbox_pending;
  uint32_t last_ok_ms;
};
void packers_info(PackersInfo *out);
