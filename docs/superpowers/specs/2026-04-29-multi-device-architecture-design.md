# Multi-Device Architecture HTML — Design Spec

**Date:** 2026-04-29
**Branch:** serialWriter
**Output file:** `docs/multi-device-architecture.html`

## Purpose

A standalone HTML document that visually explains how the three Weighsoft ESP32
firmware variants (Serial Reader, Display, Serial Writer) compose into larger
systems. The document is a single self-contained file using only plain HTML,
CSS, and inline JavaScript — no external libraries, no CDN dependencies, no
build step.

## Audience

Developers and integrators evaluating which firmware variant to flash for a
given deployment, and how multiple ESP32 devices interconnect. The page
should be openable as a static file from anywhere and render fully offline.

## Structure

A vertically scrolling page with a sticky top navigation bar. Sections appear
in this order:

1. **Header** — page title, subtitle, branch context.
2. **Device Reference** — three colored cards (Reader, Display, Writer) each
   showing the device's Server role and Client role with the protocols it
   speaks.
3. **Use Case A** — Multiple Readers → Node-RED.
4. **Use Case B** — 1 Reader + Display + Writer reading from Reader's server.
5. **Use Case C** — Reader as local monitor AND cloud forwarder simultaneously.
6. **Use Case D** — Multiple Displays from one Reader.
7. **Use Case E** — Node-RED commanding a Writer.
8. **Use Case F** — MQTT broker as hub.
9. **Use Case G** — Serial WiFi bridge (Reader feeds Writer).
10. **Use Case H** — Mixed protocol fan-out from one Reader.

The sticky nav lists each use case as an anchor link with smooth scrolling.

## Visual Style

- Background: `#0f172a` (dark navy).
- Card surface: `#1e293b` with subtle border `#334155`.
- Body text: `#e2e8f0`. Muted text: `#94a3b8`.
- Headings: white, geometric sans-serif (system stack: `system-ui`).
- Accent colors per device:
  - Serial Reader: `#3b82f6` (blue)
  - Display: `#10b981` (emerald)
  - Serial Writer: `#f59e0b` (amber)
  - Node-RED: `#ef4444` (red)
  - MQTT broker: `#06b6d4` (cyan)
  - Mobile/BLE client: `#ec4899` (pink)
  - Physical serial device (scale, sensor, printer): `#8b5cf6` (purple)

Each device card has a colored left border in its accent color. Each diagram
device box uses the same accent on its border and a small filled badge
indicating "Server" or "Client" mode in that scenario.

## Device Reference Cards

Three cards laid out as a CSS grid (`grid-template-columns: repeat(3, 1fr)`),
collapsing to a single column below 900px viewport width.

Each card contains:

- Device name and accent stripe.
- Brief one-line summary.
- "Server role" sub-block: list of protocols served, default endpoint paths.
- "Client role" sub-block: what the device connects out to and which service
  inside the firmware does it.
- Source-of-truth references: `src/` directory and key service class name.

### Serial Reader card content

- Server role: `/rest/serial`, `/ws/serial`, MQTT publish
  `weighsoft/serial/{id}/data`, BLE read+notify on
  `12340000-…-1234`/`12340001-…-1234`. Any client can subscribe.
- Client role: `WeightForwarderService` HTTP POSTs read lines to a remote URL
  (e.g. Node-RED).
- Owning services: `SerialService` (server side), `WeightForwarderService`
  (client side).

### Display card content

- Server role: `/rest/display`, `/ws/display`, MQTT subscribe
  `weighsoft/display/{id}/set`. Any client can push lines onto the LCD.
- Client role: `DisplayService` Serial Bridge — connects out to a remote
  reader's `/ws/serial` (or MQTT topic, or BLE peripheral) and mirrors that
  data onto the LCD.
- Owning service: `DisplayService` (handles both server and bridge client).

### Serial Writer card content

- Server role: `/rest/serialWriter`, `/ws/serialWriter`. Any client can POST
  `pending_line` and the writer flushes it to the physical UART (and
  optionally USB CDC).
- Client role: `SerialWriterForwarderService` HTTP-polls or WebSocket-
  subscribes to a remote reader, then enqueues lines back through the
  writer's hardware sinks.
- Owning services: `SerialWriterService` (server side),
  `SerialWriterForwarderService` (client side).

## Use Case Sections

Each use case section is the same template:

- Section title (e.g. "Use Case A — Multiple Readers → Node-RED").
- One-line purpose statement.
- Animated SVG diagram, full card width, ~360px tall.
- "How it works" caption (2–4 lines).
- "Real-world example" caption (1 line).

### Diagram conventions

- Devices drawn as rounded rectangles with their accent color on a 3px border.
- A small label above each device indicates its role for that scenario:
  "Reader · Server", "Display · Client", "Node-RED", etc.
- Data flows are SVG paths (straight lines or quadratic curves) with stroke
  color matching the source device.
- Flow direction shown by animated dots — small filled circles
  (`<circle r="4">`) traveling along the path using SVG
  `<animateMotion>` tied to the path. Each dot loops continuously. Multiple
  dots per path with staggered begin times so the line always shows movement.
- Where two flows cross or overlap, the rear one uses lower opacity so the
  layering reads clearly.
- Endpoint protocol labels (`HTTP`, `WS`, `MQTT`, `BLE`) sit as small pills
  near the midpoint of each path.

### Use Case A — Multiple Readers → Node-RED

- Three Reader devices on the left, each connected to its own purple physical
  serial device (e.g. scale).
- One Node-RED node on the right.
- Three flow paths converge from readers to Node-RED, each labeled `HTTP POST`.
- Animated dots flow left-to-right on all three paths.
- Real-world example: "Three weighbridges across a yard, all forwarding to a
  central Node-RED dashboard."

### Use Case B — 1 Reader + Display + Writer pulling from Reader

- One Reader in the center with a purple physical serial device on its left.
- Display above-right and Writer below-right.
- Two flow paths from Reader's server: Reader → Display (`WS`) and Reader →
  Writer (`HTTP poll` or `WS`).
- Animated dots flow outward from Reader.
- Real-world example: "One scale read by an ESP, mirrored to a floor LCD and
  echoed onto a printer's serial port."

### Use Case C — Reader serving Display AND forwarding to Node-RED

- Reader in the center with physical serial device on its left.
- Display above-right (subscribed to `/ws/serial`).
- Node-RED below-right (target of `WeightForwarder` HTTP POST).
- Two outbound paths active simultaneously, both animated.
- Caption emphasizes Reader is acting as both server (for Display) and client
  (Forwarder pushing to Node-RED) at the same moment.
- Real-world example: "Local LCD on the production line for the operator,
  cloud logging in parallel for the office."

### Use Case D — Multiple Displays from one Reader

- Reader on the left with physical serial device.
- Three Displays stacked on the right, all subscribed to Reader's `/ws/serial`.
- Three animated paths fanning out from Reader.
- Real-world example: "One scale read by an ESP, the same value shown on
  every status board on the warehouse floor."

### Use Case E — Node-RED commanding a Writer

- Node-RED on the left.
- Writer on the right with a purple physical serial device beyond it.
- Single path from Node-RED to Writer labeled `HTTP POST /rest/serialWriter`,
  then a short serial line from Writer to the physical device.
- Animated dots flow left-to-right.
- Real-world example: "Node-RED triggers a label printer or PLC by POSTing
  the command string to the Writer."

### Use Case F — MQTT broker as hub

- MQTT broker (cyan) drawn as a central rounded square with broker icon.
- Reader on the left publishes to broker (`weighsoft/serial/{id}/data`).
- Display on the upper-right subscribes to broker
  (`weighsoft/display/{id}/set`).
- Writer on the lower-right subscribes via MQTT.
- Optional: a Node-RED node on the far right also subscribes.
- All paths labeled with their MQTT topic.
- Animated dots flow Reader → Broker → fan-out to subscribers.
- Real-world example: "All ESPs talk to one broker; replacing or adding a
  device requires no IP reconfiguration on the others."

### Use Case G — Serial WiFi bridge (Reader feeds Writer)

- Physical serial device A on the far left → Reader → (WiFi) → Writer →
  physical serial device B on the far right.
- Single horizontal flow line with the WiFi gap visualized as a dashed
  segment between the two ESPs.
- Animated dots traverse the entire chain end-to-end.
- Caption: Writer's Forwarder is connected to Reader as a client, pulling
  every line and writing it back onto its own UART.
- Real-world example: "Two pieces of legacy serial equipment that need to
  share data but are physically too far apart for an RS-232 cable."

### Use Case H — Mixed protocol fan-out from one Reader

- Reader at the center-left with physical serial device beside it.
- Four destinations on the right, vertically distributed: Display (WS),
  Writer (HTTP poll), Node-RED (Forwarder HTTP POST), and a Mobile phone
  icon (BLE notify).
- Four animated paths in distinct accent colors radiating from the Reader.
- Each path labeled with its protocol pill.
- Real-world example: "One reader, four consumers — a single source of truth
  feeding every part of the operation in the protocol that suits each
  consumer."

## Animation Implementation

- Use SVG `<animateMotion>` with `dur` set per path (between 1.6s and 2.8s
  depending on path length, longer paths get longer durations to keep
  perceived dot speed roughly uniform).
- Each path gets 2–3 dots with staggered `begin` offsets so the path looks
  like a continuous stream rather than discrete pulses.
- `repeatCount="indefinite"` on every animation.
- Use `prefers-reduced-motion: reduce` media query to disable
  `<animateMotion>` (set CSS `animation-play-state: paused`, hide dots).

## Sticky Navigation

- Top bar with the page title on the left and 8 anchor links on the right
  (`A · B · C · D · E · F · G · H`), each scrolling to the corresponding
  section.
- `position: sticky; top: 0; backdrop-filter: blur(6px);` over a
  semi-transparent variant of the background.
- Active section highlighted via `IntersectionObserver` toggling an
  `.active` class on the matching nav link.

## Accessibility

- All SVG diagrams have a `<title>` element describing the diagram in plain
  text.
- Section headings are real `<h2>` elements so screen readers can navigate.
- Color is never the only carrier of meaning — every device is also labeled.
- Reduced-motion support pauses animations.

## File Layout

Single file: `docs/multi-device-architecture.html`. Contains:

- `<!DOCTYPE html>` plus `<html lang="en">`.
- `<head>` with title, meta charset, meta viewport, and inline `<style>`.
- `<body>` with sticky `<nav>`, header, device reference grid, then 8 use
  case `<section>` blocks.
- A single inline `<script>` for the IntersectionObserver-based active
  nav highlight (everything else is CSS/SVG).

## Out of Scope

- No build step, npm, or bundler.
- No external CSS or JS frameworks.
- No interactive editing — diagrams are read-only.
- Not embedded in firmware. This is a static reference doc, not part of the
  React UI.
- No dark/light theme toggle. Dark only, matching the project's existing
  `system-overview.html`.
