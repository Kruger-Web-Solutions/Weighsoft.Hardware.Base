#ifndef BoardDisplayService_h
#define BoardDisplayService_h

#include <Arduino.h>
#include <Features.h>

class SerialService;
class SerialWriterForwarderService;
class SerialWriterService;
class UartModeService;

class BoardDisplayService {
 public:
  BoardDisplayService(SerialService* serialService,
                      SerialWriterService* serialWriterService,
                      SerialWriterForwarderService* serialWriterForwarderService,
                      UartModeService* uartModeService);

  void begin();
  void loop();

 private:
  SerialService* _serialService;
  SerialWriterService* _serialWriterService;
  SerialWriterForwarderService* _serialWriterForwarderService;
  UartModeService* _uartModeService;

  String _lastModeText;
  String _lastWeightText;
  String _lastWifiText;
  String _lastDetailText;
  String _lastTouchText;
  unsigned long _lastRenderMs;
  unsigned long _lastTouchPollMs;
  bool _touchReady;
  bool _touchPressed;
  uint16_t _touchX;
  uint16_t _touchY;
  int16_t _touchZ;
  bool _chromeDrawn;
  bool _started;

  void pollTouch();
  void render(bool force);
  String sanitizeForDisplay(const String& value, size_t maxLen) const;
};

#endif
