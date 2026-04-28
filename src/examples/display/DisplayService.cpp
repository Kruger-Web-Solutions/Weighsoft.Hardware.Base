#ifdef HAS_TFT_DISPLAY

#include <ESPFS.h>
#include <examples/display/DisplayService.h>

// ─── Colour palette ────────────────────────────────────────────────────────
static constexpr uint16_t COL_BG          = TFT_BLACK;
static constexpr uint16_t COL_STATUS_BG   = 0x1082;   // dark navy
static constexpr uint16_t COL_STATUS_TEXT = TFT_WHITE;
static constexpr uint16_t COL_WEIGHT_OK   = 0x07E0;   // bright green
static constexpr uint16_t COL_WEIGHT_STALE= 0xFD20;   // amber
static constexpr uint16_t COL_WEIGHT_ERR  = TFT_RED;
static constexpr uint16_t COL_UNIT        = 0xC618;   // light grey
static constexpr uint16_t COL_RAW         = 0x632C;   // muted grey-green
static constexpr uint16_t COL_ACCENT      = 0x04FF;   // cyan accent line

// ─── Constructor ───────────────────────────────────────────────────────────
DisplayService::DisplayService(RemoteWeightService* remoteWeightService)
    : _remoteWeightService(remoteWeightService),
      _needsWeightRedraw(false),
      _lastWeightTime(0),
      _drawnConnected(false),
      _displayOn(true),
      _lastStatusDraw(0) {
  // Register for weight updates from RemoteWeightService.
  // This runs in an async HTTP task — only set flag + copy strings, no TFT calls.
  _remoteWeightService->addUpdateHandler(
      [this](const String& originId) {
        _remoteWeightService->read([this](const RemoteWeightState& state) {
          _pendingWeight  = state.weight;
          _pendingLine    = state.lastLine;
          _lastWeightTime = millis();
          _needsWeightRedraw = true;
        });
      },
      false);
}

// ─── begin() ───────────────────────────────────────────────────────────────
void DisplayService::begin() {
  // Backlight on FIRST
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Manual RESET pulse — give display plenty of time (many cheap panels need this).
  // Some boards (e.g. CYD / ESP32-2432S028) tie the panel reset to the ESP32's
  // EN line and have no separate reset GPIO, so TFT_RST is set to -1 — skip the
  // pulse in that case and rely on TFT_eSPI's software reset.
#if defined(TFT_RST) && (TFT_RST >= 0)
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH); delay(50);
  digitalWrite(TFT_RST, LOW);  delay(200);
  digitalWrite(TFT_RST, HIGH); delay(200);
#endif

  _tft.init();
  delay(200);

  // CYD-style ILI9341 panels (Sunton ESP32-2432S028) need tweaks vs the
  // 480x320 ILI9488 we were originally targeting. After bench testing on
  // the actual hardware (see handover Phase 9 + the colour-inversion photo
  // session), the correct settings on this CYD batch are:
  //   1. invertDisplay(false) — this batch ships WITHOUT the controller-
  //      level inversion bit set; calling invertDisplay(true) produced a
  //      cream/white background with purple text. (false) gives the
  //      intended black background with cyan/orange accents.
  //   2. setRotation(1) — landscape with the USB connector on the LEFT,
  //      matching how the panel is held on the user's bench. setRotation(3)
  //      put the USB at the top, which read sideways in normal use.
  //   3. RGB order swap is not needed for this batch (verified empirically).
  // All gated on TFT_INVERT_DISPLAY so the 3.5" ILI9488 env (esp32dev) is
  // unaffected — it keeps its existing rotation(1) / no-inversion behaviour.
#if defined(TFT_INVERT_DISPLAY) && TFT_INVERT_DISPLAY
  _tft.invertDisplay(false);
  // setRotation(0) — portrait orientation. With this rotation, holding the
  // board with the USB connector on the LEFT puts the status bar at the
  // TOP and the weight digits read left-to-right naturally. The TPM408-2.8
  // panel variant (this batch) behaves differently from the standard CYD
  // wiring, so 0 here gives the user-facing orientation that 1 / 3 don't.
  _tft.setRotation(0);
#else
  _tft.setRotation(1);  // standard landscape (3.5" ILI9488)
#endif

  // Pull actual panel dimensions and derive the layout. Proportional values
  // were chosen so the same numbers that look right on a 480x320 ILI9488
  // also fit a 320x240 ILI9341 (CYD) without anything going off-screen.
  _w = _tft.width();
  _h = _tft.height();
  _statusBarH = _h / 9;                 // ~36 on 320H, ~27 on 240H
  if (_statusBarH < 24) _statusBarH = 24;
  _weightCenterY = (_h + _statusBarH) / 2;
  _rawLineY = _h - 18;                  // raw scale-line strip near the bottom
  // Text sizes — shrink the splash + weight digits for narrower panels so
  // the strings stay inside _w. GLCD font is 6px wide per char per size unit.
  if (_w >= 400) {
    _titleTextSize = 3; _statusTextSize = 2; _weightTextSize = 6; _unitTextSize = 3;
  } else {
    _titleTextSize = 2; _statusTextSize = 1; _weightTextSize = 5; _unitTextSize = 2;
  }
  Serial.printf("[Display] Panel %dx%d statusH=%d weightCenterY=%d rawLineY=%d\n",
                _w, _h, _statusBarH, _weightCenterY, _rawLineY);

  // Colour flash test — watch the screen for 1 sec each
  Serial.println(F("[Display] Colour test: RED"));
  _tft.fillScreen(TFT_RED);   delay(1000);
  Serial.println(F("[Display] Colour test: GREEN"));
  _tft.fillScreen(TFT_GREEN); delay(1000);
  Serial.println(F("[Display] Colour test: BLUE"));
  _tft.fillScreen(TFT_BLUE);  delay(1000);
  Serial.println(F("[Display] Colour test: WHITE"));
  _tft.fillScreen(TFT_WHITE); delay(1000);

  _tft.fillScreen(COL_BG);
  _tft.setTextDatum(MC_DATUM);

  drawSplash();
  Serial.println(F("[Display] TFT begin() done"));
}

void DisplayService::setBacklight(bool on) {
  digitalWrite(TFT_BL, on ? HIGH : LOW);
}

void DisplayService::loop() {
  bool shouldBeOn = _remoteWeightService->isDisplayEnabled();
  if (shouldBeOn != _displayOn) {
    _displayOn = shouldBeOn;
    setBacklight(_displayOn);
    if (_displayOn) {
      drawSplash();
    } else {
      _tft.fillScreen(TFT_BLACK);
    }
    Serial.printf("[Display] %s\n", _displayOn ? "Enabled" : "Disabled");
  }
  if (!_displayOn) return;

  unsigned long now = millis();
  bool connected    = (now - _lastWeightTime) < DISPLAY_TIMEOUT_MS && _lastWeightTime > 0;

  // Full redraw when a new weight arrives
  if (_needsWeightRedraw) {
    _needsWeightRedraw = false;
    drawWeightScreen(_pendingWeight, _pendingLine, connected);
    _drawnWeight    = _pendingWeight;
    _drawnConnected = connected;
    _lastStatusDraw = now;
    return;
  }

  // Connectivity state changed (went offline / came back)
  if (connected != _drawnConnected) {
    _drawnConnected = connected;
    drawWeightScreen(_drawnWeight, _pendingLine, connected);
    _lastStatusDraw = now;
    return;
  }

  // Refresh status bar clock every second
  if (now - _lastStatusDraw >= DISPLAY_STATUS_INTERVAL_MS) {
    _lastStatusDraw = now;
    drawStatusBar(connected, now - _lastWeightTime);
  }
}

// ─── Splash screen shown at boot ───────────────────────────────────────────
void DisplayService::drawSplash() {
  _tft.fillScreen(COL_BG);

  int16_t cx = _w / 2;
  int16_t cy = _h / 2;

  // Top accent line
  _tft.drawFastHLine(0, cy - 50, _w, COL_ACCENT);

  _tft.setTextColor(COL_ACCENT, COL_BG);
  _tft.setTextDatum(MC_DATUM);
  _tft.setFreeFont(NULL);
  _tft.setTextSize(_titleTextSize);
  _tft.drawString("WEIGHSOFT", cx, cy - 18);

  _tft.setTextSize(1);
  _tft.setTextColor(COL_STATUS_TEXT, COL_BG);
  _tft.drawString("Scale Display Module", cx, cy + 12);

  _tft.setTextColor(COL_RAW, COL_BG);
  _tft.drawString("Waiting for WiFi and weight data...", cx, cy + 32);

  // Bottom accent line
  _tft.drawFastHLine(0, cy + 50, _w, COL_ACCENT);
}

// ─── Full weight screen layout ─────────────────────────────────────────────
void DisplayService::drawWeightScreen(const String& weight, const String& line, bool connected) {
  _tft.fillScreen(COL_BG);

  // Status bar at top
  drawStatusBar(connected, connected ? 0 : (millis() - _lastWeightTime));

  // Thin accent divider under status bar
  _tft.drawFastHLine(0, _statusBarH, _w, COL_ACCENT);

  // Big weight number
  drawWeightValue(weight, connected);

  // Raw serial line at bottom
  drawRawLine(line);

  // Bottom accent line
  _tft.drawFastHLine(0, _h - 20, _w, COL_STATUS_BG);
}

// ─── Status bar ────────────────────────────────────────────────────────────
void DisplayService::drawStatusBar(bool connected, unsigned long ageMs) {
  _tft.fillRect(0, 0, _w, _statusBarH, COL_STATUS_BG);

  _tft.setTextDatum(ML_DATUM);
  _tft.setTextColor(COL_ACCENT, COL_STATUS_BG);
  _tft.setFreeFont(NULL);
  _tft.setTextSize(_statusTextSize);
  _tft.drawString("WEIGHSOFT", 8, _statusBarH / 2);

  // Connection status on the right
  _tft.setTextDatum(MR_DATUM);
  if (connected) {
    _tft.setTextColor(COL_WEIGHT_OK, COL_STATUS_BG);
    _tft.drawString("LIVE", _w - 8, _statusBarH / 2);
  } else if (_lastWeightTime == 0) {
    _tft.setTextColor(COL_WEIGHT_STALE, COL_STATUS_BG);
    _tft.drawString("WAITING", _w - 8, _statusBarH / 2);
  } else {
    // Show how many seconds since last update
    char buf[24];
    unsigned long secs = ageMs / 1000;
    if (secs < 60) {
      snprintf(buf, sizeof(buf), "NO SIGNAL %lus", secs);
    } else {
      snprintf(buf, sizeof(buf), "NO SIGNAL %lum", secs / 60);
    }
    _tft.setTextColor(COL_WEIGHT_ERR, COL_STATUS_BG);
    _tft.drawString(buf, _w - 8, _statusBarH / 2);
  }
}

// ─── Large weight number ────────────────────────────────────────────────────
void DisplayService::drawWeightValue(const String& weight, bool connected) {
  // Clear weight area (everything between status bar and the raw-line strip)
  int16_t areaTop = _statusBarH + 1;
  int16_t areaH = _rawLineY - areaTop - 4;
  _tft.fillRect(0, areaTop, _w, areaH, COL_BG);

  uint16_t colour = connected ? COL_WEIGHT_OK : COL_WEIGHT_STALE;
  int16_t cx = _w / 2;

  if (!connected && _lastWeightTime == 0) {
    // Never received data — draw "-- kg" placeholder
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COL_WEIGHT_STALE, COL_BG);
    _tft.setFreeFont(NULL);
    _tft.setTextSize(_unitTextSize);
    _tft.drawString("-- kg", cx, _weightCenterY);
    return;
  }

  // Big weight number, then "kg" suffix below in smaller font
  String displayWeight = weight.length() > 0 ? weight : "--";
  _tft.setFreeFont(NULL);
  _tft.setTextDatum(MC_DATUM);
  _tft.setTextColor(colour, COL_BG);
  _tft.setTextSize(_weightTextSize);
  _tft.drawString(displayWeight, cx, _weightCenterY - 12);

  _tft.setTextSize(_unitTextSize);
  _tft.setTextColor(COL_UNIT, COL_BG);
  // Offset proportional to weightTextSize so kg never touches the digits
  _tft.drawString("kg", cx, _weightCenterY + (_weightTextSize * 6));
}

// ─── Raw serial line strip at bottom ───────────────────────────────────────
void DisplayService::drawRawLine(const String& line) {
  _tft.fillRect(0, _rawLineY - 9, _w, 18, COL_BG);
  _tft.setTextDatum(MC_DATUM);
  _tft.setTextColor(COL_RAW, COL_BG);
  _tft.setFreeFont(NULL);
  _tft.setTextSize(1);

  // Truncate if too long for the panel width (chars are 6 px wide at size 1)
  size_t maxChars = (size_t)((_w - 8) / 6);
  String display = (line.length() > maxChars) ? (line.substring(0, maxChars - 3) + "...") : line;
  _tft.drawString(display, _w / 2, _rawLineY);
}

#endif  // HAS_TFT_DISPLAY
