#include "wsoft_wifi_update.h"

#include <stdio.h>
#include <string.h>

const char *wsoft_wifi_update_error_code(WsoftWifiUpdateError error) {
  switch (error) {
    case WSOFT_WIFI_UPDATE_ONE_COPY: return "ONE_COPY";
    case WSOFT_WIFI_UPDATE_APPROVAL_ON: return "APPROVAL_ON";
    case WSOFT_WIFI_UPDATE_PASSWORD_FORMAT: return "PASSWORD_FORMAT";
    case WSOFT_WIFI_UPDATE_PASSWORD_FACTORY: return "PASSWORD_FACTORY";
    case WSOFT_WIFI_UPDATE_PASSWORD_REQUIRED: return "PASSWORD_REQUIRED";
    case WSOFT_WIFI_UPDATE_NOTHING: return "NOTHING";
    case WSOFT_WIFI_UPDATE_BUSY: return "BUSY";
    default: return "";
  }
}

const char *wsoft_wifi_update_error_text(WsoftWifiUpdateError error) {
  switch (error) {
    case WSOFT_WIFI_UPDATE_ONE_COPY:
      return "This board has room for one copy of the software only. It updates by cable.";
    case WSOFT_WIFI_UPDATE_APPROVAL_ON:
      return "Locked while Approval is ON. Switch Approval OFF first.";
    case WSOFT_WIFI_UPDATE_PASSWORD_FORMAT:
      return "The password is 4 to 64 letters, digits or signs, with no spaces.";
    case WSOFT_WIFI_UPDATE_PASSWORD_FACTORY:
      return "That is a factory password. Choose this board's own password.";
    case WSOFT_WIFI_UPDATE_PASSWORD_REQUIRED:
      return "Set this board's own update password first.";
    case WSOFT_WIFI_UPDATE_NOTHING:
      return "Nothing to change.";
    case WSOFT_WIFI_UPDATE_BUSY:
      return "The board is busy. Try again.";
    default:
      return "";
  }
}

// Well-known passwords of this firmware family. All are shorter than the minimum today; the
// list keeps the rule true if the minimum ever changes, and catches a padded copy.
static const char *const FACTORY_WORDS[] = {"esp-react", "change-me-at-least-8-chars"};

static bool has_factory_word(const char *password) {
  for (size_t i = 0; i < sizeof FACTORY_WORDS / sizeof FACTORY_WORDS[0]; i++) {
    if (strstr(password, FACTORY_WORDS[i]) != nullptr) return true;
  }
  return false;
}

WsoftWifiUpdateError wsoft_wifi_update_password_check(const char *password) {
  if (password == nullptr) return WSOFT_WIFI_UPDATE_PASSWORD_FORMAT;
  size_t length = strlen(password);
  if (length < WSOFT_WIFI_UPDATE_PASSWORD_MIN || length > WSOFT_WIFI_UPDATE_PASSWORD_MAX) {
    return WSOFT_WIFI_UPDATE_PASSWORD_FORMAT;
  }
  for (size_t i = 0; i < length; i++) {
    if (password[i] < 0x21 || password[i] > 0x7E) return WSOFT_WIFI_UPDATE_PASSWORD_FORMAT;
  }
  if (has_factory_word(password)) return WSOFT_WIFI_UPDATE_PASSWORD_FACTORY;
  return WSOFT_WIFI_UPDATE_OK;
}

WsoftWifiUpdateError wsoft_wifi_update_check(const WsoftWifiUpdateRequest &request,
                                             const WsoftWifiUpdateState &state, WsoftWifiUpdateChange *out) {
  WsoftWifiUpdateChange change;
  change.on = state.on;
  bool wants_off_only = request.has_on && !request.on && request.password == nullptr;
  if (!wants_off_only) {
    if (!request.has_on && request.password == nullptr) return WSOFT_WIFI_UPDATE_NOTHING;
    if (!state.two_copies) return WSOFT_WIFI_UPDATE_ONE_COPY;
    if (state.approval_on) return WSOFT_WIFI_UPDATE_APPROVAL_ON;
  }
  if (request.password != nullptr) {
    WsoftWifiUpdateError error = wsoft_wifi_update_password_check(request.password);
    if (error != WSOFT_WIFI_UPDATE_OK) return error;
    change.password_changed = true;
  }
  if (request.has_on) {
    if (request.on && !state.password_set && !change.password_changed) return WSOFT_WIFI_UPDATE_PASSWORD_REQUIRED;
    change.on = request.on;
    change.on_changed = request.on != state.on;
  }
  *out = change;
  return WSOFT_WIFI_UPDATE_OK;
}

bool wsoft_wifi_update_listening(const WsoftWifiUpdateState &state) {
  return state.two_copies && state.on && state.password_set && !state.approval_on;
}

WsoftWifiUpdateError wsoft_wifi_update_blocked(const WsoftWifiUpdateState &state) {
  if (!state.two_copies) return WSOFT_WIFI_UPDATE_ONE_COPY;
  if (!state.on) return WSOFT_WIFI_UPDATE_OK;
  if (!state.password_set) return WSOFT_WIFI_UPDATE_PASSWORD_REQUIRED;
  if (state.approval_on) return WSOFT_WIFI_UPDATE_APPROVAL_ON;
  return WSOFT_WIFI_UPDATE_OK;
}

bool wsoft_wifi_update_event(const WsoftWifiUpdateChange &change, WsoftWifiUpdateEvent *event) {
  if (change.on_changed && change.on) {
    *event = change.password_changed ? WSOFT_WIFI_UPDATE_EVENT_ON_PASSWORD : WSOFT_WIFI_UPDATE_EVENT_ON;
  } else if (change.on_changed) {
    *event = WSOFT_WIFI_UPDATE_EVENT_OFF;  // a password sent with OFF is still stored; OFF is the news
  } else if (change.password_changed) {
    *event = WSOFT_WIFI_UPDATE_EVENT_PASSWORD;
  } else {
    return false;
  }
  return true;
}

static const char *event_text(WsoftWifiUpdateEvent event) {
  switch (event) {
    case WSOFT_WIFI_UPDATE_EVENT_ON: return "ON";
    case WSOFT_WIFI_UPDATE_EVENT_OFF: return "OFF";
    case WSOFT_WIFI_UPDATE_EVENT_PASSWORD: return "NEW-PASSWORD";
    case WSOFT_WIFI_UPDATE_EVENT_ON_PASSWORD: return "ON+NEW-PASSWORD";
    default: return "INSTALLED";
  }
}

bool wsoft_wifi_update_log_line(uint32_t counter, const char *when, WsoftWifiUpdateEvent event, const char *who,
                                const char *version, char *out, size_t size) {
  if (out == nullptr || size == 0) return false;
  char name[WSOFT_WIFI_UPDATE_WHO_MAX + 1];
  size_t n = 0;
  if (who != nullptr) {
    for (; who[n] != 0 && n < (size_t)WSOFT_WIFI_UPDATE_WHO_MAX; n++) {
      char c = who[n];
      name[n] = (c < 0x21 || c > 0x7E) ? '_' : c;
    }
  }
  name[n] = 0;
  if (n == 0) strcpy(name, "unknown");
  char was[17];
  snprintf(was, sizeof was, "%s", version == nullptr ? "" : version);
  int written = snprintf(out, size, "%lu %s WIFI-UPDATE %s by=%s was=%s", (unsigned long)counter,
                         when == nullptr ? "" : when, event_text(event), name, was);
  if (written < 0 || (size_t)written >= size) {
    out[0] = 0;
    return false;
  }
  return true;
}
