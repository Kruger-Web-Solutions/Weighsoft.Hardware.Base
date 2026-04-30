# Multi-Mode Serial: Reader · Writer · Diagnostics — Design Document

**Date:** 2026-04-30
**Branch:** `SerialReaderWriter`
**Status:** Approved for implementation planning
**Audience:** Project lead, firmware engineers, integrators

---

## 1. The big picture

This is one ESP32 firmware that can run as a Reader, as a Writer, or as a Diagnostics box. A device picks exactly one of those modes and stays in it until the user changes it. The mode owns the device's RS-232 serial port — only the active mode talks to the wire.

**Reader** is the device with the scale plugged into it. It reads scale data and serves that data to anyone who asks (other devices, web pages, MQTT brokers, BLE clients, Node-RED, etc.).

**Writer** is a device whose serial port goes outwards. It writes data to whatever's connected (a printer, a remote display, another scale-receiver, etc.). It can receive data to write from five places: typed into the local UI, posted via REST, pushed via WebSocket, received from MQTT, **or pulled from a remote Reader**.

**Diagnostics** is the existing hardware-test mode, unchanged.

**The new multi-device shape:** one Reader at a site can serve many Writers. Writers are clients — each one is configured with the address of its source Reader, opens a WebSocket, and pulls every scale line as it arrives.

```
   ⚖️ Scale ──RS232──► 📥 Reader ◄── pulls ── 📤 Writer #1 (printer)
                          ▲                  
                          ├────── pulls ── 📤 Writer #2 (floor display)
                          │                  
                          └────── pulls ── 📤 Writer #N (other receiver)
```

Connection between Reader and Writers can be over the site's WiFi, or over the Reader's own hotspot (so a site without WiFi can still wire everything up).

This document covers three phases, all to be designed in this single spec but each phase is implementable on its own:

- **Phase 1** — mode switch + the six Writer screens (the foundation; nothing else works without this).
- **Phase 2** — Reader's "Writers" tab (the persistent list of known Writers with online/offline status).
- **Phase 3** — Hotspot mode, auto-discovery of Readers on the network.

---

## 2. The mode switch

A 3-button toggle sits at the top of the Serial section: **Reader · Writer · Diagnostics**.

The toggle replaces the existing 2-state Reader/Diagnostics toggle (`UartModeService` already enforces single-owner exclusivity for two states; we extend to three).

### What happens on a switch

When the user clicks a different mode:

1. A small confirmation appears: *"Switch to Writer? Reading will pause."* The user can cancel.
2. The current owner's serial port handler is suspended cleanly (existing pattern: `SerialService::suspendSerial()`).
3. The new owner starts up and configures the serial port.
4. The mode is saved to flash. Next boot comes up in the new mode.
5. The UI re-routes: tabs that don't apply to the new mode disappear, mode-specific tabs appear.

### The "NEW" state (fresh device)

A freshly-flashed device has no mode set. While in this state:

- The first thing the web UI shows is a single-screen prompt: *"Pick a mode to get started: Reader / Writer / Diagnostics."*
- The hotspot SSID is `Weighsoft-Esp-NEW` (see Section 8).
- Once the user picks, the choice is saved and the prompt never appears again.

This is implemented by storing mode as `String` in the persisted config; an empty value means NEW. The migration path from the previous `LIVE_MONITORING` value is: any old config containing `"live"` becomes `"reader"`. `"diagnostics"` stays as-is.

### Mode-aware tabs

| Tab in the Serial section | Reader mode | Writer mode | Diagnostics mode |
|---|---|---|---|
| Information | ✓ | – | – |
| Status | – | ✓ | – |
| Configuration / Settings | ✓ (Configuration) | ✓ (Settings) | – |
| REST View | ✓ | – | – |
| Live Stream | ✓ (incoming) | ✓ (outgoing) | – |
| BLE Stream | ✓ | – | – |
| Writers (list) | ✓ | – | – |
| Send Message | – | ✓ | – |
| Send via Web (HTTP) | – | ✓ | – |
| MQTT | – | ✓ | – |

When the device is in **Diagnostics mode**, the Serial section shows only the mode switcher and a small explainer panel: *"Diagnostics mode is active — open the Diagnostics top-level menu to run hardware tests."* All Serial sub-tabs are hidden because the serial port is owned by the diagnostics service.

Tab routing redirects the user away from any URL that doesn't apply to the current mode (e.g. visiting `/serial/send-message` while in Reader mode redirects to `/serial/information`).

---

## 3. Reader-mode screens

Existing tabs stay as they are: Information, Configuration, REST View, Live Stream, BLE Stream. One new tab is added.

### 3.1 New tab — "Writers"

Lists every Writer that has ever connected to this Reader (persistent — survives reboot). For each entry:

- **Friendly name** (e.g. "Floor Display", "Printer-1"). Defaults to the Writer's hardware unique-ID short hash.
- **Online indicator** — green dot if currently subscribed, grey dot if not.
- **IP address** in muted text.
- **Connected since** (when the current connection started — only shown if online).
- **Last message received** (the last scale line the Reader broadcast while this Writer was connected) and timestamp.
- **Forget button** — removes this Writer from the persistent list. Confirmation prompt: *"Forget Writer-1 permanently? It can re-add itself by reconnecting."*

#### Help block at the top

A small Material info alert with the Reader's current IP and a copy-to-clipboard button:

> *"To add a Writer, go to that Writer's device, open Serial → Settings, and paste this Reader's address: `192.168.1.50`. Or pick this Reader from the auto-discovered list on the Writer's screen."*

#### Empty state

> *"No Writers connected yet. Set up a Writer device, point it at this Reader, and it'll appear here."*

#### Information tab gains one line

A small badge near the top of Information shows: *"3 Writers connected"* with a click-through to the Writers tab.

### 3.2 How the persistent list is maintained

- When a new WebSocket subscriber connects with a `?role=writer&name=<friendly-name>&id=<hardware-id>` query, the Reader checks its persistent list for that hardware ID.
  - If present — update last-seen, mark online, update name if changed.
  - If absent — append a new entry.
- When the WebSocket disconnects — mark offline, persist the timestamp.
- The Reader does not auto-prune offline entries. Users explicitly Forget them.
- List capped at e.g. 32 entries; if exceeded, oldest-offline entries are auto-removed.
- Persisted to `/config/knownWriters.json`.

---

## 4. Writer-mode screens

Six tabs, friendly names, Material Design styling that matches the rest of the app.

Every screen has a one-line tip box at the top stating what it does in plain words.

### 4.1 Status

What you see (top to bottom):

- A friendly-name editor at the top: input field defaulting to the device's short ID, with a save button. (e.g. "Floor Display")
- Connection card: *"Source Reader: `192.168.1.50` — ● connected · last received `ST,GS,+ 2321kg` 2 seconds ago."* Or in red: *"Reconnecting (attempt 3)…"*
- Local outbound stats card: bytes sent today, last message sent, last source (MANUAL / REST / WS / MQTT / READER).
- Serial port summary: *"9600 8N1 · CRLF line endings"* with a link to Settings.

### 4.2 Settings

Two grouped sections:

**Connection (Source Reader):**
- Source Reader address — text field with a dropdown of currently-discovered Readers. Manual entry always allowed.
- Auto-reconnect — switch (default ON).
- Connection method — radio: WebSocket (default, real-time) / HTTP polling (fallback for restrictive networks).

**Serial port:**
- Baud rate — select (defaults 9600; same options as Reader's Configuration).
- Data bits / Stop bits / Parity — radio groups, same defaults as Reader.
- Line ending — select: None / CR (`\r`) / LF (`\n`) / CRLF (`\r\n`). Default CRLF.

MQTT topic configuration lives on the dedicated **MQTT** tab (Section 4.6), not in Settings.

### 4.3 Send Message

- Tip box at top: *"Most scales respond to `P` to print weight, or `ZERO` to tare."*
- Single text field for the message.
- Line-ending dropdown — defaults to whatever Settings has, can override per-send.
- Send button. Pressing Enter while focused on the text field also sends; Shift+Enter inserts a newline.
- After a successful send: a green Material Alert: *"Sent — 3 bytes at 09:42:18."*
- Recent sends: chip list of the last 8 distinct sends, click to re-send. Stored in-memory only (not persisted).

Layout note: the user explicitly chose the "simple" version over a power-user variant. No hex mode, no repeat N times, no console-style log.

### 4.4 Send via Web (HTTP)

- Endpoint info card: shows `POST /rest/serialWriter` with the JSON body shape `{"data":"..."}`.
- Try-it form: text field + Send button. Same look as Send Message but identifies the source as REST.
- Response card showing `200 OK` / errors with plain-language explanations.
- Curl example block, one click to copy.

### 4.5 Live Stream

- Tip box: *"Every message that left this Writer's serial port — by anyone."*
- Connection indicator at top: green/grey dot for the live socket status.
- "Send through this socket" mini-form: text field + Send button. Sends are tagged WS.
- Activity log table — last 100 messages, newest at top. Columns: timestamp · source tag · message text · byte count.
- Source tags shown as colored Material chips:
  - **READER** (Material primary blue) — pulled from the source Reader.
  - **MANUAL** (light blue) — typed in Send Message.
  - **REST** (orange) — POSTed via Send via Web.
  - **WS** (green) — pushed through the live WebSocket on this very screen.
  - **MQTT** (purple) — received via the configured MQTT topic.
- Clear button at top of the log clears the in-memory buffer.

### 4.6 MQTT

- Tip box: *"Subscribe to a topic and any message it receives gets written to the serial port."*
- Subscribe topic field (default suggestion: `weighsoft/serialwriter/{device-id}/send`).
- Status publish topic field (default: `weighsoft/serialwriter/{device-id}/status`) — shows whether the Writer is connected to its source Reader, last received line, etc.
- Connection indicator: connected to broker / disconnected.
- Last 5 messages received via this MQTT subscription (read-only list).

---

## 5. Writer → Reader connection (the live link)

Each Writer maintains exactly one WebSocket subscription to its configured source Reader.

### 5.1 Connection flow

1. On boot, if the Writer has a saved source Reader address, it opens a WebSocket to `ws://<reader-address>/ws/serial?role=writer&name=<friendly-name>&id=<hardware-id>`.
2. The Reader's framework accepts the connection, looks up the hardware ID in its persistent Writers list, and updates the entry (or creates it).
3. Every scale line the Reader's `SerialService` parses is broadcast to every connected WebSocket — Writers receive each line as it arrives.
4. The Writer's `SerialWriterForwarderService` (new) receives the line, appends the configured line ending, and writes it to the local serial port. The line is also added to the local Live Stream feed tagged READER.

### 5.2 Auto-reconnect

If the WebSocket closes unexpectedly:

- The Writer attempts reconnect after 1s, then 2s, 4s, 8s, 16s, 30s. Plateaus at 30s thereafter, retries forever.
- Each attempt logs to the Live Stream as a system message (e.g. *"Reconnecting to source Reader (attempt 3)…"*).
- The Writer's Status screen shows the reconnect attempt count and last error reason.

### 5.3 What if the Reader reboots

- All connected Writers see the link drop, start reconnecting.
- The Reader on boot loads its persistent Writers list — entries already exist so when Writers reconnect, names and IDs match.
- Online/offline indicators in the Reader's Writers tab update in real time as connections come back.

### 5.4 HTTP polling fallback

Per the architecture document, an HTTP-polling client is also supported. Used only when WebSockets are blocked at the network level (some restrictive corporate WiFi). The Writer's Settings has a radio to choose; default is WebSocket.

---

## 6. Local Writer input paths

Independent of the source Reader, the Writer also accepts data from four local channels. All of them flow through one method internally that:

1. Validates the message is non-empty.
2. Appends the configured line ending (or per-send override for Manual).
3. Writes the bytes to the serial port.
4. Adds an entry to the Live Stream feed tagged with the source.
5. Updates last-sent stats.

| Channel | How it works |
|---|---|
| **Manual** (Send Message tab) | UI form posts to internal endpoint, tagged MANUAL. |
| **REST** | `POST /rest/serialWriter` with `{"data":"..."}`. Requires existing security framework auth if enabled. |
| **WebSocket** | Open `/ws/serialWriter` and send text frames or `{"data":"..."}` JSON. Same socket also receives live activity feed for read-only consumers. |
| **MQTT** | Subscribe to configured topic; payload is the message text. |

These local channels are unaffected by the source Reader connection. A Writer with no source Reader configured (or with the source Reader offline) still works for local channels.

---

## 7. Mode coordination — extending UartModeService

The existing `UartModeService` already coordinates `LIVE_MONITORING` and `DIAGNOSTICS` mutual exclusivity. We extend it:

```cpp
enum class UartModeType { READER, WRITER, DIAGNOSTICS };
```

`LIVE_MONITORING` is renamed to `READER`. The migration on first read-from-flash is: `"live"` → `"reader"`. The `UartModeService` registers three services and calls suspend/resume on the previous/next owner during a switch:

- `SerialService` (Reader)
- `SerialWriterService` + `SerialWriterForwarderService` (Writer — there are two services because one accepts inbound channels and the other pulls from the source Reader; they share the serial port via the same suspend/resume hook)
- `DiagnosticsService` (existing)

The Writer mode services live in a new directory: `src/examples/serialwriter/`.

---

## 8. Hotspot mode

### 8.1 Default behaviour

- Hotspot is **on by default** the first time a device boots.
- The moment the device successfully joins a WiFi network, the hotspot **automatically turns off**.
- The user can flip a switch in WiFi Settings to **force hotspot on even while joined to WiFi**. This is the only way the hotspot stays up after WiFi joins.

### 8.2 SSID and password

- **SSID:** `Weighsoft-Esp-Reader`, `Weighsoft-Esp-Writer`, or `Weighsoft-Esp-NEW` depending on the device's current mode.
- **Password:** `weighsoft` (default — user can change in Settings, but the default is the same on every device).
- A prominent warning in the hotspot settings: *"Anyone on the same hotspot can read or write to this device. Change the password if this isn't a private setup."*
- When the user changes the mode, the SSID changes the next time the hotspot starts. Existing clients lose the connection and have to reconnect.

### 8.3 Auto-discovery

- Each device announces itself on the local network via mDNS, advertising `_weighsoft._tcp.local` with TXT records: `mode=reader|writer|new`, `name=<friendly-name>`, `id=<hardware-id>`.
- The Writer's Settings → Source Reader field is a dropdown of currently-discovered devices with `mode=reader`. The user always confirms; auto-pick is intentionally not implemented.
- Refresh button next to the dropdown re-scans.
- Manual entry (typing an IP or hostname) is always permitted.

### 8.4 Setup walkthrough — site with no shared WiFi

1. Plug Reader in. Open its UI on a phone (connect to `Weighsoft-Esp-NEW` hotspot, password `weighsoft`).
2. Set mode to Reader. SSID changes to `Weighsoft-Esp-Reader`; reconnect.
3. Plug Writer in. Phone connects to its `Weighsoft-Esp-NEW` hotspot.
4. On the Writer's UI, set its WiFi to the **Reader's hotspot** SSID/password (Settings → WiFi).
5. Set Writer's mode to Writer. SSID becomes `Weighsoft-Esp-Writer`.
6. On Writer's Serial → Settings → Source Reader, the dropdown shows the Reader. Pick it.
7. Done. Data flows.

---

## 9. Configuration and persistence

### 9.1 Persisted to flash

| File | Owner | Contents |
|---|---|---|
| `/config/uartMode.json` | `UartModeService` | `{"mode":"reader|writer|diagnostics|"}` (empty = NEW) |
| `/config/serialConfig.json` | `SerialService` | Reader hardware config (existing) |
| `/config/serialWriterConfig.json` | `SerialWriterService` | Writer hardware config + line ending + friendly name + MQTT topics |
| `/config/serialWriterSource.json` | `SerialWriterForwarderService` | Source Reader address + reconnect prefs + connection method |
| `/config/knownWriters.json` | `SerialService` (Reader role) | Persistent list of known Writers |
| `/config/wifiSettings.json` | (existing) | WiFi creds + new hotspot fields (force-on switch, custom password) |

### 9.2 Not persisted (in-memory only)

- Recent-sends history on the Send Message screen.
- Live Stream activity feed on the Writer's Live Stream tab (capped at 100 entries).
- Last-5 MQTT received messages on the MQTT tab.

---

## 10. Reliability and edge cases

| Scenario | Behaviour |
|---|---|
| WiFi blip (Writer side) | Auto-reconnect to source Reader with backoff. |
| Reader reboots | Writers reconnect once Reader is back up. |
| Source Reader address misconfigured | Writer Status shows the failed address and last connection error in plain words. |
| User Forgets a Writer that's currently online | Writer entry removed; if it stays connected, it re-adds itself on the next message broadcast. |
| Hotspot toggle conflict (user forces hotspot on but device is on a busy WiFi) | ESP32 supports STA+AP simultaneously; if it can't, an explanatory error and the join-WiFi setting wins. |
| A `?role=writer` query missing from a connecting WebSocket | Connection still allowed; treated as a generic listener (e.g. a browser viewing Live Stream). Not added to Writers list. |
| Writers list reaches 32 entries | Oldest *offline* entry is auto-removed. Online Writers are never auto-pruned. |
| Old config with `"live"` mode loaded | Migrated to `"reader"` on first read; saved back on next change. |

---

## 11. Out of scope (deliberate)

| Area | Why deferred |
|---|---|
| BLE input on Writer | YAGNI for v1; clear extension point as a 7th tab. |
| Scheduled / periodic auto-send on Writer | Adds complexity, not in stated use cases. |
| Hex mode / repeat-N on Send Message | Power-user features; user explicitly chose the simple layout. |
| Per-Writer message transformations on the Reader | All Writers receive the same scale lines. Filtering/rewriting happens on the Writer side if needed. |
| Per-Writer authentication | Whole network is trust-bounded by hotspot password or WiFi membership. |
| Multiple source Readers per Writer | Each Writer follows exactly one Reader. |
| HTTPS / TLS between Reader and Writer | Plaintext over WiFi for v1; site is assumed trusted. |
| Auto-pick a Reader (no user confirm) | User always picks; less surprising. |
| Display LCD firmware | Separate firmware on a separate branch (`display`). |
| USB CDC output on Writer | UART only for v1. |
| Persistent log of every message | Only Live Stream in-memory and capped. |

---

## 12. Implementation map (high level)

This section sketches where each piece lives. Detailed step-by-step plan comes from the writing-plans skill afterwards.

### 12.1 Backend (firmware)

- **Modify:** `src/UartMode.h`, `src/UartModeService.{h,cpp}` — add `WRITER` to the enum, add `setWriterService` registration, three-way coordination in `applyMode()`, migration of old `"live"` value, "NEW" empty-string state.
- **Modify:** `src/main.cpp` — register the new Writer services, wire them into `UartModeService`.
- **New:** `src/examples/serialwriter/SerialWriterService.{h,cpp}` and `SerialWriterState.{h}` — the Writer's local input handler. Owns `/rest/serialWriter`, `/ws/serialWriter`, MQTT subscribe/publish. Uses the existing `HttpEndpoint`, `WebSocketTxRx`, `MqttPubSub`, `FSPersistence` patterns from `SerialService`. Owns the serial port when the active mode is Writer; suspends when not.
- **New:** `src/examples/serialwriter/SerialWriterForwarderService.{h,cpp}` and state — the WebSocket client that subscribes to the source Reader's `/ws/serial` and forwards each line through `SerialWriterService::transmit(data, source=READER)`. Auto-reconnect with exponential backoff. HTTP-polling fallback path.
- **Modify:** `src/examples/serial/SerialService.{h,cpp}` — track connected Writers (via `?role=writer&name=&id=` query), maintain the persistent `knownWriters.json`, expose `GET /rest/writers` and `DELETE /rest/writers/<id>` for the UI.
- **Modify:** WiFi service (existing) — add `hotspotForceOn` setting, dynamic SSID based on mode, default-on behaviour and auto-off-when-WiFi-joined logic.
- **New:** mDNS announce + browse helper service — `_weighsoft._tcp.local` with TXT records.

### 12.2 Frontend (interface)

- **Modify:** `interface/src/examples/serial/SerialMonitor.tsx` — read current mode from `UartModeService`, render reader-tabs OR writer-tabs accordingly. The mode switcher (`UartModeSwitcher`) extends to a 3-way toggle.
- **Modify:** `interface/src/components/UartModeSwitcher.tsx` — three buttons; confirm dialog before switch.
- **New:** `interface/src/examples/serial/writer/` directory containing one file per writer screen: `WriterStatus.tsx`, `WriterSettings.tsx`, `SendMessage.tsx`, `SendViaWeb.tsx`, `WriterLiveStream.tsx`, `WriterMqtt.tsx`.
- **New:** `interface/src/examples/serial/reader/Writers.tsx` — the persistent Writers list tab on the Reader.
- **Modify:** `interface/src/api/` — add typed clients for the new endpoints (`/rest/serialWriter`, `/rest/writers`, etc.), a hook for the live source-Reader connection.
- **Modify:** `interface/src/AppRouting.tsx` and the WiFi connection page — surface the hotspot force-on switch and the SSID/password block.
- **Reuse:** existing components (`Alert`, `Chip`, `ToggleButtonGroup`, `TextField`, etc. from `@mui/material`). No custom gradients or new visual chrome.

### 12.3 Tests / verification

- Manual end-to-end checklist: NEW → Reader → Writer pair across the hotspot path; Writer auto-reconnect after Reader reboot; Forget a Writer and confirm it re-adds; mode switch persists across reboot.
- Regression: existing live/diagnostics flows still work after the rename + three-way toggle.
- The architecture document's use cases (B, D, F from `WHB-ARCH-001`) should all be reproducible against a Writer device running this firmware.

---

## 13. Open items (none currently — all resolved during brainstorm)

All structural questions were answered during brainstorming. The implementation plan generated next will identify any remaining unknowns.

---

## Decisions captured during brainstorm

For traceability, the design decisions made during the brainstorming session:

1. **Writer use cases:** Manual, REST, WebSocket, MQTT (BLE and scheduled-send deferred).
2. **Menu pattern:** Single "Serial" entry with mode-aware sub-tabs (rather than two top-level menu items).
3. **Mode enum:** Three-way clean — `READER`, `WRITER`, `DIAGNOSTICS`. `LIVE_MONITORING` renamed to `READER`.
4. **Tab structure:** Six Reader tabs + six Writer tabs.
5. **Tab naming:** Friendly Material/non-developer names for Writer tabs only; Reader tab names left unchanged for stability (Information, Configuration, REST View, Live Stream, BLE Stream, Writers).
6. **Send Message layout:** Simple variant (no hex / repeat / console).
7. **UI aesthetic:** Match existing app's Material Design — no custom gradients or emoji chrome.
8. **Plain language** in all helper text and explanations.
9. **REST + WS coupling:** All four local channels feed one transmit method; the Live Stream tab shows every successful send tagged by source.
10. **Single firmware with mode switch** — deliberate deviation from the architecture document's "separate firmware per branch" pattern.
11. **All three phases in one design** — Phase 1 (mode switch + Writer screens), Phase 2 (Reader's Writers list), Phase 3 (hotspot + auto-discovery).
12. **Direction of data flow:** Writer is the client and pulls from Reader (Reader is a passive server). Many Writers per Reader.
13. **Persistent Writers list:** Writers stay on the Reader's list once seen, marked offline when disconnected; user can Forget them.
14. **Hotspot default:** ON, auto-off on WiFi join, manual override available; SSID reflects mode (`Weighsoft-Esp-Reader|Writer|NEW`); password `weighsoft` everywhere by default.
