#include "BoardDisplayViewModel.h"

BoardDisplayViewModel buildBoardDisplayViewModel(const String& modeStr,
                                                 const BoardDisplayReaderSnapshot& readerState,
                                                 const BoardDisplayWriterSnapshot& writerState,
                                                 const BoardDisplayForwarderSnapshot& forwarderState) {
  BoardDisplayViewModel model;
  if (modeStr == "writer") {
    model.weight = forwarderState.lastWeight;
    model.detailLine = forwarderState.lastReceived.length() > 0 ? forwarderState.lastReceived : writerState.lastSent;
    model.timestamp = forwarderState.lastReceivedAt > 0 ? forwarderState.lastReceivedAt : writerState.lastSentAt;
    model.serialSuspended = writerState.serialSuspended;
    model.serialStarted = writerState.serialStarted;

    if (model.detailLine.length() > 0) {
      model.statusMessage = model.detailLine;
    } else if (forwarderState.lastError.length() > 0) {
      model.statusMessage = forwarderState.lastError;
    } else if (model.serialStarted && !model.serialSuspended && forwarderState.connected) {
      model.statusMessage = "Waiting for reader data...";
    } else if (model.serialStarted && !model.serialSuspended) {
      model.statusMessage = "Connecting to reader...";
    } else if (model.serialSuspended) {
      model.statusMessage = "Writer serial paused";
    } else {
      model.statusMessage = "Writer starting...";
    }
  } else {
    model.weight = readerState.weight;
    model.detailLine = readerState.lastLine;
    model.timestamp = readerState.timestamp;
    model.serialSuspended = readerState.serialSuspended;
    model.serialStarted = readerState.serialStarted;

    if (model.detailLine.length() > 0) {
      model.statusMessage = model.detailLine;
    } else if (model.serialStarted && !model.serialSuspended) {
      model.statusMessage = "Waiting for weight data...";
    } else if (model.serialSuspended) {
      model.statusMessage = "Serial paused by diagnostics";
    } else {
      model.statusMessage = "Serial monitor starting...";
    }
  }

  return model;
}
