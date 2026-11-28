#include "wsoft_log.h"
#include "hub.h"
#include "approval.h"
#include "wsoft_ui.h"
#ifdef WSOFT_ON_BASE
#include "hilo.h"
#include "loadcell.h"
#include "hub_web.h"
#include "packers.h"
#include "switches.h"
#include "weighings.h"
#include "wifi_update.h"

#include <wsoft_hub_commands.h>
#include <wsoft_hub_screen.h>
#include <wsoft_packers.h>
#include <wsoft_ui_model.h>
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
#include <sys/time.h>
#include <time.h>
#include <wsoft_hub_code.h>
#include <wsoft_http_date.h>

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
#elif defined(WSOFT_BOARD_S3_35) && WSOFT_BOARD_S3_35
static const char *STATION_MODEL = "wsoft-s3-35";
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
static SemaphoreHandle_t config_mutex = nullptr;
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
static volatile bool configured = false;
static volatile HubState state = HUB_SETUP;
static volatile bool ota_busy = false;
// The clock was set from the Hub's Date header (no internet time on this network).
static volatile bool clock_from_hub = false;

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

struct HubConfigSnapshot {
  bool configured = false;
  char base_url[WSOFT_HUB_URL_MAX] = "";
  char key_id[64] = "";
  uint8_t signing_key[32] = {};
};

static void lock_config() {
  if (config_mutex != nullptr) xSemaphoreTake(config_mutex, portMAX_DELAY);
}

static void unlock_config() {
  if (config_mutex != nullptr) xSemaphoreGive(config_mutex);
}

static void snap_config(HubConfigSnapshot *out) {
  memset(out, 0, sizeof *out);
  lock_config();
  out->configured = configured;
#ifdef WSOFT_ON_BASE
  strlcpy(out->base_url, hub_urls[failover.active], sizeof out->base_url);
#else
  strlcpy(out->base_url, hub_url.c_str(), sizeof out->base_url);
#endif
  strlcpy(out->key_id, key_id.c_str(), sizeof out->key_id);
  memcpy(out->signing_key, signing_key, sizeof out->signing_key);
  unlock_config();
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

  uint8_t next_key[32];
  // The HMAC key is sha256(pairing secret); the Hub stores only that digest.
  mbedtls_sha256(secret, secret_length, next_key, 0);
  memset(secret, 0, sizeof secret);

  lock_config();
  memcpy(signing_key, next_key, sizeof signing_key);
  memset(next_key, 0, sizeof next_key);
  hub_url = url;
  key_id = id;
#ifdef WSOFT_ON_BASE
  // The address list, read by the same host-tested parser the web page uses. Static: the parse
  // result is over 1 KB.
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
  unlock_config();
  return true;
}

// Every request to the Hub reports here. No answer at all (status <= 0) moves to the next
// backup address; any HTTP answer, even an error, means this address works.
static void record_hub_answer(int status) {
#ifdef WSOFT_ON_BASE
  char from_url[WSOFT_HUB_URL_MAX] = "";
  char to_url[WSOFT_HUB_URL_MAX] = "";
  bool switched = false;
  lock_config();
  uint8_t before = failover.active;
  if (wsoft_hub_failover_result(&failover, status)) {
    active_url = failover.active;
    strlcpy(from_url, hub_urls[before], sizeof from_url);
    strlcpy(to_url, hub_urls[failover.active], sizeof to_url);
    switched = true;
  }
  unlock_config();
  if (switched) {
    WSOFT_LOG.printf("HUB no answer from %s, trying backup address %s\n", from_url, to_url);
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
      // Room for the longest command word ("HUB PAIR ") plus a full WSOFT_HUB_CODE_MAX pairing
      // code; at 600 a maximum-length code lost its end (wsoft-waveshare43 5a1515d).
      if (line.length() < WSOFT_HUB_CODE_MAX + 16) line += ch;
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
    } else if (command.startsWith("LOADCELL")) {
      // LOADCELL INFO / SET: the load cell's setup, read and changed over the cable.
      loadcell_handle_command(command, port, approval_on());
    } else if (command.startsWith("SCREEN")) {
      // SCREEN CRC / SHOT / TAP: checking the screen without standing at the board.
      ui_handle_command(command, port);
#ifdef WSOFT_ON_BASE
    } else if (command == "PACKER LIST") {
      packers_print_list(port);
    } else if (command.startsWith("PACKER IN ")) {
      String rest = command.substring(10);
      rest.trim();
      int sp = rest.indexOf(' ');
      if (sp <= 0) {
        port.println("PACKER_IN ok=0 code=");
      } else {
        String code = rest.substring(0, sp);
        String pin = rest.substring(sp + 1);
        code.trim();
        pin.trim();
        WsoftPackerAuthReply reply;
        if (hub_packer_auth(code.c_str(), pin.c_str(), "sign_in", &reply)) {
          if (reply.ok) packers_apply_auth(reply);
          port.printf("PACKER_IN ok=%d code=%s\n", reply.ok ? 1 : 0, code.c_str());
        } else {
          uint32_t locked_s = 0;
          WsoftPackerPinResult r = packers_sign_in_offline(code.c_str(), pin.c_str(), millis(), &locked_s);
          port.printf("PACKER_IN ok=%d code=%s offline=1\n", r == WSOFT_PACKER_PIN_OK ? 1 : 0, code.c_str());
        }
      }
    } else if (command.startsWith("PACKER USE ")) {
      String code = command.substring(11);
      code.trim();
      bool ok = packers_select(code.c_str(), millis());
      port.printf("PACKER_USE ok=%d code=%s\n", ok ? 1 : 0, code.c_str());
    } else if (command.startsWith("PACKER OUT ")) {
      String code = command.substring(11);
      code.trim();
      packers_sign_out(code.c_str());
      hub_packer_auth_start(code.c_str(), nullptr, "sign_out", true);
      port.printf("PACKER_OUT ok=1 code=%s\n", code.c_str());
#endif
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

static String sign_with(const HubConfigSnapshot &cfg, const String &timestamp, const String &nonce, const String &body) {
  uint8_t mac[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, cfg.signing_key, sizeof cfg.signing_key);
  String head = String(SIGNING_PREFIX) + "\n" + cfg.key_id + "\n" + timestamp + "\n" + nonce + "\n";
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
  HubConfigSnapshot cfg;
  snap_config(&cfg);
  time_t now = time(nullptr);
  uint8_t nonce_bytes[16];
  esp_fill_random(nonce_bytes, sizeof nonce_bytes);
  String nonce = hex(nonce_bytes, sizeof nonce_bytes);
  String timestamp = String((unsigned long)now);
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(String(cfg.base_url) + path)) {
    memset(cfg.signing_key, 0, sizeof cfg.signing_key);
    record_hub_answer(0);
    return -1;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-ScaleModels-Key-Id", cfg.key_id);
  http.addHeader("X-ScaleModels-Timestamp", timestamp);
  http.addHeader("X-ScaleModels-Nonce", nonce);
  http.addHeader("X-ScaleModels-Signature", sign_with(cfg, timestamp, nonce, body));
  memset(cfg.signing_key, 0, sizeof cfg.signing_key);
  int status = http.POST(body);
  if (reply) *reply = status > 0 ? http.getString() : http.errorToString(status);
  http.end();
  record_hub_answer(status);
  return status;
}

static void send_weighings() {
  String tag = WiFi.macAddress();
  tag.replace(":", "");
  WsoftWeighingMeta meta = {tag.c_str(), weighings_source(), STATION_MODEL, false, false, 0, nullptr, false, nullptr,
                            nullptr, nullptr};
  for (int i = 0; i < WEIGHINGS_PER_HEARTBEAT; i++) {
    TradeWeight w;
    bool for_trade = false;
    int32_t tare_value = 0;
    WsoftHiloStamp hilo;
    bool alarm_override = false;
    char packer_code[WSOFT_PACKER_CODE_MAX + 1] = "";
    char packer_name[WSOFT_PACKER_NAME_MAX + 1] = "";
    char bag_batch_id[WSOFT_PACKER_BATCH_ID_MAX + 1] = "";
    if (!weighings_oldest(&w, &for_trade, &tare_value, &hilo, &alarm_override, packer_code, sizeof packer_code,
                          packer_name, sizeof packer_name, bag_batch_id, sizeof bag_batch_id)) {
      return;
    }
    meta.alarm_test_override = alarm_override;
    meta.for_trade = for_trade;
    meta.tared = tare_value > 0;
    meta.tare_value = tare_value;
    meta.hilo = &hilo;
    meta.packer_id = packer_code[0] ? packer_code : nullptr;
    meta.packer_name = packer_name[0] ? packer_name : nullptr;
    meta.bag_batch_id = bag_batch_id[0] ? bag_batch_id : nullptr;
    char body[1024];
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

// Hub task scratch buffers on the heap, taken once at boot while the heap is whole: static DRAM
// is full on the yellow board (ESP32, no PSRAM) since the packer work (28 Nov 2026).
static WsoftHiloList &hub_hilo_buf = *new WsoftHiloList();
static WsoftPackerRoster &hub_packers_buf = *new WsoftPackerRoster();
static char *const hub_bag_request_buf = new char[WSOFT_BAG_EVENTS_REQUEST_MAX];
static WsoftBagEventResult *const hub_bag_results_buf = new WsoftBagEventResult[WSOFT_BAG_OUTBOX_MAX];
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
  HubConfigSnapshot cfg;
  snap_config(&cfg);
  String body = request;
  time_t now = time(nullptr);
  uint8_t nonce_bytes[16];
  esp_fill_random(nonce_bytes, sizeof nonce_bytes);
  String nonce = hex(nonce_bytes, sizeof nonce_bytes);
  String timestamp = String((unsigned long)now);

  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(String(cfg.base_url) + path)) {
    memset(cfg.signing_key, 0, sizeof cfg.signing_key);
    record_hub_answer(0);
    return false;
  }
  const char *answer_headers[] = {"X-ScaleModels-Key-Id", "X-ScaleModels-Timestamp", "X-ScaleModels-Nonce",
                                  "X-ScaleModels-Signature"};
  http.collectHeaders(answer_headers, 4);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-ScaleModels-Key-Id", cfg.key_id);
  http.addHeader("X-ScaleModels-Timestamp", timestamp);
  http.addHeader("X-ScaleModels-Nonce", nonce);
  http.addHeader("X-ScaleModels-Signature", sign_with(cfg, timestamp, nonce, body));
  int status = http.POST(body);
  record_hub_answer(status);
  *reply = status > 0 ? http.getString() : "";
  String answer_key = http.header("X-ScaleModels-Key-Id");
  String answer_time = http.header("X-ScaleModels-Timestamp");
  String answer_nonce = http.header("X-ScaleModels-Nonce");
  String answer_signature = http.header("X-ScaleModels-Signature");
  http.end();

  if (status != 200) {
    memset(cfg.signing_key, 0, sizeof cfg.signing_key);
    WSOFT_LOG.printf("HUB %s status=%d\n", what, status);
    return false;
  }
  bool sig_ok = wsoft_hub_verify_answer(cfg.signing_key, prefix, cfg.key_id, nonce.c_str(), answer_key.c_str(),
                                        answer_time.c_str(), answer_nonce.c_str(), answer_signature.c_str(),
                                        reply->c_str(), reply->length());
  memset(cfg.signing_key, 0, sizeof cfg.signing_key);
  if (!sig_ok) {
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
  WsoftHiloList &list = hub_hilo_buf;  // too big for the Hub task's stack
  if (!wsoft_hilo_parse(reply.c_str(), reply.length(), &list)) {
    WSOFT_LOG.println("HUB hilo bad body");
    hilo_failed();
    return;
  }
  hilo_take(list);
}

// ---------- Packers & bag ledger sync (Hardware.Base, Hub ADR 0011, Sprint 2) ----------
static void pull_packers() {
  static char request[WSOFT_PACKERS_REQUEST_MAX];
  if (!packers_request(request, sizeof request)) {
    packers_failed();
    return;
  }
  String reply;
  if (!pull_signed("/api/v1/ingest/packers", request, WSOFT_HUB_PACKERS_PREFIX, "packers", &reply)) {
    packers_failed();
    return;
  }
  // Too big for the Hub task's stack; lives on the heap (see hub_packers_buf).
  WsoftPackerRoster &roster = hub_packers_buf;
  if (!wsoft_packers_parse(reply.c_str(), reply.length(), &roster)) {
    WSOFT_LOG.println("HUB packers bad body");
    packers_failed();
    return;
  }
  packers_take(roster);
}

static void push_bag_events() {
  char *request = hub_bag_request_buf;
  size_t count = 0;
  if (!packers_bag_events_request(request, WSOFT_BAG_EVENTS_REQUEST_MAX, &count) || count == 0) return;
  String reply;
  if (!pull_signed("/api/v1/ingest/bag-events", request, WSOFT_HUB_BAG_EVENTS_PREFIX, "bag-events", &reply)) {
    packers_bag_events_failed();
    return;
  }
  WsoftBagEventResult *results = hub_bag_results_buf;
  size_t result_count = 0;
  if (!wsoft_bag_events_parse(reply.c_str(), reply.length(), results, WSOFT_BAG_OUTBOX_MAX, &result_count)) {
    WSOFT_LOG.println("HUB bag-events bad body");
    packers_bag_events_failed();
    return;
  }
  packers_bag_events_take(results, result_count);
}

static TaskHandle_t hub_task_handle = nullptr;

struct PackerAuthJob {
  uint32_t job_id = 0;
  uint32_t seq = 0;
  HubPackerAuthState state = HUB_PACKER_AUTH_IDLE;
  bool fire_and_forget = false;
  char code[WSOFT_PACKER_CODE_MAX + 1] = "";
  char pin[WSOFT_PACKER_PIN_MAX + 1] = "";
  char action[16] = "";
  WsoftPackerAuthReply reply;
};

struct SignOutItem {
  uint32_t seq = 0;
  char code[WSOFT_PACKER_CODE_MAX + 1] = "";
};

static portMUX_TYPE auth_lock = portMUX_INITIALIZER_UNLOCKED;
static PackerAuthJob auth_job;
static uint32_t next_auth_job_id = 0;
static uint32_t next_auth_seq = 0;
static SignOutItem sign_out_queue[WSOFT_PACKER_STRIP_MAX];
static size_t sign_out_count = 0;

static bool build_packer_auth_body(const char *code, const char *pin, const char *action, char *req, size_t size) {
  if (strcmp(action, "sign_in") == 0) return wsoft_packer_auth_sign_in_request(code, pin, req, size);
  if (strcmp(action, "sign_out") == 0) return wsoft_packer_auth_sign_out_request(code, req, size);
  return false;
}

uint32_t hub_packer_auth_start(const char *code, const char *pin, const char *action, bool fire_and_forget) {
  if (code == nullptr || action == nullptr) return 0;
  char req[WSOFT_PACKER_AUTH_REQUEST_MAX];
  bool valid = build_packer_auth_body(code, pin, action, req, sizeof req);
  memset(req, 0, sizeof req);
  if (!valid) return 0;

  if (!configured || ota_busy || WiFi.status() != WL_CONNECTED || state != HUB_ONLINE ||
      time(nullptr) < CLOCK_VALID_AFTER || hub_task_handle == nullptr) {
    return 0;
  }

  if (fire_and_forget && strcmp(action, "sign_out") == 0) {
    bool queued = false;
    portENTER_CRITICAL(&auth_lock);
    size_t existing = sign_out_count;
    for (size_t i = 0; i < sign_out_count; i++) {
      if (strcasecmp(sign_out_queue[i].code, code) == 0) {
        existing = i;
        break;
      }
    }
    if (existing < sign_out_count) {
      // Move re-queued sign-out to tail with a fresh sequence number to preserve enqueue order.
      for (size_t i = existing + 1; i < sign_out_count; i++) sign_out_queue[i - 1] = sign_out_queue[i];
      sign_out_count--;
    }
    if (sign_out_count < WSOFT_PACKER_STRIP_MAX) {
      sign_out_queue[sign_out_count].seq = ++next_auth_seq;
      strlcpy(sign_out_queue[sign_out_count].code, code, sizeof sign_out_queue[0].code);
      sign_out_count++;
      queued = true;
    }
    portEXIT_CRITICAL(&auth_lock);
    if (queued) xTaskNotifyGive(hub_task_handle);
    return queued ? 1U : 0U;
  }

  uint32_t job_id = 0;
  portENTER_CRITICAL(&auth_lock);
  if (auth_job.state != HUB_PACKER_AUTH_PENDING) {
    job_id = ++next_auth_job_id;
    if (job_id == 0) job_id = ++next_auth_job_id;
    auth_job.job_id = job_id;
    auth_job.seq = ++next_auth_seq;
    auth_job.state = HUB_PACKER_AUTH_PENDING;
    auth_job.fire_and_forget = fire_and_forget;
    strlcpy(auth_job.code, code, sizeof auth_job.code);
    strlcpy(auth_job.pin, pin != nullptr ? pin : "", sizeof auth_job.pin);
    strlcpy(auth_job.action, action, sizeof auth_job.action);
    memset(&auth_job.reply, 0, sizeof auth_job.reply);
  }
  portEXIT_CRITICAL(&auth_lock);

  if (job_id != 0) xTaskNotifyGive(hub_task_handle);
  return job_id;
}

HubPackerAuthState hub_packer_auth_poll(uint32_t job_id, WsoftPackerAuthReply *out, bool cancel_if_pending) {
  if (job_id == 0) return HUB_PACKER_AUTH_IDLE;
  HubPackerAuthState st = HUB_PACKER_AUTH_IDLE;
  portENTER_CRITICAL(&auth_lock);
  if (auth_job.job_id == job_id) {
    st = auth_job.state;
    if (st == HUB_PACKER_AUTH_DONE) {
      if (out != nullptr) *out = auth_job.reply;
      auth_job.state = HUB_PACKER_AUTH_IDLE;
      auth_job.job_id = 0;
      memset(&auth_job.reply, 0, sizeof auth_job.reply);
    } else if (st == HUB_PACKER_AUTH_OFFLINE) {
      auth_job.state = HUB_PACKER_AUTH_IDLE;
      auth_job.job_id = 0;
    } else if (st == HUB_PACKER_AUTH_PENDING && cancel_if_pending) {
      auth_job.state = HUB_PACKER_AUTH_IDLE;
      auth_job.job_id = 0;
      memset(auth_job.pin, 0, sizeof auth_job.pin);
      memset(&auth_job.reply, 0, sizeof auth_job.reply);
      st = HUB_PACKER_AUTH_OFFLINE;
    }
  }
  portEXIT_CRITICAL(&auth_lock);
  return st;
}

void hub_packer_auth_cancel(uint32_t job_id) {
  if (job_id == 0) return;
  portENTER_CRITICAL(&auth_lock);
  if (auth_job.job_id == job_id) {
    auth_job.state = HUB_PACKER_AUTH_IDLE;
    auth_job.job_id = 0;
    memset(auth_job.pin, 0, sizeof auth_job.pin);
    memset(&auth_job.reply, 0, sizeof auth_job.reply);
  }
  portEXIT_CRITICAL(&auth_lock);
}

static bool run_packer_auth_http(const char *code, const char *pin, const char *action, WsoftPackerAuthReply *out) {
  memset(out, 0, sizeof *out);
  char req[WSOFT_PACKER_AUTH_REQUEST_MAX];
  if (!build_packer_auth_body(code, pin, action, req, sizeof req)) {
    memset(req, 0, sizeof req);
    return false;
  }
  String body = req;
  memset(req, 0, sizeof req);

  HubConfigSnapshot cfg;
  snap_config(&cfg);
  if (!cfg.configured || cfg.base_url[0] == 0) {
    memset(cfg.signing_key, 0, sizeof cfg.signing_key);
    return false;
  }

  time_t now = time(nullptr);
  uint8_t nonce_bytes[16];
  esp_fill_random(nonce_bytes, sizeof nonce_bytes);
  String nonce = hex(nonce_bytes, sizeof nonce_bytes);
  String timestamp = String((unsigned long)now);

  HTTPClient http;
  http.setTimeout(2500);
  if (!http.begin(String(cfg.base_url) + "/api/v1/ingest/packer-auth")) {
    memset(cfg.signing_key, 0, sizeof cfg.signing_key);
    record_hub_answer(0);
    return false;
  }
  const char *answer_headers[] = {"X-ScaleModels-Key-Id", "X-ScaleModels-Timestamp", "X-ScaleModels-Nonce",
                                  "X-ScaleModels-Signature"};
  http.collectHeaders(answer_headers, 4);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-ScaleModels-Key-Id", cfg.key_id);
  http.addHeader("X-ScaleModels-Timestamp", timestamp);
  http.addHeader("X-ScaleModels-Nonce", nonce);
  http.addHeader("X-ScaleModels-Signature", sign_with(cfg, timestamp, nonce, body));
  int status = http.POST(body);
  record_hub_answer(status);
  String reply = status > 0 ? http.getString() : "";
  String answer_key = http.header("X-ScaleModels-Key-Id");
  String answer_time = http.header("X-ScaleModels-Timestamp");
  String answer_nonce = http.header("X-ScaleModels-Nonce");
  String answer_signature = http.header("X-ScaleModels-Signature");
  http.end();

  if (status != 200) {
    memset(cfg.signing_key, 0, sizeof cfg.signing_key);
    WSOFT_LOG.printf("HUB packer-auth status=%d\n", status);
    return false;
  }
  bool sig_ok = wsoft_hub_packer_auth_verify(cfg.signing_key, cfg.key_id, nonce.c_str(), answer_key.c_str(),
                                             answer_time.c_str(), answer_nonce.c_str(), answer_signature.c_str(),
                                             reply.c_str(), reply.length());
  memset(cfg.signing_key, 0, sizeof cfg.signing_key);
  if (!sig_ok) {
    WSOFT_LOG.println("HUB packer-auth bad signature");
    return false;
  }
  if (!wsoft_packer_auth_parse(reply.c_str(), reply.length(), out)) {
    WSOFT_LOG.println("HUB packer-auth bad body");
    return false;
  }
  return true;
}

static bool packer_auth_online_now() {
  return configured && !ota_busy && WiFi.status() == WL_CONNECTED && state == HUB_ONLINE &&
         time(nullptr) >= CLOCK_VALID_AFTER;
}

static void process_packer_auth() {
  static WsoftPackerAuthReply reply;
  bool so_failed = false;

  for (;;) {
    bool online = packer_auth_online_now() && !so_failed;
    char so_code[WSOFT_PACKER_CODE_MAX + 1] = "";
    uint32_t so_seq = 0;
    uint32_t job_id = 0;
    bool fire_and_forget = false;
    char code[WSOFT_PACKER_CODE_MAX + 1] = "";
    char pin[WSOFT_PACKER_PIN_MAX + 1] = "";
    char action[16] = "";

    // Pick whichever pending item has the earlier sequence number (unsigned wrap-safe diff).
    portENTER_CRITICAL(&auth_lock);
    bool have_so = sign_out_count > 0;
    bool have_job = auth_job.state == HUB_PACKER_AUTH_PENDING;
    bool take_so = have_so && online &&
                   (!have_job || (int32_t)(sign_out_queue[0].seq - auth_job.seq) < 0);
    if (take_so) {
      strlcpy(so_code, sign_out_queue[0].code, sizeof so_code);
      so_seq = sign_out_queue[0].seq;
    } else if (have_job && (!have_so || !online || (int32_t)(auth_job.seq - sign_out_queue[0].seq) < 0)) {
      job_id = auth_job.job_id;
      fire_and_forget = auth_job.fire_and_forget;
      strlcpy(code, auth_job.code, sizeof code);
      strlcpy(pin, auth_job.pin, sizeof pin);
      strlcpy(action, auth_job.action, sizeof action);
      memset(auth_job.pin, 0, sizeof auth_job.pin);
    }
    portEXIT_CRITICAL(&auth_lock);

    if (so_code[0] != 0) {
      if (run_packer_auth_http(so_code, nullptr, "sign_out", &reply)) {
        portENTER_CRITICAL(&auth_lock);
        if (sign_out_count > 0 && sign_out_queue[0].seq == so_seq) {
          for (size_t i = 1; i < sign_out_count; i++) sign_out_queue[i - 1] = sign_out_queue[i];
          sign_out_count--;
          sign_out_queue[sign_out_count] = SignOutItem();
        }
        portEXIT_CRITICAL(&auth_lock);
      } else {
        so_failed = true;
      }
      continue;
    }
    if (job_id == 0) break;

    bool ok = online && run_packer_auth_http(code, pin, action, &reply);
    memset(pin, 0, sizeof pin);

    bool apply_fg = false;
    portENTER_CRITICAL(&auth_lock);
    if (auth_job.job_id == job_id && auth_job.state == HUB_PACKER_AUTH_PENDING) {
      if (fire_and_forget) {
        auth_job.state = HUB_PACKER_AUTH_IDLE;
        auth_job.job_id = 0;
        apply_fg = ok && reply.ok && strcmp(action, "sign_in") == 0;
      } else if (ok) {
        auth_job.reply = reply;
        auth_job.state = HUB_PACKER_AUTH_DONE;
      } else {
        auth_job.state = HUB_PACKER_AUTH_OFFLINE;
      }
    }
    portEXIT_CRITICAL(&auth_lock);

    if (apply_fg) packers_apply_auth(reply);
  }
}

static bool has_pending_sign_out() {
  portENTER_CRITICAL(&auth_lock);
  bool pending = sign_out_count > 0;
  portEXIT_CRITICAL(&auth_lock);
  return pending;
}

bool hub_packer_auth(const char *code, const char *pin, const char *action, WsoftPackerAuthReply *out) {
  if (out == nullptr || action == nullptr) return false;
  memset(out, 0, sizeof *out);
  uint32_t job_id = hub_packer_auth_start(code, pin, action, false);
  if (job_id == 0) return false;
  uint32_t started = millis();
  while (millis() - started < 3500) {
    HubPackerAuthState st = hub_packer_auth_poll(job_id, out);
    if (st == HUB_PACKER_AUTH_DONE) return true;
    if (st != HUB_PACKER_AUTH_PENDING) return false;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  hub_packer_auth_cancel(job_id);
  return false;
}

void hub_trigger_bag_events_push() {
  if (hub_task_handle != nullptr) xTaskNotifyGive(hub_task_handle);
}

#endif
#ifdef WSOFT_ON_BASE
// ---------- the Hub Control room (Hub ADR 0010) ----------
// The words on this board's screen go to the Hub, which shows them on its Control room wall.
// Same JSON as GET /rest/wsoftScreen; when to push is wsoft_screen_push_due() (host-tested).
static WsoftScreenPush screen_push;
static char screen_json[WSOFT_UI_JSON_MAX];
static char screen_body[WSOFT_SCREEN_BODY_MAX];

// A re-pair restarts the board, so this state never carries over to another Hub.
static void push_screen() {
  if (!wsoft_screen_push_soon(screen_push, millis())) return;  // skip building the JSON
  if (!ui_model_json(screen_json, sizeof screen_json)) return;  // no screen poll yet
  uint32_t hash = wsoft_screen_hash(screen_json);
  if (!wsoft_screen_push_due(screen_push, millis(), hash)) return;
  char sent_at[32];
  time_t now = time(nullptr);
  struct tm utc;
  gmtime_r(&now, &utc);
  strftime(sent_at, sizeof sent_at, "%Y-%m-%dT%H:%M:%SZ", &utc);
  if (!wsoft_screen_push_body(sent_at, screen_json, screen_body, sizeof screen_body)) return;
  String reply;
  int status = post_signed("/api/v1/ingest/screen", String(screen_body), &reply);
  wsoft_screen_push_result(&screen_push, millis(), hash, status);
  if (status != 202) WSOFT_LOG.printf("HUB screen status=%d %s\n", status, reply.substring(0, 80).c_str());
}
#endif

static void send_heartbeat() {
  HubConfigSnapshot cfg;
  snap_config(&cfg);
  time_t now = time(nullptr);
  uint8_t nonce_bytes[16];
  esp_fill_random(nonce_bytes, sizeof nonce_bytes);
  String nonce = hex(nonce_bytes, sizeof nonce_bytes);
  String timestamp = String((unsigned long)now);
  uint32_t trades_through = 0;
  String body = heartbeat_body(now, &trades_through);

  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(String(cfg.base_url) + "/api/v1/ingest/heartbeats")) {
    memset(cfg.signing_key, 0, sizeof cfg.signing_key);
    state = HUB_UNREACHABLE;
    record_hub_answer(0);
    return;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-ScaleModels-Key-Id", cfg.key_id);
  http.addHeader("X-ScaleModels-Timestamp", timestamp);
  http.addHeader("X-ScaleModels-Nonce", nonce);
  http.addHeader("X-ScaleModels-Signature", sign_with(cfg, timestamp, nonce, body));
  memset(cfg.signing_key, 0, sizeof cfg.signing_key);
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

// ---------- clock from the Hub ----------
// Ported from the 31 Oct 2026 Waveshare branch (c49a592), Benjamin 18 Nov 2026.
// The Hub checks each signature against its own clock (300 s window), so the Hub's clock is the
// one that matters. Its health route is public and every reply carries a Date header.
// Used only while the board has no valid time; internet time (NTP) still wins whenever it works.
// The Date header is not signed: whatever answers on the Hub's address sets this clock, and
// weighings and trade weights saved meanwhile carry that time.

static const uint32_t CLOCK_RETRY_MS = 5000;

static void set_clock_from_hub() {
  HubConfigSnapshot cfg;
  snap_config(&cfg);
  HTTPClient http;
  http.setTimeout(5000);
  if (!http.begin(String(cfg.base_url) + "/api/v1/health")) {
    memset(cfg.signing_key, 0, sizeof cfg.signing_key);
    return;
  }
  memset(cfg.signing_key, 0, sizeof cfg.signing_key);
  const char *wanted[] = {"Date"};
  http.collectHeaders(wanted, 1);
  int status = http.GET();
  String date = status > 0 ? http.header("Date") : String();
  http.end();
  // No answer at all moves to a backup address, as for a heartbeat (none is sent without a clock).
  record_hub_answer(status);

  time_t now = 0;
  if (!wsoft_parse_http_date(date.c_str(), &now) || now < CLOCK_VALID_AFTER) {
    WSOFT_LOG.printf("HUB clock from hub failed status=%d\n", status);
    return;
  }
  if (time(nullptr) >= CLOCK_VALID_AFTER) return;  // internet time got there first
  // Whole seconds; the 300 s window leaves plenty of room.
  struct timeval tv = {now, 0};
  settimeofday(&tv, nullptr);
  clock_from_hub = true;
  WSOFT_LOG.printf("HUB clock set from hub: %s\n", date.c_str());
}

static void hub_task(void *) {
#ifdef WSOFT_ON_BASE
  hub_task_handle = xTaskGetCurrentTaskHandle();
#endif
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
  uint32_t next_clock_try = 0;
#ifdef WSOFT_ON_BASE
  commands_applied = load_commands_applied();
  uint32_t seen_switch_changes = switches_change_count();
  uint32_t next_command_pull = 0;
#endif
  for (;;) {
#ifdef WSOFT_ON_BASE
    // Always resolve any queued UI auth (online if connected, or immediate OFFLINE transition if link dropped).
    process_packer_auth();
#endif
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
      // A bench or site network often has no internet time. Ask the Hub instead.
      if (!ota_busy && (int32_t)(millis() - next_clock_try) >= 0) {
        next_clock_try = millis() + CLOCK_RETRY_MS;
        set_clock_from_hub();
      }
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
    if (state == HUB_ONLINE && !ota_busy) process_packer_auth();
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
    if (state == HUB_ONLINE && !ota_busy) process_packer_auth();
    if (state == HUB_ONLINE && !ota_busy && !pairing_changed && (int32_t)(millis() - next_command_pull) >= 0) {
      pull_commands();
      next_command_pull = millis() + COMMANDS_MS;
    }
    if (state == HUB_ONLINE && !ota_busy && !pairing_changed && hilo_pull_due()) pull_hilo();
    if (state == HUB_ONLINE && !ota_busy && !pairing_changed && !has_pending_sign_out() && packers_pull_due()) pull_packers();
    if (state == HUB_ONLINE && !ota_busy && !pairing_changed && packers_bag_events_due()) push_bag_events();
    // The screen for the Hub's Control room: at most every 3 s while it changes, else every 30 s.
    if (state == HUB_ONLINE && !ota_busy && !pairing_changed) push_screen();
    TradeWeight waiting;
    bool waiting_for_trade;
    int32_t waiting_tare;
    if (state == HUB_ONLINE && !ota_busy && (int32_t)(millis() - next_weighing_try) >= 0 &&
        weighings_oldest(&waiting, &waiting_for_trade, &waiting_tare, nullptr)) {
      send_weighings();
      next_weighing_try = millis() + RETRY_MS;
    }
#endif
#ifdef WSOFT_ON_BASE
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(250));
#else
    vTaskDelay(pdMS_TO_TICKS(250));
#endif
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
#else
  // Hardware.Base: the Wi-Fi update switch (wifi_update.h) owns the update service.
  wifi_update_poll();
  ota_busy = wifi_update_busy();
#endif
}

bool hub_ota_busy() {
  return ota_busy;
}

// ---------- public ----------

void hub_begin() {
  config_mutex = xSemaphoreCreateMutex();
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
  HubConfigSnapshot cfg;
  snap_config(&cfg);
#ifdef WSOFT_ON_BASE
  WSOFT_LOG.printf("HUB configured for %s (%u address%s) key %s\n", cfg.base_url, (unsigned)failover.count,
                   failover.count == 1 ? "" : "es", cfg.key_id);
#else
  WSOFT_LOG.printf("HUB configured for %s key %s\n", cfg.base_url, cfg.key_id);
#endif
  memset(cfg.signing_key, 0, sizeof cfg.signing_key);
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
  out->clock_from_hub = clock_from_hub;
}
#endif
