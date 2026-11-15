#ifdef WSOFT_ON_BASE

#include "wsoft_log.h"
#include "wifi_update.h"

#include <ArduinoOTA.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <freertos/semphr.h>

#include "approval.h"
#include "approval_base.h"

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
// Held across "check, then write" of one change from the page.
static SemaphoreHandle_t change_mutex = nullptr;

// Shared between the web task and the screen task: under the spinlock.
static bool begun = false;
static bool two_copies = false;
static bool on = false;
static bool password_set = false;
static bool restart_service = false;  // the password changed: the service must start again
static volatile bool listening = false;
static volatile bool busy = false;

// Audit lines the web task asked for; the screen task writes them (it owns the Approval log).
static const int EVENT_SLOTS = 4;
struct PendingEvent {
  WsoftWifiUpdateEvent event;
  char who[WSOFT_WIFI_UPDATE_WHO_MAX + 1];
};
static PendingEvent pending[EVENT_SLOTS];
static int pending_count = 0;

static char software_version[17] = "";

// Screen task only.
static ArduinoOTAClass *service = nullptr;

struct LineContext {
  WsoftWifiUpdateEvent event;
  const char *who;
};

static bool make_line(uint32_t counter, const char *when, char *out, size_t size, void *context) {
  LineContext *c = (LineContext *)context;
  return wsoft_wifi_update_log_line(counter, when, c->event, c->who, software_version, out, size);
}

// Screen task.
static void write_event(WsoftWifiUpdateEvent event, const char *who) {
  LineContext context = {event, who};
  approval_audit_event(make_line, &context);
}

static WsoftWifiUpdateState state_now() {
  WsoftWifiUpdateState s;
  portENTER_CRITICAL(&lock);
  s.two_copies = two_copies;
  s.on = on;
  s.password_set = password_set;
  portEXIT_CRITICAL(&lock);
  s.approval_on = approval_on();
  return s;
}

void wifi_update_begin(const char *version) {
  if (change_mutex == nullptr) change_mutex = xSemaphoreCreateMutex();
  strlcpy(software_version, version == nullptr ? "" : version, sizeof software_version);
  // Asked of the flash itself, not of the build: a board whose partition table has no second
  // app slot (the yellow board) answers no, whatever software is on it.
  bool room = esp_ota_get_next_update_partition(nullptr) != nullptr;
  {
    // A read-only open of a namespace that was never written logs an error: make it once.
    Preferences make;
    make.begin("wifiupd", false);
    make.end();
  }
  Preferences store;
  store.begin("wifiupd", true);
  bool stored_on = store.getBool("on", false);
  bool have_password = wsoft_wifi_update_password_check(store.getString("pw", "").c_str()) == WSOFT_WIFI_UPDATE_OK;
  store.end();
  portENTER_CRITICAL(&lock);
  two_copies = room;
  on = stored_on;
  password_set = have_password;
  begun = true;
  portEXIT_CRITICAL(&lock);
  WSOFT_LOG.printf("WIFI-UPDATE %s switch=%s password=%s\n", room ? "possible" : "not possible (one copy: cable only)",
                   stored_on ? "ON" : "OFF", have_password ? "set" : "not set");
}

bool wifi_update_busy() {
  return busy;
}

void wifi_update_info(WifiUpdateInfo *out) {
  WsoftWifiUpdateState s = state_now();
  out->two_copies = s.two_copies;
  out->on = s.on;
  out->password_set = s.password_set;
  out->approval_on = s.approval_on;
  out->listening = listening;
  out->blocked = wsoft_wifi_update_blocked(s);
}

WsoftWifiUpdateError wifi_update_set(const WsoftWifiUpdateRequest &request, const char *who) {
  if (change_mutex == nullptr || xSemaphoreTake(change_mutex, pdMS_TO_TICKS(WIFI_UPDATE_SET_WAIT_MS)) != pdTRUE) {
    return WSOFT_WIFI_UPDATE_BUSY;
  }
  WsoftWifiUpdateChange change;
  WsoftWifiUpdateError error = wsoft_wifi_update_check(request, state_now(), &change);
  WsoftWifiUpdateEvent event;
  if (error == WSOFT_WIFI_UPDATE_OK && wsoft_wifi_update_event(change, &event)) {
    portENTER_CRITICAL(&lock);
    bool room = pending_count < EVENT_SLOTS;
    portEXIT_CRITICAL(&lock);
    if (!room) {
      // No change without its audit line.
      xSemaphoreGive(change_mutex);
      return WSOFT_WIFI_UPDATE_BUSY;
    }
    Preferences store;
    store.begin("wifiupd", false);
    if (change.password_changed) store.putString("pw", request.password);
    if (change.on_changed) store.putBool("on", change.on);
    store.end();
    portENTER_CRITICAL(&lock);
    on = change.on;
    if (change.password_changed) {
      password_set = true;
      restart_service = true;
    }
    PendingEvent &slot = pending[pending_count++];
    slot.event = event;
    strlcpy(slot.who, who == nullptr ? "" : who, sizeof slot.who);
    portEXIT_CRITICAL(&lock);
  }
  xSemaphoreGive(change_mutex);
  if (error != WSOFT_WIFI_UPDATE_OK) {
    WSOFT_LOG.printf("WIFI-UPDATE change refused %s\n", wsoft_wifi_update_error_code(error));
  }
  return error;
}

// ---------- screen task ----------

static void flush_events() {
  for (;;) {
    PendingEvent next;
    portENTER_CRITICAL(&lock);
    bool have = pending_count > 0;
    if (have) {
      next = pending[0];
      for (int i = 1; i < pending_count; i++) pending[i - 1] = pending[i];
      pending_count--;
    }
    portEXIT_CRITICAL(&lock);
    if (!have) return;
    write_event(next.event, next.who);
  }
}

static void stop_service(const char *why) {
  if (service == nullptr) return;
  service->end();
  delete service;
  service = nullptr;
  listening = false;
  WSOFT_LOG.printf("WIFI-UPDATE not listening: %s\n", why);
}

static void start_service() {
  String password;
  {
    Preferences store;
    store.begin("wifiupd", true);
    password = store.getString("pw", "");
    store.end();
  }
  // Checked again here: nothing listens with a password that does not pass the rules.
  if (wsoft_wifi_update_password_check(password.c_str()) != WSOFT_WIFI_UPDATE_OK) return;
  service = new ArduinoOTAClass;
  service->setPort(WIFI_UPDATE_PORT);
  service->setPassword(password.c_str());
  service->setTimeout(10000);
  // The update is sent to the board's address; W-Soft's own mDNS service stays untouched.
  service->setMdnsEnabled(false);
  service->setRebootOnSuccess(true);
  service->onStart([]() {
    // The service is stopped within one pass of Approval going ON; this closes the last gap.
    if (approval_on()) {
      Update.abort();
      WSOFT_LOG.println("WIFI-UPDATE refused: Approval is ON");
      return;
    }
    busy = true;
    WSOFT_LOG.println("WIFI-UPDATE start");
  });
  service->onEnd([]() {
    // The new software is in place and starts at the restart that follows.
    write_event(WSOFT_WIFI_UPDATE_EVENT_INSTALLED, "wifi");
    WSOFT_LOG.println("WIFI-UPDATE installed, restarting");
  });
  service->onError([](ota_error_t error) {
    busy = false;
    WSOFT_LOG.printf("WIFI-UPDATE error %u (%s)\n", (unsigned)error,
                     error == OTA_AUTH_ERROR ? "wrong password" : "nothing was installed");
  });
  service->begin();
  password = "";
  listening = true;
  WSOFT_LOG.printf("WIFI-UPDATE listening ip=%s port=%u\n", WiFi.localIP().toString().c_str(),
                   (unsigned)WIFI_UPDATE_PORT);
}

void wifi_update_poll() {
  portENTER_CRITICAL(&lock);
  bool ready = begun;
  bool restart = restart_service;
  restart_service = false;
  portEXIT_CRITICAL(&lock);
  if (!ready) return;
  if (restart && busy) {
    // An update is coming in: the new password takes effect once it is over.
    portENTER_CRITICAL(&lock);
    restart_service = true;
    portEXIT_CRITICAL(&lock);
    restart = false;
  }
  flush_events();

  WsoftWifiUpdateState s = state_now();
  bool want = wsoft_wifi_update_listening(s) && WiFi.status() == WL_CONNECTED;
  if (service != nullptr && (!want || restart) && !busy) {
    stop_service(restart ? "new password" : s.approval_on ? "Approval is ON" : !s.on ? "switched off" : "no Wi-Fi");
  }
  if (service == nullptr && want) start_service();
  if (service != nullptr) service->handle();
}

#endif  // WSOFT_ON_BASE
