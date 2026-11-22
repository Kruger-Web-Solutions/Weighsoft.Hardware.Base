#include "wsoft_log.h"
#include "loadcell.h"

#include <Preferences.h>

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
// Held only around the 25 clock pulses, so nothing can stretch a pulse.
static portMUX_TYPE pulse_lock = portMUX_INITIALIZER_UNLOCKED;

// A count older than this is no longer live. At the chip's slow rate (10 a second) a count
// comes every 100 ms, so a full second of silence means the board is not answering.
static const uint32_t LOADCELL_STALE_MS = 1000;

static int pin_dt = -1;
static int pin_sck = -1;
static bool started = false;

// Shared between tasks: under the spinlock.
static WsoftLoadcellCal cal;
static WsoftLoadcellFilter filter;
static uint32_t last_sample_ms = 0;
static bool have_sample = false;

static void load(WsoftLoadcellCal *c) {
  wsoft_loadcell_defaults(c);
  Preferences store;
  store.begin("loadcell", true);
  c->zero_set = store.getBool("zs", false);
  c->zero_raw = store.getInt("zr", 0);
  c->span_set = store.getBool("ss", false);
  c->span_counts = store.getInt("sc", 0);
  c->span_value = store.getInt("sv", 0);
  c->decimals = store.getInt("dec", c->decimals);
  c->unit = store.getInt("unit", c->unit);
  c->step = store.getInt("step", c->step);
  store.end();
  // A store that does not add up is treated as not calibrated, never as a weight.
  if (c->decimals < 0 || c->decimals > 4 || (c->unit != 1 && c->unit != 3) || c->span_counts == 0 ||
      c->span_value <= 0) {
    c->span_set = false;
  }
  if (!wsoft_loadcell_step_valid(c->step)) c->step = 1;
}

static void store_cal(const WsoftLoadcellCal &c) {
  Preferences store;
  store.begin("loadcell", false);
  store.putBool("zs", c.zero_set);
  store.putInt("zr", c.zero_raw);
  store.putBool("ss", c.span_set);
  store.putInt("sc", c.span_counts);
  store.putInt("sv", c.span_value);
  store.putInt("dec", c.decimals);
  store.putInt("unit", c.unit);
  store.putInt("step", c.step);
  store.end();
}

// One count from the chip. Call only when DT is low.
static int32_t read_count() {
  uint32_t bits = 0;
  portENTER_CRITICAL(&pulse_lock);
  for (int i = 0; i < 24; i++) {
    digitalWrite(pin_sck, HIGH);
    delayMicroseconds(1);
    bits = (bits << 1) | (digitalRead(pin_dt) ? 1u : 0u);
    digitalWrite(pin_sck, LOW);
    delayMicroseconds(1);
  }
  // The 25th pulse: the next count is channel A at gain 128.
  digitalWrite(pin_sck, HIGH);
  delayMicroseconds(1);
  digitalWrite(pin_sck, LOW);
  portEXIT_CRITICAL(&pulse_lock);
  return wsoft_hx711_decode(bits);
}

static void reader_task(void *) {
  for (;;) {
    if (digitalRead(pin_dt) == LOW) {
      int32_t raw = read_count();
      uint32_t now = millis();
      portENTER_CRITICAL(&lock);
      if (wsoft_hx711_saturated(raw)) {
        // Out of range: drop the average so an old weight does not linger.
        // Not caught here: an HX711 board that is plugged in but has no power can hold DT low
        // and read as the same count over and over. The Calibrate page shows the raw count for
        // that: a count that never moves when the load cell is pressed is a wiring fault.
        filter.reset();
        have_sample = false;
      } else {
        filter.add(raw);
        last_sample_ms = now;
        have_sample = true;
      }
      portEXIT_CRITICAL(&lock);
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void loadcell_begin(int dt_pin, int sck_pin) {
  if (started || dt_pin < 0 || sck_pin < 0) return;
  pin_dt = dt_pin;
  pin_sck = sck_pin;
  WsoftLoadcellCal loaded;
  load(&loaded);
  portENTER_CRITICAL(&lock);
  cal = loaded;
  filter.reset();
  have_sample = false;
  portEXIT_CRITICAL(&lock);
  // The pull-up keeps DT high (= not ready) when nothing is plugged in.
  pinMode(pin_dt, INPUT_PULLUP);
  pinMode(pin_sck, OUTPUT);
  digitalWrite(pin_sck, LOW);
  started = true;
  xTaskCreate(reader_task, "wsoftLoadcell", 3072, nullptr, 1, nullptr);
  WSOFT_LOG.printf("LOADCELL HX711 dt=GPIO%d sck=GPIO%d %s\n", dt_pin, sck_pin,
                   wsoft_loadcell_calibrated(loaded) ? "calibrated" : "NOT calibrated yet");
}

// Under the lock: the averaged count, when the chip answered within the live window.
static bool live_raw(int32_t *raw) {
  if (!started || !have_sample || millis() - last_sample_ms >= LOADCELL_STALE_MS) return false;
  return filter.average(raw);
}

bool loadcell_weight(int32_t *value, int *decimals, int *unit) {
  int32_t raw = 0;
  portENTER_CRITICAL(&lock);
  bool have = live_raw(&raw);
  WsoftLoadcellCal c = cal;
  portEXIT_CRITICAL(&lock);
  if (!have || !wsoft_loadcell_weight(c, raw, value)) return false;
  *decimals = c.decimals;
  *unit = c.unit;
  return true;
}

void loadcell_info(LoadcellInfo *out) {
  memset(out, 0, sizeof *out);
  portENTER_CRITICAL(&lock);
  out->started = started;
  out->have_raw = live_raw(&out->raw);
  out->cal = cal;
  portEXIT_CRITICAL(&lock);
  out->present = out->have_raw;
  out->calibrated = wsoft_loadcell_calibrated(out->cal);
  out->have_weight = out->have_raw && wsoft_loadcell_weight(out->cal, out->raw, &out->value);
}

// Applies one change to a copy, then stores and publishes it. NVS is written outside the
// spinlock (a flash write must not run with interrupts off).
template <typename Change>
static WsoftLoadcellError change_cal(Change change, const char *what, const char *who) {
  if (!started) return WSOFT_LOADCELL_NO_READING;
  int32_t raw = 0;
  portENTER_CRITICAL(&lock);
  bool have = live_raw(&raw);
  WsoftLoadcellCal next = cal;
  portEXIT_CRITICAL(&lock);
  WsoftLoadcellError error = change(&next, have, raw);
  if (error != WSOFT_LOADCELL_OK) {
    WSOFT_LOG.printf("LOADCELL %s refused: %s by=%s\n", what, wsoft_loadcell_error_code(error), who ? who : "?");
    return error;
  }
  store_cal(next);
  portENTER_CRITICAL(&lock);
  cal = next;
  portEXIT_CRITICAL(&lock);
  WSOFT_LOG.printf("LOADCELL %s count=%ld zero=%ld span_counts=%ld span_value=%ld decimals=%d unit=%d step=%d by=%s\n",
                   what, (long)raw, (long)next.zero_raw, (long)next.span_counts, (long)next.span_value, next.decimals,
                   next.unit, next.step, who ? who : "?");
  return WSOFT_LOADCELL_OK;
}

WsoftLoadcellError loadcell_set_zero(const char *who) {
  return change_cal([](WsoftLoadcellCal *c, bool have, int32_t raw) { return wsoft_loadcell_set_zero(c, have, raw); },
                    "ZERO", who);
}

WsoftLoadcellError loadcell_set_span(int32_t value, int decimals, int unit, const char *who) {
  return change_cal(
      [=](WsoftLoadcellCal *c, bool have, int32_t raw) {
        return wsoft_loadcell_set_span(c, have, raw, value, decimals, unit);
      },
      "SPAN", who);
}

WsoftLoadcellError loadcell_set_format(int decimals, int unit, int step, const char *who) {
  return change_cal(
      [=](WsoftLoadcellCal *c, bool, int32_t) { return wsoft_loadcell_set_format(c, decimals, unit, step); },
      "FORMAT", who);
}
