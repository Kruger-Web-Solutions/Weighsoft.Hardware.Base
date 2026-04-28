#ifndef RemoteWeightService_h
#define RemoteWeightService_h

#include <ESPFS.h>
#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <WebSocketTxRx.h>
#include <examples/remoteweight/RemoteWeightState.h>

#define REMOTE_WEIGHT_ENDPOINT_PATH "/rest/remoteWeight"
#define REMOTE_WEIGHT_SOCKET_PATH "/ws/remoteWeight"
// Audit endpoint deliberately uses a sibling path, NOT a sub-path, so it won't
// collide with HttpEndpoint's prefix matching for /rest/remoteWeight.
#define REMOTE_WEIGHT_AUDIT_PATH "/rest/remoteWeightAudit"
#define REMOTE_WEIGHT_CONFIG_FILE "/config/remoteWeightConfig.json"

// Heap pressure guard. If free heap drops below this, we drop incoming weight
// POSTs (return 503-equivalent UNCHANGED) and log the event. Empirically the
// device started failing around 12 KB; 25 KB gives us margin for the WiFi /
// AsyncWebServer call paths that allocate ~5-10 KB per request.
#define REMOTE_WEIGHT_MIN_FREE_HEAP 25000

// How often the loop reaps disconnected WebSocket clients. Each browser
// refresh / closed tab leaves a stale client behind; without periodic
// cleanup their pending TX buffers accumulate and fragment the heap.
#define REMOTE_WEIGHT_WS_CLEANUP_INTERVAL_MS 5000UL

// Audit log — fixed-size ring buffer of recent significant events. Sized to
// fit one TCP MTU when serialized so /audit responses don't allocate large
// JSON documents themselves.
#define REMOTE_WEIGHT_AUDIT_CAPACITY 32

enum class RemoteWeightAuditReason : uint8_t {
  PERIODIC = 0,           // periodic heap snapshot
  POST_RECEIVED = 1,      // a weight POST arrived
  POST_DROPPED_HEAP = 2,  // POST rejected because heap was below threshold
  ECHO_FIRED = 3,         // periodic USB echo emitted
  WEIGHT_CHANGED = 4,     // actual weight value changed (triggers WS broadcast)
  BOOT = 5,               // service initialised
};

struct RemoteWeightAuditEntry {
  uint32_t millis_at;     // millis() when entry was recorded
  uint32_t free_heap;     // ESP.getFreeHeap()
  uint32_t min_heap;      // ESP.getMinFreeHeap()
  uint32_t max_alloc;     // ESP.getMaxAllocHeap()
  uint16_t posts_total;   // running total of POSTs received
  uint16_t posts_dropped; // running total of POSTs dropped due to heap
  uint8_t  reason;        // RemoteWeightAuditReason
  uint8_t  ws_clients;    // current WS client count (best-effort)
};

class RemoteWeightService : public StatefulService<RemoteWeightState> {
 public:
  RemoteWeightService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager);
  void begin();
  void loop();

  bool isEnabled() const { return _state.enabled; }
  bool isDisplayEnabled() const { return _state.displayEnabled; }

 private:
  AsyncWebServer* _server;
  SecurityManager* _securityManager;
  HttpEndpoint<RemoteWeightState> _httpEndpoint;
  FSPersistence<RemoteWeightState> _fsPersistence;
  WebSocketTxRx<RemoteWeightState> _webSocket;

  // Audit log ring buffer.
  RemoteWeightAuditEntry _audit[REMOTE_WEIGHT_AUDIT_CAPACITY];
  uint8_t _auditHead = 0;   // next write slot
  uint8_t _auditCount = 0;  // entries currently held (caps at CAPACITY)

  // Stats.
  uint16_t _postsTotal = 0;
  uint16_t _postsDropped = 0;
  uint32_t _minHeapSeen = UINT32_MAX;
  unsigned long _lastCleanupMillis = 0;
  unsigned long _lastPeriodicMillis = 0;
  unsigned long _lastSeenTimestamp = 0;  // set when a weight payload arrives

  // Helpers.
  void recordAudit(RemoteWeightAuditReason reason);
  void registerPostReceived();
  void registerPostDropped();
  void registerAuditEndpoint();
  static void readAuditAndStats(RemoteWeightService* self, JsonObject& root);
};

#endif
