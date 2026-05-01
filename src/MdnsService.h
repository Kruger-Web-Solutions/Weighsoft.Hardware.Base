#ifndef MdnsService_h
#define MdnsService_h

#include <Arduino.h>
#include "UartModeService.h"

class MdnsService {
 public:
  MdnsService(UartModeService* uartModeService);

  void begin();
  void refresh();

 private:
  UartModeService* _uartModeService;
  bool             _started = false;

  String currentMode() const;
  String currentName() const;
  String currentId() const;
  void   applyServiceTxt();
};

#endif
