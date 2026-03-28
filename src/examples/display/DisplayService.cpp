#ifdef HAS_TFT_DISPLAY

#include <examples/display/DisplayService.h>

// ─── Layout constants ──────────────────────────────────────────────────────
// Display is 480 wide × 320 tall (landscape, ILI9488 rotated 1)
static constexpr int DISP_W = 480;
static constexpr int DISP_H = 320;

static constexpr int STATUS_BAR_H = 36;    // top status bar
static constexpr int WEIGHT_Y     = 80;    // baseline of big weight text
static constexpr int UNIT_Y       = 82;    // baseline of unit suffix
static constexpr int RAW_LINE_Y   = 270;   // raw serial line near bottom

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

  // Manual RESET pulse — give display plenty of time (many cheap panels need this)
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH); delay(50);
  digitalWrite(TFT_RST, LOW);  delay(200);
  digitalWrite(TFT_RST, HIGH); delay(200);

  _tft.init();
  delay(200);

  // Force display out of sleep and turn display ON — some ILI9486 panels
  // need this even after init() because the init sequence in TFT_eSPI
  // may not send DISPON for all panel variants.
  _tft.writecommand(0x11);  // SLPOUT — exit sleep mode
  delay(150);
  _tft.writecommand(0x29);  // DISPON — display on
  delay(50);

  _tft.setRotation(1);  // landscape

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

// ─── loop() — called every iteration of main loop() ────────────────────────
void DisplayService::loop() {
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

  // Accent line
  _tft.drawFastHLine(0, DISP_H / 2 - 60, DISP_W, COL_ACCENT);

  _tft.setTextColor(COL_ACCENT, COL_BG);
  _tft.setTextDatum(MC_DATUM);
  _tft.setFreeFont(NULL);
  _tft.setTextSize(3);
  _tft.drawString("WEIGHSOFT", DISP_W / 2, DISP_H / 2 - 20);

  _tft.setTextSize(1);
  _tft.setTextColor(COL_STATUS_TEXT, COL_BG);
  _tft.drawString("Scale Display Module", DISP_W / 2, DISP_H / 2 + 20);

  _tft.setTextColor(COL_RAW, COL_BG);
  _tft.drawString("Waiting for WiFi and weight data...", DISP_W / 2, DISP_H / 2 + 50);

  _tft.drawFastHLine(0, DISP_H / 2 + 70, DISP_W, COL_ACCENT);
}

// ─── Full weight screen layout ─────────────────────────────────────────────
void DisplayService::drawWeightScreen(const String& weight, const String& line, bool connected) {
  _tft.fillScreen(COL_BG);

  // Status bar at top
  drawStatusBar(connected, connected ? 0 : (millis() - _lastWeightTime));

  // Thin accent divider under status bar
  _tft.drawFastHLine(0, STATUS_BAR_H, DISP_W, COL_ACCENT);

  // Big weight number
  drawWeightValue(weight, connected);

  // Raw serial line at bottom
  drawRawLine(line);

  // Bottom accent line
  _tft.drawFastHLine(0, DISP_H - 20, DISP_W, COL_STATUS_BG);
}

// ─── Status bar ────────────────────────────────────────────────────────────
void DisplayService::drawStatusBar(bool connected, unsigned long ageMs) {
  _tft.fillRect(0, 0, DISP_W, STATUS_BAR_H, COL_STATUS_BG);

  _tft.setTextDatum(ML_DATUM);
  _tft.setTextColor(COL_ACCENT, COL_STATUS_BG);
  _tft.setFreeFont(NULL);
  _tft.setTextSize(2);
  _tft.drawString("WEIGHSOFT", 8, STATUS_BAR_H / 2);

  // Connection status on the right
  _tft.setTextDatum(MR_DATUM);
  if (connected) {
    _tft.setTextColor(COL_WEIGHT_OK, COL_STATUS_BG);
    _tft.drawString("LIVE", DISP_W - 8, STATUS_BAR_H / 2);
  } else if (_lastWeightTime == 0) {
    _tft.setTextColor(COL_WEIGHT_STALE, COL_STATUS_BG);
    _tft.drawString("WAITING", DISP_W - 8, STATUS_BAR_H / 2);
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
    _tft.drawString(buf, DISP_W - 8, STATUS_BAR_H / 2);
  }
}

// ─── Large weight number ────────────────────────────────────────────────────
void DisplayService::drawWeightValue(const String& weight, bool connected) {
  // Clear weight area
  _tft.fillRect(0, STATUS_BAR_H + 1, DISP_W, DISP_H - STATUS_BAR_H - 40, COL_BG);

  uint16_t colour = connected ? COL_WEIGHT_OK : COL_WEIGHT_STALE;

  if (!connected && _lastWeightTime == 0) {
    // Never received data
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COL_WEIGHT_STALE, COL_BG);
    _tft.setFreeFont(NULL);
    _tft.setTextSize(3);
    _tft.drawString("-- kg", DISP_W / 2, DISP_H / 2);
    return;
  }

  if (!connected) {
    colour = COL_WEIGHT_STALE;
  }

  // Draw value - use Font 7 (7-segment style, large digits) for weight number
  String displayWeight = weight.length() > 0 ? weight : "--";
  _tft.setTextDatum(MC_DATUM);
  _tft.setTextColor(colour, COL_BG);

  // Try to split number and unit for visual separation
  // Weight format is typically "2328" (unit appended separately)
  _tft.setFreeFont(NULL);

  // Draw large number using font size 8 (tallest available without FreeFont)
  _tft.setTextSize(1);
  // Use built-in Font 7 (large 7-seg style digits)
  _tft.loadFont(NULL);

  // Font 6 gives ~48px tall digits - draw number
  _tft.setTextSize(1);
  _tft.drawString(displayWeight + " kg", DISP_W / 2, (DISP_H + STATUS_BAR_H) / 2 - 10);

  // Attempt with a bigger size
  _tft.fillRect(0, STATUS_BAR_H + 1, DISP_W, DISP_H - STATUS_BAR_H - 40, COL_BG);
  _tft.setTextSize(6);
  _tft.setTextColor(colour, COL_BG);
  _tft.setTextDatum(MC_DATUM);
  _tft.drawString(displayWeight, DISP_W / 2, (DISP_H + STATUS_BAR_H) / 2 - 20);

  // Unit "kg" beside / below in smaller size
  _tft.setTextSize(3);
  _tft.setTextColor(COL_UNIT, COL_BG);
  _tft.setTextDatum(MC_DATUM);
  _tft.drawString("kg", DISP_W / 2, (DISP_H + STATUS_BAR_H) / 2 + 50);
}

// ─── Raw serial line strip at bottom ───────────────────────────────────────
void DisplayService::drawRawLine(const String& line) {
  _tft.fillRect(0, DISP_H - 38, DISP_W, 18, COL_BG);
  _tft.setTextDatum(MC_DATUM);
  _tft.setTextColor(COL_RAW, COL_BG);
  _tft.setFreeFont(NULL);
  _tft.setTextSize(1);

  // Truncate if too long for display
  String display = line.length() > 60 ? line.substring(0, 57) + "..." : line;
  _tft.drawString(display, DISP_W / 2, DISP_H - 29);
}

#endif  // HAS_TFT_DISPLAY
