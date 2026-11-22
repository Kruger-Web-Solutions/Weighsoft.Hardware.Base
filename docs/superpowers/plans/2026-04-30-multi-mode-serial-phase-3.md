# Multi-Mode Serial — Phase 3 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Set sensible hotspot defaults (`Weighsoft-Esp-{mode}` SSID, `weighsoft` password), make the SSID auto-update when the mode changes, and announce the device via mDNS so other devices on the network can find it.

**Architecture:** The framework already has `APSettingsService` with `AP_MODE_DISCONNECTED` provisioning that auto-turns-off the AP when WiFi connects — exactly what the spec wants. Phase 3 just needs to (a) change the factory defaults, (b) compose the mode-aware SSID at boot and on mode change via a small main.cpp hook, and (c) add a new `MdnsService` that announces `_weighsoft._tcp.local` with mode/name/id TXT records.

**Tech Stack:** Same as Phase 1 + ESPmDNS (built into ESP32 Arduino framework, no extra deps).

**Reference spec:** [`docs/superpowers/specs/2026-04-30-multi-mode-serial-reader-writer-design.md`](../specs/2026-04-30-multi-mode-serial-reader-writer-design.md) Section 8.

---

## File map

### Created

- `src/MdnsService.h` / `src/MdnsService.cpp` — wraps the ESPmDNS API, announces this device with mode/name/id TXT records, refreshes when mode/name changes

### Modified

- `factory_settings.ini` — change AP SSID default to `Weighsoft-Esp-NEW`, password default to `weighsoft`
- `src/main.cpp` — instantiate `MdnsService`, force SSID to match current mode at boot, hook the mode-change callback

---

## Task 1 — Update factory AP defaults

**File to modify:** `factory_settings.ini`

- [ ] **Step 1: Edit the AP defaults.**

Find the `Access point settings` block (lines ~14–23). Change two lines:

- `-D FACTORY_AP_SSID=\"ESP8266-React-#{unique_id}\"` → `-D FACTORY_AP_SSID=\"Weighsoft-Esp-NEW\"`
- `-D FACTORY_AP_PASSWORD=\"esp-react\"` → `-D FACTORY_AP_PASSWORD=\"weighsoft\"`

Leave the rest of the AP settings as-is (the channel, IP addresses, etc. are fine).

The `FACTORY_AP_PROVISION_MODE` is already `AP_MODE_DISCONNECTED` which is what the spec wants — no change needed there.

- [ ] **Step 2: Build to confirm the change compiles cleanly.**

Run: `pio run -e esp32s3`
Expected: clean build. No code references these defaults beyond `APSettingsService.h` reading them.

> **Note:** Existing devices that already have a saved `apSettings.json` will keep their old SSID/password until the file is reset. The new defaults only apply to fresh devices or after a factory reset. This is normal framework behavior.

- [ ] **Step 3: Commit.**

```bash
git add factory_settings.ini
git commit -m "feat(ap): default SSID Weighsoft-Esp-NEW, default password weighsoft"
```

---

## Task 2 — Mode-aware SSID composition

**File to modify:** `src/main.cpp`

- [ ] **Step 1: Read main.cpp to understand the current setup() flow.**

Identify where `apSettingsService` (or its pointer equivalent) is started, and where `uartModeService->begin()` is called. Confirm `apSettingsService->begin()` is called BEFORE the SSID-update logic we're about to add — otherwise the saved SSID hasn't been loaded yet.

- [ ] **Step 2: Add a helper inline in main.cpp.**

Below other static helpers in main.cpp (or just before `setup()`), add:

```cpp
// Compose the mode-aware AP SSID for the current device mode.
// Returns: "Weighsoft-Esp-Reader" / "Weighsoft-Esp-Writer" / "Weighsoft-Esp-Diagnostics" / "Weighsoft-Esp-NEW"
static String composeApSsid(UartModeService& m) {
  if (m.isWriter()) return "Weighsoft-Esp-Writer";
  if (m.isDiagnostics()) return "Weighsoft-Esp-Diagnostics";
  if (m.isReader()) return "Weighsoft-Esp-Reader";
  return "Weighsoft-Esp-NEW";
}

// Update the AP SSID to match the current mode if (and only if) the current SSID
// matches the auto-managed pattern. Hand-edited SSIDs are preserved.
static void syncApSsidToMode(APSettingsService& ap, UartModeService& m) {
  String desired = composeApSsid(m);
  ap.update([&](APSettings& s) {
    bool isAutoManaged = s.ssid.startsWith("Weighsoft-Esp-");
    if (isAutoManaged && s.ssid != desired) {
      s.ssid = desired;
      return StateUpdateResult::CHANGED;
    }
    return StateUpdateResult::UNCHANGED;
  }, "mode-ssid-sync");
}
```

(Adjust namespacing — if `apSettingsService` and `uartModeService` are accessed via getters on a framework object, replace `APSettingsService& ap` and `UartModeService& m` with the appropriate types/accessors.)

- [ ] **Step 3: Call the sync at boot.**

In `setup()`, AFTER both `apSettingsService->begin()` and `uartModeService->begin()` have run, add:

```cpp
syncApSsidToMode(*apSettingsService, *uartModeService);
```

(Adjust pointer/reference syntax to match the existing main.cpp code.)

- [ ] **Step 4: Hook mode-change events.**

Add an update handler on `uartModeService` that re-runs the sync. Since `UartModeService` extends `StatefulService<UartModeState>`, it already supports `addUpdateHandler`. Right after the `syncApSsidToMode` call in `setup()`:

```cpp
uartModeService->addUpdateHandler([&](const String& origin) {
  syncApSsidToMode(*apSettingsService, *uartModeService);
}, false);
```

- [ ] **Step 5: Build.**

Run: `pio run -e esp32s3`
Expected: clean build. RAM/Flash usage practically unchanged.

- [ ] **Step 6: Commit.**

```bash
git add src/main.cpp
git commit -m "feat(ap): auto-sync AP SSID to current UART mode at boot and on change"
```

---

## Task 3 — MdnsService

**Files to CREATE:**
- `src/MdnsService.h`
- `src/MdnsService.cpp`

- [ ] **Step 1: Write `src/MdnsService.h`.**

```cpp
#ifndef MdnsService_h
#define MdnsService_h

#include <Arduino.h>
#include "UartModeService.h"

class MdnsService {
 public:
  MdnsService(UartModeService* uartModeService);

  // Call once after WiFi is configured and either AP or STA is up.
  // Safe to call multiple times — re-applies the announcement.
  void begin();

  // Re-publish the TXT records (mode/name/id). Call after a mode change
  // or whenever the friendly name should be reflected.
  void refresh();

 private:
  UartModeService* _uartModeService;
  bool             _started = false;

  String currentMode() const;
  String currentName() const;
  String currentId() const;
  void   applyServiceTxt();
};

#endif
```

- [ ] **Step 2: Write `src/MdnsService.cpp`.**

```cpp
#include "MdnsService.h"
#include <ESPmDNS.h>
#include <SettingValue.h>

#define MDNS_SERVICE_NAME  "_weighsoft"
#define MDNS_SERVICE_PROTO "_tcp"
#define MDNS_SERVICE_PORT  80

MdnsService::MdnsService(UartModeService* uartModeService)
    : _uartModeService(uartModeService) {}

void MdnsService::begin() {
  if (_started) return;

  String hostname = String("weighsoft-") + currentId();
  if (!MDNS.begin(hostname.c_str())) {
    Serial.printf("[mDNS] begin(%s) failed\n", hostname.c_str());
    return;
  }
  MDNS.addService(MDNS_SERVICE_NAME, MDNS_SERVICE_PROTO, MDNS_SERVICE_PORT);
  applyServiceTxt();
  _started = true;
  Serial.printf("[mDNS] announced %s.local with service %s.%s\n",
                hostname.c_str(), MDNS_SERVICE_NAME, MDNS_SERVICE_PROTO);
}

void MdnsService::refresh() {
  if (!_started) return;
  applyServiceTxt();
}

void MdnsService::applyServiceTxt() {
  // ESPmDNS exposes addServiceTxt for adding individual records. Older versions
  // require removing+re-adding the service to update; this implementation keeps
  // it simple and just adds — clients that browse will see the latest values
  // because mDNS records are advertised on every announce.
  String mode = currentMode();
  String name = currentName();
  String id = currentId();
  MDNS.addServiceTxt(MDNS_SERVICE_NAME, MDNS_SERVICE_PROTO, "mode", mode.c_str());
  MDNS.addServiceTxt(MDNS_SERVICE_NAME, MDNS_SERVICE_PROTO, "name", name.c_str());
  MDNS.addServiceTxt(MDNS_SERVICE_NAME, MDNS_SERVICE_PROTO, "id", id.c_str());
}

String MdnsService::currentMode() const {
  if (!_uartModeService) return "new";
  if (_uartModeService->isReader()) return "reader";
  if (_uartModeService->isWriter()) return "writer";
  if (_uartModeService->isDiagnostics()) return "diagnostics";
  return "new";
}

String MdnsService::currentName() const {
  // For now use the hardware ID as the friendly name. A future task can pull
  // a user-configured name (e.g. from SerialWriterState.friendlyName when in
  // Writer mode) but that requires a service handle this class doesn't have.
  return currentId();
}

String MdnsService::currentId() const {
  return SettingValue::getUniqueId();
}
```

- [ ] **Step 3: Build.**

Run: `pio run -e esp32s3`
Expected: build will fail because `MdnsService` isn't instantiated anywhere yet. Once it's wired in main.cpp (Task 4) the build will succeed. To pre-verify the file parses, the build should at least get past the compilation of `MdnsService.cpp` with no errors before failing at link/main.cpp.

If the build complains about `<ESPmDNS.h>` not being found, the ESP32 Arduino core normally bundles it — confirm by running `find ~/.platformio -name "ESPmDNS.h" 2>/dev/null` (or the Windows equivalent). If it's missing, escalate as DONE_WITH_CONCERNS.

- [ ] **Step 4: Commit.**

```bash
git add src/MdnsService.h src/MdnsService.cpp
git commit -m "feat(mdns): add MdnsService — announces _weighsoft._tcp with mode/name/id TXT records"
```

---

## Task 4 — Wire MdnsService in main.cpp

**File to modify:** `src/main.cpp`

- [ ] **Step 1: Add the include.**

Near the top of main.cpp, after the existing service includes:

```cpp
#include "MdnsService.h"
```

- [ ] **Step 2: Instantiate MdnsService.**

Alongside other services (or at the global scope):

```cpp
MdnsService mdnsService(uartModeService);  // pass pointer if uartModeService is a pointer
```

(Adapt to whatever pattern main.cpp uses — `uartModeService` may be `&uartModeService` or a pointer.)

- [ ] **Step 3: Call begin().**

In `setup()`, AFTER the WiFi/AP services have started (so the network stack is up), call:

```cpp
mdnsService.begin();
```

If `setup()` has multiple WiFi-up paths, call it after the latest one. Most ESP32 mDNS implementations are tolerant of being called before WiFi is fully connected, but for reliability put it as late as possible in setup().

- [ ] **Step 4: Hook mode-change refresh.**

Add another update handler (or extend the one from Task 2) so the mDNS TXT records refresh when the mode changes:

```cpp
uartModeService->addUpdateHandler([&](const String& origin) {
  mdnsService.refresh();
}, false);
```

If the Task 2 handler already exists, you can combine into a single handler:

```cpp
uartModeService->addUpdateHandler([&](const String& origin) {
  syncApSsidToMode(*apSettingsService, *uartModeService);
  mdnsService.refresh();
}, false);
```

- [ ] **Step 5: Build.**

Run: `pio run -e esp32s3`
Expected: clean build success.

- [ ] **Step 6: Commit.**

```bash
git add src/main.cpp
git commit -m "feat(mdns): instantiate and wire MdnsService in main.cpp"
```

---

## Task 5 — End-to-end smoke test (manual, requires hardware)

- [ ] **Step 1: Flash a device with this firmware.**

`pio run -e esp32s3 -t upload --upload-port <PORT>` (or via OTA).

- [ ] **Step 2: Disconnect the device from any WiFi (or boot a fresh device).**

The AP should auto-start. From a phone, scan for WiFi networks. You should see `Weighsoft-Esp-NEW` (or `Weighsoft-Esp-Reader` if a mode was already saved).

- [ ] **Step 3: Connect to the hotspot using password `weighsoft`.**

Open `http://192.168.4.1` (the default AP IP). The web UI should load.

- [ ] **Step 4: Configure WiFi credentials and reboot.**

Once the device joins the WiFi network, the hotspot should automatically turn off (because provisionMode is `AP_MODE_DISCONNECTED`). Confirm by re-scanning your phone's WiFi list.

- [ ] **Step 5: Force the AP back on for testing.**

Open the AP settings page in the web UI. Switch provisionMode to "Always". Save. The hotspot should appear again alongside the WiFi connection.

- [ ] **Step 6: Switch the device's UART mode (Reader → Writer).**

Confirm:
- The web UI's mode toggle works as before
- After ~1 second, scanning WiFi networks again shows the SSID has updated to match the new mode (e.g. `Weighsoft-Esp-Writer`)

- [ ] **Step 7: Verify mDNS announcement.**

From a device on the same network, run:

```bash
# macOS / Linux
dns-sd -B _weighsoft._tcp local
# OR
avahi-browse -r _weighsoft._tcp
# Windows (PowerShell)
dns-sd /B _weighsoft._tcp local
```

Expected: the device appears with TXT records `mode=`, `name=`, `id=`. Switching modes should reflect in the TXT records (may need to wait for the mDNS cache to refresh, or rebrowse).

- [ ] **Step 8: Final commit.**

```bash
git commit --allow-empty -m "chore: phase-3 end-to-end smoke test passed"
```

---

## Self-review notes

- Spec coverage: Section 8.1 (default ON, auto-off on WiFi connect), 8.2 (SSID and password format), partial 8.3 (mDNS announcement). Section 8.3's auto-discovery DROPDOWN on the Writer's Settings page is deferred — the source Reader URL field already accepts manual entry.
- Existing devices with already-saved `apSettings.json` will keep their old SSID/password until factory-reset; this is documented in Task 1 as expected behavior.
- Hand-edited SSIDs (those that don't start with `Weighsoft-Esp-`) are preserved by `syncApSsidToMode`'s pattern check.

## Out of scope for this plan

- Auto-discovery dropdown on the Writer's Settings page (manual URL entry remains available — adding mDNS browse on the Writer side is significant work and isn't blocking the rest of the system)
- Captive portal redirect (the existing framework's DNS server handles this)
- A "regenerate hotspot password" button (the user can change the password manually in AP settings)
