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

}  // namespace

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
  size_t url_length = strlen(code_out.hub_url);
  while (url_length > 0 && code_out.hub_url[url_length - 1] == '/') code_out.hub_url[--url_length] = 0;
  bool http = strncmp(code_out.hub_url, "http://", 7) == 0 && url_length > 7;
  bool https = strncmp(code_out.hub_url, "https://", 8) == 0 && url_length > 8;
  if (!http && !https) return WSOFT_HUB_CODE_NO_URL;

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
