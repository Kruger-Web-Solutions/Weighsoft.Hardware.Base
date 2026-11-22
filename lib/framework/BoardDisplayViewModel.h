#ifndef BoardDisplayViewModel_h
#define BoardDisplayViewModel_h

#include <Arduino.h>

struct BoardDisplayReaderSnapshot {
  String weight;
  String lastLine;
  unsigned long timestamp = 0;
  bool serialSuspended = false;
  bool serialStarted = false;
};

struct BoardDisplayWriterSnapshot {
  String lastSent;
  unsigned long lastSentAt = 0;
  bool serialSuspended = false;
  bool serialStarted = false;
};

struct BoardDisplayForwarderSnapshot {
  bool connected = false;
  String lastReceived;
  String lastWeight;
  unsigned long lastReceivedAt = 0;
  String lastError;
};

struct BoardDisplayViewModel {
  String weight;
  String detailLine;
  String statusMessage;
  unsigned long timestamp = 0;
  bool serialSuspended = false;
  bool serialStarted = false;
};

BoardDisplayViewModel buildBoardDisplayViewModel(const String& modeStr,
                                                 const BoardDisplayReaderSnapshot& readerState,
                                                 const BoardDisplayWriterSnapshot& writerState,
                                                 const BoardDisplayForwarderSnapshot& forwarderState);

#endif
