#include "hub_pairing.h"

#include <stdio.h>
#include <string.h>

namespace {

int b64_value(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+' || c == '-') return 62;
  if (c == '/' || c == '_') return 63;
  return -1;
}

bool is_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Finds "key":"value" in compact JSON with plain string values (what the Hub emits).
// Copies the value into out (cap includes the terminator). Returns false if missing,
// if the value holds an escape, or if it does not fit.
bool json_string(const char *json, const char *key, char *out, size_t cap) {
  char needle[40];
  int n = snprintf(needle, sizeof needle, "\"%s\":\"", key);
  if (n <= 0 || (size_t)n >= sizeof needle) return false;
  const char *start = strstr(json, needle);
  if (!start) return false;
  start += n;
  const char *end = strchr(start, '"');
  if (!end) return false;
  size_t len = (size_t)(end - start);
  if (len + 1 > cap) return false;
  if (memchr(start, '\\', len)) return false;
  memcpy(out, start, len);
  out[len] = 0;
  return true;
}

bool starts_with(const char *text, const char *prefix) {
  return strncmp(text, prefix, strlen(prefix)) == 0;
}

int month_index(const char *m) {
  static const char *const names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  for (int i = 0; i < 12; i++) {
    if (strncmp(m, names[i], 3) == 0) return i;
  }
  return -1;
}

// Days from 1970-01-01 to y-m-d (proleptic Gregorian), no timegm() needed.
long days_from_civil(int y, int m, int d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}

}  // namespace

bool hub_base64url_decode(const char *text, size_t text_len, uint8_t *out, size_t capacity, size_t *length) {
  uint32_t buffer = 0;
  int bits = 0;
  size_t used = 0;
  size_t i = 0;
  for (; i < text_len; i++) {
    char c = text[i];
    if (c == '=') break;
    int v = b64_value(c);
    if (v < 0) return false;
    buffer = (buffer << 6) | (uint32_t)v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      if (used >= capacity) return false;
      out[used++] = (uint8_t)((buffer >> bits) & 0xFF);
    }
  }
  // Only padding may follow the first '='.
  for (; i < text_len; i++) {
    if (text[i] != '=') return false;
  }
  // A single leftover character (6 bits) is never valid base64.
  if (bits >= 6) return false;
  *length = used;
  return true;
}

HubCodeResult hub_code_parse(const char *code, HubPairing *out) {
  if (!code) return HUB_CODE_EMPTY;
  while (is_space(*code)) code++;
  size_t len = strlen(code);
  while (len > 0 && is_space(code[len - 1])) len--;
  if (len == 0) return HUB_CODE_EMPTY;
  if (len > HUB_CODE_MAX) return HUB_CODE_TOO_LONG;

  static const size_t JSON_CAP = (HUB_CODE_MAX * 3) / 4 + 4;
  char json[JSON_CAP + 1];
  size_t json_len = 0;
  if (!hub_base64url_decode(code, len, (uint8_t *)json, JSON_CAP, &json_len)) return HUB_CODE_NOT_BASE64;
  json[json_len] = 0;
  if (json_len < 2 || json[0] != '{' || strlen(json) != json_len) return HUB_CODE_NOT_JSON;

  HubPairing p;
  memset(&p, 0, sizeof p);
  if (!json_string(json, "hub_url", p.hub_url, sizeof p.hub_url) ||
      !(starts_with(p.hub_url, "http://") || starts_with(p.hub_url, "https://"))) {
    return HUB_CODE_NO_URL;
  }
  size_t url_len = strlen(p.hub_url);
  while (url_len > 0 && p.hub_url[url_len - 1] == '/') p.hub_url[--url_len] = 0;
  if (url_len <= strlen("https://")) return HUB_CODE_NO_URL;

  if (!json_string(json, "key_id", p.key_id, sizeof p.key_id) || p.key_id[0] == 0) return HUB_CODE_NO_KEY;
  if (!json_string(json, "device_id", p.device_id, sizeof p.device_id)) p.device_id[0] = 0;

  char secret_text[64];
  size_t secret_len = 0;
  uint8_t secret[48];
  if (!json_string(json, "secret", secret_text, sizeof secret_text) ||
      !hub_base64url_decode(secret_text, strlen(secret_text), secret, sizeof secret, &secret_len) ||
      secret_len != HUB_SECRET_LEN) {
    memset(secret, 0, sizeof secret);
    memset(secret_text, 0, sizeof secret_text);
    memset(json, 0, sizeof json);
    return HUB_CODE_BAD_SECRET;
  }
  memcpy(p.secret, secret, HUB_SECRET_LEN);
  memset(secret, 0, sizeof secret);
  memset(secret_text, 0, sizeof secret_text);
  memset(json, 0, sizeof json);

  if (out) *out = p;
  memset(&p, 0, sizeof p);
  return HUB_CODE_OK;
}

const char *hub_code_result_text(HubCodeResult result) {
  switch (result) {
    case HUB_CODE_OK: return "Code is valid";
    case HUB_CODE_EMPTY: return "Paste the pairing code from the Hub";
    case HUB_CODE_TOO_LONG: return "That is too long to be a Hub pairing code";
    case HUB_CODE_NOT_BASE64: return "That is not a Hub pairing code (check you copied all of it)";
    case HUB_CODE_NOT_JSON: return "That is not a Hub pairing code (check you copied all of it)";
    case HUB_CODE_NO_URL: return "The code has no Hub address";
    case HUB_CODE_NO_KEY: return "The code has no key id";
    case HUB_CODE_BAD_SECRET: return "The code's secret is damaged (copy it again)";
  }
  return "Unknown pairing code problem";
}

HubUrlHint hub_url_hint(const char *hub_url) {
  if (!hub_url) return HUB_URL_FINE;
  const char *host = strstr(hub_url, "://");
  host = host ? host + 3 : hub_url;
  if (starts_with(host, "localhost") || starts_with(host, "127.") || starts_with(host, "[::1]")) {
    return HUB_URL_LOCALHOST;
  }
  unsigned a = 0, b = 0;
  if (sscanf(host, "%u.%u.", &a, &b) == 2 && a == 100 && b >= 64 && b <= 127) return HUB_URL_TAILSCALE;
  return HUB_URL_FINE;
}

const char *hub_url_hint_text(HubUrlHint hint) {
  switch (hint) {
    case HUB_URL_LOCALHOST:
      return "This code points at 'localhost', which is this box itself. Open the Hub with the laptop's "
             "network address (for example 192.168.1.x) and make a new code.";
    case HUB_URL_TAILSCALE:
      return "This code points at a Tailscale address (100.x). This box is not on Tailscale. Open the Hub "
             "with the laptop's network address (for example 192.168.1.x) and make a new code.";
    default:
      return "";
  }
}

bool hub_parse_http_date(const char *text, time_t *out) {
  if (!text) return false;
  // "Sat, 31 Oct 2026 08:05:56 GMT"
  char wday[4], mon[4], zone[4];
  int day = 0, year = 0, hh = 0, mm = 0, ss = 0;
  if (sscanf(text, "%3s, %d %3s %d %d:%d:%d %3s", wday, &day, mon, &year, &hh, &mm, &ss, zone) != 8) return false;
  if (strcmp(zone, "GMT") != 0) return false;
  int m = month_index(mon);
  if (m < 0 || day < 1 || day > 31 || year < 2020 || year > 2200 || hh > 23 || mm > 59 || ss > 60 || hh < 0 ||
      mm < 0 || ss < 0) {
    return false;
  }
  long days = days_from_civil(year, m + 1, day);
  *out = (time_t)(days * 86400L + hh * 3600L + mm * 60L + ss);
  return true;
}
