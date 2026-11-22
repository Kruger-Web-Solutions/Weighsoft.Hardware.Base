#include <Features.h>
#if FT_ENABLED(FT_WSOFT)

#include <examples/wsoft/WSoftProductsService.h>

#include <hilo.h>

static const size_t ROWS_MAX = WSOFT_HILO_MAX + WSOFT_LOCAL_MAX;
// About 140 bytes of JSON a row, plus the rest.
static const size_t PRODUCTS_JSON_SIZE = ROWS_MAX * 160 + 256;

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

void sendProducts(AsyncWebServerRequest* request) {
  // One request at a time on the web server task, so a static buffer is safe (and off its stack).
  static HiloRow rows[ROWS_MAX];
  char current[WSOFT_HILO_CODE_MAX + 1] = "";
  size_t count = hilo_rows(rows, ROWS_MAX, current, sizeof current);

  AsyncJsonResponse* response = new AsyncJsonResponse(false, PRODUCTS_JSON_SIZE);
  JsonObject root = response->getRoot().to<JsonObject>();
  root["current"] = current;
  root["board_max"] = WSOFT_LOCAL_MAX;
  JsonArray list = root.createNestedArray("products");
  for (size_t i = 0; i < count; i++) {
    const WsoftHiloProduct& p = rows[i].product;
    JsonObject row = list.createNestedObject();
    row["code"] = p.code;
    row["name"] = p.name;
    row["low_g"] = p.low_g;
    row["target_g"] = p.target_g;
    row["high_g"] = p.high_g;
    row["from"] = rows[i].from_hub ? "hub" : "board";
    row["hidden"] = rows[i].hidden;
  }
  response->setCode(200);
  response->setLength();
  request->send(response);
}

// A whole number of grams that fits an int32, else false (never rounded).
bool grams(JsonVariant value, int32_t* out) {
  if (!value.is<long>()) return false;
  long g = value.as<long>();
  if (g < INT32_MIN || g > INT32_MAX) return false;
  *out = (int32_t)g;
  return true;
}

// Copies a string field into a fixed buffer; false when missing or too long.
bool text(JsonVariant value, char* out, size_t size) {
  if (!value.is<const char*>()) return false;
  const char* s = value.as<const char*>();
  if (strlen(s) >= size) return false;
  strlcpy(out, s, size);
  return true;
}

void sendResult(AsyncWebServerRequest* request, WsoftProductResult result) {
  if (result == WSOFT_PRODUCT_OK) {
    sendProducts(request);
    return;
  }
  int code = result == WSOFT_PRODUCT_BAD_CODE || result == WSOFT_PRODUCT_BAD_NAME || result == WSOFT_PRODUCT_BAD_LIMITS
                 ? 400
                 : 409;
  sendError(request, code, wsoft_product_result_code(result), wsoft_product_result_text(result));
}

}  // namespace

WSoftProductsService::WSoftProductsService(AsyncWebServer* server, SecurityManager* securityManager) {
  server->on(WSOFT_PRODUCTS_ENDPOINT_PATH,
             HTTP_GET,
             securityManager->wrapRequest(std::bind(&WSoftProductsService::read, this, std::placeholders::_1),
                                          AuthenticationPredicates::IS_AUTHENTICATED));
  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler(
      WSOFT_PRODUCTS_ENDPOINT_PATH,
      securityManager->wrapCallback(
          std::bind(&WSoftProductsService::update, this, std::placeholders::_1, std::placeholders::_2),
          AuthenticationPredicates::IS_ADMIN),
      512);
  handler->setMethod(HTTP_POST);
  server->addHandler(handler);
}

void WSoftProductsService::read(AsyncWebServerRequest* request) {
  sendProducts(request);
}

void WSoftProductsService::update(AsyncWebServerRequest* request, JsonVariant& json) {
  const char* action = json.is<JsonObject>() ? (json["action"] | "") : "";
  if (strcmp(action, "put") == 0) {
    JsonVariant in = json["product"];
    char old_code[WSOFT_HILO_CODE_MAX + 1] = "";
    WsoftHiloProduct product;
    if (!in.is<JsonObject>() || (!json["old_code"].isNull() && !text(json["old_code"], old_code, sizeof old_code))) {
      sendError(request, 400, "BAD_REQUEST", "Send the product: code, name, low_g, target_g and high_g.");
      return;
    }
    if (!text(in["code"], product.code, sizeof product.code)) {
      sendResult(request, WSOFT_PRODUCT_BAD_CODE);
      return;
    }
    if (!text(in["name"], product.name, sizeof product.name)) {
      sendResult(request, WSOFT_PRODUCT_BAD_NAME);
      return;
    }
    if (!grams(in["low_g"], &product.low_g) || !grams(in["target_g"], &product.target_g) ||
        !grams(in["high_g"], &product.high_g)) {
      sendResult(request, WSOFT_PRODUCT_BAD_LIMITS);
      return;
    }
    sendResult(request, hilo_board_put(old_code, product));
    return;
  }
  if (strcmp(action, "delete") == 0) {
    char code[WSOFT_HILO_CODE_MAX + 1];
    if (!text(json["code"], code, sizeof code)) {
      sendResult(request, WSOFT_PRODUCT_NOT_FOUND);
      return;
    }
    sendResult(request, hilo_board_remove(code));
    return;
  }
  if (strcmp(action, "use") == 0) {
    char code[WSOFT_HILO_CODE_MAX + 1];
    if (!text(json["code"], code, sizeof code) || !hilo_pick(code)) {
      sendResult(request, WSOFT_PRODUCT_NOT_FOUND);
      return;
    }
    sendProducts(request);
    return;
  }
  sendError(request, 400, "BAD_REQUEST", "Send action: put, delete or use.");
}

#endif  // FT_WSOFT
