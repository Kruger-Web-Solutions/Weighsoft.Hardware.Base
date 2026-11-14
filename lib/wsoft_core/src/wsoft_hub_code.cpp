#include "wsoft_hub_code.h"

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

// Decodes standard or url-safe base64, padded or not. False on any other character,
// on a dangling single character, or when the result does not fit.
bool b64_decode(const char *text, size_t length, uint8_t *out, size_t capacity, size_t *written) {
  while (length > 0 && text[length - 1] == '=') length--;
  if (length % 4 == 1) return false;
  size_t n = 0;
  uint32_t bits = 0;
  int count = 0;
  for (size_t i = 0; i < length; i++) {
    int v = b64_value(text[i]);
    if (v < 0) return false;
    bits = (bits << 6) | (uint32_t)v;
    count += 6;
    if (count >= 8) {
      count -= 8;
      if (n >= capacity) return false;
      out[n++] = (uint8_t)((bits >> count) & 0xFF);
    }
  }
  *written = n;
  return true;
}

// Copies the string value of "key" from flat JSON (no escapes in these fields).
bool json_string(const char *json, const char *key, char *out, size_t size) {
  char needle[40];
  size_t key_length = strlen(key);
  if (key_length + 3 > sizeof needle) return false;
  needle[0] = '"';
  memcpy(needle + 1, key, key_length);
  needle[key_length + 1] = '"';
  needle[key_length + 2] = 0;
  const char *p = strstr(json, needle);
  if (p == nullptr) return false;
  p += key_length + 2;
  while (*p == ' ') p++;
  if (*p++ != ':') return false;
  while (*p == ' ') p++;
  if (*p++ != '"') return false;
  const char *end = strchr(p, '"');
  if (end == nullptr || (size_t)(end - p) >= size) return false;
  memcpy(out, p, (size_t)(end - p));
  out[end - p] = 0;
  return true;
}

bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

// Strips trailing slashes. False unless it is an http(s):// address with something after the scheme.
bool clean_url(char *url) {
  size_t length = strlen(url);
  while (length > 0 && url[length - 1] == '/') url[--length] = 0;
  bool http = strncmp(url, "http://", 7) == 0 && length > 7;
  bool https = strncmp(url, "https://", 8) == 0 && length > 8;
  return http || https;
}

// Adds url to the list unless it is not a LAN address, already listed, or the list is full.
void keep_lan_address(WsoftHubCode *c, const char *url) {
  if (!wsoft_hub_url_is_lan(url) || c->hub_url_count >= WSOFT_HUB_URLS_MAX) return;
  for (uint8_t i = 0; i < c->hub_url_count; i++) {
    if (strcmp(c->hub_urls[i], url) == 0) return;
  }
  strncpy(c->hub_urls[c->hub_url_count], url, WSOFT_HUB_URL_MAX - 1);
  c->hub_urls[c->hub_url_count][WSOFT_HUB_URL_MAX - 1] = 0;
  c->hub_url_count++;
}

// Reads "hub_urls":["...","..."] (plain strings, what the Hub emits). Entries that are not
// usable (too long, escaped, not http(s)) are skipped; a broken list just ends early.
void read_hub_urls(const char *json, WsoftHubCode *c) {
  static const char NEEDLE[] = "\"hub_urls\":[";
  const char *cursor = strstr(json, NEEDLE);
  if (cursor == nullptr) return;
  cursor += sizeof NEEDLE - 1;
  char url[WSOFT_HUB_URL_MAX];
  for (;;) {
    while (is_space(*cursor)) cursor++;
    if (*cursor != '"') return;
    const char *start = cursor + 1;
    const char *end = strchr(start, '"');
    if (end == nullptr) return;
    size_t length = (size_t)(end - start);
    if (length < sizeof url && memchr(start, '\\', length) == nullptr) {
      memcpy(url, start, length);
      url[length] = 0;
      if (clean_url(url)) keep_lan_address(c, url);
    }
    cursor = end + 1;
    while (is_space(*cursor)) cursor++;
    if (*cursor != ',') return;
    cursor++;
  }
}

// hub_urls = the LAN addresses this board can use. The Hub's own first choice (hub_url) leads
// when it is a LAN address; no LAN address at all keeps the list at just hub_url, as before.
void read_addresses(const char *json, WsoftHubCode *c) {
  c->hub_url_count = 0;
  keep_lan_address(c, c->hub_url);
  read_hub_urls(json, c);
  if (c->hub_url_count == 0) {
    memcpy(c->hub_urls[0], c->hub_url, sizeof c->hub_url);
    c->hub_url_count = 1;
    return;
  }
  memcpy(c->hub_url, c->hub_urls[0], sizeof c->hub_url);
}

}  // namespace

bool wsoft_hub_url_is_lan(const char *url) {
  if (url == nullptr) return false;
  const char *host = strstr(url, "://");
  host = host ? host + 3 : url;
  if (strncmp(host, "localhost", 9) == 0 || strncmp(host, "127.", 4) == 0 || strncmp(host, "[::1]", 5) == 0) {
    return false;
  }
  // 100.64.0.0/10 (Tailscale): first part 100, second 64..127.
  if (strncmp(host, "100.", 4) == 0) {
    int second = 0;
    const char *p = host + 4;
    int digits = 0;
    while (*p >= '0' && *p <= '9' && digits < 4) {
      second = second * 10 + (*p - '0');
      p++;
      digits++;
    }
    if (digits > 0 && *p == '.' && second >= 64 && second <= 127) return false;
  }
  return true;
}

void wsoft_hub_failover_begin(WsoftHubFailover *failover, uint8_t count) {
  failover->count = count > 0 ? count : 1;
  failover->active = 0;
}

bool wsoft_hub_failover_result(WsoftHubFailover *failover, int http_status) {
  if (http_status > 0 || failover->count <= 1) return false;
  failover->active = (uint8_t)((failover->active + 1) % failover->count);
  return true;
}

WsoftHubCodeError wsoft_hub_code_parse(const char *code, WsoftHubCode *out) {
  if (code == nullptr) return WSOFT_HUB_CODE_EMPTY;
  while (is_space(*code)) code++;
  size_t length = strlen(code);
  while (length > 0 && is_space(code[length - 1])) length--;
  if (length == 0) return WSOFT_HUB_CODE_EMPTY;
  if (length > WSOFT_HUB_CODE_MAX) return WSOFT_HUB_CODE_TOO_LONG;

  char json[WSOFT_HUB_CODE_MAX];
  size_t json_length = 0;
  if (!b64_decode(code, length, (uint8_t *)json, sizeof json - 1, &json_length)) return WSOFT_HUB_CODE_NOT_BASE64;
  json[json_length] = 0;
  if (json_length == 0 || json[0] != '{') return WSOFT_HUB_CODE_NOT_BASE64;

  WsoftHubCode code_out;
  memset(&code_out, 0, sizeof code_out);
  if (!json_string(json, "hub_url", code_out.hub_url, sizeof code_out.hub_url)) return WSOFT_HUB_CODE_NO_URL;
  if (!clean_url(code_out.hub_url)) return WSOFT_HUB_CODE_NO_URL;
  read_addresses(json, &code_out);

  if (!json_string(json, "key_id", code_out.key_id, sizeof code_out.key_id) || code_out.key_id[0] == 0) {
    return WSOFT_HUB_CODE_NO_KEY_ID;
  }
  json_string(json, "device_id", code_out.device_id, sizeof code_out.device_id);

  char secret_text[64];
  size_t secret_length = 0;
  bool secret_ok = json_string(json, "secret", secret_text, sizeof secret_text) &&
                   b64_decode(secret_text, strlen(secret_text), code_out.secret, sizeof code_out.secret, &secret_length) &&
                   secret_length == sizeof code_out.secret;
  memset(secret_text, 0, sizeof secret_text);
  memset(json, 0, sizeof json);
  if (!secret_ok) return WSOFT_HUB_CODE_BAD_SECRET;

  if (out != nullptr) *out = code_out;
  memset(&code_out, 0, sizeof code_out);
  return WSOFT_HUB_CODE_OK;
}

const char *wsoft_hub_code_error_text(WsoftHubCodeError error) {
  switch (error) {
    case WSOFT_HUB_CODE_OK: return "ok";
    case WSOFT_HUB_CODE_EMPTY: return "Paste the pairing key from the Hub.";
    case WSOFT_HUB_CODE_TOO_LONG: return "That is too long to be a Hub pairing key.";
    case WSOFT_HUB_CODE_NOT_BASE64: return "That is not a Hub pairing key. Copy it again from the Hub.";
    case WSOFT_HUB_CODE_NO_URL: return "The key has no Hub address in it. Make a new key on the Hub.";
    case WSOFT_HUB_CODE_NO_KEY_ID: return "The key is incomplete. Make a new key on the Hub.";
    case WSOFT_HUB_CODE_BAD_SECRET: return "The key is incomplete. Make a new key on the Hub.";
  }
  return "Unknown error.";
}
