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

  void setBacklight(bool on);
  void drawSplash();
  void drawWeightScreen(const String& weight, const String& line, bool connected);
  void drawStatusBar(bool connected, unsigned long ageMs);
  void drawWeightValue(const String& weight, bool connected);
  void drawRawLine(const String& line);
};

#endif  // HAS_TFT_DISPLAY
#endif  // DisplayService_h
