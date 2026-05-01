# Multi-Mode Serial — Phase 2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the Reader's persistent "Writers" tab that lists every Writer device that has ever connected, shows online/offline status with friendly names, last message received, and provides a Forget button. Builds on Phase 1.

**Architecture:** New backend service `KnownWritersService` maintains a persistent registry at `/config/knownWriters.json`. The existing `/ws/serial` WebSocket gets a connect-time hook that reads `?role=writer&id=&name=` query parameters (already sent by `SerialWriterForwarderService` from Phase 1) and registers/updates the writer. REST endpoints `GET /rest/writers` and `DELETE /rest/writers/{id}` expose the list. New frontend tab `Writers.tsx` polls the list and renders it.

**Tech Stack:** Same as Phase 1 — ESP32-S3 PlatformIO/Arduino C++, ESPAsyncWebServer, ArduinoJson, FSPersistence, React/TypeScript/MUI.

**Reference spec:** [`docs/superpowers/specs/2026-04-30-multi-mode-serial-reader-writer-design.md`](../specs/2026-04-30-multi-mode-serial-reader-writer-design.md) Section 3.

---

## File map

### Backend — created

- `src/examples/serial/KnownWritersState.h` — registry data class (per-Writer entries, JSON shape)
- `src/examples/serial/KnownWritersService.h`/`.cpp` — service exposing GET/DELETE REST endpoints, persistence

### Backend — modified

- `src/examples/serial/SerialService.h`/`.cpp` — hook the `/ws/serial` connect event to register Writers; on broadcast, update each connected Writer's last-message timestamp
- `src/main.cpp` — instantiate and wire `KnownWritersService`

### Frontend — created

- `interface/src/types/knownWriters.ts` — TypeScript types
- `interface/src/api/knownWriters.ts` — REST clients
- `interface/src/examples/serial/Writers.tsx` — the Writers tab component

### Frontend — modified

- `interface/src/examples/serial/SerialReader.tsx` — add 6th tab "Writers"
- `interface/src/examples/serial/SerialInfo.tsx` (or whichever component renders the Information tab) — add small "N writers connected" line

---

## Task 1 — KnownWriters state class

**File to CREATE:** `src/examples/serial/KnownWritersState.h`

- [ ] **Step 1: Write the state header.**

```cpp
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

  // Add or update; returns pointer to the entry.
  KnownWriter* upsert(const String& id, const String& name, const String& ip, unsigned long now) {
    KnownWriter* existing = findById(id);
    if (existing) {
      if (name.length() > 0) existing->friendlyName = name;
      existing->ip = ip;
      existing->lastSeenAt = now;
      existing->online = true;
      return existing;
    }

    // Cap reached: prune oldest offline entry to make room
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
      // If all entries are online, we just don't add — caller can check size after
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
    // Updates from REST are limited to renames; bulk replace not supported via REST
    return StateUpdateResult::UNCHANGED;
  }

  // Persistence: only the durable bits, all online flags reset to false on load
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
```

- [ ] **Step 2: Build (smoke check).**

Run: `pio run -e esp32s3`
Expected: clean build (the header is included nowhere yet, so it's a no-op until Task 2).

- [ ] **Step 3: Commit.**

```bash
git add src/examples/serial/KnownWritersState.h
git commit -m "feat(serial): add KnownWritersState — persistent registry data class"
```

---

## Task 2 — KnownWritersService

**Files:**
- Create: `src/examples/serial/KnownWritersService.h`
- Create: `src/examples/serial/KnownWritersService.cpp`

- [ ] **Step 1: Write the header.**

```cpp
#ifndef KnownWritersService_h
#define KnownWritersService_h

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <SecurityManager.h>
#include <examples/serial/KnownWritersState.h>

#define KNOWN_WRITERS_ENDPOINT_PATH "/rest/writers"
#define KNOWN_WRITERS_CONFIG_FILE   "/config/knownWriters.json"

class KnownWritersService : public StatefulService<KnownWritersState> {
 public:
  KnownWritersService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager);
  void begin();

  // Called by SerialService when a Writer subscribes to /ws/serial
  void onWriterConnected(const String& id, const String& name, const String& ip);
  void onWriterDisconnected(const String& id);

  // Called by SerialService when broadcasting a parsed scale line — updates last-message
  // for every currently-online Writer.
  void recordBroadcastToOnlineWriters(const String& message);

  // For other services: expose count for the Information tab badge.
  size_t connectedCount() const;

 private:
  HttpEndpoint<KnownWritersState>  _httpEndpoint;
  FSPersistence<KnownWritersState> _fsPersistence;
  AsyncWebServer*                  _server;
  SecurityManager*                 _securityManager;

  void registerForgetEndpoint();
  void persistAsync();
};

#endif
```

- [ ] **Step 2: Write the implementation.**

```cpp
#include <examples/serial/KnownWritersService.h>

KnownWritersService::KnownWritersService(AsyncWebServer* server,
                                         FS* fs,
                                         SecurityManager* securityManager) :
    _httpEndpoint(KnownWritersState::read,
                  KnownWritersState::update,
                  this,
                  server,
                  KNOWN_WRITERS_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::IS_AUTHENTICATED),
    _fsPersistence(KnownWritersState::readConfig,
                   KnownWritersState::updateConfig,
                   this,
                   fs,
                   KNOWN_WRITERS_CONFIG_FILE),
    _server(server),
    _securityManager(securityManager) {}

void KnownWritersService::begin() {
  _fsPersistence.readFromFS();
  registerForgetEndpoint();
}

void KnownWritersService::onWriterConnected(const String& id, const String& name, const String& ip) {
  if (id.length() == 0) return;  // ignore connections without identity
  update([&](KnownWritersState& s) {
    s.upsert(id, name, ip, millis());
    return StateUpdateResult::CHANGED;
  }, "writer-connect");
  persistAsync();
}

void KnownWritersService::onWriterDisconnected(const String& id) {
  if (id.length() == 0) return;
  update([&](KnownWritersState& s) {
    s.markOffline(id, millis());
    return StateUpdateResult::CHANGED;
  }, "writer-disconnect");
  persistAsync();
}

void KnownWritersService::recordBroadcastToOnlineWriters(const String& message) {
  bool changed = false;
  unsigned long now = millis();
  update([&](KnownWritersState& s) {
    for (auto& w : s.writers) {
      if (w.online) {
        w.lastMessage = message;
        w.lastMessageAt = now;
        w.lastSeenAt = now;
        changed = true;
      }
    }
    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
  }, "writer-broadcast");
  // Don't persist on every broadcast — caller can persist periodically if needed.
  // For v1, we accept that lastMessage is in-memory only between persists.
}

size_t KnownWritersService::connectedCount() const {
  size_t n = 0;
  // Use read() which is non-const in this framework; cast away const for the call site
  const_cast<KnownWritersService*>(this)->read([&](const KnownWritersState& s) {
    for (const auto& w : s.writers) if (w.online) n++;
  });
  return n;
}

void KnownWritersService::persistAsync() {
  _fsPersistence.writeToFS();
}

void KnownWritersService::registerForgetEndpoint() {
  // DELETE /rest/writers/{id} — remove a single entry
  _server->on("^\\/rest\\/writers\\/([a-zA-Z0-9-]+)$",
              HTTP_DELETE,
              [this](AsyncWebServerRequest* req) {
                if (!_securityManager->filterRequest(req, AuthenticationPredicates::IS_AUTHENTICATED)) {
                  req->send(401, "application/json", "{\"error\":\"unauthorized\"}");
                  return;
                }
                String id = req->pathArg(0);
                bool removed = false;
                update([&](KnownWritersState& s) {
                  removed = s.removeById(id);
                  return removed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
                }, "writer-forget");
                if (removed) persistAsync();
                req->send(removed ? 200 : 404, "application/json",
                          removed ? "{\"removed\":true}" : "{\"error\":\"not found\"}");
              });
}
```

- [ ] **Step 3: Build.**

Run: `pio run -e esp32s3`
Expected: clean build. If `_securityManager->filterRequest` signature differs in this codebase, adapt to match how other services check auth. If regex routing isn't supported by this AsyncWebServer version, replace with a manual prefix-match handler in `registerForgetEndpoint`.

- [ ] **Step 4: Commit.**

```bash
git add src/examples/serial/KnownWritersService.h src/examples/serial/KnownWritersService.cpp
git commit -m "feat(serial): add KnownWritersService — registry endpoints + persistence"
```

---

## Task 3 — Wire KnownWritersService into SerialService and main.cpp

**Files:**
- Modify: `src/examples/serial/SerialService.h`
- Modify: `src/examples/serial/SerialService.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Add a setter on SerialService.**

In `src/examples/serial/SerialService.h`, add a forward declaration and setter:

```cpp
class KnownWritersService;  // forward
```

In the public section, add:

```cpp
void setKnownWritersService(KnownWritersService* svc) { _knownWriters = svc; }
```

In the private section:

```cpp
KnownWritersService* _knownWriters = nullptr;
```

- [ ] **Step 2: Hook the WebSocket connect/disconnect events.**

In `src/examples/serial/SerialService.cpp`, find where the `_webSocket` is constructed. The framework's `WebSocketTxRx` typically exposes connect/disconnect hooks via the underlying `AsyncWebSocket`. Read the existing wiring to understand. Then:

- On WS client connect: extract `?role=writer&id=&name=` from the request URL. If `role=writer`, call `_knownWriters->onWriterConnected(id, name, clientIp)`.
- On WS client disconnect: if you have the client's id stored, call `_knownWriters->onWriterDisconnected(id)`.

The simplest implementation: maintain a small `std::map<uint32_t, String>` from AsyncWebSocketClient* (or its id) to writer id. On connect: parse URL, store mapping, register. On disconnect: look up mapping, deregister, remove.

Look at the existing `WebSocketTxRx` in the framework (`lib/framework/WebSocketTxRx.h` or similar) for how to attach connect/disconnect callbacks. If the framework class doesn't expose these directly, you may need to add a separate `AsyncWebSocket` mounted alongside, OR subclass — pick the least invasive option.

If the existing `WebSocketTxRx` does NOT expose connect callbacks, **mark this task DONE_WITH_CONCERNS** with a clear note. The `recordBroadcastToOnlineWriters` call in Step 3 below can still update timestamps for any writer whose connection we've identified through other means; without the connect hook the registry will just not auto-populate, and the user can manually rename entries that appear via heartbeat.

- [ ] **Step 3: Hook the broadcast path.**

In `SerialService.cpp`, find where parsed scale lines are broadcast (search for `_webSocket.send` or similar — often inside `enqueueCompletedLine` or the publish path). After the broadcast call, add:

```cpp
if (_knownWriters) {
  _knownWriters->recordBroadcastToOnlineWriters(line);
}
```

- [ ] **Step 4: Wire in main.cpp.**

After other service instantiations:

```cpp
KnownWritersService knownWritersService(&server, &ESPFS, esp32sveltekit.getSecurityManager());
```

(Or however the existing `SerialService` accesses these — match the pattern.)

In `setup()`:

```cpp
knownWritersService.begin();
serialService.setKnownWritersService(&knownWritersService);
```

(After both `serialService.begin()` and `knownWritersService.begin()` — the order is: known writers first so when serialService starts it has the service to call.)

- [ ] **Step 5: Build.**

Run: `pio run -e esp32s3`
Expected: clean build success.

- [ ] **Step 6: Commit.**

```bash
git add src/examples/serial/SerialService.h src/examples/serial/SerialService.cpp src/main.cpp
git commit -m "feat(serial): wire KnownWritersService into SerialService and main.cpp"
```

---

## Task 4 — Frontend types and API

**Files:**
- Create: `interface/src/types/knownWriters.ts`
- Create: `interface/src/api/knownWriters.ts`

- [ ] **Step 1: Create the types file.**

```ts
export interface KnownWriter {
  id: string;
  friendly_name: string;
  ip: string;
  first_seen_at: number;
  last_seen_at: number;
  last_message_at: number;
  last_message: string;
  online: boolean;
}

export interface KnownWritersData {
  writers: KnownWriter[];
}
```

- [ ] **Step 2: Create the API client.**

```ts
import { AxiosPromise } from 'axios';
import { AXIOS } from './endpoints';
import { KnownWritersData } from '../types/knownWriters';

export const KNOWN_WRITERS_PATH = '/rest/writers';

export const readKnownWriters = (): AxiosPromise<KnownWritersData> =>
  AXIOS.get(KNOWN_WRITERS_PATH);

export const forgetWriter = (id: string): AxiosPromise<{ removed: boolean }> =>
  AXIOS.delete(`${KNOWN_WRITERS_PATH}/${encodeURIComponent(id)}`);
```

- [ ] **Step 3: Build.**

```bash
cd interface && npm run build
```

- [ ] **Step 4: Commit.**

```bash
git add interface/src/types/knownWriters.ts interface/src/api/knownWriters.ts
git commit -m "feat(interface): add types and API client for known writers"
```

---

## Task 5 — Writers tab component

**File to CREATE:** `interface/src/examples/serial/Writers.tsx`

- [ ] **Step 1: Write the component.**

```tsx
import React, { FC, useEffect, useState } from 'react';
import { Box, Card, CardContent, Typography, Alert, Chip, Button, IconButton, Tooltip, Dialog, DialogTitle, DialogContent, DialogActions } from '@mui/material';
import DeleteOutlineIcon from '@mui/icons-material/DeleteOutline';
import ContentCopyIcon from '@mui/icons-material/ContentCopy';
import { SectionContent } from '../../components';
import { readKnownWriters, forgetWriter } from '../../api/knownWriters';
import { KnownWriter } from '../../types/knownWriters';

const formatRelative = (ts: number): string => {
  if (!ts) return '—';
  const elapsed = Math.floor((Date.now() - ts) / 1000);
  if (elapsed < 5) return 'just now';
  if (elapsed < 60) return `${elapsed}s ago`;
  if (elapsed < 3600) return `${Math.floor(elapsed / 60)}m ago`;
  if (elapsed < 86400) return `${Math.floor(elapsed / 3600)}h ago`;
  return `${Math.floor(elapsed / 86400)}d ago`;
};

const Writers: FC = () => {
  const [writers, setWriters] = useState<KnownWriter[] | null>(null);
  const [hostIp, setHostIp] = useState<string>('');
  const [pendingForget, setPendingForget] = useState<KnownWriter | null>(null);

  useEffect(() => {
    setHostIp(window.location.hostname);
    const tick = () => readKnownWriters().then((r) => setWriters(r.data.writers)).catch(() => {});
    tick();
    const id = setInterval(tick, 2000);
    return () => clearInterval(id);
  }, []);

  const confirmForget = async () => {
    if (!pendingForget) return;
    try {
      await forgetWriter(pendingForget.id);
      setWriters((prev) => prev ? prev.filter((w) => w.id !== pendingForget.id) : prev);
    } finally {
      setPendingForget(null);
    }
  };

  return (
    <SectionContent title="Writers" titleGutter>
      <Alert severity="info" sx={{ mb: 2 }}>
        To add a Writer, open that device's UI, go to <b>Serial → Settings</b>, and paste this Reader's address:&nbsp;
        <code>{hostIp}</code>
        <Tooltip title="Copy address">
          <IconButton size="small" onClick={() => navigator.clipboard?.writeText(hostIp)}>
            <ContentCopyIcon fontSize="small" />
          </IconButton>
        </Tooltip>
      </Alert>

      {!writers ? (
        <Typography color="text.secondary">Loading…</Typography>
      ) : writers.length === 0 ? (
        <Card><CardContent>
          <Typography color="text.secondary">
            No Writers connected yet. Set up a Writer device, point it at this Reader, and it'll appear here.
          </Typography>
        </CardContent></Card>
      ) : (
        writers.map((w) => (
          <Card key={w.id} sx={{ mb: 1 }}>
            <CardContent>
              <Box display="flex" alignItems="center" justifyContent="space-between" gap={2}>
                <Box flex={1}>
                  <Box display="flex" alignItems="center" gap={1}>
                    <Chip
                      size="small"
                      color={w.online ? 'success' : 'default'}
                      label={w.online ? 'online' : 'offline'}
                    />
                    <Typography variant="h6">{w.friendly_name}</Typography>
                    <Typography variant="caption" color="text.secondary">{w.ip}</Typography>
                  </Box>
                  <Typography variant="body2" color="text.secondary" sx={{ mt: 0.5 }}>
                    Last seen {formatRelative(w.last_seen_at)} · last message {formatRelative(w.last_message_at)}
                  </Typography>
                  <Typography variant="body2" sx={{ fontFamily: 'monospace', mt: 0.5, wordBreak: 'break-all' }}>
                    {w.last_message || '(no messages received yet)'}
                  </Typography>
                </Box>
                <Tooltip title="Forget this Writer">
                  <IconButton color="error" onClick={() => setPendingForget(w)}>
                    <DeleteOutlineIcon />
                  </IconButton>
                </Tooltip>
              </Box>
            </CardContent>
          </Card>
        ))
      )}

      <Dialog open={pendingForget !== null} onClose={() => setPendingForget(null)}>
        <DialogTitle>Forget {pendingForget?.friendly_name}?</DialogTitle>
        <DialogContent>
          <Typography>
            This Writer will be removed from the list permanently. It can re-add itself by reconnecting.
          </Typography>
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setPendingForget(null)}>Cancel</Button>
          <Button color="error" variant="contained" onClick={confirmForget}>Forget</Button>
        </DialogActions>
      </Dialog>
    </SectionContent>
  );
};

export default Writers;
```

- [ ] **Step 2: Build.**

```bash
cd interface && npm run build
```

- [ ] **Step 3: Commit.**

```bash
git add interface/src/examples/serial/Writers.tsx
git commit -m "feat(interface): add Writers tab on Reader"
```

---

## Task 6 — Wire Writers tab into the Reader

**Files:**
- Modify: `interface/src/examples/serial/SerialReader.tsx`

- [ ] **Step 1: Add the tab and route.**

Open `interface/src/examples/serial/SerialReader.tsx`. Add the import:

```tsx
import Writers from './Writers';
```

Inside `<RouterTabs>`, after the BLE tab, add:

```tsx
<Tab value="writers" label="Writers" />
```

Inside `<Routes>`, add:

```tsx
<Route path="writers" element={<Writers />} />
```

- [ ] **Step 2: Build.**

```bash
cd interface && npm run build
```

- [ ] **Step 3: Commit.**

```bash
git add interface/src/examples/serial/SerialReader.tsx
git commit -m "feat(interface): add Writers tab to SerialReader"
```

---

## Task 7 — "N writers connected" badge on Information tab

**File:** find whichever component renders the Information tab content. Likely `interface/src/examples/serial/SerialInfo.tsx`.

- [ ] **Step 1: Add a polling fetch of the writers list.**

At the top of the component, add:

```tsx
import { readKnownWriters } from '../../api/knownWriters';

// Inside the component:
const [connectedCount, setConnectedCount] = React.useState<number | null>(null);
React.useEffect(() => {
  const tick = () => readKnownWriters()
    .then((r) => setConnectedCount(r.data.writers.filter((w) => w.online).length))
    .catch(() => setConnectedCount(null));
  tick();
  const id = setInterval(tick, 5000);
  return () => clearInterval(id);
}, []);
```

- [ ] **Step 2: Add a small line in the rendered output.**

Somewhere near the top of the Information content, add:

```tsx
{connectedCount !== null && connectedCount > 0 && (
  <Alert severity="success" sx={{ mb: 2 }}>
    <Typography variant="body2">
      <strong>{connectedCount}</strong> Writer{connectedCount === 1 ? '' : 's'} currently connected.
      <Button size="small" sx={{ ml: 1 }} onClick={() => window.location.hash = '#/project/serial/writers'}>View list</Button>
    </Typography>
  </Alert>
)}
```

(If the routing scheme differs from `/project/serial/writers`, adapt to whatever this codebase uses — read SerialReader.tsx for the actual path.)

- [ ] **Step 3: Build.**

```bash
cd interface && npm run build
```

- [ ] **Step 4: Commit.**

```bash
git add interface/src/examples/serial/SerialInfo.tsx
git commit -m "feat(interface): show connected-writers count on Information tab"
```

---

## Task 8 — End-to-end smoke test (manual, requires hardware)

**Prerequisites:** at least two ESP32 devices flashed with this firmware, on the same WiFi network. One configured as Reader, the other as Writer.

- [ ] **Step 1: Power up both devices. Set device A to Reader mode, device B to Writer mode.**

- [ ] **Step 2: On device B (Writer), open Serial → Settings, set Source Reader URL to device A's IP.**

E.g., `ws://192.168.2.50/ws/serial`. Enable. Save.

- [ ] **Step 3: On device A (Reader), open Serial → Writers tab.**

Confirm:
- Device B appears in the list
- Online indicator is green
- Friendly name shows (defaults to first 8 chars of hardware ID)
- IP is shown
- Last message appears once the scale sends data

- [ ] **Step 4: Disconnect device B from WiFi briefly.**

Confirm:
- Device A's Writers tab marks B as offline (grey dot)
- Last seen timestamp updates
- Once B reconnects, indicator returns to green

- [ ] **Step 5: Press the Forget button on device B's entry.**

Confirm:
- Confirmation dialog appears
- After confirming, B disappears from the list
- If B is still connected, it should re-add itself within a few seconds

- [ ] **Step 6: Reboot device A.**

Confirm:
- After boot, the Writers list shows previously-known writers as offline
- When B reconnects, it goes back online with the same friendly name

- [ ] **Step 7: Open Information tab.**

Confirm: "1 Writer currently connected" line appears with View list button.

- [ ] **Step 8: Final commit.**

```bash
git commit --allow-empty -m "chore: phase-2 end-to-end smoke test passed"
```

---

## Self-review notes

- Spec coverage: Section 3.1 (Writers tab) and Section 9.1 row for `knownWriters.json` — implemented in Tasks 1-7.
- Task 3 has a contingency for the case where `WebSocketTxRx` doesn't expose connect callbacks — agent reports DONE_WITH_CONCERNS in that scenario.
- Task 7 references `SerialInfo.tsx` — agent should verify by file inspection before editing.

## Out of scope for this plan

- mDNS auto-discovery of Readers from a Writer (Phase 3)
- Hotspot mode (Phase 3)
