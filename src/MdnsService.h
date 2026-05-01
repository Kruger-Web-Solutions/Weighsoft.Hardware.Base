#ifndef MdnsService_h
#define MdnsService_h

#include <Arduino.h>
#include "UartModeService.h"

class MdnsService {
 public:
  MdnsService(UartModeService* uartModeService);

  void begin();
  void loop();
  void refresh();

 private:
  UartModeService* _uartModeService;
  bool             _started = false;
  bool             _enabled = false;        // begin() requested; loop() will start when network is up
  unsigned long    _nextRetryMs = 0;

  String currentMode() const;
  String currentName() const;
  String currentId() const;
  void   applyServiceTxt();
};

#endif
