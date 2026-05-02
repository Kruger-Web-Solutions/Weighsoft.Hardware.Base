#ifndef KnownWritersState_h
#define KnownWritersState_h

#include <vector>
#include <StatefulService.h>

#define KNOWN_WRITERS_MAX 32

struct KnownWriter {
  String        id;             // hardware unique id, persistent identity
  String        friendlyName;   // user-set or default to id-prefix
  String        ip;
  unsigned long firstSeenAt = 0;
  unsigned long lastSeenAt = 0;
  unsigned long lastMessageAt = 0;
  String        lastMessage;
  bool          online = false;
};

class KnownWritersState {
 public:
  std::vector<KnownWriter> writers;

  KnownWriter* findById(const String& id) {
    for (auto& w : writers) if (w.id == id) return &w;
    return nullptr;
  }

  bool removeById(const String& id) {
    for (auto it = writers.begin(); it != writers.end(); ++it) {
      if (it->id == id) { writers.erase(it); return true; }
    }
    return false;
  }

  KnownWriter* upsert(const String& id, const String& name, const String& ip, unsigned long now) {
    KnownWriter* existing = findById(id);
    if (existing) {
      if (name.length() > 0) existing->friendlyName = name;
      existing->ip = ip;
      existing->lastSeenAt = now;
      existing->online = true;
      return existing;
    }

    if (writers.size() >= KNOWN_WRITERS_MAX) {
      auto oldestOfflineIdx = (size_t)-1;
      unsigned long oldestSeen = (unsigned long)-1;
      for (size_t i = 0; i < writers.size(); i++) {
        if (!writers[i].online && writers[i].lastSeenAt < oldestSeen) {
          oldestSeen = writers[i].lastSeenAt;
          oldestOfflineIdx = i;
        }
      }
      if (oldestOfflineIdx != (size_t)-1) writers.erase(writers.begin() + oldestOfflineIdx);
    }

    if (writers.size() >= KNOWN_WRITERS_MAX) return nullptr;

    KnownWriter w;
    w.id = id;
    w.friendlyName = name.length() > 0 ? name : id.substring(0, 8);
    w.ip = ip;
    w.firstSeenAt = now;
    w.lastSeenAt = now;
    w.online = true;
    writers.push_back(w);
    return &writers.back();
  }

  void markOffline(const String& id, unsigned long now) {
    KnownWriter* w = findById(id);
    if (w) { w->online = false; w->lastSeenAt = now; }
  }

  void recordMessageReceived(const String& id, const String& message, unsigned long now) {
    KnownWriter* w = findById(id);
    if (w) { w->lastMessage = message; w->lastMessageAt = now; w->lastSeenAt = now; }
  }

  static void read(KnownWritersState& state, JsonObject& root) {
    JsonArray arr = root.createNestedArray("writers");
    for (const auto& w : state.writers) {
      JsonObject o = arr.createNestedObject();
      o["id"]              = w.id;
      o["friendly_name"]   = w.friendlyName;
      o["ip"]              = w.ip;
      o["first_seen_at"]   = w.firstSeenAt;
      o["last_seen_at"]    = w.lastSeenAt;
      o["last_message_at"] = w.lastMessageAt;
      o["last_message"]    = w.lastMessage;
      o["online"]          = w.online;
    }
  }

  static StateUpdateResult update(JsonObject& root, KnownWritersState& state) {
    String id = root["id"] | "";
    if (id.length() > 0) {
      String name = root["name"] | "";
      String ip = root["ip"] | "";
      state.upsert(id, name, ip, millis());
      return StateUpdateResult::CHANGED;
    }
    return StateUpdateResult::UNCHANGED;
  }

  static void readConfig(KnownWritersState& state, JsonObject& root) {
    JsonArray arr = root.createNestedArray("writers");
    for (const auto& w : state.writers) {
      JsonObject o = arr.createNestedObject();
      o["id"]              = w.id;
      o["friendly_name"]   = w.friendlyName;
      o["ip"]              = w.ip;
      o["first_seen_at"]   = w.firstSeenAt;
      o["last_seen_at"]    = w.lastSeenAt;
      o["last_message_at"] = w.lastMessageAt;
      o["last_message"]    = w.lastMessage;
    }
  }

  static StateUpdateResult updateConfig(JsonObject& root, KnownWritersState& state) {
    state.writers.clear();
    JsonArray arr = root["writers"];
    for (JsonObject o : arr) {
      KnownWriter w;
      w.id              = o["id"]              | "";
      w.friendlyName    = o["friendly_name"]   | "";
      w.ip              = o["ip"]              | "";
      w.firstSeenAt     = o["first_seen_at"]   | 0UL;
      w.lastSeenAt      = o["last_seen_at"]    | 0UL;
      w.lastMessageAt   = o["last_message_at"] | 0UL;
      w.lastMessage     = o["last_message"]    | "";
      w.online          = false;  // always start offline on boot
      if (w.id.length() > 0) state.writers.push_back(w);
    }
    return StateUpdateResult::CHANGED;
  }
};

#endif
