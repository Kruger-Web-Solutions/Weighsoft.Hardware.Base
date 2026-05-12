#include "BoardDisplayService.h"

#include <BoardDisplayViewModel.h>
#include <examples/serial/SerialService.h>
#include <examples/serialwriter/SerialWriterForwarderService.h>
#include <examples/serialwriter/SerialWriterService.h>
#include "UartModeService.h"

#if FT_ENABLED(FT_BOARD_ESP32_2432S028)
#include <WiFi.h>
#include <bb_spi_lcd.h>

namespace {
constexpr int TFT_MOSI_PIN = 13;
constexpr int TFT_MISO_PIN = 12;
constexpr int TFT_SCLK_PIN = 14;
constexpr int TFT_CS_PIN = 15;
constexpr int TFT_DC_PIN = 2;
constexpr int TFT_BACKLIGHT_PIN = 21;
constexpr int TOUCH_MOSI_PIN = 32;
constexpr int TOUCH_MISO_PIN = 39;
constexpr int TOUCH_SCLK_PIN = 25;
constexpr int TOUCH_CS_PIN = 33;
constexpr int TFT_ROTATION_DEGREES = 270;
constexpr unsigned long DISPLAY_REFRESH_MS = 250UL;
constexpr unsigned long TOUCH_POLL_MS = 60UL;
constexpr uint16_t TOUCH_IDLE_X = 0;
constexpr uint16_t TOUCH_IDLE_Y = 0;
constexpr int16_t TOUCH_IDLE_Z = 0;
constexpr const char* TFT_DRIVER_NAME = "CYD_2USB_ST7789";
constexpr uint16_t HEADER_HEIGHT = 36;
constexpr uint16_t WEIGHT_TOP = 74;
constexpr uint16_t SCREEN_MARGIN = 10;
constexpr uint16_t STATUS_PANEL_HEIGHT = 56;
constexpr uint16_t WEIGHT_RENDER_HEIGHT = 78;
constexpr uint16_t BIG_WEIGHT_GLYPH_WIDTH = 34;
constexpr uint16_t BIG_WEIGHT_DOT_WIDTH = 10;
constexpr uint16_t BIG_WEIGHT_GLYPH_GAP = 4;
constexpr uint16_t BIG_WEIGHT_SEGMENT_THICKNESS = 6;
constexpr uint16_t TEXT_LINE_HEIGHT_8 = 10;
constexpr uint16_t TEXT_LINE_HEIGHT_6 = 8;

BB_SPI_LCD g_tft;

const char* wifiModeShort(wifi_mode_t mode) {
  switch (mode) {
    case WIFI_STA:
      return "STA";
    case WIFI_AP:
      return "AP";
    case WIFI_AP_STA:
      return "AP+STA";
    case WIFI_OFF:
      return "OFF";
    default:
      return "OTHER";
  }
}

bool canRenderBigWeight(const String& value) {
  if (value.length() == 0 || value.length() > 10) {
    return false;
  }
  for (size_t i = 0; i < value.length(); i++) {
    const char c = value[i];
    const bool allowed =
        (c >= '0' && c <= '9') || c == '.' || c == '-' || c == ' ' || c == '+';
    if (!allowed) {
      return false;
    }
  }
  return true;
}

int glyphWidthForWeight(char c) {
  if (c == '.') {
    return BIG_WEIGHT_DOT_WIDTH;
  }
  return BIG_WEIGHT_GLYPH_WIDTH;
}

int measureBigWeightWidth(const String& value) {
  if (value.length() == 0) {
    return 0;
  }

  int totalWidth = 0;
  for (size_t i = 0; i < value.length(); i++) {
    totalWidth += glyphWidthForWeight(value[i]);
    if (i + 1 < value.length()) {
      totalWidth += BIG_WEIGHT_GLYPH_GAP;
    }
  }
  return totalWidth;
}

void fillSegmentRect(int x, int y, int w, int h, uint16_t color) {
  if (w <= 0 || h <= 0) {
    return;
  }
  g_tft.fillRect(x, y, w, h, color);
}

void drawBigWeightGlyph(char c, int x, int y, uint16_t color) {
  if (c == ' ') {
    return;
  }
  if (c == '.') {
    g_tft.fillCircle(x + 4, y + WEIGHT_RENDER_HEIGHT - 8, 4, color);
    return;
  }

  bool a = false;
  bool b = false;
  bool cSeg = false;
  bool d = false;
  bool e = false;
  bool f = false;
  bool g = false;

  switch (c) {
    case '0':
      a = b = cSeg = d = e = f = true;
      break;
    case '1':
      b = cSeg = true;
      break;
    case '2':
      a = b = d = e = g = true;
      break;
    case '3':
      a = b = cSeg = d = g = true;
      break;
    case '4':
      b = cSeg = f = g = true;
      break;
    case '5':
      a = cSeg = d = f = g = true;
      break;
    case '6':
      a = cSeg = d = e = f = g = true;
      break;
    case '7':
      a = b = cSeg = true;
      break;
    case '8':
      a = b = cSeg = d = e = f = g = true;
      break;
    case '9':
      a = b = cSeg = d = f = g = true;
      break;
    case '-':
      g = true;
      break;
    case '+':
      g = true;
      fillSegmentRect(x + (BIG_WEIGHT_GLYPH_WIDTH / 2) - (BIG_WEIGHT_SEGMENT_THICKNESS / 2),
                      y + (WEIGHT_RENDER_HEIGHT / 4),
                      BIG_WEIGHT_SEGMENT_THICKNESS,
                      WEIGHT_RENDER_HEIGHT / 2,
                      color);
      break;
    default:
      return;
  }

  const int w = BIG_WEIGHT_GLYPH_WIDTH;
  const int h = WEIGHT_RENDER_HEIGHT;
  const int t = BIG_WEIGHT_SEGMENT_THICKNESS;
  const int midY = y + (h / 2) - (t / 2);
  const int topVerticalHeight = (h / 2) - t - 2;
  const int bottomVerticalHeight = (h / 2) - t - 2;

  if (a) fillSegmentRect(x + t, y, w - (t * 2), t, color);
  if (d) fillSegmentRect(x + t, y + h - t, w - (t * 2), t, color);
  if (g) fillSegmentRect(x + t, midY, w - (t * 2), t, color);
  if (f) fillSegmentRect(x, y + t, t, topVerticalHeight, color);
  if (b) fillSegmentRect(x + w - t, y + t, t, topVerticalHeight, color);
  if (e) fillSegmentRect(x, midY + t, t, bottomVerticalHeight, color);
  if (cSeg) fillSegmentRect(x + w - t, midY + t, t, bottomVerticalHeight, color);
}

void drawBigWeight(const String& value, int areaX, int areaY, int areaW, int areaH, uint16_t color) {
  const int totalWidth = measureBigWeightWidth(value);
  int cursorX = areaX + ((areaW - totalWidth) / 2);
  if (cursorX < areaX) {
    cursorX = areaX;
  }
  const int startY = areaY + ((areaH - WEIGHT_RENDER_HEIGHT) / 2);

  for (size_t i = 0; i < value.length(); i++) {
    const char c = value[i];
    drawBigWeightGlyph(c, cursorX, startY, color);
    cursorX += glyphWidthForWeight(c);
    if (i + 1 < value.length()) {
      cursorX += BIG_WEIGHT_GLYPH_GAP;
    }
  }
}

void drawRightAlignedText(const String& text, int rightX, int y) {
  BB_RECT bounds = {0, 0, 0, 0};
  g_tft.getStringBox(text, &bounds);
  int drawX = rightX - bounds.w;
  if (drawX < SCREEN_MARGIN) {
    drawX = SCREEN_MARGIN;
  }
  g_tft.setCursor(drawX, y);
  g_tft.print(text);
}

String displayWeightText(const String& safeWeight) {
  return safeWeight.length() > 0 ? safeWeight : String("WAITING");
}

void drawWeightValue(const String& weightText, int areaX, int areaY, int areaW, int areaH, uint16_t color) {
  g_tft.setTextColor(color, TFT_BLACK);
  if (weightText.length() > 0 && canRenderBigWeight(weightText)) {
    drawBigWeight(weightText, areaX, areaY, areaW, areaH, color);
    return;
  }

  g_tft.setFont(FONT_16x32);
  BB_RECT weightBounds = {0, 0, 0, 0};
  g_tft.getStringBox(weightText, &weightBounds);
  const int drawX = areaX + ((areaW - weightBounds.w) / 2);
  const int drawY = areaY + ((areaH - weightBounds.h) / 2);
  g_tft.setCursor(drawX < areaX ? areaX : drawX, drawY < areaY ? areaY : drawY);
  g_tft.print(weightText);
}

void clearTextLine(uint16_t screenWidth, int y, uint16_t height) {
  const int clearY = y > 1 ? y - 1 : 0;
  const int clearHeight = (int)height + 2;
  g_tft.fillRect(SCREEN_MARGIN, clearY, (int)screenWidth - (SCREEN_MARGIN * 2), clearHeight, TFT_BLACK);
}
}  // namespace
#endif

BoardDisplayService::BoardDisplayService(SerialService* serialService,
                                         SerialWriterService* serialWriterService,
                                         SerialWriterForwarderService* serialWriterForwarderService,
                                         UartModeService* uartModeService)
    : _serialService(serialService),
      _serialWriterService(serialWriterService),
      _serialWriterForwarderService(serialWriterForwarderService),
      _uartModeService(uartModeService),
      _lastModeText(""),
      _lastWeightText(""),
      _lastWifiText(""),
      _lastDetailText(""),
      _lastTouchText(""),
      _lastRenderMs(0),
      _lastTouchPollMs(0),
      _touchReady(false),
      _touchPressed(false),
      _touchX(0),
      _touchY(0),
      _touchZ(0),
      _chromeDrawn(false),
      _started(false) {
}

void BoardDisplayService::begin() {
#if FT_ENABLED(FT_BOARD_ESP32_2432S028)
  g_tft.begin(DISPLAY_CYD_2USB);
  g_tft.setRotation(TFT_ROTATION_DEGREES);
  g_tft.backlight(true);
  g_tft.fillScreen(TFT_BLACK);
  g_tft.setWordwrap(false);
  _chromeDrawn = false;
  _lastModeText = "";
  _lastWeightText = "";
  _lastWifiText = "";
  _lastDetailText = "";
  _lastTouchText = "";
  _touchReady = (g_tft.rtInit() == BB_ERROR_SUCCESS);

  Serial.printf("[DisplayBoard] TFT initialized (%s, rot=%d, size=%ux%u, SCK=%d MOSI=%d MISO=%d CS=%d DC=%d BL=%d)\n",
                TFT_DRIVER_NAME,
                TFT_ROTATION_DEGREES,
                (unsigned)g_tft.width(),
                (unsigned)g_tft.height(),
                TFT_SCLK_PIN,
                TFT_MOSI_PIN,
                TFT_MISO_PIN,
                TFT_CS_PIN,
                TFT_DC_PIN,
                TFT_BACKLIGHT_PIN);
  Serial.printf("[DisplayBoard] Touch initialized (%s, ready=%d, rot=%d, SCK=%d MOSI=%d MISO=%d CS=%d)\n",
                "XPT2046",
                _touchReady ? 1 : 0,
                TFT_ROTATION_DEGREES,
                TOUCH_SCLK_PIN,
                TOUCH_MOSI_PIN,
                TOUCH_MISO_PIN,
                TOUCH_CS_PIN);
  _started = true;
  pollTouch();
  render(true);
#endif
}

void BoardDisplayService::loop() {
#if FT_ENABLED(FT_BOARD_ESP32_2432S028)
  if (!_started) {
    return;
  }
  if ((unsigned long)(millis() - _lastRenderMs) < DISPLAY_REFRESH_MS) {
    pollTouch();
    return;
  }
  pollTouch();
  render(false);
#endif
}

String BoardDisplayService::sanitizeForDisplay(const String& value, size_t maxLen) const {
  String out;
  out.reserve((unsigned int)maxLen);
  for (size_t i = 0; i < value.length() && out.length() < maxLen; i++) {
    const char c = value[i];
    if (c >= 32 && c <= 126) {
      out += c;
    } else if (c == '\t') {
      out += ' ';
    } else {
      out += '.';
    }
  }
  return out;
}

void BoardDisplayService::pollTouch() {
#if FT_ENABLED(FT_BOARD_ESP32_2432S028)
  if (!_touchReady) {
    return;
  }

  const unsigned long now = millis();
  if ((unsigned long)(now - _lastTouchPollMs) < TOUCH_POLL_MS) {
    return;
  }
  _lastTouchPollMs = now;

  TOUCHINFO touchInfo;
  if (!g_tft.rtReadTouch(&touchInfo) || touchInfo.count < 1) {
    _touchPressed = false;
    _touchX = TOUCH_IDLE_X;
    _touchY = TOUCH_IDLE_Y;
    _touchZ = TOUCH_IDLE_Z;
    return;
  }

  _touchPressed = true;
  _touchX = touchInfo.x[0];
  _touchY = touchInfo.y[0];
  _touchZ = touchInfo.pressure[0];
#endif
}

void BoardDisplayService::render(bool force) {
#if FT_ENABLED(FT_BOARD_ESP32_2432S028)
  BoardDisplayReaderSnapshot readerSnapshot;
  BoardDisplayWriterSnapshot writerSnapshot;
  BoardDisplayForwarderSnapshot forwarderSnapshot;
  String modeStr = "reader";

  if (_serialService != nullptr) {
    _serialService->read([&](const SerialState& state) {
      readerSnapshot.weight = state.weight;
      readerSnapshot.lastLine = state.lastLine;
      readerSnapshot.timestamp = state.timestamp;
      readerSnapshot.serialSuspended = state.dbgSuspended != 0;
      readerSnapshot.serialStarted = state.dbgSerialStarted != 0;
    });
  }

  if (_serialWriterService != nullptr) {
    _serialWriterService->read([&](const SerialWriterState& state) {
      writerSnapshot.lastSent = state.lastSent;
      writerSnapshot.lastSentAt = state.lastSentAt;
      writerSnapshot.serialSuspended = state.suspended != 0;
      writerSnapshot.serialStarted = state.serialStarted != 0;
    });
  }

  if (_serialWriterForwarderService != nullptr) {
    _serialWriterForwarderService->read([&](const SerialWriterForwarderState& state) {
      forwarderSnapshot.connected = state.connected;
      forwarderSnapshot.lastReceived = state.lastReceived;
      forwarderSnapshot.lastWeight = state.lastWeight;
      forwarderSnapshot.lastReceivedAt = state.lastReceivedAt;
      forwarderSnapshot.lastError = state.lastError;
    });
  }

  if (_uartModeService != nullptr) {
    _uartModeService->read([&](UartModeState& state) {
      modeStr = state.modeStr.length() > 0 ? state.modeStr : "waiting";
    });
  }

  const wifi_mode_t wifiMode = WiFi.getMode();
  const bool staConnected = (wifiMode == WIFI_STA || wifiMode == WIFI_AP_STA) && WiFi.isConnected();
  const bool apActive = (wifiMode == WIFI_AP || wifiMode == WIFI_AP_STA);
  const String staIp = staConnected ? WiFi.localIP().toString() : String("0.0.0.0");
  const String apIp = apActive ? WiFi.softAPIP().toString() : String("0.0.0.0");
  const BoardDisplayViewModel model = buildBoardDisplayViewModel(modeStr, readerSnapshot, writerSnapshot, forwarderSnapshot);

  const String safeWeight = sanitizeForDisplay(model.weight, 12);
  const String safeLine = sanitizeForDisplay(model.detailLine, 36);
  const String safeMode = sanitizeForDisplay(modeStr, 12);
  const String safeStatus = sanitizeForDisplay(model.statusMessage, 36);
  const uint16_t screenWidth = (uint16_t)g_tft.width();
  const uint16_t screenHeight = (uint16_t)g_tft.height();
  const uint16_t statusTop = screenHeight > STATUS_PANEL_HEIGHT
                                 ? (uint16_t)(screenHeight - STATUS_PANEL_HEIGHT)
                                 : screenHeight;
  const uint16_t footerTop = screenHeight > 12 ? (uint16_t)(screenHeight - 10) : screenHeight;
  const int weightAreaX = SCREEN_MARGIN;
  const int weightAreaY = WEIGHT_TOP;
  const int weightAreaW = (int)screenWidth - (SCREEN_MARGIN * 2);
  const int weightAreaH = (int)statusTop - WEIGHT_TOP - 6;
  const String weightText = displayWeightText(safeWeight);
  const String wifiText = staConnected ? String("WiFi ") + staIp
                                       : apActive ? String("AP ") + apIp
                                                  : String("Waiting for WiFi");
  const String detailText = safeLine.length() > 0 ? safeLine : safeStatus;
  const String touchSummary = _touchReady ? String("Touch ready") : String("Touch offline");
  const String safeTouch = sanitizeForDisplay(touchSummary, 28);

  const bool chromeNeedsRedraw = force || !_chromeDrawn;
  const bool modeChanged = chromeNeedsRedraw || safeMode != _lastModeText;
  const bool weightChanged = chromeNeedsRedraw || weightText != _lastWeightText;
  const bool wifiChanged = chromeNeedsRedraw || wifiText != _lastWifiText;
  const bool detailChanged = chromeNeedsRedraw || detailText != _lastDetailText;
  const bool touchChanged = chromeNeedsRedraw || safeTouch != _lastTouchText;

  if (!modeChanged && !weightChanged && !wifiChanged && !detailChanged && !touchChanged) {
    _lastRenderMs = millis();
    return;
  }
  _lastRenderMs = millis();

  if (chromeNeedsRedraw) {
    g_tft.fillScreen(TFT_BLACK);
    g_tft.drawRect(0, 0, screenWidth, screenHeight, TFT_ORANGE);
    g_tft.drawLine(0, HEADER_HEIGHT, screenWidth - 1, HEADER_HEIGHT, TFT_ORANGE);
    g_tft.drawLine(0, statusTop, screenWidth - 1, statusTop, TFT_ORANGE);

    g_tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    g_tft.setFont(FONT_12x16);
    g_tft.setCursor(SCREEN_MARGIN, 10);
    g_tft.print("WEIGHSOFT");

    g_tft.setTextColor(TFT_WHITE, TFT_BLACK);
    g_tft.setFont(FONT_12x16);
    g_tft.setCursor(SCREEN_MARGIN, 48);
    g_tft.print("Scale Display");

    g_tft.setTextColor(TFT_GREY, TFT_BLACK);
    g_tft.setFont(FONT_6x8);
    g_tft.setCursor(SCREEN_MARGIN, footerTop);
    g_tft.print("ESP32-2432S028 / ");
    g_tft.print(TFT_DRIVER_NAME);
    g_tft.print(" / XPT2046");
    _chromeDrawn = true;
  }

  if (modeChanged) {
    g_tft.fillRect(screenWidth / 2, 6, (screenWidth / 2) - SCREEN_MARGIN, HEADER_HEIGHT - 12, TFT_BLACK);
    g_tft.setTextColor(TFT_CYAN, TFT_BLACK);
    g_tft.setFont(FONT_12x16);
    drawRightAlignedText(safeMode, (int)screenWidth - SCREEN_MARGIN, 10);
    _lastModeText = safeMode;
  }

  if (weightChanged) {
    if (chromeNeedsRedraw || _lastWeightText.length() == 0) {
      g_tft.fillRect(weightAreaX, weightAreaY, weightAreaW, weightAreaH, TFT_BLACK);
    } else {
      drawWeightValue(_lastWeightText, weightAreaX, weightAreaY, weightAreaW, weightAreaH, TFT_BLACK);
    }
    drawWeightValue(weightText, weightAreaX, weightAreaY, weightAreaW, weightAreaH, TFT_GREEN);
    _lastWeightText = weightText;
  }

  if (wifiChanged) {
    g_tft.setTextColor(TFT_WHITE, TFT_BLACK);
    g_tft.setFont(FONT_8x8);
    clearTextLine(screenWidth, statusTop + 10, TEXT_LINE_HEIGHT_8);
    g_tft.setCursor(SCREEN_MARGIN, statusTop + 10);
    g_tft.print(wifiText);
    _lastWifiText = wifiText;
  }

  if (detailChanged) {
    g_tft.setTextColor(TFT_WHITE, TFT_BLACK);
    g_tft.setFont(FONT_8x8);
    clearTextLine(screenWidth, statusTop + 22, TEXT_LINE_HEIGHT_8);
    g_tft.setCursor(SCREEN_MARGIN, statusTop + 22);
    g_tft.print(detailText);
    _lastDetailText = detailText;
  }

  if (touchChanged) {
    g_tft.setTextColor(TFT_WHITE, TFT_BLACK);
    g_tft.setFont(FONT_8x8);
    clearTextLine(screenWidth, statusTop + 34, TEXT_LINE_HEIGHT_8);
    g_tft.setCursor(SCREEN_MARGIN, statusTop + 34);
    g_tft.print(safeTouch);
    _lastTouchText = safeTouch;
  }
#else
  (void)force;
#endif
}
