#pragma once

#include <Arduino.h>

#include "save_logic.h"

// Keeping a weighing on the W-Soft: auto-save (W-Soft > Settings), the SAVE button on the touch
// screen, and the Save button on the web Live page. The rules are host-tested in lib/wsoft_core
// (save_logic). This file adds flash storage (NVS namespace "wsave") and the hand-over to the Hub.
//
// - Each saved weighing goes to flash before anything else sees it, so a power cut keeps it.
// - The queue holds 32. Full means SAVE is refused ("queue_full"), never that one is dropped.
// - weighing_id = "wsoft-<mac>-<series>-<seq>". series is a random number kept in flash, so a box
//   whose flash was wiped (seq back to 1) never reuses an id the Hub already has.
// - The Hub task (hub.cpp) sends the oldest one with a signed POST /api/v1/ingest/weighings and
//   reports the answer; the screen task then removes it from flash (same split as approval.cpp).
// Approval and the trade path (approval.cpp) are untouched: with approval ON the trade weight
// still goes out in the heartbeat as before; the saved weighing is extra and marked trade:true.

static const size_t SAVE_RECENT_MAX = 10;

struct SaveStatus {
  bool can_save = false;  // a SAVE press now would be accepted
  SaveCheck check = SAVE_NO_SIGNAL;
  bool auto_on = false;
  uint32_t auto_stable_s = 3;
  SavedWeighing last;      // last saved since boot (valid false if none)
  uint32_t last_saved_ms = 0;  // millis() of that save, for the brief "SAVED n" on the screen
  uint32_t queue = 0;      // waiting for the Hub
  int hub_status = 0;      // last HTTP status for a weighing POST, 0 before the first
};

void save_begin();

// From WSoftSettingsService (start-up and every settings change). Any task.
// ignore_djb_alarm: bench test mode (W-Soft Settings), has no effect while approval is ON.
void save_set_auto(bool on, uint32_t stable_s, bool ignore_djb_alarm);

// Screen task, after every DJB poll. fresh is false when the poll failed (carried-over value).
SaveCheck save_update(bool linked, bool fresh, bool alarm, int32_t value, int decimals, int unit);

// Screen task (touch SAVE): ask for a save at the next poll; the screen shows the answer.
void save_press();

// Web task (POST /rest/wsoftSave): ask for a save and wait for the screen task's answer.
// Returns false when no answer came in time (screen paused, e.g. during a firmware upload).
// On true, *check is SAVE_OK and *seq is set when it was saved, otherwise *check says why not.
bool save_request(uint32_t timeout_ms, SaveCheck *check, uint32_t *seq);

void save_status(SaveStatus *out);
// Newest first. *queued_from is the seq of the oldest weighing still waiting for the Hub
// (0 when none wait): anything below it has been delivered.
size_t save_recent(SavedWeighing *out, size_t max, uint32_t *queued_from);

// ---------- Hub task (hub.cpp) ----------
size_t save_pending();
bool save_oldest(SavedWeighing *out);
// "wsoft-<mac>-<series>"; set in save_begin() and never changed after.
const char *save_id_prefix();
// The Hub stored it (201) or already had it (200).
void save_delivered(uint32_t seq);
void save_hub_result(int http_status);
