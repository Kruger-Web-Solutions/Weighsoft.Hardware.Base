#include <examples/weighing/WeighingService.h>
#include <AsyncJson.h>

WeighingService::WeighingService(AsyncWebServer*  server,
                                 FS*              fs,
                                 SecurityManager* securityManager,
                                 AsyncMqttClient* mqttClient
#if FT_ENABLED(FT_BLE)
                                 , BLEServer* bleServer
#endif
                                 ) :
    _httpEndpoint(WeighingState::read,
                  WeighingState::update,
                  this,
                  server,
                  WEIGHING_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::IS_AUTHENTICATED,
                  WEIGHING_JSON_SIZE),
    _fsPersistence(WeighingState::readConfig,
                   WeighingState::updateConfig,
                   this,
                   fs,
                   WEIGHING_CONFIG_FILE,
                   WEIGHING_JSON_SIZE),
    _mqttPubSub(WeighingState::read, WeighingState::update, this, mqttClient, "", "", false, WEIGHING_JSON_SIZE),
    _webSocket(WeighingState::read,
               WeighingState::update,
               this,
               server,
               WEIGHING_SOCKET_PATH,
               securityManager,
               AuthenticationPredicates::IS_AUTHENTICATED,
               WEIGHING_JSON_SIZE),
    _mqttClient(mqttClient),
    _server(server),
    _securityManager(securityManager),
    _adc(),
    _auditTrail(fs),
#if FT_ENABLED(FT_BLE)
    _blePubSub(WeighingState::read, WeighingState::update, this, bleServer),
    _bleServer(bleServer),
    _bleService(nullptr),
    _bleCharacteristic(nullptr),
#endif
    _lastAztTime(0),
    _lastBroadcast(0),
    _stabilityIdx(0),
    _stabilityFull(false),
    _powerOnZeroDone(false)
{
  // Initialise stability buffer
  memset(_stabilityBuf, 0, sizeof(_stabilityBuf));

  // Initialise factory PIN hash
  weighingInitPinHash();

  // Inline MQTT paths
  _mqttBasePath  = SettingValue::format("weighsoft/weighing/#{unique_id}");
  _mqttName      = SettingValue::format("weighing-#{unique_id}");
  _mqttUniqueId  = SettingValue::format("weighing-#{unique_id}");

  // Disable FSPersistence auto-save -- we call saveConfig() explicitly
  _fsPersistence.disableUpdateHandler();

  // Register MQTT reconnect handler
  _mqttClient->onConnect(std::bind(&WeighingService::configureMqtt, this));

  // Register update handler: handle commands and config saves
  // Filter out hardware-origin updates to avoid re-triggering on every ADC read
  addUpdateHandler([&](const String& originId) {
    if (originId != "weighing_hw" && originId != "init") {
      onCommandReceived(originId);
    }
  }, false);

  // Register the separate audit trail GET endpoint (admin only)
  server->on(WEIGHING_AUDIT_PATH,
             HTTP_GET,
             securityManager->wrapRequest(
               [this](AsyncWebServerRequest* request) {
                 String log = _auditTrail.readLog();
                 AsyncWebServerResponse* response =
                     request->beginResponse(200, "application/json", log);
                 response->addHeader("Cache-Control", "no-cache");
                 request->send(response);
               },
               AuthenticationPredicates::IS_ADMIN));
}

void WeighingService::begin() {
  // Load persisted config (calibration, seal state, event counter)
  _fsPersistence.readFromFS();

  // Init audit trail filesystem
  _auditTrail.begin();

  // Log firmware boot event (OIML 6.3.8.4 traced updates)
  _auditTrail.logEvent("boot", "firmware", "", "", _state.eventCounter);

  // Init ADC hardware
  _adc.begin();
  _adc.setSpeed(_state.speedMode == 1);

  Serial.printf("[Weighing] Service started. Seal=%s EventCounter=%u\n",
                _state.sealActive ? "ACTIVE" : "open", _state.eventCounter);

  // Attempt power-on zero after first stable reading (done in loop)
}

void WeighingService::loop() {
  if (!_adc.isConnected()) {
    updateWithoutPropagation([&](WeighingState& s) {
      s.adcConnected = false;
      s.stable       = false;
      return StateUpdateResult::CHANGED;
    });
    return;
  }

  // Read averaged raw value
  int32_t raw = _adc.readRawAverage(_state.samplesToAverage);
  if (raw == INT32_MIN) {
    updateWithoutPropagation([&](WeighingState& s) {
      s.adcConnected = false;
      s.stable       = false;
      return StateUpdateResult::CHANGED;
    });
    return;
  }

  // Capture config snapshot for calculations (avoid mutex re-entry)
  int32_t  zeroOffset        = _state.zeroOffset;
  float    calibFactor       = _state.calibrationFactor;
  float    tareValue         = _state.tareValue;
  String   weightMode        = _state.weightMode;
  float    maxCapacity       = _state.maxCapacity;
  float    minDivision       = _state.minDivision;
  uint8_t  decPlaces         = _state.decimalPlaces;
  bool     ztEnabled         = _state.zeroTrackingEnabled;
  float    ztRange           = _state.zeroTrackingRange;
  float    poZeroRange       = _state.powerOnZeroRange;

  // Calculate gross weight
  float gross = (float)(raw - zeroOffset) * calibFactor;

  // Rounding to display resolution
  float scale = powf(10.0f, (float)decPlaces);
  gross = roundf(gross * scale) / scale;

  // Net weight
  float net = gross - tareValue;
  net = roundf(net * scale) / scale;

  // Displayed weight
  float displayed = (weightMode == "net") ? net : gross;

  // Stability
  bool stable = computeStability(gross);

  // Center of zero (gross within ±0.25d, no tare active)
  bool coz = (fabsf(gross) <= (0.25f * minDivision)) && (tareValue == 0.0f);

  // Overload / underload
  bool overload  = (gross > (maxCapacity + 9.0f * minDivision));
  bool underload = (gross < (-20.0f * minDivision));

  // Power-on zero (once, after first stable reading)
  if (!_powerOnZeroDone && stable) {
    _powerOnZeroDone = true;
    float poRange = (poZeroRange / 100.0f) * maxCapacity;
    if (fabsf(gross) <= poRange) {
      executePowerOnZero();
      // Recalculate after zeroing
      zeroOffset = _state.zeroOffset;
      gross      = (float)(raw - zeroOffset) * calibFactor;
      gross      = roundf(gross * scale) / scale;
      net        = gross - tareValue;
      net        = roundf(net * scale) / scale;
      displayed  = (weightMode == "net") ? net : gross;
      coz        = (fabsf(gross) <= (0.25f * minDivision)) && (tareValue == 0.0f);
    }
  }

  // Automatic Zero Tracking
  if (ztEnabled && stable && coz && (tareValue == 0.0f)) {
    unsigned long now = millis();
    if (now - _lastAztTime > 500) {
      _lastAztTime = now;
      performAzt(gross);
      // Recalculate after AZT correction
      gross     = (float)(raw - _state.zeroOffset) * calibFactor;
      gross     = roundf(gross * scale) / scale;
      net       = gross - tareValue;
      net       = roundf(net * scale) / scale;
      displayed = (weightMode == "net") ? net : gross;
      coz       = (fabsf(gross) <= (0.25f * minDivision)) && (tareValue == 0.0f);
    }
  }

  // Update state silently (no broadcast) to avoid flooding WebSocket clients
  updateWithoutPropagation([&](WeighingState& s) {
    s.rawValue     = raw;
    s.grossWeight  = gross;
    s.netWeight    = net;
    s.weight       = displayed;
    s.stable       = stable;
    s.centerOfZero = coz;
    s.overload     = overload;
    s.underload    = underload;
    s.adcConnected = true;
    return StateUpdateResult::CHANGED;
  });

  // Broadcast to WS/MQTT/BLE at max ~2 Hz to limit memory pressure
  unsigned long now = millis();
  if (now - _lastBroadcast >= 500) {
    _lastBroadcast = now;
    _webSocket.cleanupClients(2);
    if (ESP.getFreeHeap() > 32768) {
      callUpdateHandlers("weighing_hw");
    }
  }
}

bool WeighingService::computeStability(float weight) {
  _stabilityBuf[_stabilityIdx] = weight;
  _stabilityIdx = (_stabilityIdx + 1) % WEIGHING_STABILITY_SAMPLES;
  if (_stabilityIdx == 0) _stabilityFull = true;

  uint8_t count = _stabilityFull ? WEIGHING_STABILITY_SAMPLES : _stabilityIdx;
  if (count < 2) return false;

  float mn = _stabilityBuf[0], mx = _stabilityBuf[0];
  for (uint8_t i = 1; i < count; i++) {
    if (_stabilityBuf[i] < mn) mn = _stabilityBuf[i];
    if (_stabilityBuf[i] > mx) mx = _stabilityBuf[i];
  }
  return (mx - mn) < _state.minDivision;
}

void WeighingService::performAzt(float grossWeight) {
  // Correct by up to zeroTrackingRange per AZT tick
  float correction = -grossWeight;
  float limit = _state.zeroTrackingRange;
  if (correction > limit)  correction = limit;
  if (correction < -limit) correction = -limit;

  // Convert to raw counts
  if (_state.calibrationFactor != 0.0f) {
    int32_t rawCorrection = (int32_t)(correction / _state.calibrationFactor);
    _state.zeroOffset -= rawCorrection;
  }
}

void WeighingService::executeTare() {
  Serial.printf("[Weighing] Tare requested. stable=%d gross=%.4f\n",
                _state.stable, _state.grossWeight);
  if (!_state.stable) {
    Serial.println("[Weighing] Tare rejected: not stable");
    return;
  }
  float oldTare = _state.tareValue;
  update([&](WeighingState& s) {
    s.tareValue  = s.grossWeight;
    s.weightMode = "net";
    s.netWeight  = s.grossWeight - s.tareValue;
    s.weight     = s.netWeight;
    s.tareCommand = false;
    return StateUpdateResult::CHANGED;
  }, "weighing_cmd");
  Serial.printf("[Weighing] Tare OK: %.4f -> %.4f\n", oldTare, _state.tareValue);
}

void WeighingService::executePresetTare(float value) {
  update([&](WeighingState& s) {
    s.tareValue         = value;
    s.weightMode        = "net";
    s.netWeight         = s.grossWeight - s.tareValue;
    s.weight            = s.netWeight;
    s.presetTareCommand = false;
    return StateUpdateResult::CHANGED;
  }, "weighing_cmd");
}

void WeighingService::executeClearTare() {
  update([&](WeighingState& s) {
    s.tareValue        = 0.0f;
    s.weightMode       = "gross";
    s.netWeight        = 0.0f;
    s.weight           = s.grossWeight;
    s.clearTareCommand = false;
    return StateUpdateResult::CHANGED;
  }, "weighing_cmd");
}

void WeighingService::executeZero() {
  bool uncalibrated = (_state.calibrationFactor == 1.0f);

  if (!uncalibrated) {
    if (!_state.stable) {
      Serial.println("[Weighing] Zero rejected: not stable");
      return;
    }
    float manualRange = (_state.manualZeroRange / 100.0f) * _state.maxCapacity;
    if (fabsf(_state.grossWeight) > manualRange) {
      Serial.printf("[Weighing] Zero rejected: gross=%.3f exceeds range=%.3f\n",
                    _state.grossWeight, manualRange);
      return;
    }
  } else {
    Serial.println("[Weighing] Uncalibrated — skipping stability/range checks");
  }

  int32_t oldOffset = _state.zeroOffset;
  int32_t rawCorrection = _state.rawValue - _state.zeroOffset;

  update([&](WeighingState& s) {
    s.zeroOffset   = s.zeroOffset + rawCorrection;
    s.eventCounter++;
    s.zeroCommand  = false;
    return StateUpdateResult::CHANGED;
  }, "weighing_cmd");

  logAndCount("zero", "zero_offset",
              String(oldOffset), String(_state.zeroOffset));
  saveConfig();
}

void WeighingService::executeCalibration(float knownWeight) {
  bool uncalibrated = (_state.calibrationFactor == 1.0f);
  Serial.printf("[Weighing] Calibrate requested. knownWeight=%.4f raw=%d zeroOffset=%d stable=%d uncal=%d\n",
                knownWeight, _state.rawValue, _state.zeroOffset, _state.stable, uncalibrated);
  if (!uncalibrated && !_state.stable) {
    Serial.println("[Weighing] Calibration rejected: not stable");
    return;
  }
  if (knownWeight <= 0.0f) {
    Serial.println("[Weighing] Calibration rejected: known weight must be > 0");
    return;
  }

  int32_t rawSpan = _state.rawValue - _state.zeroOffset;
  Serial.printf("[Weighing] rawSpan = %d - %d = %d\n", _state.rawValue, _state.zeroOffset, rawSpan);
  if (rawSpan == 0) {
    Serial.println("[Weighing] Calibration rejected: zero span (nothing on scale?)");
    return;
  }

  float oldFactor = _state.calibrationFactor;
  float newFactor = knownWeight / (float)rawSpan;
  Serial.printf("[Weighing] factor: %.8f -> %.8f\n", oldFactor, newFactor);

  update([&](WeighingState& s) {
    s.calibrationFactor = newFactor;
    s.eventCounter++;
    time_t now;
    struct tm ti;
    time(&now);
    char buf[25] = {0};
    if (now > 1000000000UL) {
      gmtime_r(&now, &ti);
      strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &ti);
    }
    s.lastCalibrationTime = (strlen(buf) > 0) ? String(buf) : "NO_NTP_" + String(millis());
    s.calibrateCommand    = false;
    return StateUpdateResult::CHANGED;
  }, "weighing_cmd");

  logAndCount("calibrate", "calibration_factor",
              String(oldFactor, 8), String(newFactor, 8));
  saveConfig();
  Serial.printf("[Weighing] Calibration OK. ec=%u\n", _state.eventCounter);
}

void WeighingService::executePowerOnZero() {
  int32_t rawCorrection = _state.rawValue - _state.zeroOffset;
  _state.zeroOffset += rawCorrection;
  _auditTrail.logEvent("zero", "zero_offset",
                       "power_on", String(_state.zeroOffset),
                       _state.eventCounter);
  saveConfig();
  Serial.println("[Weighing] Power-on zero applied");
}

void WeighingService::executeSeal() {
  Serial.printf("[Weighing] Seal requested. current=%d ec=%u\n", _state.sealActive, _state.eventCounter);
  String oldVal = _state.sealActive ? "true" : "false";

  update([&](WeighingState& s) {
    s.sealActive   = true;
    s.eventCounter++;
    s.sealCommand  = false;
    return StateUpdateResult::CHANGED;
  }, "weighing_cmd");

  logAndCount("seal", "seal_active", oldVal, "true");
  saveConfig();
  Serial.printf("[Weighing] Instrument SEALED. ec=%u\n", _state.eventCounter);
}

void WeighingService::executeUnseal(const String& pin) {
  Serial.printf("[Weighing] Unseal requested. sealed=%d pin_len=%d\n", _state.sealActive, pin.length());
  if (!weighingVerifyPin(pin)) {
    uint32_t lockSecs = weighingPinLockoutSeconds();
    if (lockSecs > 0) {
      Serial.printf("[Weighing] Unseal LOCKED: %u seconds remaining\n", lockSecs);
      _auditTrail.logEvent("unseal_fail", "seal_active",
                           "locked", "true", _state.eventCounter);
    } else {
      Serial.printf("[Weighing] Unseal FAILED: wrong PIN (attempt %d)\n", _failedPinAttempts);
      _auditTrail.logEvent("unseal_fail", "seal_active",
                           "wrong_pin", "true", _state.eventCounter);
    }
    update([&](WeighingState& s) {
      s.unsealCommand = false;
      s.pin = "";
      return StateUpdateResult::CHANGED;
    }, "weighing_cmd");
    return;
  }

  update([&](WeighingState& s) {
    s.sealActive    = false;
    s.eventCounter++;
    s.unsealCommand = false;
    s.pin           = "";
    return StateUpdateResult::CHANGED;
  }, "weighing_cmd");

  logAndCount("unseal", "seal_active", "true", "false");
  saveConfig();
  Serial.printf("[Weighing] Instrument UNSEALED. ec=%u\n", _state.eventCounter);
}

void WeighingService::onCommandReceived(const String& originId) {
  Serial.printf("[Weighing] cmd from %s heap=%u tare=%d zero=%d cal=%d seal=%d unseal=%d\n",
                originId.c_str(), ESP.getFreeHeap(),
                _state.tareCommand, _state.zeroCommand, _state.calibrateCommand,
                _state.sealCommand, _state.unsealCommand);
  if (_state.tareCommand)         executeTare();
  if (_state.presetTareCommand)   executePresetTare(_state.presetTareValue);
  if (_state.clearTareCommand)    executeClearTare();
  if (_state.zeroCommand)         executeZero();
  if (_state.calibrateCommand)    executeCalibration(_state.knownWeight);
  if (_state.sealCommand)         executeSeal();
  if (_state.unsealCommand)       executeUnseal(_state.pin);

  // Config-only changes (not commands): persist if any config field changed
  if (!_state.tareCommand && !_state.calibrateCommand && !_state.zeroCommand
      && !_state.sealCommand && !_state.unsealCommand) {
    // Could be a config param change -- save it
    saveConfig();
  }
}

void WeighingService::logAndCount(const String& event,
                                  const String& param,
                                  const String& oldVal,
                                  const String& newVal) {
  _auditTrail.logEvent(event, param, oldVal, newVal, _state.eventCounter);
}

void WeighingService::saveConfig() {
  _fsPersistence.writeToFS();
}

void WeighingService::configureMqtt() {
  if (!_mqttClient->connected()) return;

  String pubTopic = _mqttBasePath + "/data";
  String subTopic = _mqttBasePath + "/set";
  _mqttPubSub.configureTopics(pubTopic, subTopic);

  Serial.printf("[Weighing] MQTT configured: pub=%s sub=%s\n",
                pubTopic.c_str(), subTopic.c_str());
}

#if FT_ENABLED(FT_BLE)
void WeighingService::configureBle() {
  if (_bleServer == nullptr) {
    Serial.println("[Weighing] BLE server not available, skipping BLE configuration");
    return;
  }

  Serial.println("[Weighing] Configuring BLE service...");

  _bleService = _bleServer->createService(BLE_SERVICE_UUID);
  _bleCharacteristic = _bleService->createCharacteristic(
      BLE_CHAR_UUID,
      BLECharacteristic::PROPERTY_READ  |
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_NOTIFY
  );

  _blePubSub.configureCharacteristic(_bleCharacteristic);
  _bleService->start();

  Serial.printf("[Weighing] BLE configured - Service: %s Char: %s\n",
                BLE_SERVICE_UUID, BLE_CHAR_UUID);
}
#endif
