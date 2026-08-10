#include <examples/liveweight/LiveWeightService.h>
#include <examples/relay/RelayBoardService.h>
#include <Features.h>
#include <AsyncJson.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <regex.h>
#endif
#ifdef ESP32
#include <WiFi.h>
#include <regex.h>
#endif

namespace {

// One row of the CSV report at a time. Holding the open file plus a small
// pending buffer keeps peak RAM at one row instead of the whole report.
struct CsvPump {
  File file;
  String pending;
  bool headerSent = false;
};

// RFC 4180 quoting. Product names are operator-typed, so a comma or a quote in
// "Sand, washed" would otherwise shift every later column silently.
String csvField(const String& value) {
  bool mustQuote = false;
  for (size_t i = 0; i < value.length(); i++) {
    const char c = value[i];
    if (c == ',' || c == '"' || c == '\n' || c == '\r') {
      mustQuote = true;
      break;
    }
  }
  if (!mustQuote) {
    return value;
  }
  String out = "\"";
  for (size_t i = 0; i < value.length(); i++) {
    const char c = value[i];
    if (c == '"') {
      out += "\"\"";  // a quote is escaped by doubling it
    } else {
      out += c;
    }
  }
  out += '"';
  return out;
}

regex_t s_cachedRegex;
String s_cachedRegexPattern;
bool s_regexReady = false;

void freeCachedRegex() {
  if (s_regexReady) {
    regfree(&s_cachedRegex);
    s_regexReady = false;
  }
  s_cachedRegexPattern = "";
}
}  // namespace

LiveWeightService::LiveWeightService(AsyncWebServer* server,
                                     FS* fs,
                                     SecurityManager* securityManager,
                                     AsyncMqttClient* mqttClient) :
    _httpEndpoint(LiveWeightState::read,
                  LiveWeightState::update,
                  this,
                  server,
                  LIVE_WEIGHT_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::NONE_REQUIRED),
    _httpConfigEndpoint(LiveWeightState::readConfig,
                        LiveWeightState::updateConfig,
                        this,
                        server,
                        LIVE_WEIGHT_CONFIG_ENDPOINT_PATH,
                        securityManager,
                        AuthenticationPredicates::IS_AUTHENTICATED),
    _fsPersistence(LiveWeightState::readConfig,
                   LiveWeightState::updateConfig,
                   this,
                   fs,
                   LIVE_WEIGHT_CONFIG_FILE),
#if FT_ENABLED(FT_MQTT)
    _mqttPubSub(LiveWeightState::read, LiveWeightState::update, this, mqttClient),
#endif
    _webSocket(LiveWeightState::read,
               LiveWeightState::update,
               this,
               server,
               LIVE_WEIGHT_SOCKET_PATH,
               securityManager,
               AuthenticationPredicates::NONE_REQUIRED),
#if FT_ENABLED(FT_MQTT)
    _mqttClient(mqttClient),
#endif
    _server(server),
    _securityManager(securityManager),
    _fs(fs),
    _relayBoard(nullptr),
    _productCount(0),
    _serialConfigured(false),
    _appliedSource(LIVE_WEIGHT_SOURCE_WIFI),
    _appliedBaud(LIVE_WEIGHT_DEFAULT_BAUD),
    _appliedRegex(LIVE_WEIGHT_DEFAULT_REGEX),
    _appliedRs485Enabled(false),
    _appliedRs485Address(1),
    _appliedRangeEnabled(false),
    _appliedRangeLow(1.0f),
    _appliedRangeHigh(2.0f),
    _appliedRelayLow(1),
    _appliedRelayOk(2),
    _appliedRelayHigh(3),
    _appliedCount(1),
    _appliedUnit("kg"),
    _appliedDi1Action("none"),
    _appliedDi2Action("none"),
    _appliedPrinterEnabled(false),
    _appliedPrinterIp(""),
    _appliedPrinterPort(9100),
    _lastDrivenZone(255),
    _pendingAction(""),
    _printPending(false),
    _txPending(false),
    _lastSerialPublishMs(0) {
#if FT_ENABLED(FT_MQTT)
  _mqttBasePath = SettingValue::format("weighsoft/liveWeight/#{unique_id}");
  _mqttClient->onConnect(std::bind(&LiveWeightService::configureMqtt, this));
#else
  (void)mqttClient;
#endif
  _fsPersistence.disableUpdateHandler();

  addUpdateHandler(
      [this](const String& originId) {
        if (originId == "init" || originId == "band" || originId == "print" || originId == "print_prep" ||
            originId == "di_action") {
          return;
        }
        // Queue print from REST trigger_action — never block AsyncWebServer disconnect path
        if (_state.printRequested) {
          _printPending = true;
        }
        // Same for the weigh log. A DI press logs directly in handleDiAction; a REST/UI "next"
        // reached here doing nothing, so on-screen Next incremented the count but recorded
        // no transaction. File I/O is deferred to loop() for the same reason print is.
        if (_state.nextRequested) {
          _txPending = true;
        }
        if (originId == "serial_hw" || originId == "http" || originId == "mqtt" || originId.startsWith("websocket")) {
          if (configChanged()) {
            onConfigUpdated();
          }
          evaluateBandAndDrive(originId);
          return;
        }
        if (configChanged()) {
          onConfigUpdated();
          evaluateBandAndDrive("config");
        } else {
          evaluateBandAndDrive(originId);
        }
      },
      false);
}

void LiveWeightService::setRelayBoardService(RelayBoardService* relayBoard) {
  _relayBoard = relayBoard;
}

bool LiveWeightService::sourceSettingsChanged() const {
  return _state.source != _appliedSource || _state.baudrate != _appliedBaud || _state.regexPattern != _appliedRegex ||
         _state.rs485Enabled != _appliedRs485Enabled || _state.rs485Address != _appliedRs485Address;
}

bool LiveWeightService::configChanged() const {
  // Persisted settings only — not runtime last_action / action_seq / job_running
  return sourceSettingsChanged() || _state.rangeEnabled != _appliedRangeEnabled || _state.rangeLow != _appliedRangeLow ||
         _state.rangeHigh != _appliedRangeHigh || _state.relayLow != _appliedRelayLow ||
         _state.relayOk != _appliedRelayOk || _state.relayHigh != _appliedRelayHigh || _state.plu != _appliedPlu ||
         _state.product != _appliedProduct || _state.count != _appliedCount || _state.unit != _appliedUnit ||
         _state.di1Action != _appliedDi1Action || _state.di2Action != _appliedDi2Action ||
         _state.printerEnabled != _appliedPrinterEnabled || _state.printerIp != _appliedPrinterIp ||
         _state.printerPort != _appliedPrinterPort;
}

void LiveWeightService::syncAppliedConfig() {
  _appliedSource = _state.source;
  _appliedBaud = _state.baudrate;
  _appliedRegex = _state.regexPattern;
  _appliedRs485Enabled = _state.rs485Enabled;
  _appliedRs485Address = _state.rs485Address;
  _appliedRangeEnabled = _state.rangeEnabled;
  _appliedRangeLow = _state.rangeLow;
  _appliedRangeHigh = _state.rangeHigh;
  _appliedRelayLow = _state.relayLow;
  _appliedRelayOk = _state.relayOk;
  _appliedRelayHigh = _state.relayHigh;
  _appliedPlu = _state.plu;
  _appliedProduct = _state.product;
  _appliedCount = _state.count;
  _appliedUnit = _state.unit;
  _appliedDi1Action = _state.di1Action;
  _appliedDi2Action = _state.di2Action;
  _appliedPrinterEnabled = _state.printerEnabled;
  _appliedPrinterIp = _state.printerIp;
  _appliedPrinterPort = _state.printerPort;
}

void LiveWeightService::begin() {
  _state.source = LIVE_WEIGHT_SOURCE_WIFI;
  _state.baudrate = LIVE_WEIGHT_DEFAULT_BAUD;
  _state.regexPattern = LIVE_WEIGHT_DEFAULT_REGEX;
  _state.rs485Enabled = false;
  _state.rs485Address = 1;
  _state.rangeEnabled = false;
  _state.rangeLow = 1.0f;
  _state.rangeHigh = 2.0f;
  _state.relayLow = 1;
  _state.relayOk = 2;
  _state.relayHigh = 3;
  _state.plu = "";
  _state.product = "";
  _state.count = 1;
  _state.unit = "kg";
  _state.total = "";
  _state.zone = LIVE_WEIGHT_ZONE_NONE;
  _state.di1Action = "none";
  _state.di2Action = "none";
  _state.jobRunning = false;
  _state.lastAction = "";
  _state.actionSeq = 0;
  _state.printerEnabled = false;
  _state.printerIp = "";
  _state.printerPort = 9100;
  _state.printRequested = false;
  _state.nextRequested = false;
  _state.weight = "";
  _state.lastLine = "";
  _state.timestamp = 0;
  _state.activeSource = "none";
  _state.statusMessage = "Waiting for weight";
  _lineBuffer = "";

  _fsPersistence.readFromFS();
  LiveWeightState::refreshTotal(_state);

  if (_state.source == LIVE_WEIGHT_SOURCE_RS485) {
    _state.statusMessage = "RS-485 selected — hardware adapter not fitted yet";
  } else if (_state.source == LIVE_WEIGHT_SOURCE_SERIAL) {
    _state.statusMessage = "Listening on UART0 (MAX3232 / PROG header)";
  } else {
    _state.statusMessage = "Ready for WiFi / WebSocket weight";
  }

  applySource();
  loadProducts();
  registerCatalogEndpoints();
  _discovery.begin();
  Serial.println(F("[LiveWeight] Service ready — /rest/liveWeight /ws/liveWeight (+ products/tx)"));
  Serial.println(F("[LiveWeight] Discovery: UDP :4210 + mDNS _weighsoft-lw._tcp"));
}

void LiveWeightService::loop() {
  if (_pendingAction.length() > 0) {
    String action = _pendingAction;
    _pendingAction = "";
    runAction(action);
  }
  if (_printPending) {
    _printPending = false;
    sendNetworkPrint();
  }
  if (_txPending) {
    _txPending = false;
    update(
        [&](LiveWeightState& state) {
          state.nextRequested = false;
          return StateUpdateResult::CHANGED;
        },
        "next_logged");
    appendTransaction("next");
  }
  if (_state.source == LIVE_WEIGHT_SOURCE_SERIAL) {
    readSerialLine();
  }
  _discovery.loop();
}

void LiveWeightService::onConfigUpdated() {
  const bool reapplySource = sourceSettingsChanged();
  _fsPersistence.writeToFS();
  if (reapplySource) {
    applySource();
  } else {
    syncAppliedConfig();
  }
}

void LiveWeightService::applySource() {
  syncAppliedConfig();
  invalidateRegexCache();

  if (_state.source == LIVE_WEIGHT_SOURCE_SERIAL) {
    uint32_t baud = _state.baudrate;
    if (baud < LIVE_WEIGHT_MIN_BAUD || baud > LIVE_WEIGHT_MAX_BAUD) {
      baud = LIVE_WEIGHT_DEFAULT_BAUD;
    }
    Serial.flush();
    Serial.begin(baud);
    delay(50);
    _serialConfigured = true;
    _lineBuffer = "";
    update(
        [&](LiveWeightState& state) {
          state.statusMessage = "Listening on UART0 (MAX3232 / PROG header)";
          state.activeSource = "serial";
          return StateUpdateResult::CHANGED;
        },
        "init");
    Serial.printf("[LiveWeight] Serial source @ %lu baud\n", (unsigned long)baud);
    return;
  }

  if (_state.source == LIVE_WEIGHT_SOURCE_RS485) {
    _serialConfigured = false;
    update(
        [&](LiveWeightState& state) {
          state.statusMessage = "RS-485 stub — fit a transceiver module to enable";
          state.activeSource = "rs485";
          return StateUpdateResult::CHANGED;
        },
        "init");
    return;
  }

  if (_serialConfigured) {
    Serial.flush();
    Serial.begin(115200);
    delay(50);
    _serialConfigured = false;
  }
  update(
      [&](LiveWeightState& state) {
        state.statusMessage = "Ready for WiFi / WebSocket weight";
        state.activeSource = "wifi";
        return StateUpdateResult::CHANGED;
      },
      "init");
}

uint8_t LiveWeightService::computeZone(float weight) const {
  if (!_state.rangeEnabled) {
    return LIVE_WEIGHT_ZONE_NONE;
  }
  if (weight < _state.rangeLow) {
    return LIVE_WEIGHT_ZONE_LOW;
  }
  if (weight > _state.rangeHigh) {
    return LIVE_WEIGHT_ZONE_HIGH;
  }
  return LIVE_WEIGHT_ZONE_OK;
}

void LiveWeightService::evaluateBandAndDrive(const String& originId) {
  uint8_t zone = LIVE_WEIGHT_ZONE_NONE;
  if (_state.weight.length() > 0) {
    zone = computeZone(_state.weight.toFloat());
  }

  String nextTotal = _state.weight;
  if (_state.weight.length() > 0 && _state.count > 0) {
    nextTotal = String(_state.weight.toFloat() * (float)_state.count, 3);
  }

  if (zone != _state.zone || nextTotal != _state.total) {
    update(
        [&](LiveWeightState& state) {
          state.zone = zone;
          LiveWeightState::refreshTotal(state);
          if (state.rangeEnabled) {
            if (zone == LIVE_WEIGHT_ZONE_LOW) {
              state.statusMessage = "UNDER target range";
            } else if (zone == LIVE_WEIGHT_ZONE_OK) {
              state.statusMessage = "IN range";
            } else if (zone == LIVE_WEIGHT_ZONE_HIGH) {
              state.statusMessage = "OVER target range";
            }
          }
          return StateUpdateResult::CHANGED;
        },
        "band");
  }

  if (!_relayBoard) {
    return;
  }

  uint8_t driveZone = _state.rangeEnabled ? zone : LIVE_WEIGHT_ZONE_NONE;
  if (driveZone == _lastDrivenZone && originId != "config") {
    return;
  }
  _lastDrivenZone = driveZone;
  _relayBoard->setWeightBandRelays(_state.relayLow, _state.relayOk, _state.relayHigh, driveZone);
}

void LiveWeightService::invalidateRegexCache() {
  freeCachedRegex();
}

bool LiveWeightService::ensureRegexCompiled(const String& pattern) {
  if (pattern.length() == 0) {
    return false;
  }
  if (s_regexReady && s_cachedRegexPattern == pattern) {
    return true;
  }
  freeCachedRegex();
  if (regcomp(&s_cachedRegex, pattern.c_str(), REG_EXTENDED) != 0) {
    return false;
  }
  s_cachedRegexPattern = pattern;
  s_regexReady = true;
  return true;
}

void LiveWeightService::readSerialLine() {
  // Bound work per loop so a chatty scale cannot starve WiFi / AsyncWebServer
  uint8_t budget = LIVE_WEIGHT_SERIAL_BYTES_PER_LOOP;
  while (budget-- > 0 && Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (_lineBuffer.length() > 0) {
        String extracted = extractWeight(_lineBuffer);
        String line = _lineBuffer;
        _lineBuffer = "";

        // Change-gate: identical line/weight → no publish
        if (_state.lastLine == line && _state.weight == extracted) {
          continue;
        }

        // Rate-limit publishes to ≤5 Hz (still drain UART every loop)
        unsigned long now = millis();
        if (_lastSerialPublishMs != 0 && (unsigned long)(now - _lastSerialPublishMs) < LIVE_WEIGHT_PUBLISH_MIN_MS) {
          continue;
        }

        StateUpdateResult result = update(
            [&](LiveWeightState& state) {
              if (state.lastLine == line && state.weight == extracted) {
                return StateUpdateResult::UNCHANGED;
              }
              state.lastLine = line;
              state.weight = extracted;
              state.timestamp = now;
              state.activeSource = "serial";
              state.statusMessage = extracted.length() ? "Weight from serial" : "Serial line (no weight match)";
              LiveWeightState::refreshTotal(state);
              return StateUpdateResult::CHANGED;
            },
            "serial_hw");
        if (result == StateUpdateResult::CHANGED) {
          _lastSerialPublishMs = now;
        }
      }
    } else {
      if (_lineBuffer.length() < LIVE_WEIGHT_LINE_MAX) {
        _lineBuffer += c;
      } else {
        // Overflow: drop line (scale framing error or noise)
        _lineBuffer = "";
      }
    }
  }
}

String LiveWeightService::extractWeightSimple(const String& line) {
  int start = -1;
  for (unsigned int i = 0; i < line.length(); i++) {
    char c = line[i];
    if ((c >= '0' && c <= '9') || c == '+' || c == '-') {
      start = (int)i;
      break;
    }
  }
  if (start < 0) {
    return "";
  }
  int end = start + 1;
  bool seenDot = false;
  for (; end < (int)line.length(); end++) {
    char c = line[end];
    if (c >= '0' && c <= '9') {
      continue;
    }
    if ((c == '.' || c == ',') && !seenDot) {
      seenDot = true;
      continue;
    }
    break;
  }
  String extracted = line.substring(start, end);
  extracted.replace(',', '.');
  float value = extracted.toFloat();
  if (value != 0.0f || extracted.indexOf('0') >= 0) {
    return String(value, 3);
  }
  return "";
}

String LiveWeightService::extractWeight(const String& line) {
  const String& pattern = _state.regexPattern;
  // Production path: simple numeric scan (cheap). Custom Tech regex only if non-default.
  if (pattern.length() == 0 || pattern == LIVE_WEIGHT_DEFAULT_REGEX) {
    return extractWeightSimple(line);
  }

  if (!ensureRegexCompiled(pattern)) {
    return extractWeightSimple(line);
  }

  regmatch_t matches[2];
  int reti = regexec(&s_cachedRegex, line.c_str(), 2, matches, 0);
  if (reti == 0) {
    int start = matches[1].rm_so >= 0 ? matches[1].rm_so : matches[0].rm_so;
    int end = matches[1].rm_eo >= 0 ? matches[1].rm_eo : matches[0].rm_eo;
    if (start >= 0 && end > start) {
      String extracted = line.substring(start, end);
      extracted.replace(',', '.');
      float value = extracted.toFloat();
      if (value != 0.0f || extracted.indexOf('0') >= 0) {
        return String(value, 3);
      }
    }
  }

  return extractWeightSimple(line);
}

void LiveWeightService::onDiEdge(uint8_t diIndex, bool active) {
  if (!active) {
    return;
  }
  // Defer work to loop() so RelayBoard can finish DI state update without blocking on TCP/FS
  if (diIndex == 1) {
    _pendingAction = _state.di1Action;
  } else if (diIndex == 2) {
    _pendingAction = _state.di2Action;
  }
}

void LiveWeightService::runAction(const String& action) {
  String a = LiveWeightState::normalizeAction(action);
  if (a == "none") {
    return;
  }

  update(
      [&](LiveWeightState& state) {
        if (a == "next") {
          state.count++;
          LiveWeightState::refreshTotal(state);
          state.lastAction = "next";
          state.actionSeq++;
          state.statusMessage = "Next piece";
          return StateUpdateResult::CHANGED;
        }
        if (a == "start") {
          state.jobRunning = true;
          state.lastAction = "start";
          state.actionSeq++;
          state.statusMessage = "Job started";
          return StateUpdateResult::CHANGED;
        }
        if (a == "stop") {
          state.jobRunning = false;
          state.lastAction = "stop";
          state.actionSeq++;
          state.statusMessage = "Job stopped";
          return StateUpdateResult::CHANGED;
        }
        if (a == "print") {
          state.lastAction = "print";
          state.actionSeq++;
          state.statusMessage = "Print requested";
          state.printRequested = false;
          return StateUpdateResult::CHANGED;
        }
        return StateUpdateResult::UNCHANGED;
      },
      "di_action");

  if (a == "next") {
    // Persist piece count without re-opening UART
    if (configChanged()) {
      onConfigUpdated();
    }
    evaluateBandAndDrive("di_action");
    appendTransaction("next");
  } else if (a == "print") {
    _printPending = true;
  }
}

void LiveWeightService::sendNetworkPrint() {
  String ip;
  uint16_t port = 9100;
  char ticket[384];
  bool abortPrint = false;

  update(
      [&](LiveWeightState& state) {
        state.printRequested = false;
        if (!state.printerEnabled || state.printerIp.length() == 0) {
          state.statusMessage = "Print failed — no printer IP";
          abortPrint = true;
          return StateUpdateResult::CHANGED;
        }
        ip = state.printerIp;
        port = state.printerPort ? state.printerPort : 9100;
        snprintf(ticket,
                 sizeof(ticket),
                 "================================\n"
                 "Weighsoft Live Weight\n"
                 "================================\n"
                 "PLU: %s\n"
                 "Product: %s\n"
                 "Weight: %s %s\n"
                 "Count: %lu\n"
                 "Total: %s %s\n"
                 "================================\n\n",
                 state.plu.c_str(),
                 state.product.c_str(),
                 state.weight.c_str(),
                 state.unit.c_str(),
                 (unsigned long)state.count,
                 state.total.c_str(),
                 state.unit.c_str());
        return StateUpdateResult::CHANGED;
      },
      "print_prep");

  if (abortPrint) {
    return;
  }

  WiFiClient client;
  client.setTimeout(2000);
  if (!client.connect(ip.c_str(), port)) {
    update(
        [&](LiveWeightState& state) {
          state.statusMessage = "Print failed — connect";
          return StateUpdateResult::CHANGED;
        },
        "print");
    return;
  }

  // ESC/POS: initialize, plain text, partial cut
  client.write((const uint8_t*)"\x1b\x40", 2);
  client.print(ticket);
  client.write((const uint8_t*)"\x1d\x56\x00", 3);
  client.flush();
  client.stop();

  update(
      [&](LiveWeightState& state) {
        state.statusMessage = "Print sent";
        return StateUpdateResult::CHANGED;
      },
      "print");
  appendTransaction("print");
}

void LiveWeightService::loadProducts() {
  _productCount = 0;
  if (!_fs || !_fs->exists(LIVE_WEIGHT_PRODUCTS_FILE)) {
    return;
  }
  File f = _fs->open(LIVE_WEIGHT_PRODUCTS_FILE, "r");
  if (!f) {
    return;
  }
  DynamicJsonDocument doc(1536);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    return;
  }
  JsonArray arr = doc["products"].as<JsonArray>();
  if (arr.isNull()) {
    return;
  }
  for (JsonObject o : arr) {
    if (_productCount >= LIVE_WEIGHT_MAX_PRODUCTS) {
      break;
    }
    _products[_productCount].plu = o["plu"] | "";
    _products[_productCount].product = o["product"] | "";
    _products[_productCount].unit = o["unit"] | "kg";
    if (_products[_productCount].plu.length() > 0) {
      _productCount++;
    }
  }
}

bool LiveWeightService::saveProducts() {
  if (!_fs) {
    return false;
  }
  DynamicJsonDocument doc(1536);
  JsonArray arr = doc.createNestedArray("products");
  for (uint8_t i = 0; i < _productCount; i++) {
    JsonObject o = arr.createNestedObject();
    o["plu"] = _products[i].plu;
    o["product"] = _products[i].product;
    o["unit"] = _products[i].unit;
  }
  File f = _fs->open(LIVE_WEIGHT_PRODUCTS_FILE, "w");
  if (!f) {
    return false;
  }
  serializeJson(doc, f);
  f.close();
  return true;
}

int LiveWeightService::findProductIndex(const String& plu) const {
  for (uint8_t i = 0; i < _productCount; i++) {
    if (_products[i].plu == plu) {
      return (int)i;
    }
  }
  return -1;
}

void LiveWeightService::appendTransaction(const char* reason) {
  if (!_fs) {
    return;
  }
  LiveWeightState snap;
  read([&](LiveWeightState& state) { snap = state; });
  if (!snap.weight.length()) {
    return;
  }
  _fs->mkdir("/log");

  char line[220];
  snprintf(line,
           sizeof(line),
           "{\"t\":%lu,\"reason\":\"%s\",\"plu\":\"%s\",\"product\":\"%s\",\"weight\":\"%s\",\"count\":%lu,\"total\":\"%s\",\"unit\":\"%s\"}\n",
           (unsigned long)millis(),
           reason ? reason : "",
           snap.plu.c_str(),
           snap.product.c_str(),
           snap.weight.c_str(),
           (unsigned long)snap.count,
           snap.total.c_str(),
           snap.unit.c_str());

  uint16_t count = 0;
  if (_fs->exists(LIVE_WEIGHT_TX_FILE)) {
    File rf = _fs->open(LIVE_WEIGHT_TX_FILE, "r");
    if (rf) {
      while (rf.available()) {
        if (rf.read() == '\n') {
          count++;
        }
      }
      rf.close();
    }
  }

  if (count < LIVE_WEIGHT_MAX_TX) {
    File af = _fs->open(LIVE_WEIGHT_TX_FILE, "a");
    if (af) {
      af.print(line);
      af.close();
    }
    return;
  }

  // Ring: rewrite file, drop oldest line, append new (temp file — ESP8266-safe)
  const char* tmpPath = "/log/transactions.tmp";
  File in = _fs->open(LIVE_WEIGHT_TX_FILE, "r");
  File out = _fs->open(tmpPath, "w");
  if (!out) {
    if (in) {
      in.close();
    }
    return;
  }
  bool skipOldest = true;
  if (in) {
    while (in.available()) {
      String l = in.readStringUntil('\n');
      l.trim();
      if (l.length() == 0) {
        continue;
      }
      if (skipOldest) {
        skipOldest = false;
        continue;
      }
      out.println(l);
    }
    in.close();
  }
  out.print(line);
  out.close();
  _fs->remove(LIVE_WEIGHT_TX_FILE);
  // LittleFS rename may be unavailable — copy-back fallback
  File src = _fs->open(tmpPath, "r");
  File dst = _fs->open(LIVE_WEIGHT_TX_FILE, "w");
  if (src && dst) {
    while (src.available()) {
      dst.write(src.read());
    }
  }
  if (src) {
    src.close();
  }
  if (dst) {
    dst.close();
  }
  _fs->remove(tmpPath);
}

void LiveWeightService::registerCatalogEndpoints() {
  if (!_server || !_securityManager) {
    return;
  }

  _server->on(
      LIVE_WEIGHT_PRODUCTS_PATH,
      HTTP_GET,
      _securityManager->wrapRequest(
          [this](AsyncWebServerRequest* request) {
            AsyncJsonResponse* response = new AsyncJsonResponse(false, 1536);
            JsonObject root = response->getRoot();
            root["max"] = LIVE_WEIGHT_MAX_PRODUCTS;
            root["count"] = _productCount;
            JsonArray arr = root.createNestedArray("products");
            for (uint8_t i = 0; i < _productCount; i++) {
              JsonObject o = arr.createNestedObject();
              o["plu"] = _products[i].plu;
              o["product"] = _products[i].product;
              o["unit"] = _products[i].unit;
            }
            response->setLength();
            request->send(response);
          },
          AuthenticationPredicates::NONE_REQUIRED));

  AsyncCallbackJsonWebHandler* productsPost = new AsyncCallbackJsonWebHandler(
      LIVE_WEIGHT_PRODUCTS_PATH,
      _securityManager->wrapCallback(
          [this](AsyncWebServerRequest* request, JsonVariant& json) {
            if (!json.is<JsonObject>()) {
              request->send(400, "application/json", "{\"error\":\"invalid json\"}");
              return;
            }
            JsonObject root = json.as<JsonObject>();
            String action = root["action"] | "upsert";
            // select = operator (public); upsert/delete = catalog config (login)
            if (action != "select") {
              Authentication auth = _securityManager->authenticateRequest(request);
              if (!AuthenticationPredicates::IS_AUTHENTICATED(auth)) {
                request->send(401, "application/json", "{\"error\":\"login required\"}");
                return;
              }
            }
            if (action == "delete") {
              String plu = root["plu"] | "";
              int idx = findProductIndex(plu);
              if (idx < 0) {
                request->send(404, "application/json", "{\"error\":\"not found\"}");
                return;
              }
              for (uint8_t i = (uint8_t)idx; i + 1 < _productCount; i++) {
                _products[i] = _products[i + 1];
              }
              _productCount--;
              saveProducts();
            } else if (action == "select") {
              String plu = root["plu"] | "";
              int idx = findProductIndex(plu);
              if (idx < 0) {
                request->send(404, "application/json", "{\"error\":\"not found\"}");
                return;
              }
              update(
                  [&](LiveWeightState& state) {
                    state.plu = _products[idx].plu;
                    state.product = _products[idx].product;
                    state.unit = _products[idx].unit;
                    LiveWeightState::refreshTotal(state);
                    return StateUpdateResult::CHANGED;
                  },
                  "catalog");
              if (configChanged()) {
                onConfigUpdated();
              }
            } else {
              String plu = root["plu"] | "";
              if (plu.length() == 0) {
                request->send(400, "application/json", "{\"error\":\"plu required\"}");
                return;
              }
              int idx = findProductIndex(plu);
              if (idx < 0) {
                if (_productCount >= LIVE_WEIGHT_MAX_PRODUCTS) {
                  request->send(400, "application/json", "{\"error\":\"max 9 products\"}");
                  return;
                }
                idx = _productCount++;
              }
              _products[idx].plu = plu;
              _products[idx].product = root["product"] | "";
              _products[idx].unit = root["unit"] | "kg";
              saveProducts();
            }

            AsyncJsonResponse* response = new AsyncJsonResponse(false, 1536);
            JsonObject out = response->getRoot();
            out["max"] = LIVE_WEIGHT_MAX_PRODUCTS;
            out["count"] = _productCount;
            JsonArray arr = out.createNestedArray("products");
            for (uint8_t i = 0; i < _productCount; i++) {
              JsonObject o = arr.createNestedObject();
              o["plu"] = _products[i].plu;
              o["product"] = _products[i].product;
              o["unit"] = _products[i].unit;
            }
            response->setLength();
            request->send(response);
          },
          AuthenticationPredicates::NONE_REQUIRED),
      1024);
  productsPost->setMethod(HTTP_POST);
  _server->addHandler(productsPost);

  _server->on(
      LIVE_WEIGHT_TX_PATH,
      HTTP_GET,
      _securityManager->wrapRequest(
          [this](AsyncWebServerRequest* request) {
            AsyncJsonResponse* response = new AsyncJsonResponse(false, 4096);
            JsonObject root = response->getRoot();
            root["max"] = LIVE_WEIGHT_MAX_TX;
            JsonArray arr = root.createNestedArray("transactions");
            uint16_t n = 0;
            if (_fs && _fs->exists(LIVE_WEIGHT_TX_FILE)) {
              File f = _fs->open(LIVE_WEIGHT_TX_FILE, "r");
              if (f) {
                while (f.available() && n < LIVE_WEIGHT_MAX_TX) {
                  String l = f.readStringUntil('\n');
                  l.trim();
                  if (l.length() == 0) {
                    continue;
                  }
                  DynamicJsonDocument lineDoc(256);
                  if (deserializeJson(lineDoc, l) == DeserializationError::Ok) {
                    arr.add(lineDoc.as<JsonObject>());
                    n++;
                  }
                }
                f.close();
              }
            }
            root["count"] = n;
            response->setLength();
            request->send(response);
          },
          AuthenticationPredicates::NONE_REQUIRED));

  // CSV report of the weigh log, for the operator to keep or send on.
  // Streamed, never assembled in RAM: 40 rows is small today, but a String that
  // size fragments a heap that has been observed at 5 KB under load (RT-080),
  // and the row cap is a product decision that could rise later.
  _server->on(
      LIVE_WEIGHT_REPORT_PATH,
      HTTP_GET,
      _securityManager->wrapRequest(
          [this](AsyncWebServerRequest* request) {
            auto pump = std::make_shared<CsvPump>();
            if (_fs && _fs->exists(LIVE_WEIGHT_TX_FILE)) {
              pump->file = _fs->open(LIVE_WEIGHT_TX_FILE, "r");
            }

            AsyncWebServerResponse* response = request->beginChunkedResponse(
                "text/csv",
                [pump](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
                  (void)index;
                  if (!pump->headerSent) {
                    pump->pending = F("timestamp_ms,reason,plu,product,weight,count,total,unit\n");
                    pump->headerSent = true;
                  }
                  // Top the buffer up a row at a time so peak RAM is one row,
                  // not one report.
                  while (pump->pending.length() < maxLen && pump->file && pump->file.available()) {
                    String line = pump->file.readStringUntil('\n');
                    line.trim();
                    if (!line.length()) {
                      continue;
                    }
                    DynamicJsonDocument doc(256);
                    if (deserializeJson(doc, line) != DeserializationError::Ok) {
                      continue;  // a torn line must not abort the whole report
                    }
                    pump->pending += String((unsigned long)(doc["t"] | 0UL));
                    pump->pending += ',';
                    pump->pending += csvField(doc["reason"] | "");
                    pump->pending += ',';
                    pump->pending += csvField(doc["plu"] | "");
                    pump->pending += ',';
                    pump->pending += csvField(doc["product"] | "");
                    pump->pending += ',';
                    pump->pending += csvField(doc["weight"] | "");
                    pump->pending += ',';
                    pump->pending += String((unsigned long)(doc["count"] | 0UL));
                    pump->pending += ',';
                    pump->pending += csvField(doc["total"] | "");
                    pump->pending += ',';
                    pump->pending += csvField(doc["unit"] | "");
                    pump->pending += '\n';
                  }

                  if (!pump->pending.length()) {
                    if (pump->file) {
                      pump->file.close();
                    }
                    return 0;  // nothing left: ends the response
                  }
                  size_t take = pump->pending.length() < maxLen ? pump->pending.length() : maxLen;
                  memcpy(buffer, pump->pending.c_str(), take);
                  pump->pending.remove(0, take);
                  return take;
                });

            // Makes the browser save a file instead of rendering the text.
            // Same id form the discovery endpoint reports, so the filename
            // matches the board the operator sees on screen.
#ifdef ESP8266
            String boardId = String(ESP.getChipId(), HEX);
#elif defined(ESP32)
            String boardId = String((uint32_t)ESP.getEfuseMac(), HEX);
#else
            String boardId = "board";
#endif
            String filename = "weighsoft-report-" + boardId + ".csv";
            response->addHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
            request->send(response);
          },
          AuthenticationPredicates::NONE_REQUIRED));

  // Discovery identity + optional unicast poke to the caller (desk / AP broadcast filter)
  _server->on(
      LIVE_WEIGHT_DISCOVERY_REST_PATH,
      HTTP_GET,
      _securityManager->wrapRequest(
          [this](AsyncWebServerRequest* request) {
            _discovery.announce();
            bool unicastOk = false;
            if (request->client()) {
              unicastOk = _discovery.announceTo(request->client()->remoteIP());
            }
            AsyncJsonResponse* response = new AsyncJsonResponse(false, 1024);
            JsonObject root = response->getRoot();
            root["v"] = 1;
            root["svc"] = LIVE_WEIGHT_DISCOVERY_SVC;
            root["udp_port"] = LIVE_WEIGHT_DISCOVERY_UDP_PORT;
            root["udp_ready"] = _discovery.udpReady();
            root["last_send_ok"] = _discovery.lastSendOk();
            root["unicast_to_client_ok"] = unicastOk;
            root["mdns"] = String("_") + LIVE_WEIGHT_DISCOVERY_SVC + "._tcp.local";
            root["rest"] = LIVE_WEIGHT_ENDPOINT_PATH;
            root["ws"] = LIVE_WEIGHT_SOCKET_PATH;
            root["http"] = 80;
            if (WiFi.status() == WL_CONNECTED) {
              root["ip"] = WiFi.localIP().toString();
#ifdef ESP8266
              root["host"] = WiFi.hostname();
              root["id"] = String(ESP.getChipId(), HEX);
#elif defined(ESP32)
              root["host"] = WiFi.getHostname() ? WiFi.getHostname() : "";
              root["id"] = String((uint32_t)ESP.getEfuseMac(), HEX);
#endif
            }
            response->setLength();
            request->send(response);
          },
          AuthenticationPredicates::NONE_REQUIRED));
}

#if FT_ENABLED(FT_MQTT)
void LiveWeightService::configureMqtt() {
  if (!_mqttClient->connected()) {
    return;
  }
  String pubTopic = _mqttBasePath + "/data";
  String subTopic = _mqttBasePath + "/set";
  _mqttPubSub.configureTopics(pubTopic, subTopic);
}
#endif
