#pragma once

// A small strict JSON reader for the Hub's signed answers (commands, Hi-Lo). It reads only
// what the caller asks for, in order, and never allocates. Pure C++ for host tests.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct WsoftJsonReader {
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

  // A whole number 1..max, no sign, no leading zero, no fraction or exponent.
  bool id(uint64_t *out, uint64_t max = 9007199254740991ULL) {
    space();
    if (p >= end || *p < '1' || *p > '9') return false;
    uint64_t v = 0;
    while (p < end && *p >= '0' && *p <= '9') {
      v = v * 10 + (uint64_t)(*p++ - '0');
      if (v > max) return false;
    }
    if (p < end && (*p == '.' || *p == 'e' || *p == 'E')) return false;
    *out = v;
    return true;
  }

  // A whole number 0..max ("0" alone, or no leading zero), no sign, fraction or exponent.
  bool whole(int64_t *out, int64_t max) {
    space();
    if (p < end && *p == '0') {
      p++;
      if (p < end && ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' || *p == 'E')) return false;
      *out = 0;
      return true;
    }
    uint64_t v = 0;
    if (!id(&v, (uint64_t)max)) return false;
    *out = (int64_t)v;
    return true;
  }

  // At the end of the text (only white space left).
  bool done() {
    space();
    return p == end;
  }
};
