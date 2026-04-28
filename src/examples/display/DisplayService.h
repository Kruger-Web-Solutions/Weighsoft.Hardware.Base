#ifndef DisplayService_h
#define DisplayService_h

#ifdef HAS_TFT_DISPLAY

#include <TFT_eSPI.h>
#include <examples/remoteweight/RemoteWeightService.h>

// How long (ms) without a weight update before we show "NO SIGNAL"
#define DISPLAY_TIMEOUT_MS 15000

// How often (ms) the status bar clock ticks
#define DISPLAY_STATUS_INTERVAL_MS 1000

class DisplayService {
 public:
  DisplayService(RemoteWeightService* remoteWeightService);
  void begin();
  void loop();

 private:
  TFT_eSPI _tft;
  RemoteWeightService* _remoteWeightService;

  volatile bool _needsWeightRedraw;
  String _pendingWeight;
  String _pendingLine;
  unsigned long _lastWeightTime;

  String _drawnWeight;
  bool _drawnConnected;
  bool _displayOn;

  unsigned long _lastStatusDraw;

  // Layout dimensions — populated in begin() from _tft.width() / _tft.height()
  // so the same code drives both 480x320 (3.5" ILI9488) and 320x240 (2.8" CYD
  // ILI9341) panels without hardcoded constants going off-screen.
  int16_t _w = 480;
  int16_t _h = 320;
  int16_t _statusBarH = 36;
  int16_t _weightCenterY = 160;
  int16_t _rawLineY = 270;
  uint8_t _titleTextSize = 3;   // splash WEIGHSOFT label
  uint8_t _statusTextSize = 2;  // status bar text
  uint8_t _weightTextSize = 6;  // big weight digits
  uint8_t _unitTextSize = 3;    // "kg" suffix

  void setBacklight(bool on);
  void drawSplash();
  void drawWeightScreen(const String& weight, const String& line, bool connected);
  void drawStatusBar(bool connected, unsigned long ageMs);
  void drawWeightValue(const String& weight, bool connected);
  void drawRawLine(const String& line);
};

#endif  // HAS_TFT_DISPLAY
#endif  // DisplayService_h
