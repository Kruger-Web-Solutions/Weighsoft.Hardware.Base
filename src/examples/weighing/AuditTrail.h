#ifndef AuditTrail_h
#define AuditTrail_h

#include <Arduino.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <time.h>

// Maximum audit entries before FIFO rotation
#define AUDIT_MAX_ENTRIES 100

// File path on SPIFFS (survives OTA)
#define AUDIT_FILE_PATH "/audit/weighing.json"

class AuditTrail {
 public:
  AuditTrail(FS* fs, const char* filePath = AUDIT_FILE_PATH);

  // Initialise: create /audit dir if needed, count existing entries
  void begin();

  // Append a legally relevant event.
  // eventType: "calibrate", "zero", "seal", "unseal", "param_change", "boot"
  // parameterName: "calibration_factor", "zero_offset", "seal_active", etc.
  // oldValue / newValue: string representation of old and new values
  // eventCounter: current OIML event counter value
  void logEvent(const String& eventType,
                const String& parameterName,
                const String& oldValue,
                const String& newValue,
                uint32_t      eventCounter);

  // Return full JSON array as a String for the REST endpoint.
  // Returns "[]" if file is empty or missing.
  String readLog();

  uint16_t getEntryCount() const { return _entryCount; }
  bool isFull() const { return _entryCount >= AUDIT_MAX_ENTRIES; }

 private:
  FS*         _fs;
  const char* _filePath;
  uint16_t    _entryCount;
  uint32_t    _seqCounter;

  // Get ISO 8601 timestamp from NTP. Falls back to "NO_NTP_<millis>" if not synced.
  String _getTimestamp();

  // Remove oldest entry from the JSON file (FIFO when full)
  void _removeOldestEntry();

  // Append one JSON object line to file
  void _appendEntry(const String& entryJson);

  // Rebuild the in-memory count and seq by scanning the file
  void _scanFile();
};

#endif
