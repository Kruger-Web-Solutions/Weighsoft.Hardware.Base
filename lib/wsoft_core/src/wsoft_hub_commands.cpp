#include "wsoft_hub_commands.h"

#include <stdio.h>
#include <string.h>

#include "wsoft_sha256.h"

static bool code_ok(const char *code) {
  size_t length = strlen(code);
  if (length == 0 || length > WSOFT_CMD_ERROR_MAX) return false;
  for (size_t i = 0; i < length; i++) {
    if (!((code[i] >= 'A' && code[i] <= 'Z') || code[i] == '_')) return false;
  }
  return true;
}

bool wsoft_hub_commands_request(uint64_t applied_through, const WsoftCmdResult *results, size_t count, char *out,
                                size_t size) {
  if (out == nullptr || size == 0 || count > WSOFT_CMD_RESULTS_MAX) return false;
  size_t used = 0;
  int n = snprintf(out, size, "{\"applied_through\":%llu,\"results\":[", (unsigned long long)applied_through);
  if (n < 0 || (size_t)n >= size) return false;
  used = (size_t)n;
  for (size_t i = 0; i < count; i++) {
    const WsoftCmdResult &r = results[i];
    if (r.ok) {
      n = snprintf(out + used, size - used, "%s{\"id\":%llu,\"ok\":true,\"error\":null}", i ? "," : "",
                   (unsigned long long)r.id);
    } else {
      n = snprintf(out + used, size - used, "%s{\"id\":%llu,\"ok\":false,\"error\":\"%s\"}", i ? "," : "",
                   (unsigned long long)r.id, code_ok(r.error) ? r.error : "REFUSED");
    }
    if (n < 0 || (size_t)n >= size - used) return false;
    used += (size_t)n;
  }
  n = snprintf(out + used, size - used, "]}");
  return n > 0 && (size_t)n < size - used;
}

// ---------- strict reader for the answer ----------

namespace {

struct Reader {
  const char *p;
  const char *end;

  void space() {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
  }
  bool take(char c) {
    space();
    if (p < end && *p == c) {
      p++;
      return true;
    }
    return false;
  }
  bool peek(char c) {
    space();
    return p < end && *p == c;
  }

  static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  }

  // A JSON string, cut to cap - 1 bytes; a cut string ends in '~'. A \u escape above 0x7f
  // becomes 0x7f, so later checks still see that it was not plain ASCII. \u0000 is refused:
  // it would end the text early and hide the rest.
  bool string(char *out, size_t cap) {
    if (!take('"')) return false;
    size_t n = 0;
    bool cut = false;
    while (p < end && *p != '"') {
      char c = *p++;
      if ((unsigned char)c < 0x20) return false;
      if (c == '\\') {
        if (p >= end) return false;
        char e = *p++;
        switch (e) {
          case '"': c = '"'; break;
          case '\\': c = '\\'; break;
          case '/': c = '/'; break;
          case 'b': c = '\b'; break;
          case 'f': c = '\f'; break;
          case 'n': c = '\n'; break;
          case 'r': c = '\r'; break;
          case 't': c = '\t'; break;
          case 'u': {
            if (end - p < 4) return false;
            int code = 0;
            for (int i = 0; i < 4; i++) {
              int d = hex_digit(*p++);
              if (d < 0) return false;
              code = code * 16 + d;
            }
            if (code == 0) return false;
            c = code <= 0x7E ? (char)code : (char)0x7F;
            break;
          }
          default: return false;
        }
      }
      if (n + 1 < cap) out[n++] = c;
      else cut = true;
    }
    if (p >= end) return false;
    p++;  // closing quote
    if (cut && n > 0) out[n - 1] = '~';
    out[n] = 0;
    return true;
  }

  bool word(const char *w) {
    space();
    size_t length = strlen(w);
    if ((size_t)(end - p) < length || memcmp(p, w, length) != 0) return false;
    p += length;
    return true;
  }

  bool boolean(bool *out) {
    if (word("true")) *out = true;
    else if (word("false")) *out = false;
    else return false;
    return true;
  }

  // A whole number 1..2^53-1, no sign, no leading zero, no fraction or exponent.
  bool id(uint64_t *out) {
    space();
    if (p >= end || *p < '1' || *p > '9') return false;
    uint64_t v = 0;
    while (p < end && *p >= '0' && *p <= '9') {
      v = v * 10 + (uint64_t)(*p++ - '0');
      if (v > WSOFT_CMD_ID_MAX) return false;
    }
    if (p < end && (*p == '.' || *p == 'e' || *p == 'E')) return false;
    *out = v;
    return true;
  }
};

bool read_command(Reader &r, WsoftHubCommand *cmd) {
  if (!r.take('{')) return false;
  enum { ID = 1, SWITCH = 2, ON = 4, BY = 8, REASON = 16, ALL = 31 };
  int seen = 0;
  do {
    char key[16];
    if (!r.string(key, sizeof key) || !r.take(':')) return false;
    int bit;
    bool ok;
    if (strcmp(key, "id") == 0) {
      bit = ID;
      ok = r.id(&cmd->id);
    } else if (strcmp(key, "switch") == 0) {
      bit = SWITCH;
      char name[16];
      ok = r.string(name, sizeof name) && wsoft_switch_parse(name, &cmd->which);
    } else if (strcmp(key, "on") == 0) {
      bit = ON;
      ok = r.boolean(&cmd->on);
    } else if (strcmp(key, "by") == 0) {
      bit = BY;
      ok = r.string(cmd->by, sizeof cmd->by);
    } else if (strcmp(key, "reason") == 0) {
      bit = REASON;
      ok = r.string(cmd->reason, sizeof cmd->reason);
    } else {
      return false;
    }
    if (!ok || (seen & bit)) return false;
    seen |= bit;
  } while (r.take(','));
  return r.take('}') && seen == ALL;
}

}  // namespace

bool wsoft_hub_commands_parse(const char *body, size_t length, WsoftHubCommand out[WSOFT_CMD_PER_PULL],
                              size_t *count) {
  *count = 0;
  if (body == nullptr) return false;
  Reader r = {body, body + length};
  char key[16];
  if (!r.take('{') || !r.string(key, sizeof key) || strcmp(key, "commands") != 0 || !r.take(':') || !r.take('[')) {
    return false;
  }
  size_t n = 0;
  if (!r.peek(']')) {
    do {
      if (n == WSOFT_CMD_PER_PULL) return false;
      out[n] = WsoftHubCommand();
      if (!read_command(r, &out[n])) return false;
      n++;
    } while (r.take(','));
  }
  if (!r.take(']') || !r.take('}')) return false;
  r.space();
  if (r.p != r.end) return false;
  *count = n;
  return true;
}

void wsoft_hub_signature(const uint8_t key[32], const char *prefix, const char *key_id, const char *timestamp,
                         const char *nonce, const char *body, size_t length, char out[WSOFT_CMD_SIGNATURE_SIZE]) {
  char head[256];
  int n = snprintf(head, sizeof head, "%s\n%s\n%s\n%s\n", prefix, key_id, timestamp, nonce);
  if (n < 0 || (size_t)n >= sizeof head) {
    out[0] = 0;
    return;
  }
  uint8_t mac[32];
  wsoft_hmac_sha256(key, 32, (const uint8_t *)head, (size_t)n, (const uint8_t *)body, length, mac);
  static const char digits[] = "0123456789abcdef";
  memcpy(out, "sha256=", 7);
  for (int i = 0; i < 32; i++) {
    out[7 + i * 2] = digits[mac[i] >> 4];
    out[8 + i * 2] = digits[mac[i] & 0x0F];
  }
  out[WSOFT_CMD_SIGNATURE_SIZE - 1] = 0;
}

bool wsoft_hub_commands_verify(const uint8_t key[32], const char *sent_key_id, const char *sent_nonce,
                               const char *key_id, const char *timestamp, const char *nonce, const char *signature,
                               const char *body, size_t length) {
  if (!sent_key_id || !sent_nonce || !key_id || !timestamp || !nonce || !signature || !body) return false;
  if (strcmp(key_id, sent_key_id) != 0 || strcmp(nonce, sent_nonce) != 0) return false;
  size_t digits = strlen(timestamp);
  if (digits == 0 || digits > 19) return false;
  for (size_t i = 0; i < digits; i++) {
    if (timestamp[i] < '0' || timestamp[i] > '9') return false;
  }
  if (strlen(signature) != WSOFT_CMD_SIGNATURE_SIZE - 1) return false;
  char expected[WSOFT_CMD_SIGNATURE_SIZE];
  wsoft_hub_signature(key, WSOFT_HUB_COMMAND_PREFIX, key_id, timestamp, nonce, body, length, expected);
  if (expected[0] == 0) return false;
  return wsoft_same_bytes((const uint8_t *)expected, (const uint8_t *)signature, WSOFT_CMD_SIGNATURE_SIZE - 1);
}
