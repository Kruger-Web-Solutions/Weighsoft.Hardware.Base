#include "scale.h"

#include <Preferences.h>
#include <freertos/semphr.h>

#include "approval.h"

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t save_mutex = nullptr;

// Shared between tasks: under the spinlock.
static bool rs485 = true;
static WsoftScaleSetup setup;
static WsoftCardFormat card;
static uint32_t audit = 0;
static WsoftTare tare;

// Screen task only.
static WsoftStableSamples samples;

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

void scale_begin(bool is_rs485) {
  if (save_mutex == nullptr) save_mutex = xSemaphoreCreateMutex();
  WsoftScaleSetup loaded;
  uint32_t counter = 0;
  load(&loaded, &counter);
  portENTER_CRITICAL(&lock);
  rs485 = is_rs485;
  setup = loaded;
  audit = counter;
  portEXIT_CRITICAL(&lock);
  Serial0.printf("SETUP %s audit=%lu\n", is_rs485 ? "rs485 (kept on the board)" : "rs232 (set on the indicator)",
                 (unsigned long)counter);
}

bool scale_rs485() {
  portENTER_CRITICAL(&lock);
  bool value = rs485;
  portEXIT_CRITICAL(&lock);
  return value;
}

void scale_lock() {
  if (save_mutex != nullptr) xSemaphoreTake(save_mutex, portMAX_DELAY);
}

void scale_unlock() {
  if (save_mutex != nullptr) xSemaphoreGive(save_mutex);
}

bool scale_setup_complete() {
  portENTER_CRITICAL(&lock);
  WsoftScaleSetup s = setup;
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

void scale_card_written() {
  scale_lock();
  portENTER_CRITICAL(&lock);
  uint32_t counter = ++audit;
  portEXIT_CRITICAL(&lock);
  Preferences store;
  store.begin("scale", false);
  store.putUInt("audit", counter);
  store.end();
  scale_unlock();
  Serial0.printf("SETUP card write audit=%lu\n", (unsigned long)counter);
}

void scale_info(ScaleInfo *out) {
  portENTER_CRITICAL(&lock);
  out->rs485 = rs485;
  out->setup = setup;
  out->card = card;
  out->audit = audit;
  portEXIT_CRITICAL(&lock);
}

WsoftScaleHold scale_hold(bool linked, bool fresh, int32_t value, int32_t net, int decimals, int unit) {
  samples.update(linked, fresh, value);
  portENTER_CRITICAL(&lock);
  bool on_rs485 = rs485;
  WsoftScaleSetup s = setup;
  WsoftCardFormat c = card;
  portEXIT_CRITICAL(&lock);
  if (!on_rs485 || !linked) return WSOFT_HOLD_NONE;
  return wsoft_scale_hold(s, c, value, net, decimals, unit, samples.stable(s.stable_samples));
}

WsoftTareError scale_tare_toggle(bool stable, int32_t value, int decimals, int unit) {
  portENTER_CRITICAL(&lock);
  bool was_on = tare.on;
  WsoftScaleSetup s = setup;
  bool on_rs485 = rs485;
  if (was_on) tare = WsoftTare();
  portEXIT_CRITICAL(&lock);
  if (!on_rs485) return WSOFT_TARE_OFF;
  if (was_on) {
    Serial0.println("TARE cleared");
    return WSOFT_TARE_OK;
  }
  WsoftTare next;
  WsoftTareError error = wsoft_tare_take(s, stable, value, decimals, unit, &next);
  if (error == WSOFT_TARE_OK) {
    portENTER_CRITICAL(&lock);
    tare = next;
    portEXIT_CRITICAL(&lock);
  }
  Serial0.printf("TARE %s\n", error == WSOFT_TARE_OK ? "on" : wsoft_tare_error_code(error));
  return error;
}

WsoftTareError scale_tare_preset(int decimals, int unit) {
  portENTER_CRITICAL(&lock);
  WsoftScaleSetup s = setup;
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
  Serial0.printf("TARE preset %s\n", error == WSOFT_TARE_OK ? "on" : wsoft_tare_error_code(error));
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
    Serial0.println("TARE cleared: the scale's unit or decimals changed");
  }
  return false;
}

WsoftSetupError scale_save(const WsoftScaleSetupInput &in, const char *who) {
  scale_lock();
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
    setup = next;
    audit = counter;
    tare = WsoftTare();  // the tare limits may have changed
    portEXIT_CRITICAL(&lock);
  }
  scale_unlock();
  if (error != WSOFT_SETUP_OK) {
    Serial0.printf("SETUP save refused %s by=%s\n", wsoft_setup_error_code(error), who);
  } else if (changed) {
    Serial0.printf("SETUP saved audit=%lu by=%s why=%s\n", (unsigned long)counter, who, in.reason);
  }
  return error;
}
