#include <Features.h>
#if FT_ENABLED(FT_WSOFT)

#include <examples/wsoft/WSoftSwitchesService.h>

#include <approval_base.h>
#include <pin.h>
#include <scale.h>
#include <switches.h>
#include <wsoft_scale_setup.h>
#include <wsoft_switch.h>

// The screen task takes a switch within one pass (5 ms) unless it is in a DJB poll or a draw.
static const uint32_t SWITCH_TIMEOUT_MS = 1500;
// 8 log lines of up to WSOFT_SWITCH_LOG_MAX bytes, copied into the JSON, plus the rest.
static const size_t SWITCHES_JSON_SIZE = 3072;
static const size_t SETUP_JSON_SIZE = 1024;

namespace {

void sendError(AsyncWebServerRequest* request, int code, const char* error, const char* message) {
  AsyncJsonResponse* response = new AsyncJsonResponse(false, 256);
  JsonObject root = response->getRoot().to<JsonObject>();
  root["error"] = error;
  root["message"] = message;
  response->setCode(code);
  response->setLength();
  request->send(response);
}

// A1.
void sendSwitches(AsyncWebServerRequest* request) {
  SwitchesInfo info;
  switches_info(&info);
  // One request at a time on the web server task, so a static buffer is safe (and off its stack).
  static char lines[8][WSOFT_SWITCH_LOG_MAX];
  int count = approval_log_lines(lines, 8);

  AsyncJsonResponse* response = new AsyncJsonResponse(false, SWITCHES_JSON_SIZE);
  JsonObject root = response->getRoot().to<JsonObject>();
  root["source"] = info.rs485 ? "rs485" : "rs232";
  JsonObject approval = root.createNestedObject("approval");
  approval["on"] = info.approval_on;
  approval["audit_counter"] = info.audit_counter;
  approval["trade_check"] = trade_check_text(info.trade_check);
  approval["can_switch_on"] = info.blocked == WSOFT_SWITCH_OK;
  if (info.blocked == WSOFT_SWITCH_OK) {
    approval["blocked"] = (const char*)nullptr;
  } else {
    approval["blocked"] = wsoft_switch_error_code(info.blocked);
  }
  root["auto_save"] = info.auto_save;
  root["pin_set"] = info.pin_set;
  root["pin_locked_s"] = info.pin_locked_s;
  JsonArray log = root.createNestedArray("log");
  for (int i = 0; i < count; i++) log.add(const_cast<char*>(lines[i]));  // char*: ArduinoJson copies it
  response->setCode(200);
  response->setLength();
  request->send(response);
}

void putKg(JsonObject& board, const char* key, int64_t kg) {
  char text[24];
  wsoft_kg_text(kg, text, sizeof text);
  board[key] = const_cast<char*>(text);
}

// A4.
void sendScaleSetup(AsyncWebServerRequest* request) {
  ScaleInfo info;
  scale_info(&info);
  bool approval = approval_on();
  const char* missing[5];
  int missing_count = wsoft_scale_setup_missing(info.setup, info.card, missing);

  AsyncJsonResponse* response = new AsyncJsonResponse(false, SETUP_JSON_SIZE);
  JsonObject root = response->getRoot().to<JsonObject>();
  root["source"] = info.rs485 ? "rs485" : "rs232";
  // Whether the setup may change right now, whoever asks; the page adds the admin check.
  root["editable"] = info.rs485 && !approval;
  if (!info.rs485) {
    root["locked"] = "RS232";
  } else if (approval) {
    root["locked"] = "APPROVAL_ON";
  } else {
    root["locked"] = (const char*)nullptr;
  }
  root["complete"] = missing_count == 0;
  JsonArray keys = root.createNestedArray("missing");
  for (int i = 0; i < missing_count; i++) keys.add(missing[i]);  // string literals
  root["setup_audit"] = info.audit;

  JsonObject board = root.createNestedObject("board");
  board["sa_number"] = const_cast<char*>(info.setup.sa_number);
  putKg(board, "max_kg", info.setup.max_kg);
  putKg(board, "min_kg", info.setup.min_kg);
  if (info.setup.blank_below_e == WSOFT_BLANK_UNSET) {
    board["blank_below_e"] = (const char*)nullptr;
  } else {
    board["blank_below_e"] = info.setup.blank_below_e;
  }
  board["blank_above_e"] = info.setup.blank_above_e;
  board["stable_samples"] = info.setup.stable_samples;
  board["tare_enabled"] = info.setup.tare_enabled;
  putKg(board, "tare_max_kg", info.setup.tare_max_kg);
  putKg(board, "preset_tare_kg", info.setup.preset_tare_kg);

  if (info.card.read) {
    JsonObject card = root.createNestedObject("card");
    card["division"] = (long)info.card.division;
    card["decimal"] = info.card.decimal;
    card["unit"] = info.card.unit;
  } else {
    root["card"] = (const char*)nullptr;
  }
  response->setCode(200);
  response->setLength();
  request->send(response);
}

WsoftSetupText readText(JsonObject& board, const char* key) {
  WsoftSetupText t;
  if (!board.containsKey(key)) return t;
  t.given = true;
  JsonVariant v = board[key];
  if (v.is<const char*>()) {
    t.text = v.as<const char*>();
  } else {
    t.bad_type = true;
  }
  return t;
}

WsoftSetupInt readInt(JsonObject& board, const char* key) {
  WsoftSetupInt n;
  if (!board.containsKey(key)) return n;
  n.given = true;
  JsonVariant v = board[key];
  if (v.isNull()) {
    n.null = true;
  } else if (v.is<long>()) {
    n.value = v.as<long>();
  } else {
    n.bad_type = true;
  }
  return n;
}

}  // namespace

WSoftSwitchesService::WSoftSwitchesService(AsyncWebServer* server, SecurityManager* securityManager) :
    _securityManager(securityManager) {
  server->on(WSOFT_SWITCHES_ENDPOINT_PATH,
             HTTP_GET,
             securityManager->wrapRequest(std::bind(&WSoftSwitchesService::readSwitches, this, std::placeholders::_1),
                                          AuthenticationPredicates::IS_AUTHENTICATED));
  AsyncCallbackJsonWebHandler* switchHandler = new AsyncCallbackJsonWebHandler(
      WSOFT_SWITCHES_ENDPOINT_PATH,
      securityManager->wrapCallback(
          std::bind(&WSoftSwitchesService::updateSwitch, this, std::placeholders::_1, std::placeholders::_2),
          AuthenticationPredicates::IS_ADMIN),
      512);
  switchHandler->setMethod(HTTP_POST);
  server->addHandler(switchHandler);

  AsyncCallbackJsonWebHandler* pinHandler = new AsyncCallbackJsonWebHandler(
      WSOFT_PIN_ENDPOINT_PATH,
      securityManager->wrapCallback(
          std::bind(&WSoftSwitchesService::updatePin, this, std::placeholders::_1, std::placeholders::_2),
          AuthenticationPredicates::IS_ADMIN),
      128);
  pinHandler->setMethod(HTTP_POST);
  server->addHandler(pinHandler);

  server->on(WSOFT_SCALE_SETUP_ENDPOINT_PATH,
             HTTP_GET,
             securityManager->wrapRequest(std::bind(&WSoftSwitchesService::readScaleSetup, this, std::placeholders::_1),
                                          AuthenticationPredicates::IS_AUTHENTICATED));
  AsyncCallbackJsonWebHandler* setupHandler = new AsyncCallbackJsonWebHandler(
      WSOFT_SCALE_SETUP_ENDPOINT_PATH,
      securityManager->wrapCallback(
          std::bind(&WSoftSwitchesService::updateScaleSetup, this, std::placeholders::_1, std::placeholders::_2),
          AuthenticationPredicates::IS_ADMIN),
      1024);
  setupHandler->setMethod(HTTP_POST);
  server->addHandler(setupHandler);
}

// The signed-in name for the log. The admin check has already passed.
String WSoftSwitchesService::username(AsyncWebServerRequest* request) {
  Authentication auth = _securityManager->authenticateRequest(request);
  if (auth.authenticated && auth.user != nullptr && auth.user->username.length()) return auth.user->username;
  return "admin";
}

void WSoftSwitchesService::readSwitches(AsyncWebServerRequest* request) {
  sendSwitches(request);
}

// A2: {"switch":"approval"|"auto_save","on":true|false,"reason":"text"}.
void WSoftSwitchesService::updateSwitch(AsyncWebServerRequest* request, JsonVariant& json) {
  WsoftSwitchWhich which;
  if (!json.is<JsonObject>() || !wsoft_switch_parse(json["switch"] | "", &which) || !json["on"].is<bool>() ||
      (json.containsKey("reason") && !json["reason"].is<const char*>())) {
    sendError(request, 400, "BAD_REQUEST", "Send switch (approval or auto_save), on (true or false) and a reason.");
    return;
  }
  String who = username(request);
  WsoftSwitchError error =
      switches_set(which, json["on"].as<bool>(), who.c_str(), WSOFT_WHERE_BROWSER, json["reason"] | "", SWITCH_TIMEOUT_MS);
  if (error != WSOFT_SWITCH_OK) {
    sendError(request, 409, wsoft_switch_error_code(error), wsoft_switch_error_text(error));
    return;
  }
  sendSwitches(request);
}

// A3: {"pin":"1234"}. The PIN is checked and hashed here and never echoed or logged.
void WSoftSwitchesService::updatePin(AsyncWebServerRequest* request, JsonVariant& json) {
  const char* pin = json.is<JsonObject>() && json["pin"].is<const char*>() ? json["pin"].as<const char*>() : nullptr;
  if (pin == nullptr || !pin_save(pin)) {
    sendError(request, 400, "PIN_FORMAT", "The PIN is 4 to 6 digits.");
    return;
  }
  AsyncJsonResponse* response = new AsyncJsonResponse(false, 64);
  JsonObject root = response->getRoot().to<JsonObject>();
  root["pin_set"] = true;
  response->setCode(200);
  response->setLength();
  request->send(response);
}

void WSoftSwitchesService::readScaleSetup(AsyncWebServerRequest* request) {
  sendScaleSetup(request);
}

// A5: {"board":{...A4.board keys...},"reason":"text"}. A key that is not sent keeps its value.
void WSoftSwitchesService::updateScaleSetup(AsyncWebServerRequest* request, JsonVariant& json) {
  if (!json.is<JsonObject>() || !json["board"].is<JsonObject>() ||
      (json.containsKey("reason") && !json["reason"].is<const char*>())) {
    sendError(request, 400, "BAD_REQUEST", "Send board (the setup fields) and a reason.");
    return;
  }
  JsonObject board = json["board"].as<JsonObject>();
  WsoftScaleSetupInput in;
  in.sa_number = readText(board, "sa_number");
  in.max_kg = readText(board, "max_kg");
  in.min_kg = readText(board, "min_kg");
  in.tare_max_kg = readText(board, "tare_max_kg");
  in.preset_tare_kg = readText(board, "preset_tare_kg");
  in.blank_below_e = readInt(board, "blank_below_e");
  in.blank_above_e = readInt(board, "blank_above_e");
  in.stable_samples = readInt(board, "stable_samples");
  if (board.containsKey("tare_enabled")) {
    in.has_tare_enabled = true;
    if (board["tare_enabled"].is<bool>()) {
      in.tare_enabled = board["tare_enabled"].as<bool>();
    } else {
      in.tare_enabled_bad_type = true;
    }
  }
  in.reason = json["reason"] | "";
  String who = username(request);
  WsoftSetupError error = scale_save(in, who.c_str());
  if (error != WSOFT_SETUP_OK) {
    sendError(request, 409, wsoft_setup_error_code(error), wsoft_setup_error_text(error));
    return;
  }
  sendScaleSetup(request);
}

#endif  // FT_WSOFT
