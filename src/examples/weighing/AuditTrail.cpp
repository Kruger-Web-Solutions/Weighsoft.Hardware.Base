#include <examples/weighing/AuditTrail.h>

AuditTrail::AuditTrail(FS* fs, const char* filePath)
    : _fs(fs), _filePath(filePath), _entryCount(0), _seqCounter(0) {
}

void AuditTrail::begin() {
  // Create /audit directory if it doesn't exist
  if (!_fs->exists("/audit")) {
    _fs->mkdir("/audit");
  }

  // If the file exists, scan it to count entries and find max seq
  if (_fs->exists(_filePath)) {
    _scanFile();
  }

  Serial.printf("[AuditTrail] Initialised: %d entries, next seq=%u\n",
                _entryCount, _seqCounter);
}

void AuditTrail::_scanFile() {
  File f = _fs->open(_filePath, "r");
  if (!f) return;

  _entryCount = 0;
  _seqCounter = 0;
  String line;
  while (f.available()) {
    char c = f.read();
    if (c == '\n') {
      line.trim();
      if (line.length() > 2) {
        _entryCount++;
        // Extract seq field to find highest
        DynamicJsonDocument doc(256);
        if (deserializeJson(doc, line) == DeserializationError::Ok) {
          uint32_t seq = doc["seq"] | (uint32_t)0;
          if (seq >= _seqCounter) _seqCounter = seq + 1;
        }
      }
      line = "";
    } else {
      line += c;
    }
  }
  // Handle last line without trailing newline
  line.trim();
  if (line.length() > 2) {
    _entryCount++;
    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, line) == DeserializationError::Ok) {
      uint32_t seq = doc["seq"] | (uint32_t)0;
      if (seq >= _seqCounter) _seqCounter = seq + 1;
    }
  }
  f.close();
}

String AuditTrail::_getTimestamp() {
  time_t now;
  struct tm timeinfo;
  time(&now);
  if (now < 1000000000UL) {
    // NTP not synced -- use millis fallback
    return "NO_NTP_" + String(millis());
  }
  gmtime_r(&now, &timeinfo);
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(buf);
}

void AuditTrail::_removeOldestEntry() {
  // Read all lines, skip the first non-empty one, rewrite the file
  File f = _fs->open(_filePath, "r");
  if (!f) return;

  // Read all lines into a buffer (max AUDIT_MAX_ENTRIES lines)
  String lines[AUDIT_MAX_ENTRIES];
  uint16_t count = 0;
  String line;
  while (f.available() && count < AUDIT_MAX_ENTRIES) {
    char c = f.read();
    if (c == '\n') {
      line.trim();
      if (line.length() > 2) {
        lines[count++] = line;
      }
      line = "";
    } else {
      line += c;
    }
  }
  line.trim();
  if (line.length() > 2 && count < AUDIT_MAX_ENTRIES) {
    lines[count++] = line;
  }
  f.close();

  if (count <= 1) {
    // Nothing left after removing oldest -- delete file
    _fs->remove(_filePath);
    _entryCount = 0;
    return;
  }

  // Rewrite from index 1 (skip oldest)
  File fw = _fs->open(_filePath, "w");
  if (!fw) return;
  for (uint16_t i = 1; i < count; i++) {
    fw.println(lines[i]);
  }
  fw.close();
  _entryCount = count - 1;
}

void AuditTrail::_appendEntry(const String& entryJson) {
  File f = _fs->open(_filePath, "a");
  if (!f) {
    Serial.println("[AuditTrail] ERROR: Could not open audit file for append");
    return;
  }
  f.println(entryJson);
  f.close();
}

void AuditTrail::logEvent(const String& eventType,
                          const String& parameterName,
                          const String& oldValue,
                          const String& newValue,
                          uint32_t      eventCounter) {
  // Rotate if at capacity
  if (_entryCount >= AUDIT_MAX_ENTRIES) {
    _removeOldestEntry();
  }

  // Build JSON entry
  DynamicJsonDocument doc(512);
  doc["seq"]           = _seqCounter++;
  doc["timestamp"]     = _getTimestamp();
  doc["event"]         = eventType;
  doc["parameter"]     = parameterName;
  doc["old_value"]     = oldValue;
  doc["new_value"]     = newValue;
  doc["event_counter"] = eventCounter;

  String entryJson;
  serializeJson(doc, entryJson);

  _appendEntry(entryJson);
  _entryCount++;

  Serial.printf("[AuditTrail] Logged: %s / %s -> %s (ec=%u)\n",
                eventType.c_str(), oldValue.c_str(), newValue.c_str(), eventCounter);
}

String AuditTrail::readLog() {
  if (!_fs->exists(_filePath)) {
    return "[]";
  }

  File f = _fs->open(_filePath, "r");
  if (!f) return "[]";

  String result = "[";
  bool first = true;
  String line;

  while (f.available()) {
    char c = f.read();
    if (c == '\n') {
      line.trim();
      if (line.length() > 2) {
        if (!first) result += ",";
        result += line;
        first = false;
      }
      line = "";
    } else {
      line += c;
    }
  }
  // Handle last line without trailing newline
  line.trim();
  if (line.length() > 2) {
    if (!first) result += ",";
    result += line;
  }
  f.close();

  result += "]";
  return result;
}
