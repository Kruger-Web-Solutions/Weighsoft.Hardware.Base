#include "wsoft_switch.h"

#include <stdio.h>
#include <string.h>

bool wsoft_switch_parse(const char *text, WsoftSwitchWhich *which) {
  if (text == nullptr) return false;
  if (strcmp(text, "approval") == 0) *which = WSOFT_SWITCH_APPROVAL;
  else if (strcmp(text, "auto_save") == 0) *which = WSOFT_SWITCH_AUTO_SAVE;
  else return false;
  return true;
}

const char *wsoft_switch_name(WsoftSwitchWhich which) {
  return which == WSOFT_SWITCH_AUTO_SAVE ? "auto_save" : "approval";
}

const char *wsoft_switch_where_text(WsoftSwitchWhere where) {
  switch (where) {
    case WSOFT_WHERE_SCREEN: return "screen";
    case WSOFT_WHERE_BROWSER: return "browser";
    case WSOFT_WHERE_HUB: return "hub";
    case WSOFT_WHERE_USB: return "usb";
  }
  return "screen";
}

const char *wsoft_switch_error_code(WsoftSwitchError error) {
  switch (error) {
    case WSOFT_SWITCH_OK: return "";
    case WSOFT_SWITCH_ALREADY: return "ALREADY";
    case WSOFT_SWITCH_SETUP_INCOMPLETE: return "SETUP_INCOMPLETE";
    case WSOFT_SWITCH_REASON_REQUIRED: return "REASON_REQUIRED";
    case WSOFT_SWITCH_REASON_TOO_LONG: return "REASON_TOO_LONG";
    case WSOFT_SWITCH_BUSY: return "BUSY";
    case WSOFT_SWITCH_WHO_REQUIRED: return "WHO_REQUIRED";
  }
  return "BUSY";
}

const char *wsoft_switch_error_text(WsoftSwitchError error) {
  switch (error) {
    case WSOFT_SWITCH_OK: return "Done.";
    case WSOFT_SWITCH_ALREADY: return "It is already like that. Nothing changed.";
    case WSOFT_SWITCH_SETUP_INCOMPLETE: return "Finish the scale setup before you switch Approval on.";
    case WSOFT_SWITCH_REASON_REQUIRED: return "Give a short reason (letters, digits and signs only).";
    case WSOFT_SWITCH_REASON_TOO_LONG: return "The reason can be at most 80 characters.";
    case WSOFT_SWITCH_BUSY: return "The board is busy. Try again.";
    case WSOFT_SWITCH_WHO_REQUIRED: return "No name to log the change under.";
  }
  return "The board is busy. Try again.";
}

static bool is_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Copies text without the spaces at both ends. Returns the trimmed length (may exceed cap - 1;
// then only cap - 1 bytes are copied).
static size_t copy_trimmed(const char *text, char *out, size_t cap) {
  out[0] = 0;
  if (text == nullptr) return 0;
  while (*text && is_space(*text)) text++;
  size_t length = strlen(text);
  while (length > 0 && is_space(text[length - 1])) length--;
  size_t n = length < cap - 1 ? length : cap - 1;
  memcpy(out, text, n);
  out[n] = 0;
  return length;
}

static bool printable(const char *text) {
  for (; *text; text++) {
    unsigned char c = (unsigned char)*text;
    if (c < 0x20 || c > 0x7E) return false;
  }
  return true;
}

WsoftSwitchError wsoft_switch_request(WsoftSwitchWhich which, bool on, const char *who, WsoftSwitchWhere where,
                                      const char *reason, WsoftSwitchRequest *out) {
  out->which = which;
  out->on = on;
  out->where = where;

  size_t length = copy_trimmed(who, out->who, sizeof out->who);
  if (out->who[0] == 0) return WSOFT_SWITCH_WHO_REQUIRED;
  if (length > (size_t)WSOFT_SWITCH_WHO_MAX) out->who[WSOFT_SWITCH_WHO_MAX - 1] = '~';
  for (char *p = out->who; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (c <= 0x20 || c > 0x7E) *p = '_';
  }

  return wsoft_reason_check(reason, which == WSOFT_SWITCH_APPROVAL, out->reason);
}

WsoftSwitchError wsoft_reason_check(const char *reason, bool required, char out[WSOFT_SWITCH_REASON_MAX + 1]) {
  // Refuse other bytes rather than change them, so the browser and the board agree.
  char longer[WSOFT_SWITCH_REASON_MAX + 2];
  size_t length = copy_trimmed(reason, longer, sizeof longer);
  out[0] = 0;
  if (!printable(longer)) return WSOFT_SWITCH_REASON_REQUIRED;
  if (length > (size_t)WSOFT_SWITCH_REASON_MAX) return WSOFT_SWITCH_REASON_TOO_LONG;
  if (length == 0 && required) return WSOFT_SWITCH_REASON_REQUIRED;
  memcpy(out, longer, length + 1);
  return WSOFT_SWITCH_OK;
}

WsoftSwitchError wsoft_switch_blocked(const WsoftSwitchState &state) {
  return state.rs485 && !state.setup_complete ? WSOFT_SWITCH_SETUP_INCOMPLETE : WSOFT_SWITCH_OK;
}

WsoftSwitchError wsoft_switch_check(const WsoftSwitchRequest &request, const WsoftSwitchState &state) {
  bool now = request.which == WSOFT_SWITCH_APPROVAL ? state.approval_on : state.auto_save;
  if (request.on == now) return WSOFT_SWITCH_ALREADY;
  if (request.which == WSOFT_SWITCH_APPROVAL && request.on) return wsoft_switch_blocked(state);
  return WSOFT_SWITCH_OK;
}

bool wsoft_switch_log_line(uint32_t counter, const char *when, bool from_on, bool to_on, const char *who,
                           WsoftSwitchWhere where, const char *reason, char *out, size_t size) {
  if (out == nullptr || size == 0) return false;
  int n = snprintf(out, size, "%lu %s %s->%s by=%s where=%s why=%s", (unsigned long)counter, when,
                   from_on ? "ON" : "OFF", to_on ? "ON" : "OFF", who, wsoft_switch_where_text(where), reason);
  if (n < 0 || (size_t)n >= size) {
    out[0] = 0;
    return false;
  }
  return true;
}
