#include "hub.h"
#include "approval.h"
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
static const char *STATION_MODEL = "waveshare-djb-demo";
static const char *RELEASE_VERSION = "bench-2026-10-24";
static const uint32_t HEARTBEAT_MS = 30000;
static const uint32_t RETRY_MS = 5000;
// While trade weights wait for the Hub, heartbeat sooner (the Hub allows 120 a minute per device).
static const uint32_t TRADE_QUEUE_MS = 2000;
// The Hub rejects a signature more than 300 s from its own clock; anything before this is "unset".
static const time_t CLOCK_VALID_AFTER = 1700000000;

static Preferences prefs;
static String wifi_ssid, wifi_pass, hub_url, key_id, ota_pass;
static uint8_t signing_key[32];
static bool configured = false;
static volatile HubState state = HUB_SETUP;
static volatile bool ota_busy = false;

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
  return true;
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
  body += "}}";
  return body;
}

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
  if (!http.begin(hub_url + "/api/v1/ingest/heartbeats")) {
    state = HUB_UNREACHABLE;
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

  if (status == 202) {
    state = HUB_ONLINE;
    if (trades_through > 0) approval_delivered(trades_through);
  } else if (status == 401 || status == 400 || status == 409) {
    state = HUB_REJECTED;
  } else {
    state = HUB_UNREACHABLE;
  }
  Serial0.printf("HUB heartbeat status=%d %s\n", status, reply.substring(0, 120).c_str());
}

static void hub_task(void *) {
#ifndef WSOFT_ON_BASE
  WiFi.onEvent(
      [](WiFiEvent_t, WiFiEventInfo_t info) {
        Serial0.printf("HUB wifi refused reason=%d\n", info.wifi_sta_disconnected.reason);
      },
      ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  configTime(0, 0, "pool.ntp.org", "time.google.com");
#endif

  uint32_t next = 0;
  uint32_t next_report = 0;
  for (;;) {
    if ((int32_t)(millis() - next_report) >= 0) {
      next_report = millis() + 10000;
      Serial0.printf("HUB state=%s wifi_status=%d ip=%s clock=%ld\n", hub_state_text(state), (int)WiFi.status(),
                     WiFi.localIP().toString().c_str(), (long)time(nullptr));
#ifndef WSOFT_ON_BASE
      if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect();
        int found = WiFi.scanNetworks();
        bool seen = false;
        for (int i = 0; i < found; i++) {
          if (WiFi.SSID(i) == wifi_ssid) {
            seen = true;
            Serial0.printf("HUB wifi seen rssi=%d channel=%d\n", WiFi.RSSI(i), WiFi.channel(i));
          }
        }
        if (!seen) Serial0.printf("HUB wifi not seen among %d networks (needs 2.4 GHz)\n", found);
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
    // A heartbeat uses the same network stack as a firmware upload. Leave it until the upload finishes.
    if ((int32_t)(millis() - next) >= 0 && !ota_busy) {
      send_heartbeat();
      uint32_t wait = state != HUB_ONLINE ? RETRY_MS : approval_pending() > 0 ? TRADE_QUEUE_MS : HEARTBEAT_MS;
      next = millis() + wait;
    }
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
    ArduinoOTA.onStart([]() { ota_busy = true; Serial0.println("HUB ota start"); });
    ArduinoOTA.onEnd([]() { Serial0.println("HUB ota end"); });
    ArduinoOTA.onError([](ota_error_t error) {
      ota_busy = false;
      Serial0.printf("HUB ota error %u\n", (unsigned)error);
    });
    ArduinoOTA.begin();
    ota_started = true;
    Serial0.printf("HUB ota ready ip=%s\n", WiFi.localIP().toString().c_str());
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
    Serial0.println("HUB not configured: send HUB WIFI / HUB PASS / HUB PAIR / HUB SAVE over USB");
#else
    Serial0.println("HUB not configured: send HUB PAIR / HUB SAVE over USB (Wi-Fi is set in the web interface)");
#endif
    return;
  }
  Serial0.printf("HUB configured for %s key %s\n", hub_url.c_str(), key_id.c_str());
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
