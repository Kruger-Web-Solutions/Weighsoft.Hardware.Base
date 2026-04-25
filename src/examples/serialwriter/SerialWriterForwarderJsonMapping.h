#ifndef SerialWriterForwarderJsonMapping_h
#define SerialWriterForwarderJsonMapping_h

#include <ArduinoJson.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>

/**
 * Outcome of parsing a forwarder JSON message (HTTP body or WS text frame)
 * and extracting the configured json_field as a line.
 */
enum class SerialWriterForwarderJsonOutcome : int {
  /** Field found and non-empty after trim */
  Ok = 0,
  /** deserializeJson failed */
  ParseError = 1,
  /** Valid JSON but json_field absent at top-level and under payload */
  FieldMissing = 2,
  /** Field present but empty or whitespace-only */
  FieldEmpty = 3
};

namespace SerialWriterForwarderJsonMappingDetail {

inline bool copyCstrTrimmed(const char* p, char* lineOut, size_t lineOutCap) {
  if (!p || !lineOut || lineOutCap == 0) {
    return false;
  }
  while (*p != '\0' && isspace((unsigned char)*p)) {
    ++p;
  }
  size_t len = strlen(p);
  while (len > 0 && isspace((unsigned char)p[len - 1])) {
    --len;
  }
  if (len == 0) {
    return false;
  }
  if (len + 1 > lineOutCap) {
    return false;
  }
  memcpy(lineOut, p, len);
  lineOut[len] = '\0';
  return true;
}

/** Fill lineOut from a JSON variant (string, number, bool, etc.) without Arduino String. */
inline bool fillLineFromVariant(JsonVariant v, char* lineOut, size_t lineOutCap) {
  if (v.isNull()) {
    return false;
  }
  if (v.is<const char*>()) {
    const char* p = v.as<const char*>();
    return copyCstrTrimmed(p ? p : "", lineOut, lineOutCap);
  }
  size_t n = serializeJson(v, lineOut, lineOutCap);
  if (n == 0 || n >= lineOutCap) {
    return false;
  }
  lineOut[n] = '\0';
  // Trim serialized primitives (e.g. numbers have no surrounding whitespace)
  const char* p = lineOut;
  while (*p != '\0' && isspace((unsigned char)*p)) {
    ++p;
  }
  if (p != lineOut) {
    memmove(lineOut, p, strlen(p) + 1);
  }
  size_t len = strlen(lineOut);
  while (len > 0 && isspace((unsigned char)lineOut[len - 1])) {
    lineOut[--len] = '\0';
  }
  return len > 0;
}

}  // namespace SerialWriterForwarderJsonMappingDetail

/**
 * Extract line from an already-parsed JSON document (HTTP path).
 */
template <typename TDoc>
inline SerialWriterForwarderJsonOutcome serialWriterForwarderExtractLineFromDocument(
    const char* jsonField,
    TDoc& doc,
    char* lineOut,
    size_t lineOutCap,
    char* sourcePathOut,
    size_t sourcePathCap) {
  using SerialWriterForwarderJsonMappingDetail::fillLineFromVariant;

  if (!jsonField || jsonField[0] == '\0' || !lineOut || lineOutCap == 0) {
    return SerialWriterForwarderJsonOutcome::ParseError;
  }
  lineOut[0] = '\0';
  if (sourcePathOut && sourcePathCap > 0) {
    sourcePathOut[0] = '\0';
  }

  if (doc.containsKey(jsonField)) {
    JsonVariant v = doc[jsonField];
    if (v.isNull()) {
      return SerialWriterForwarderJsonOutcome::FieldEmpty;
    }
    if (!fillLineFromVariant(v, lineOut, lineOutCap)) {
      return SerialWriterForwarderJsonOutcome::FieldEmpty;
    }
    if (sourcePathOut && sourcePathCap > 0) {
      strncpy(sourcePathOut, "top-level", sourcePathCap - 1);
      sourcePathOut[sourcePathCap - 1] = '\0';
    }
    return SerialWriterForwarderJsonOutcome::Ok;
  }

  JsonVariant payload = doc["payload"];
  if (!payload.isNull() && payload.is<JsonObject>()) {
    JsonObject po = payload.as<JsonObject>();
    if (po.containsKey(jsonField)) {
      JsonVariant v = po[jsonField];
      if (v.isNull()) {
        return SerialWriterForwarderJsonOutcome::FieldEmpty;
      }
      if (!fillLineFromVariant(v, lineOut, lineOutCap)) {
        return SerialWriterForwarderJsonOutcome::FieldEmpty;
      }
      if (sourcePathOut && sourcePathCap > 0) {
        strncpy(sourcePathOut, "payload", sourcePathCap - 1);
        sourcePathOut[sourcePathCap - 1] = '\0';
      }
      return SerialWriterForwarderJsonOutcome::Ok;
    }
  }

  return SerialWriterForwarderJsonOutcome::FieldMissing;
}

/**
 * Parse raw JSON bytes and extract configured field (WS path).
 */
inline SerialWriterForwarderJsonOutcome serialWriterForwarderExtractLineFromJsonBytes(
    const char* jsonField,
    const uint8_t* payload,
    size_t length,
    char* lineOut,
    size_t lineOutCap,
    char* sourcePathOut,
    size_t sourcePathCap) {
  if (!jsonField || !payload || !lineOut || lineOutCap == 0) {
    return SerialWriterForwarderJsonOutcome::ParseError;
  }
  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err != DeserializationError::Ok) {
    return SerialWriterForwarderJsonOutcome::ParseError;
  }
  return serialWriterForwarderExtractLineFromDocument(jsonField, doc, lineOut, lineOutCap, sourcePathOut,
                                                      sourcePathCap);
}

#endif
