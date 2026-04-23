#include <SystemObservability.h>

#ifdef ESP32

#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_wifi_types.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class AsyncWebServer;

namespace {

AsyncWebServer* g_webServer = nullptr;
bool g_wifiLogRegistered = false;
unsigned long g_lastHealthLogMs = 0;
unsigned long g_minFreeHeapSinceBoot = 0xFFFFFFFFUL;

// Health log interval: frequent enough to catch ~2 min failures without flooding serial.
constexpr unsigned long kHealthLogIntervalMs = 20000;

const char* resetReasonStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_UNKNOWN:
      return "UNKNOWN";
    case ESP_RST_POWERON:
      return "POWERON";
    case ESP_RST_EXT:
      return "EXT_PIN";
    case ESP_RST_SW:
      return "SW_RESET";
    case ESP_RST_PANIC:
      return "PANIC/EXCEPTION";
    case ESP_RST_INT_WDT:
      return "INT_WDT";
    case ESP_RST_TASK_WDT:
      return "TASK_WDT";
    case ESP_RST_WDT:
      return "OTHER_WDT";
    case ESP_RST_DEEPSLEEP:
      return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:
      return "BROWNOUT";
    case ESP_RST_SDIO:
      return "SDIO";
    default:
      return "OTHER";
  }
}

const char* wifiModeStr(wifi_mode_t m) {
  switch (m) {
    case WIFI_MODE_NULL:
      return "NULL";
    case WIFI_MODE_STA:
      return "STA";
    case WIFI_MODE_AP:
      return "AP";
    case WIFI_MODE_APSTA:
      return "APSTA";
    default:
      return "?";
  }
}

const char* wlStatusStr(wl_status_t s) {
  switch (s) {
    case WL_IDLE_STATUS:
      return "IDLE";
    case WL_NO_SSID_AVAIL:
      return "NO_SSID";
    case WL_SCAN_COMPLETED:
      return "SCAN_DONE";
    case WL_CONNECTED:
      return "CONNECTED";
    case WL_CONNECT_FAILED:
      return "CONNECT_FAILED";
    case WL_CONNECTION_LOST:
      return "CONN_LOST";
    case WL_DISCONNECTED:
      return "DISCONNECTED";
    default:
      return "OTHER";
  }
}

void logWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  const unsigned long ms = millis();
  switch (event) {
    case ARDUINO_EVENT_WIFI_READY:
      Serial.printf("[Diag][WiFi] t=%lu READY\n", ms);
      break;
    case ARDUINO_EVENT_WIFI_SCAN_DONE:
      Serial.printf("[Diag][WiFi] t=%lu SCAN_DONE status=%u num=%u scan_id=%u\n",
                    ms,
                    (unsigned)info.wifi_scan_done.status,
                    (unsigned)info.wifi_scan_done.number,
                    (unsigned)info.wifi_scan_done.scan_id);
      break;
    case ARDUINO_EVENT_WIFI_STA_START:
      Serial.printf("[Diag][WiFi] t=%lu STA_START\n", ms);
      break;
    case ARDUINO_EVENT_WIFI_STA_STOP:
      Serial.printf("[Diag][WiFi] t=%lu STA_STOP\n", ms);
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED: {
      const uint8_t* b = info.wifi_sta_connected.bssid;
      Serial.printf("[Diag][WiFi] t=%lu STA_CONNECTED ch=%u auth=%u bssid=%02x:%02x:%02x:%02x:%02x:%02x\n",
                    ms,
                    (unsigned)info.wifi_sta_connected.channel,
                    (unsigned)info.wifi_sta_connected.authmode,
                    b[0], b[1], b[2], b[3], b[4], b[5]);
      break;
    }
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
      const wifi_err_reason_t reason = (wifi_err_reason_t)info.wifi_sta_disconnected.reason;
      const char* name = WiFi.disconnectReasonName(reason);
      Serial.printf("[Diag][WiFi] t=%lu STA_DISCONNECTED reason=%d (%s) rssi=%d\n",
                    ms,
                    (int)reason,
                    name ? name : "?",
                    (int)WiFi.RSSI());
      break;
    }
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[Diag][WiFi] t=%lu STA_GOT_IP ip=%s mask=%s gw=%s ip_changed=%d\n",
                    ms,
                    IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str(),
                    IPAddress(info.got_ip.ip_info.netmask.addr).toString().c_str(),
                    IPAddress(info.got_ip.ip_info.gw.addr).toString().c_str(),
                    info.got_ip.ip_changed ? 1 : 0);
      break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      Serial.printf("[Diag][WiFi] t=%lu STA_LOST_IP\n", ms);
      break;
    case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
      Serial.printf("[Diag][WiFi] t=%lu STA_AUTHMODE_CHANGE old=%u new=%u\n",
                    ms,
                    (unsigned)info.wifi_sta_authmode_change.old_mode,
                    (unsigned)info.wifi_sta_authmode_change.new_mode);
      break;
    case ARDUINO_EVENT_WIFI_AP_START:
      Serial.printf("[Diag][WiFi] t=%lu AP_START\n", ms);
      break;
    case ARDUINO_EVENT_WIFI_AP_STOP:
      Serial.printf("[Diag][WiFi] t=%lu AP_STOP\n", ms);
      break;
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      Serial.printf("[Diag][WiFi] t=%lu AP_STACONNECTED aid=%u mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
                    ms,
                    (unsigned)info.wifi_ap_staconnected.aid,
                    info.wifi_ap_staconnected.mac[0],
                    info.wifi_ap_staconnected.mac[1],
                    info.wifi_ap_staconnected.mac[2],
                    info.wifi_ap_staconnected.mac[3],
                    info.wifi_ap_staconnected.mac[4],
                    info.wifi_ap_staconnected.mac[5]);
      break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      Serial.printf("[Diag][WiFi] t=%lu AP_STADISCONNECTED aid=%u mesh_child=%d mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
                    ms,
                    (unsigned)info.wifi_ap_stadisconnected.aid,
                    info.wifi_ap_stadisconnected.is_mesh_child ? 1 : 0,
                    info.wifi_ap_stadisconnected.mac[0],
                    info.wifi_ap_stadisconnected.mac[1],
                    info.wifi_ap_stadisconnected.mac[2],
                    info.wifi_ap_stadisconnected.mac[3],
                    info.wifi_ap_stadisconnected.mac[4],
                    info.wifi_ap_stadisconnected.mac[5]);
      break;
    case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
      Serial.printf("[Diag][WiFi] t=%lu AP_STAIPASSIGNED ip=%s\n",
                    ms,
                    IPAddress(info.wifi_ap_staipassigned.ip.addr).toString().c_str());
      break;
    case ARDUINO_EVENT_ETH_START:
    case ARDUINO_EVENT_ETH_STOP:
    case ARDUINO_EVENT_ETH_CONNECTED:
    case ARDUINO_EVENT_ETH_DISCONNECTED:
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.printf("[Diag][WiFi] t=%lu ETH_EVENT id=%d\n", ms, (int)event);
      break;
    default:
      Serial.printf("[Diag][WiFi] t=%lu EVENT id=%d\n", ms, (int)event);
      break;
  }
}

void logPeriodicHealth() {
  const unsigned long ms = millis();
  const uint32_t heap = ESP.getFreeHeap();
  if (heap < g_minFreeHeapSinceBoot) {
    g_minFreeHeapSinceBoot = heap;
  }
  const uint32_t maxAlloc = ESP.getMaxAllocHeap();
  const wifi_mode_t mode = WiFi.getMode();
  const wl_status_t st = WiFi.status();
  const bool staUp = WiFi.isConnected();
  const int rssi = staUp ? WiFi.RSSI() : 0;
  const char* ipStr = staUp ? WiFi.localIP().toString().c_str() : "0.0.0.0";

  UBaseType_t stackWords = uxTaskGetStackHighWaterMark(nullptr);

  Serial.printf(
      "[Diag][Health] t=%lu up=%lus heap=%u minHeap=%u maxAlloc=%u mode=%s wl=%s staConn=%d ip=%s rssi=%d "
      "core0StackFreeWords=%u webServer=%p\n",
      ms,
      ms / 1000UL,
      (unsigned)heap,
      (unsigned)g_minFreeHeapSinceBoot,
      (unsigned)maxAlloc,
      wifiModeStr(mode),
      wlStatusStr(st),
      staUp ? 1 : 0,
      ipStr,
      rssi,
      (unsigned)stackWords,
      (void*)g_webServer);

  // Soft warning when heap is critically low (typical symptom before WiFi / LWIP instability).
  if (heap < 25000) {
    Serial.printf("[Diag][Health] WARN low free heap=%u (threshold 25000)\n", (unsigned)heap);
  }
}

}  // namespace

void systemObservabilityBootLog() {
  const esp_reset_reason_t rr = esp_reset_reason();
  Serial.println(F("[Diag][Boot] ========== cold/warm start diagnostics =========="));
  Serial.printf("[Diag][Boot] reset_reason=%d (%s)\n", (int)rr, resetReasonStr(rr));
  if (rr == ESP_RST_BROWNOUT) {
    Serial.println(F("[Diag][Boot] NOTE: last reset was BROWNOUT — check PSU, USB cable, and load."));
  }
  if (rr == ESP_RST_TASK_WDT || rr == ESP_RST_INT_WDT) {
    Serial.println(F("[Diag][Boot] NOTE: last reset was WATCHDOG — a task blocked too long."));
  }
  if (rr == ESP_RST_PANIC) {
    Serial.println(F("[Diag][Boot] NOTE: last reset was PANIC/crash — check serial backtrace if captured."));
  }
  Serial.printf("[Diag][Boot] chip_model=%s rev=%u cores=%u cpu_MHz=%u flash_MHz=%u\n",
                ESP.getChipModel(),
                ESP.getChipRevision(),
                ESP.getChipCores(),
                (unsigned)ESP.getCpuFreqMHz(),
                (unsigned)ESP.getFlashChipSpeed() / 1000000U);
  Serial.printf("[Diag][Boot] sdk=%s heap=%u maxAlloc=%u\n",
                ESP.getSdkVersion(),
                (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMaxAllocHeap());
  Serial.println(F("[Diag][Boot] ================================================="));
}

void systemObservabilityRegister(AsyncWebServer* webServer) {
  g_webServer = webServer;
  if (g_wifiLogRegistered) {
    return;
  }
  g_wifiLogRegistered = true;
  WiFi.onEvent(logWifiEvent);
  Serial.println(F("[Diag] SystemObservability: Wi-Fi event logging registered (all events)."));
}

void systemObservabilityLoop() {
  const unsigned long now = millis();
  if (g_lastHealthLogMs != 0 && (now - g_lastHealthLogMs) < kHealthLogIntervalMs) {
    return;
  }
  g_lastHealthLogMs = now;
  logPeriodicHealth();
}

#else

void systemObservabilityBootLog() {}

void systemObservabilityRegister(AsyncWebServer*) {}

void systemObservabilityLoop() {}

#endif
