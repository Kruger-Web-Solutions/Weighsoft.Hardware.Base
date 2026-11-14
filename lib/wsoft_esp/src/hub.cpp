#include "wsoft_log.h"
#include "hub.h"
#include "approval.h"
#include "wsoft_ui.h"
#ifdef WSOFT_ON_BASE
#include "hilo.h"
#include "hub_web.h"
#include "switches.h"
#include "weighings.h"

#include <wsoft_hub_commands.h>
#endif
#ifndef WSOFT_ON_BASE
#include "ota_secret.h"
#endif

#ifndef WSOFT_ON_BASE
#include <ArduinoOTA.h>
#endif
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_random.h>
#include <mbedtls/base64.h>
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>
#include <time.h>

// Must match server/src/signing/codec.ts in Weighsoft.Indicator.Hub.
static const char *SIGNING_PREFIX = "scale-models-hub-v1";
#ifndef WSOFT_ON_BASE
static const char *STATION_MODEL = "waveshare-djb-demo";
static const char *RELEASE_VERSION = "bench-2026-10-24";
#else
// Hardware.Base: name the board this firmware was built for (the Hub shows both, max 32 / 64 chars).
#ifndef WSOFT_RELEASE_VERSION
#define WSOFT_RELEASE_VERSION "hwbase-2026-11-07"
#endif
#if defined(WSOFT_BOARD_CYD)
static const char *STATION_MODEL = "wsoft-cyd28";
#elif defined(WSOFT_BOARD_HEADLESS) && WSOFT_BOARD_HEADLESS
static const char *STATION_MODEL = "wsoft-s3-headless";
#else
static const char *STATION_MODEL = "wsoft-waveshare43";
#endif
static const char *RELEASE_VERSION = WSOFT_RELEASE_VERSION;
#endif
static const uint32_t HEARTBEAT_MS = 30000;
static const uint32_t RETRY_MS = 5000;
// While trade weights wait for the Hub, heartbeat sooner (the Hub allows 120 a minute per device).
static const uint32_t TRADE_QUEUE_MS = 2000;
// The Hub rejects a signature more than 300 s from its own clock; anything before this is "unset".
static const time_t CLOCK_VALID_AFTER = 1700000000;

static Preferences prefs;
static String wifi_ssid, wifi_pass, hub_url, key_id, ota_pass;
static uint8_t signing_key[32];
#ifdef WSOFT_ON_BASE
// Backup Hub addresses (Rhyno, 14 Nov 2026; first built 31 Oct on the Waveshare branch). The
// pairing code of a Hub with 2+ addresses lists them all (e.g. Wi-Fi and cable); the LAN ones are
// kept here, fixed until a restart. Same Hub, same key: only the address changes. The hub task
// moves to the next address when the Hub gives no answer at all (wsoft_hub_failover_result).
// Only the one-byte index changes at runtime, so the web task reads it safely.
static char hub_urls[WSOFT_HUB_URLS_MAX][WSOFT_HUB_URL_MAX];
static WsoftHubFailover failover;          // hub task only
static volatile uint8_t active_url = 0;    // copy of failover.active for the web task
#endif
static bool configured = false;
static volatile HubState state = HUB_SETUP;
static volatile bool ota_busy = false;

#ifdef WSOFT_ON_BASE
static volatile int last_status = 0;
static volatile uint32_t last_heartbeat_ms = 0;
// Set when the pairing changes (the board then restarts). The applied command id belongs to
// the old pairing: it is removed, and the Hub task stops writing it.
static volatile bool pairing_changed = false;

static void forget_commands_applied(Preferences &store) {
  pairing_changed = true;  // first, so a Hub task write after the remove below removes it too
  store.remove("cmd");
}

#endif
static portMUX_TYPE health_lock = portMUX_INITIALIZER_UNLOCKED;
static bool health_linked = false;
static bool health_alarm = false;
static char health_weight[24] = "";

// ---------- pairing code ----------

static bool base64_decode(String text, uint8_t *out, size_t capacity, size_t *length) {
  text.replace('-', '+');
  text.replace('_', '/');
  while (text.length() % 4) text += '=';
  return mbedtls_base64_decode(out, capacity, length, (const uint8_t *)text.c_str(), text.length()) == 0;
}

// The Hub emits compact JSON with plain string values, so a key lookup is enough here.
static String json_string(const String &json, const char *key) {
  String needle = String("\"") + key + "\":\"";
  int start = json.indexOf(needle);
  if (start < 0) return "";
  start += needle.length();
  int end = json.indexOf('"', start);
  return end < 0 ? "" : json.substring(start, end);
}

static bool load_pairing(const String &code) {
  static uint8_t decoded[512];
  size_t length = 0;
  if (!base64_decode(code, decoded, sizeof decoded - 1, &length)) return false;
  decoded[length] = 0;
  String json = (const char *)decoded;

  String url = json_string(json, "hub_url");
  String id = json_string(json, "key_id");
  uint8_t secret[48];
  size_t secret_length = 0;
  if (url.isEmpty() || id.isEmpty() ||
      !base64_decode(json_string(json, "secret"), secret, sizeof secret, &secret_length) || secret_length != 32) {
    return false;
  }
  while (url.endsWith("/")) url.remove(url.length() - 1);

  // The HMAC key is sha256(pairing secret); the Hub stores only that digest.
  mbedtls_sha256(secret, secret_length, signing_key, 0);
  memset(secret, 0, sizeof secret);
  hub_url = url;
  key_id = id;
#ifdef WSOFT_ON_BASE
  // The address list, read by the same host-tested parser the web page uses. Static: the parse
  // result is over 1 KB. Called once from hub_begin(), before the hub task exists.
  static WsoftHubCode parsed;
  if (wsoft_hub_code_parse(code.c_str(), &parsed) == WSOFT_HUB_CODE_OK) {
    memcpy(hub_urls, parsed.hub_urls, sizeof hub_urls);
    wsoft_hub_failover_begin(&failover, parsed.hub_url_count);
  } else {
    strlcpy(hub_urls[0], url.c_str(), sizeof hub_urls[0]);
    wsoft_hub_failover_begin(&failover, 1);
  }
  memset(parsed.secret, 0, sizeof parsed.secret);
  active_url = failover.active;
#endif
  return true;
}

// The Hub address in use now.
static String hub_base() {
#ifdef WSOFT_ON_BASE
  return String(hub_urls[failover.active]);
#else
  return hub_url;
#endif
}

// Every request to the Hub reports here. No answer at all (status <= 0) moves to the next
// backup address; any HTTP answer, even an error, means this address works.
static void record_hub_answer(int status) {
#ifdef WSOFT_ON_BASE
  uint8_t before = failover.active;
  if (wsoft_hub_failover_result(&failover, status)) {
    active_url = failover.active;
    WSOFT_LOG.printf("HUB no answer from %s, trying backup address %s\n", hub_urls[before], hub_urls[failover.active]);
  }
#else
  (void)status;
#endif
}

static void load_config() {
  prefs.begin("hub", true);
  wifi_ssid = prefs.getString("ssid", "");
  wifi_pass = prefs.getString("pass", "");
  ota_pass = prefs.getString("ota", "");
  String code = prefs.getString("pair", "");
  prefs.end();
#ifndef WSOFT_ON_BASE
  if (ota_pass.length() < 8) ota_pass = WAVESHARE_OTA_PASSWORD;
  configured = !wifi_ssid.isEmpty() && !code.isEmpty() && load_pairing(code);
#else
  // H3/H4 (Hardware.Base): the framework owns Wi-Fi and OTA, so only the pairing code is needed here.
  configured = !code.isEmpty() && load_pairing(code);
#endif
}

// ---------- USB provisioning ----------
// Lines: "HUB WIFI <ssid>", "HUB PASS <password>", "HUB PAIR <code>", "HUB SAVE", "HUB CLEAR".
// "APPROVAL ..." lines go to approval.cpp; this port is the USB cable only, never Wi-Fi.

static String pending_ssid, pending_pass, pending_pair, pending_ota;

void hub_handle_serial(Stream &port) {
  static String line;
  while (port.available()) {
    char ch = port.read();
    if (ch == '\r') continue;
    if (ch != '\n') {
      if (line.length() < 600) line += ch;
      continue;
    }
    String command = line;
    line = "";
#ifndef WSOFT_ON_BASE
    if (command.startsWith("HUB WIFI ")) {
      pending_ssid = command.substring(9);
      port.println("HUB_OK wifi name stored");
    } else if (command.startsWith("HUB PASS ")) {
      pending_pass = command.substring(9);
      port.println("HUB_OK wifi password stored");
    } else if (command.startsWith("HUB OTA ")) {
      pending_ota = command.substring(8);
      port.println(pending_ota.length() >= 8 ? "HUB_OK ota password stored" : "HUB_ERR ota password too short");
#else
    // H7 (Hardware.Base): Wi-Fi and OTA are set in the web interface. Nothing is stored or echoed.
    if (command.startsWith("HUB WIFI ") || command.startsWith("HUB PASS ") || command.startsWith("HUB OTA ")) {
      port.println("HUB_ERR not used on this build: set Wi-Fi and OTA in the web interface");
#endif
    } else if (command.startsWith("HUB PAIR ")) {
      pending_pair = command.substring(9);
      pending_pair.trim();
      port.println(load_pairing(pending_pair) ? "HUB_OK pairing code valid" : "HUB_ERR pairing code invalid");
    } else if (command == "HUB SAVE") {
      prefs.begin("hub", false);
      if (!pending_ssid.isEmpty()) prefs.putString("ssid", pending_ssid);
      if (!pending_pass.isEmpty()) prefs.putString("pass", pending_pass);
      if (!pending_pair.isEmpty()) prefs.putString("pair", pending_pair);
#ifdef WSOFT_ON_BASE
      if (!pending_pair.isEmpty()) forget_commands_applied(prefs);
#endif
      if (pending_ota.length() >= 8) prefs.putString("ota", pending_ota);
      prefs.end();
      pending_pass = "";
      pending_ota = "";
      port.println("HUB_OK saved, restarting");
      port.flush();
      delay(200);
      ESP.restart();
    } else if (command.startsWith("APPROVAL")) {
      approval_handle_command(command, port);
    } else if (command.startsWith("SCREEN")) {
      // SCREEN CRC / SHOT / TAP: checking the screen without standing at the board.
      ui_handle_command(command, port);
    } else if (command == "HUB CLEAR") {
      prefs.begin("hub", false);
      prefs.clear();
      prefs.end();
      port.println("HUB_OK cleared, restarting");
      port.flush();
      delay(200);
      ESP.restart();
    }
  }
}

// ---------- signed heartbeat ----------

static String hex(const uint8_t *data, size_t length) {
  static const char digits[] = "0123456789abcdef";
  String out;
  out.reserve(length * 2);
  for (size_t i = 0; i < length; i++) {
    out += digits[data[i] >> 4];
    out += digits[data[i] & 0x0F];
  }
  return out;
}

static String sign(const String &timestamp, const String &nonce, const String &body) {
  uint8_t mac[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, signing_key, sizeof signing_key);
  String head = String(SIGNING_PREFIX) + "\n" + key_id + "\n" + timestamp + "\n" + nonce + "\n";
  mbedtls_md_hmac_update(&ctx, (const uint8_t *)head.c_str(), head.length());
  mbedtls_md_hmac_update(&ctx, (const uint8_t *)body.c_str(), body.length());
  mbedtls_md_hmac_finish(&ctx, mac);
  mbedtls_md_free(&ctx);
  return "sha256=" + hex(mac, sizeof mac);
}

static String heartbeat_body(time_t now, uint32_t *trades_through) {
  char sent_at[32];
  struct tm utc;
  gmtime_r(&now, &utc);
  strftime(sent_at, sizeof sent_at, "%Y-%m-%dT%H:%M:%SZ", &utc);

  bool linked, alarm;
  char weight[24];
  portENTER_CRITICAL(&health_lock);
  linked = health_linked;
  alarm = health_alarm;
  memcpy(weight, health_weight, sizeof weight);
  portEXIT_CRITICAL(&health_lock);

  String body = "{\"sent_at\":\"";
  body += sent_at;
  body += "\",\"station_model\":\"";
  body += STATION_MODEL;
  body += "\",\"release_version\":\"";
  body += RELEASE_VERSION;
  body += "\",\"installation_uuid\":\"";
  body += WiFi.macAddress();
  body += "\",\"health\":{\"djb_link\":\"";
  body += linked ? "up" : "down";
  body += "\",\"djb_weight\":\"";
  body += weight;
  body += "\",\"djb_alarm\":";
  body += alarm ? "true" : "false";
  body += ",\"wifi_rssi\":";
  body += WiFi.RSSI();
  body += ",\"wifi_ip\":\"";
  body += WiFi.localIP().toString();
  body += "\"";
  body += approval_health_fields(trades_through);
#ifdef WSOFT_ON_BASE
  body += switches_health_fields();
#endif
  body += "}}";
  return body;
}

#ifdef WSOFT_ON_BASE
// ---------- weighings (Hardware.Base) ----------
// After each accepted heartbeat, the oldest saved weighings go to POST /api/v1/ingest/weighings,
// signed like the heartbeat. 201 = stored, 200 = the Hub already had it: both remove it here.
// A 400 means the Hub will never take it (checked before saving, so it should not happen):
// it is logged and dropped so the ones behind it are not stuck. Anything else: try again later.
static const int WEIGHINGS_PER_HEARTBEAT = 8;

static int post_signed(const char *path, const String &body, String *reply) {
  time_t now = time(nullptr);
  uint8_t nonce_bytes[16];
  esp_fill_random(nonce_bytes, sizeof nonce_bytes);
  String nonce = hex(nonce_bytes, sizeof nonce_bytes);
  String timestamp = String((unsigned long)now);
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(hub_base() + path)) {
    record_hub_answer(0);
    return -1;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-ScaleModels-Key-Id", key_id);
  http.addHeader("X-ScaleModels-Timestamp", timestamp);
  http.addHeader("X-ScaleModels-Nonce", nonce);
  http.addHeader("X-ScaleModels-Signature", sign(timestamp, nonce, body));
  int status = http.POST(body);
  if (reply) *reply = status > 0 ? http.getString() : http.errorToString(status);
  http.end();
  record_hub_answer(status);
  return status;
}

static void send_weighings() {
  String tag = WiFi.macAddress();
  tag.replace(":", "");
  WsoftWeighingMeta meta = {tag.c_str(), weighings_source(), STATION_MODEL, false, false, 0, nullptr};
  for (int i = 0; i < WEIGHINGS_PER_HEARTBEAT; i++) {
    TradeWeight w;
    bool for_trade = false;
    int32_t tare_value = 0;
    WsoftHiloStamp hilo;
    bool alarm_override = false;
    if (!weighings_oldest(&w, &for_trade, &tare_value, &hilo, &alarm_override)) return;
    meta.alarm_test_override = alarm_override;
    meta.for_trade = for_trade;
    meta.tared = tare_value > 0;
    meta.tare_value = tare_value;
    meta.hilo = &hilo;
    char body[900];
    if (!wsoft_weighing_json(w, meta, body, sizeof body)) {
      WSOFT_LOG.printf("WEIGH seq=%lu cannot be sent (weight or time out of range): dropped\n", (unsigned long)w.seq);
      weighings_delivered(w.seq);
      continue;
    }
    String reply;
    int status = post_signed("/api/v1/ingest/weighings", String(body), &reply);
    WSOFT_LOG.printf("WEIGH sent seq=%lu status=%d %s\n", (unsigned long)w.seq, status, reply.substring(0, 80).c_str());
    if (status == 201 || status == 200) {
      weighings_delivered(w.seq);
    } else if (status == 400) {
      weighings_delivered(w.seq);  // refused for good; logged above
    } else {
      return;  // offline, rate-limited or rejected key: keep it and try after the next heartbeat
    }
  }
}

// ---------- switch commands from the Hub (Hardware.Base, contract B1/B2) ----------
// Every 10 s the board posts what it has handled to POST /api/v1/ingest/commands. The Hub
// answers with the commands still waiting, signed under its own prefix with this request's
// key id and nonce. Any answer that does not check out is dropped. Each command goes through
// switches_set(where=hub), the same path as the screen and the browser, and its result is
// sent with the next pull. The highest handled id is kept in NVS ("hub", key "cmd").
static const uint32_t COMMANDS_MS = 10000;
static const uint32_t COMMAND_APPLY_TIMEOUT_MS = 3000;
static WsoftCmdResult command_results[WSOFT_CMD_RESULTS_MAX];
static size_t command_result_count = 0;
static uint64_t commands_applied = 0;

static uint64_t load_commands_applied() {
  Preferences store;  // a local handle: the USB lines use prefs on the screen task
  store.begin("hub", true);
  uint64_t value = store.getULong64("cmd", 0);
  store.end();
  return value;
}

static void save_commands_applied(uint64_t value) {
  if (pairing_changed) return;
  Preferences store;
  store.begin("hub", false);
  store.putULong64("cmd", value);
  // The pairing changed while this was written: the id is not for the new pairing.
  if (pairing_changed) store.remove("cmd");
  store.end();
}

static void keep_result(uint64_t id, WsoftSwitchError error) {
  if (command_result_count == WSOFT_CMD_RESULTS_MAX) {
    memmove(command_results, command_results + 1, sizeof command_results - sizeof command_results[0]);
    command_result_count--;
  }
  WsoftCmdResult &r = command_results[command_result_count++];
  r.id = id;
  r.ok = error == WSOFT_SWITCH_OK;
  strlcpy(r.error, r.ok ? "" : wsoft_switch_error_code(error), sizeof r.error);
}

// Posts a signed body and keeps the answer only when the Hub signed it under its own prefix
// with this request's key id and nonce (contract B2 / C1). False for anything else; what is
// logged names the pull (what).
static bool pull_signed(const char *path, const char *request, const char *prefix, const char *what, String *reply) {
  String body = request;
  time_t now = time(nullptr);
  uint8_t nonce_bytes[16];
  esp_fill_random(nonce_bytes, sizeof nonce_bytes);
  String nonce = hex(nonce_bytes, sizeof nonce_bytes);
  String timestamp = String((unsigned long)now);

  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(hub_base() + path)) {
    record_hub_answer(0);
    return false;
  }
  const char *answer_headers[] = {"X-ScaleModels-Key-Id", "X-ScaleModels-Timestamp", "X-ScaleModels-Nonce",
                                  "X-ScaleModels-Signature"};
  http.collectHeaders(answer_headers, 4);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-ScaleModels-Key-Id", key_id);
  http.addHeader("X-ScaleModels-Timestamp", timestamp);
  http.addHeader("X-ScaleModels-Nonce", nonce);
  http.addHeader("X-ScaleModels-Signature", sign(timestamp, nonce, body));
  int status = http.POST(body);
  record_hub_answer(status);
  *reply = status > 0 ? http.getString() : "";
  String answer_key = http.header("X-ScaleModels-Key-Id");
  String answer_time = http.header("X-ScaleModels-Timestamp");
  String answer_nonce = http.header("X-ScaleModels-Nonce");
  String answer_signature = http.header("X-ScaleModels-Signature");
  http.end();

  if (status != 200) {
    WSOFT_LOG.printf("HUB %s status=%d\n", what, status);
    return false;
  }
  if (!wsoft_hub_verify_answer(signing_key, prefix, key_id.c_str(), nonce.c_str(), answer_key.c_str(),
                               answer_time.c_str(), answer_nonce.c_str(), answer_signature.c_str(), reply->c_str(),
                               reply->length())) {
    WSOFT_LOG.printf("HUB %s bad signature\n", what);
    return false;
  }
  return true;
}

static void pull_commands() {
  static char request[WSOFT_CMD_REQUEST_MAX];
  if (!wsoft_hub_commands_request(commands_applied, command_results, command_result_count, request, sizeof request)) {
    return;
  }
  String reply;
  if (!pull_signed("/api/v1/ingest/commands", request, WSOFT_HUB_COMMAND_PREFIX, "commands", &reply)) return;
  // The Hub has the results it was sent.
  command_result_count = 0;

  static WsoftHubCommand commands[WSOFT_CMD_PER_PULL];
  size_t count = 0;
  if (!wsoft_hub_commands_parse(reply.c_str(), reply.length(), commands, &count)) {
    WSOFT_LOG.println("HUB commands bad body");
    return;
  }
  // Oldest first (the Hub sends them that way; sorted here in case it ever does not).
  for (size_t i = 1; i < count; i++) {
    for (size_t j = i; j > 0 && commands[j].id < commands[j - 1].id; j--) {
      WsoftHubCommand t = commands[j];
      commands[j] = commands[j - 1];
      commands[j - 1] = t;
    }
  }
  for (size_t i = 0; i < count; i++) {
    const WsoftHubCommand &c = commands[i];
    if (c.id <= commands_applied) continue;
    WsoftSwitchError error = switches_set(c.which, c.on, c.by, WSOFT_WHERE_HUB, c.reason, COMMAND_APPLY_TIMEOUT_MS);
    // BUSY: not applied, asked again at the next pull. UNCONFIRMED counts as handled (it may
    // have been applied); the heartbeat that follows carries the real state.
    if (error == WSOFT_SWITCH_BUSY) return;
    keep_result(c.id, error);
    commands_applied = c.id;
    save_commands_applied(commands_applied);
    WSOFT_LOG.printf("HUB command id=%llu %s %s: %s\n", (unsigned long long)c.id, wsoft_switch_name(c.which),
                   c.on ? "ON" : "OFF", error == WSOFT_SWITCH_OK ? "done" : wsoft_switch_error_code(error));
  }
}

// ---------- Hi-Lo products from the Hub (Hardware.Base, Hub ADR 0008, contract C1) ----------
// At most once a minute (sooner after the operator picks a product) the board posts the
// product it uses to POST /api/v1/ingest/hilo. The Hub answers with the products that have
// Hi-Lo limits and this scale's default, signed under scale-models-hub-hilo-v1. An answer that
// does not check out changes nothing. hilo.cpp keeps the list and the pick in flash.
static void pull_hilo() {
  char request[WSOFT_HILO_REQUEST_MAX];
  if (!hilo_request(request, sizeof request)) {
    hilo_failed();
    return;
  }
  String reply;
  if (!pull_signed("/api/v1/ingest/hilo", request, WSOFT_HUB_HILO_PREFIX, "hilo", &reply)) {
    hilo_failed();
    return;
  }
  static WsoftHiloList list;  // static: too big for the Hub task's stack
  if (!wsoft_hilo_parse(reply.c_str(), reply.length(), &list)) {
    WSOFT_LOG.println("HUB hilo bad body");
    hilo_failed();
    return;
  }
  hilo_take(list);
}

#endif
static void send_heartbeat() {
  time_t now = time(nullptr);
  uint8_t nonce_bytes[16];
  esp_fill_random(nonce_bytes, sizeof nonce_bytes);
  String nonce = hex(nonce_bytes, sizeof nonce_bytes);
  String timestamp = String((unsigned long)now);
  uint32_t trades_through = 0;
  String body = heartbeat_body(now, &trades_through);

  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(hub_base() + "/api/v1/ingest/heartbeats")) {
    state = HUB_UNREACHABLE;
    record_hub_answer(0);
    return;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-ScaleModels-Key-Id", key_id);
  http.addHeader("X-ScaleModels-Timestamp", timestamp);
  http.addHeader("X-ScaleModels-Nonce", nonce);
  http.addHeader("X-ScaleModels-Signature", sign(timestamp, nonce, body));
  int status = http.POST(body);
  String reply = status > 0 ? http.getString() : http.errorToString(status);
  http.end();
  record_hub_answer(status);

#ifdef WSOFT_ON_BASE
  last_status = status;
  last_heartbeat_ms = millis();
#endif
  if (status == 202) {
    state = HUB_ONLINE;
    if (trades_through > 0) approval_delivered(trades_through);
  } else if (status == 401 || status == 400 || status == 409) {
    state = HUB_REJECTED;
  } else {
    state = HUB_UNREACHABLE;
  }
  WSOFT_LOG.printf("HUB heartbeat status=%d %s\n", status, reply.substring(0, 120).c_str());
}

static void hub_task(void *) {
#ifndef WSOFT_ON_BASE
  WiFi.onEvent(
      [](WiFiEvent_t, WiFiEventInfo_t info) {
        WSOFT_LOG.printf("HUB wifi refused reason=%d\n", info.wifi_sta_disconnected.reason);
      },
      ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  configTime(0, 0, "pool.ntp.org", "time.google.com");
#endif

  uint32_t next = 0;
  uint32_t next_report = 0;
#ifdef WSOFT_ON_BASE
  commands_applied = load_commands_applied();
  uint32_t seen_switch_changes = switches_change_count();
  uint32_t next_command_pull = 0;
#endif
  for (;;) {
    if ((int32_t)(millis() - next_report) >= 0) {
      next_report = millis() + 10000;
      WSOFT_LOG.printf("HUB state=%s wifi_status=%d ip=%s clock=%ld\n", hub_state_text(state), (int)WiFi.status(),
                     WiFi.localIP().toString().c_str(), (long)time(nullptr));
#ifndef WSOFT_ON_BASE
      if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect();
        int found = WiFi.scanNetworks();
        bool seen = false;
        for (int i = 0; i < found; i++) {
          if (WiFi.SSID(i) == wifi_ssid) {
            seen = true;
            WSOFT_LOG.printf("HUB wifi seen rssi=%d channel=%d\n", WiFi.RSSI(i), WiFi.channel(i));
          }
        }
        if (!seen) WSOFT_LOG.printf("HUB wifi not seen among %d networks (needs 2.4 GHz)\n", found);
        WiFi.scanDelete();
        WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
      }
#endif
    }
    if (WiFi.status() != WL_CONNECTED) {
      state = HUB_NO_WIFI;
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    if (time(nullptr) < CLOCK_VALID_AFTER) {
      state = HUB_NO_CLOCK;
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
#ifdef WSOFT_ON_BASE
    // A switch changed (from any place): tell the Hub now, not at the next heartbeat.
    uint32_t switch_changes = switches_change_count();
    if (switch_changes != seen_switch_changes) {
      seen_switch_changes = switch_changes;
      next = millis();
    }
#endif
    // A heartbeat uses the same network stack as a firmware upload. Leave it until the upload finishes.
    if ((int32_t)(millis() - next) >= 0 && !ota_busy) {
      send_heartbeat();
      uint32_t wait = state != HUB_ONLINE ? RETRY_MS : approval_pending() > 0 ? TRADE_QUEUE_MS : HEARTBEAT_MS;
      next = millis() + wait;
    }
#ifdef WSOFT_ON_BASE
    // A weighing saved between heartbeats goes out within a few seconds, not at the next one.
    static uint32_t next_weighing_try = 0;
    if (state == HUB_ONLINE && !ota_busy && !pairing_changed && (int32_t)(millis() - next_command_pull) >= 0) {
      pull_commands();
      next_command_pull = millis() + COMMANDS_MS;
    }
    if (state == HUB_ONLINE && !ota_busy && !pairing_changed && hilo_pull_due()) pull_hilo();
    TradeWeight waiting;
    bool waiting_for_trade;
    int32_t waiting_tare;
    if (state == HUB_ONLINE && !ota_busy && (int32_t)(millis() - next_weighing_try) >= 0 &&
        weighings_oldest(&waiting, &waiting_for_trade, &waiting_tare, nullptr)) {
      send_weighings();
      next_weighing_try = millis() + RETRY_MS;
    }
#endif
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

// ---------- Wi-Fi firmware updates ----------
// The password comes from NVS, or from the build when NVS has none yet.
// ArduinoOTA.handle() has to run often, so the weigh loop skips the scale while an update is coming in.

static bool ota_started = false;

void hub_ota_poll() {
#ifndef WSOFT_ON_BASE
  if (WiFi.status() != WL_CONNECTED || ota_pass.length() < 8) return;
  if (!ota_started) {
    ArduinoOTA.setHostname("waveshare-bench");
    ArduinoOTA.setPassword(ota_pass.c_str());
    ArduinoOTA.setTimeout(10000);
    WiFi.setSleep(false);
    ArduinoOTA.onStart([]() { ota_busy = true; WSOFT_LOG.println("HUB ota start"); });
    ArduinoOTA.onEnd([]() { WSOFT_LOG.println("HUB ota end"); });
    ArduinoOTA.onError([](ota_error_t error) {
      ota_busy = false;
      WSOFT_LOG.printf("HUB ota error %u\n", (unsigned)error);
    });
    ArduinoOTA.begin();
    ota_started = true;
    WSOFT_LOG.printf("HUB ota ready ip=%s\n", WiFi.localIP().toString().c_str());
  }
  ArduinoOTA.handle();
#endif
}

bool hub_ota_busy() {
  return ota_busy;
}

// ---------- public ----------

void hub_begin() {
  load_config();
  if (!configured) {
    state = HUB_SETUP;
#ifndef WSOFT_ON_BASE
    WSOFT_LOG.println("HUB not configured: send HUB WIFI / HUB PASS / HUB PAIR / HUB SAVE over USB");
#else
    WSOFT_LOG.println("HUB not configured: paste the pairing key on the web page W-Soft > Hub, or send HUB PAIR / HUB SAVE over USB");
#endif
    return;
  }
#ifdef WSOFT_ON_BASE
  WSOFT_LOG.printf("HUB configured for %s (%u address%s) key %s\n", hub_urls[0], (unsigned)failover.count,
                   failover.count == 1 ? "" : "es", key_id.c_str());
#else
  WSOFT_LOG.printf("HUB configured for %s key %s\n", hub_url.c_str(), key_id.c_str());
#endif
  xTaskCreatePinnedToCore(hub_task, "hub", 8192, nullptr, 1, nullptr, 0);
}

void hub_set_health(bool linked, const char *weight, bool alarm) {
  portENTER_CRITICAL(&health_lock);
  health_linked = linked;
  health_alarm = alarm;
  strlcpy(health_weight, weight, sizeof health_weight);
  portEXIT_CRITICAL(&health_lock);
}

HubState hub_state() {
  return state;
}

const char *hub_state_text(HubState s) {
  switch (s) {
    case HUB_ONLINE: return "HUB ONLINE";
    case HUB_NO_WIFI: return "HUB NO WIFI";
    case HUB_NO_CLOCK: return "HUB CLOCK";
    case HUB_REJECTED: return "HUB REJECTED";
    case HUB_UNREACHABLE: return "HUB OFFLINE";
    default: return "HUB SETUP";
  }
}
#ifdef WSOFT_ON_BASE

// ---------- web pairing (Hardware.Base) ----------
// A local Preferences handle: the web server runs in its own task, apart from the USB lines.

WsoftHubCodeError hub_web_save(const char *code) {
  static WsoftHubCode parsed;  // over 1 KB: off the web task's stack (only that task calls this)
  WsoftHubCodeError error = wsoft_hub_code_parse(code, &parsed);
  memset(parsed.secret, 0, sizeof parsed.secret);
  if (error != WSOFT_HUB_CODE_OK) return error;
  String trimmed = code;
  trimmed.trim();
  Preferences store;
  store.begin("hub", false);
  store.putString("pair", trimmed);
  forget_commands_applied(store);
  store.end();
  WSOFT_LOG.printf("HUB pairing saved from the web page for %s key %s\n", parsed.hub_url, parsed.key_id);
  return WSOFT_HUB_CODE_OK;
}

void hub_web_forget() {
  Preferences store;
  store.begin("hub", false);
  store.remove("pair");
  forget_commands_applied(store);
  store.end();
  WSOFT_LOG.println("HUB pairing forgotten from the web page");
}

void hub_web_info(HubWebInfo *out) {
  memset(out, 0, sizeof *out);
  Preferences store;
  store.begin("hub", true);
  String code = store.getString("pair", "");
  store.end();
  static WsoftHubCode parsed;  // over 1 KB: off the web task's stack (only that task calls this)
  if (!code.isEmpty() && wsoft_hub_code_parse(code.c_str(), &parsed) == WSOFT_HUB_CODE_OK) {
    out->paired = true;
    strlcpy(out->hub_url, parsed.hub_url, sizeof out->hub_url);
    out->url_count = parsed.hub_url_count;
    for (uint8_t i = 0; i < parsed.hub_url_count && i < WSOFT_HUB_URLS_MAX; i++) {
      strlcpy(out->hub_urls[i], parsed.hub_urls[i], sizeof out->hub_urls[i]);
    }
    // The address in use, when the saved code is the one the Hub task runs on (it is, until a
    // restart after a new code).
    uint8_t in_use = active_url;
    out->active = in_use < out->url_count ? in_use : 0;
    strlcpy(out->key_id, parsed.key_id, sizeof out->key_id);
    strlcpy(out->device_id, parsed.device_id, sizeof out->device_id);
    memset(parsed.secret, 0, sizeof parsed.secret);
  }
  out->state = state;
  out->last_status = last_status;
  out->last_age_s = last_heartbeat_ms == 0 ? 0 : (millis() - last_heartbeat_ms) / 1000;
}
#endif
