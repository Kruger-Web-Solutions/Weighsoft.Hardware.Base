#include "wsoft_log.h"
#include "scale.h"

#include <Preferences.h>
#include <freertos/semphr.h>

#include "approval.h"

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t save_mutex = nullptr;

// Shared between tasks: under the spinlock.
static bool rs485 = true;
static WsoftScaleSetup stored_setup;
static WsoftCardFormat card;
// A card with no format registers (DJB-W1X): the decimals, e and unit kept on this board (NVS
// fdec / fdiv / funit), and whether the last read of the card found no format.
static WsoftCardFormat board_format;
static bool card_no_format = false;
static uint32_t audit = 0;
static WsoftTare tare;

// Screen task only.
static WsoftStableSamples samples;

// RS485 card tuning and the board's ZERO (23 Nov 2026). Shared: under the spinlock.
static bool card_source = false;
static WsoftRs485Tuning tuning;
static WsoftBoardZero board_zero;
static int32_t track_offset = 0;
static int32_t last_raw = 0;
static int last_decimals = 0;
static int last_unit = 0;
static bool have_raw = false;
// Screen task only.
static WsoftZeroTracker tracker;

static void load(WsoftScaleSetup *s, uint32_t *counter) {
  Preferences store;
  store.begin("scale", true);
  strlcpy(s->sa_number, store.getString("sa", "").c_str(), sizeof s->sa_number);
  s->max_kg = store.getLong64("max", WSOFT_KG_UNSET);
  s->min_kg = store.getLong64("min", WSOFT_KG_UNSET);
  s->blank_below_e = store.getInt("bb", WSOFT_BLANK_UNSET);
  s->blank_above_e = store.getInt("ba", s->blank_above_e);
  s->stable_samples = store.getInt("ss", s->stable_samples);
  s->tare_enabled = store.getBool("te", s->tare_enabled);
  s->tare_max_kg = store.getLong64("tmax", WSOFT_KG_UNSET);
  s->preset_tare_kg = store.getLong64("tpre", WSOFT_KG_UNSET);
  *counter = store.getUInt("audit", 0);
  store.end();
}

static void load_board_format(WsoftCardFormat *f) {
  Preferences store;
  store.begin("scale", true);
  f->decimal = store.getInt("fdec", 0);
  f->division = store.getInt("fdiv", 0);
  f->unit = store.getInt("funit", 1);
  store.end();
  f->read = wsoft_board_format_valid(f->decimal, f->division, f->unit);
}

static void store_setup(const WsoftScaleSetup &s, uint32_t counter) {
  Preferences store;
  store.begin("scale", false);
  store.putString("sa", s.sa_number);
  store.putLong64("max", s.max_kg);
  store.putLong64("min", s.min_kg);
  store.putInt("bb", s.blank_below_e);
  store.putInt("ba", s.blank_above_e);
  store.putInt("ss", s.stable_samples);
  store.putBool("te", s.tare_enabled);
  store.putLong64("tmax", s.tare_max_kg);
  store.putLong64("tpre", s.preset_tare_kg);
  store.putUInt("audit", counter);
  store.end();
}

static void load_tuning(WsoftRs485Tuning *t, WsoftBoardZero *z) {
  Preferences store;
  store.begin("scale", true);
  WsoftRs485Tuning d;
  t->stable_range = store.getInt("srng", d.stable_range);
  t->stable_time = store.getInt("stim", d.stable_time);
  t->zero_track_range = store.getInt("ztr", d.zero_track_range);
  t->zero_track_time = store.getInt("ztt", d.zero_track_time);
  z->set = store.getBool("zset", false);
  z->raw = store.getInt("zraw", 0);
  z->decimals = store.getInt("zdec", 0);
  z->unit = store.getInt("zunit", 0);
  store.end();
  if (!wsoft_rs485_tuning_valid(*t)) *t = d;
}

void scale_begin(bool is_rs485, bool card) {
  if (save_mutex == nullptr) save_mutex = xSemaphoreCreateMutex();
  WsoftScaleSetup loaded;
  uint32_t counter = 0;
  load(&loaded, &counter);
  WsoftCardFormat kept;
  load_board_format(&kept);
  WsoftRs485Tuning kept_tuning;
  WsoftBoardZero kept_zero;
  load_tuning(&kept_tuning, &kept_zero);
  portENTER_CRITICAL(&lock);
  rs485 = is_rs485;
  card_source = is_rs485 && card;
  tuning = kept_tuning;
  board_zero = kept_zero;
  board_format = kept;
  stored_setup = loaded;
  audit = counter;
  portEXIT_CRITICAL(&lock);
  WSOFT_LOG.printf("SETUP %s audit=%lu\n", is_rs485 ? "kept on the board" : "rs232 (set on the indicator)",
                 (unsigned long)counter);
}

bool scale_rs485_card() {
  portENTER_CRITICAL(&lock);
  bool value = card_source;
  portEXIT_CRITICAL(&lock);
  return value;
}

bool scale_rs485() {
  portENTER_CRITICAL(&lock);
  bool value = rs485;
  portEXIT_CRITICAL(&lock);
  return value;
}

bool scale_lock(uint32_t timeout_ms) {
  if (save_mutex == nullptr) return true;
  TickType_t wait = timeout_ms == SCALE_LOCK_FOREVER ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
  return xSemaphoreTake(save_mutex, wait) == pdTRUE;
}

void scale_unlock() {
  if (save_mutex != nullptr) xSemaphoreGive(save_mutex);
}

bool scale_setup_complete() {
  portENTER_CRITICAL(&lock);
  WsoftScaleSetup s = stored_setup;
  WsoftCardFormat c = card;
  portEXIT_CRITICAL(&lock);
  return wsoft_scale_setup_complete(s, c);
}

void scale_set_card(int32_t division, int decimal, int unit) {
  portENTER_CRITICAL(&lock);
  card.read = true;
  card.division = division;
  card.decimal = decimal;
  card.unit = unit;
  portEXIT_CRITICAL(&lock);
}

bool scale_card_no_format(WsoftCardFormat *kept) {
  portENTER_CRITICAL(&lock);
  card_no_format = true;
  WsoftCardFormat f = board_format;
  if (f.read) card = f;
  portEXIT_CRITICAL(&lock);
  if (kept != nullptr) *kept = f;
  return f.read;
}

void scale_card_has_format() {
  portENTER_CRITICAL(&lock);
  card_no_format = false;
  portEXIT_CRITICAL(&lock);
}

WsoftSetupError scale_save_board_format(int decimal, int32_t division, int unit, const char *who) {
  if (!wsoft_board_format_valid(decimal, division, unit)) return WSOFT_SETUP_NUMBER;
  if (!scale_lock(SCALE_LOCK_WEB_MS)) return WSOFT_SETUP_BUSY;  // web task: do not block it
  portENTER_CRITICAL(&lock);
  bool on_rs485 = rs485;
  bool same = board_format.read && board_format.decimal == decimal && board_format.division == division &&
              board_format.unit == unit;
  portEXIT_CRITICAL(&lock);
  if (!on_rs485 || approval_on()) {
    scale_unlock();
    return WSOFT_SETUP_LOCKED;
  }
  if (same) {
    scale_unlock();
    return WSOFT_SETUP_OK;
  }
  portENTER_CRITICAL(&lock);
  uint32_t counter = ++audit;
  board_format.read = true;
  board_format.decimal = decimal;
  board_format.division = division;
  board_format.unit = unit;
  if (card_no_format) card = board_format;
  tare = WsoftTare();  // the format changed
  portEXIT_CRITICAL(&lock);
  Preferences store;
  store.begin("scale", false);
  store.putInt("fdec", decimal);
  store.putInt("fdiv", division);
  store.putInt("funit", unit);
  store.putUInt("audit", counter);
  store.end();
  scale_unlock();
  WSOFT_LOG.printf("SETUP format kept on the board dec=%d step=%ld unit=%d audit=%lu by=%s\n", decimal, (long)division,
                   unit, (unsigned long)counter, who);
  return WSOFT_SETUP_OK;
}

void scale_card_written() {
  scale_lock(SCALE_LOCK_FOREVER);  // screen task
  portENTER_CRITICAL(&lock);
  uint32_t counter = ++audit;
  portEXIT_CRITICAL(&lock);
  Preferences store;
  store.begin("scale", false);
  store.putUInt("audit", counter);
  store.end();
  scale_unlock();
  WSOFT_LOG.printf("SETUP card write audit=%lu\n", (unsigned long)counter);
}

void scale_info(ScaleInfo *out) {
  portENTER_CRITICAL(&lock);
  out->rs485 = rs485;
  out->setup = stored_setup;
  out->card = card;
  out->card_no_format = card_no_format;
  out->audit = audit;
  portEXIT_CRITICAL(&lock);
}

WsoftScaleHold scale_hold(bool linked, bool fresh, int32_t value, int32_t net, int decimals, int unit) {
  int32_t range = 0;
  uint32_t still_ms = 0;
  scale_stability(&range, &still_ms);
  uint32_t now = millis();
  samples.update(linked, fresh, value, range, now);
  portENTER_CRITICAL(&lock);
  bool on_rs485 = rs485;
  WsoftScaleSetup s = stored_setup;
  WsoftCardFormat c = card;
  portEXIT_CRITICAL(&lock);
  if (!on_rs485 || !linked) return WSOFT_HOLD_NONE;
  return wsoft_scale_hold(s, c, value, net, decimals, unit, samples.stable_for(s.stable_samples, still_ms, now));
}

// ---------- RS485 card tuning and the board's ZERO ----------

WsoftRs485Tuning scale_tuning() {
  portENTER_CRITICAL(&lock);
  WsoftRs485Tuning t = tuning;
  portEXIT_CRITICAL(&lock);
  return t;
}

void scale_stability(int32_t *range, uint32_t *ms) {
  portENTER_CRITICAL(&lock);
  bool on_card = card_source;
  WsoftRs485Tuning t = tuning;
  portEXIT_CRITICAL(&lock);
  *range = on_card ? t.stable_range : 0;
  *ms = on_card ? (uint32_t)t.stable_time * 100u : 0u;
}

WsoftSetupError scale_save_tuning(const WsoftRs485Tuning &next, const char *who) {
  if (!wsoft_rs485_tuning_valid(next)) return WSOFT_SETUP_NUMBER;
  if (!scale_lock(SCALE_LOCK_WEB_MS)) return WSOFT_SETUP_BUSY;  // web task: do not block it
  portENTER_CRITICAL(&lock);
  bool on_card = card_source;
  bool same = tuning == next;
  portEXIT_CRITICAL(&lock);
  if (!on_card || approval_on()) {
    scale_unlock();
    return WSOFT_SETUP_LOCKED;
  }
  if (same) {
    scale_unlock();
    return WSOFT_SETUP_OK;
  }
  portENTER_CRITICAL(&lock);
  uint32_t counter = ++audit;
  tuning = next;
  track_offset = 0;  // the screen task resets its tracker when it sees 0 with a new tuning
  portEXIT_CRITICAL(&lock);
  Preferences store;
  store.begin("scale", false);
  store.putInt("srng", next.stable_range);
  store.putInt("stim", next.stable_time);
  store.putInt("ztr", next.zero_track_range);
  store.putInt("ztt", next.zero_track_time);
  store.putUInt("audit", counter);
  store.end();
  scale_unlock();
  WSOFT_LOG.printf("SETUP tuning stable_range=%ld stable_time=%ld zero_track=%ld zero_time=%ld audit=%lu by=%s\n",
                   (long)next.stable_range, (long)next.stable_time, (long)next.zero_track_range,
                   (long)next.zero_track_time, (unsigned long)counter, who);
  return WSOFT_SETUP_OK;
}

int32_t scale_card_shown(bool linked, bool fresh, int32_t raw, int decimals, int unit) {
  static WsoftRs485Tuning seen;
  portENTER_CRITICAL(&lock);
  bool on_card = card_source;
  WsoftRs485Tuning t = tuning;
  WsoftBoardZero z = board_zero;
  bool no_format = card_no_format;
  int32_t step = no_format && board_format.read ? board_format.division : 1;
  int needed = stored_setup.stable_samples;
  if (fresh) {
    have_raw = true;
    last_raw = raw;
    last_decimals = decimals;
    last_unit = unit;
  }
  portEXIT_CRITICAL(&lock);
  if (!on_card) return raw;
  // A zero taken in other decimals or another unit no longer means anything: drop it.
  if (z.set && (z.decimals != decimals || z.unit != unit)) {
    portENTER_CRITICAL(&lock);
    board_zero = WsoftBoardZero();
    portEXIT_CRITICAL(&lock);
    Preferences store;
    store.begin("scale", false);
    store.putBool("zset", false);
    store.end();
    WSOFT_LOG.println("ZERO cleared: the scale's unit or decimals changed");
    z = WsoftBoardZero();
  }
  if (t != seen) {
    tracker.reset();
    seen = t;
  }
  int32_t shown = raw;
  wsoft_board_zero_apply(z, tracker.offset(), raw, decimals, unit, step, &shown);
  // Auto zero on the board only for a card with no zero tracking of its own (DJB-W1X): an M1X
  // tracks its own zero (41313/41315). Judged on the reading before rounding to e.
  if (no_format && t.zero_track_range > 0) {
    int32_t unrounded = shown;
    wsoft_board_zero_apply(z, tracker.offset(), raw, decimals, unit, 1, &unrounded);
    int32_t range = 0;
    uint32_t still_ms = 0;
    scale_stability(&range, &still_ms);
    bool stable = samples.stable_for(needed, still_ms, millis());
    if (tracker.update(linked && fresh, stable, unrounded, t.zero_track_range, (uint32_t)t.zero_track_time * 100u,
                       millis())) {
      wsoft_board_zero_apply(z, tracker.offset(), raw, decimals, unit, step, &shown);
      WSOFT_LOG.printf("AUTO ZERO offset=%ld\n", (long)tracker.offset());
    }
  }
  portENTER_CRITICAL(&lock);
  track_offset = no_format ? tracker.offset() : 0;
  portEXIT_CRITICAL(&lock);
  return shown;
}

WsoftZeroError scale_card_zero(bool linked, bool stable, const char *who) {
  portENTER_CRITICAL(&lock);
  bool on_card = card_source;
  bool tared = tare.on;
  bool fresh_raw = have_raw;
  portEXIT_CRITICAL(&lock);
  WsoftZeroError error = wsoft_zero_check(on_card, approval_on(), tared, linked && fresh_raw, stable);
  if (error != WSOFT_ZERO_OK) {
    WSOFT_LOG.printf("ZERO refused %s by=%s\n", wsoft_zero_error_code(error), who);
    return error;
  }
  WsoftBoardZero next;
  next.set = true;
  portENTER_CRITICAL(&lock);
  next.raw = last_raw;
  next.decimals = last_decimals;
  next.unit = last_unit;
  board_zero = next;
  track_offset = 0;
  portEXIT_CRITICAL(&lock);
  tracker.reset();  // screen task, like scale_card_shown
  Preferences store;
  store.begin("scale", false);
  store.putBool("zset", true);
  store.putInt("zraw", next.raw);
  store.putInt("zdec", next.decimals);
  store.putInt("zunit", next.unit);
  store.end();
  WSOFT_LOG.printf("ZERO set raw=%ld dec=%d unit=%d by=%s\n", (long)next.raw, next.decimals, next.unit, who);
  return WSOFT_ZERO_OK;
}

void scale_zero_info(ScaleZeroInfo *out) {
  portENTER_CRITICAL(&lock);
  out->card = card_source;
  out->zero = board_zero;
  out->track = track_offset;
  out->board_tracks = card_source && card_no_format;
  out->raw = last_raw;
  portEXIT_CRITICAL(&lock);
}

WsoftTareError scale_tare_toggle(bool stable, int32_t value, int decimals, int unit) {
  portENTER_CRITICAL(&lock);
  bool was_on = tare.on;
  WsoftScaleSetup s = stored_setup;
  bool on_rs485 = rs485;
  if (was_on) tare = WsoftTare();
  portEXIT_CRITICAL(&lock);
  if (!on_rs485) return WSOFT_TARE_OFF;
  if (was_on) {
    WSOFT_LOG.println("TARE cleared");
    return WSOFT_TARE_OK;
  }
  WsoftTare next;
  WsoftTareError error = wsoft_tare_take(s, stable, value, decimals, unit, &next);
  if (error == WSOFT_TARE_OK) {
    portENTER_CRITICAL(&lock);
    tare = next;
    portEXIT_CRITICAL(&lock);
  }
  WSOFT_LOG.printf("TARE %s\n", error == WSOFT_TARE_OK ? "on" : wsoft_tare_error_code(error));
  return error;
}

WsoftTareError scale_tare_preset(int decimals, int unit) {
  portENTER_CRITICAL(&lock);
  WsoftScaleSetup s = stored_setup;
  bool on_rs485 = rs485;
  portEXIT_CRITICAL(&lock);
  if (!on_rs485) return WSOFT_TARE_OFF;
  WsoftTare next;
  WsoftTareError error = wsoft_tare_preset(s, decimals, unit, &next);
  if (error == WSOFT_TARE_OK) {
    portENTER_CRITICAL(&lock);
    tare = next;
    portEXIT_CRITICAL(&lock);
  }
  WSOFT_LOG.printf("TARE preset %s\n", error == WSOFT_TARE_OK ? "on" : wsoft_tare_error_code(error));
  return error;
}

WsoftTare scale_tare() {
  portENTER_CRITICAL(&lock);
  WsoftTare value = tare;
  portEXIT_CRITICAL(&lock);
  return value;
}

bool scale_net(int32_t gross, int decimals, int unit, int32_t *net) {
  WsoftTare now = scale_tare();
  if (wsoft_tare_net(now, gross, decimals, unit, net)) return true;
  if (now.on) {
    portENTER_CRITICAL(&lock);
    tare = WsoftTare();
    portEXIT_CRITICAL(&lock);
    WSOFT_LOG.println("TARE cleared: the scale's unit or decimals changed");
  }
  return false;
}

WsoftSetupError scale_save(const WsoftScaleSetupInput &in, const char *who) {
  if (!scale_lock(SCALE_LOCK_WEB_MS)) return WSOFT_SETUP_BUSY;  // web task: do not block it
  ScaleInfo now;
  scale_info(&now);
  if (!now.rs485 || approval_on()) {
    scale_unlock();
    return WSOFT_SETUP_LOCKED;
  }
  WsoftScaleSetup next = now.setup;
  bool changed = false;
  WsoftSetupError error = wsoft_scale_setup_update(in, now.card, &next, &changed);
  uint32_t counter = now.audit;
  if (error == WSOFT_SETUP_OK && changed) {
    counter++;
    store_setup(next, counter);
    portENTER_CRITICAL(&lock);
    stored_setup = next;
    audit = counter;
    tare = WsoftTare();  // the tare limits may have changed
    portEXIT_CRITICAL(&lock);
  }
  scale_unlock();
  if (error != WSOFT_SETUP_OK) {
    WSOFT_LOG.printf("SETUP save refused %s by=%s\n", wsoft_setup_error_code(error), who);
  } else if (changed) {
    WSOFT_LOG.printf("SETUP saved audit=%lu by=%s why=%s\n", (unsigned long)counter, who, in.reason);
  }
  return error;
}
