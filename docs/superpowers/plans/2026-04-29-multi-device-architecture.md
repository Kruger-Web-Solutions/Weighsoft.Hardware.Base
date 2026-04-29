# Multi-Device Architecture HTML Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a single self-contained `docs/multi-device-architecture.html` page that visually documents how the three Weighsoft ESP32 firmware variants (Serial Reader, Display, Serial Writer) compose into larger systems via 8 use case diagrams.

**Architecture:** One static HTML file. All CSS in a single `<style>` block, one inline `<script>` for `IntersectionObserver` nav highlighting and `prefers-reduced-motion` handling. Diagrams are inline SVG with `<animateMotion>` driving small dots along data flow paths. No build step, no external dependencies, no CDN.

**Tech Stack:** Plain HTML5, CSS (custom properties, grid, sticky positioning), inline SVG with SMIL `<animateMotion>`, vanilla JS (`IntersectionObserver`, `matchMedia`).

**Spec reference:** `docs/superpowers/specs/2026-04-29-multi-device-architecture-design.md`

**Verification approach:** Static HTML — no test runner. Each task ends with opening the file in a browser to visually confirm the section renders, then committing. Use the file:// protocol (e.g. `start docs/multi-device-architecture.html` on Windows or open it directly in the file explorer). Every commit must be a working page (the in-progress sections may be empty placeholders, but the page must load without console errors).

---

## File Structure

Single output file:

- **Create:** `docs/multi-device-architecture.html`

Internal organization (top to bottom):

1. `<head>` — meta tags, title, single `<style>` block.
2. `<body>` → sticky `<nav id="topnav">`.
3. `<header id="page-header">`.
4. `<section id="device-reference">` — three device cards.
5. `<section id="use-case-a">` through `<section id="use-case-h">` — eight use cases.
6. Single inline `<script>` at the end of `<body>` for nav highlight and reduced-motion.

CSS sections inside the `<style>` block, in order:

- CSS custom properties (color palette + `--diagram-h`).
- Global reset and body typography.
- Sticky nav.
- Header.
- Device reference grid + card.
- Section common rules (layout, captions, headings).
- Diagram conventions (svg wrapper, device boxes, paths, dots, protocol pills).
- Reduced-motion overrides.
- Responsive breakpoints.

Color palette (from spec, lock these as CSS variables):

```css
--bg: #0f172a;
--surface: #1e293b;
--border: #334155;
--text: #e2e8f0;
--muted: #94a3b8;
--c-reader: #3b82f6;
--c-display: #10b981;
--c-writer: #f59e0b;
--c-nodered: #ef4444;
--c-mqtt: #06b6d4;
--c-mobile: #ec4899;
--c-physical: #8b5cf6;
--diagram-h: 360px;
```

---

### Task 1: Create page skeleton with sticky nav and CSS foundation

**Files:**
- Create: `docs/multi-device-architecture.html`

- [ ] **Step 1: Create the file with full skeleton, palette, sticky nav, and empty section anchors**

Write the entire file content below. The page will render as a dark background with a sticky nav listing 8 anchor links and 9 empty sections beneath it. No diagrams yet — they're added in later tasks.

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Weighsoft Multi-Device Architecture</title>
  <style>
    :root {
      --bg: #0f172a;
      --surface: #1e293b;
      --border: #334155;
      --text: #e2e8f0;
      --muted: #94a3b8;
      --c-reader: #3b82f6;
      --c-display: #10b981;
      --c-writer: #f59e0b;
      --c-nodered: #ef4444;
      --c-mqtt: #06b6d4;
      --c-mobile: #ec4899;
      --c-physical: #8b5cf6;
      --diagram-h: 360px;
    }

    * { box-sizing: border-box; }

    html { scroll-behavior: smooth; }

    body {
      margin: 0;
      background: var(--bg);
      color: var(--text);
      font-family: system-ui, -apple-system, "Segoe UI", Roboto, sans-serif;
      line-height: 1.5;
    }

    /* Sticky nav */
    nav#topnav {
      position: sticky;
      top: 0;
      z-index: 10;
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 12px 24px;
      background: rgba(15, 23, 42, 0.85);
      backdrop-filter: blur(6px);
      -webkit-backdrop-filter: blur(6px);
      border-bottom: 1px solid var(--border);
    }
    nav#topnav .brand {
      font-weight: 600;
      font-size: 0.95rem;
      letter-spacing: 0.02em;
    }
    nav#topnav ul {
      list-style: none;
      margin: 0;
      padding: 0;
      display: flex;
      gap: 6px;
    }
    nav#topnav a {
      display: inline-block;
      padding: 6px 10px;
      border-radius: 6px;
      color: var(--muted);
      text-decoration: none;
      font-size: 0.85rem;
      font-weight: 500;
      transition: color 0.15s, background 0.15s;
    }
    nav#topnav a:hover { color: var(--text); background: var(--surface); }
    nav#topnav a.active { color: var(--text); background: var(--surface); }

    /* Page layout */
    main { max-width: 1100px; margin: 0 auto; padding: 32px 24px 64px; }

    header#page-header { padding: 32px 0 16px; }
    header#page-header h1 {
      margin: 0 0 8px;
      font-size: 2rem;
      letter-spacing: -0.01em;
    }
    header#page-header p {
      margin: 0;
      color: var(--muted);
      font-size: 1.05rem;
    }

    section { padding: 32px 0; border-top: 1px solid var(--border); }
    section h2 {
      margin: 0 0 4px;
      font-size: 1.4rem;
      letter-spacing: -0.01em;
    }
    section .purpose {
      margin: 0 0 20px;
      color: var(--muted);
    }
    section .caption {
      margin: 16px 0 6px;
      color: var(--text);
      font-size: 0.95rem;
    }
    section .example {
      margin: 0;
      color: var(--muted);
      font-size: 0.9rem;
      font-style: italic;
    }

    @media (max-width: 720px) {
      nav#topnav { flex-direction: column; align-items: flex-start; gap: 8px; }
      nav#topnav ul { flex-wrap: wrap; }
    }
  </style>
</head>
<body>

<nav id="topnav" aria-label="Page navigation">
  <span class="brand">Weighsoft Multi-Device Architecture</span>
  <ul>
    <li><a href="#use-case-a">A</a></li>
    <li><a href="#use-case-b">B</a></li>
    <li><a href="#use-case-c">C</a></li>
    <li><a href="#use-case-d">D</a></li>
    <li><a href="#use-case-e">E</a></li>
    <li><a href="#use-case-f">F</a></li>
    <li><a href="#use-case-g">G</a></li>
    <li><a href="#use-case-h">H</a></li>
  </ul>
</nav>

<main>
  <header id="page-header">
    <h1>Weighsoft Multi-Device Architecture</h1>
    <p>How the Serial Reader, Display, and Serial Writer ESP32 firmware variants compose into larger systems.</p>
  </header>

  <section id="device-reference">
    <h2>Device Reference</h2>
    <p class="purpose">Each ESP32 firmware variant runs a server side and a client side. The cards below summarize what each device serves and what it connects out to.</p>
  </section>

  <section id="use-case-a"><h2>Use Case A — Multiple Readers → Node-RED</h2></section>
  <section id="use-case-b"><h2>Use Case B — 1 Reader + Display + Writer</h2></section>
  <section id="use-case-c"><h2>Use Case C — Reader as local monitor and cloud forwarder</h2></section>
  <section id="use-case-d"><h2>Use Case D — Multiple Displays from one Reader</h2></section>
  <section id="use-case-e"><h2>Use Case E — Node-RED commanding a Writer</h2></section>
  <section id="use-case-f"><h2>Use Case F — MQTT broker as hub</h2></section>
  <section id="use-case-g"><h2>Use Case G — Serial WiFi bridge</h2></section>
  <section id="use-case-h"><h2>Use Case H — Mixed protocol fan-out</h2></section>
</main>

</body>
</html>
```

- [ ] **Step 2: Open the file in a browser to verify**

Open `docs/multi-device-architecture.html` directly (Windows: double-click or `start docs/multi-device-architecture.html`).
Expected:
- Dark navy background, no errors in DevTools console.
- Sticky nav at top with brand text on left, A–H links on right.
- Clicking each nav link smoothly scrolls to a section heading.
- Sections appear with their h2 titles, no diagram content yet.

- [ ] **Step 3: Commit**

```bash
git add docs/multi-device-architecture.html
git commit -m "docs(serialWriter): scaffold multi-device-architecture.html with sticky nav"
```

---

### Task 2: Add Device Reference cards (Reader, Display, Writer)

**Files:**
- Modify: `docs/multi-device-architecture.html` (CSS + Device Reference section body)

- [ ] **Step 1: Add device-card CSS rules to the `<style>` block**

Insert these rules immediately before the `@media (max-width: 720px)` block at the end of the `<style>` element:

```css
/* Device reference grid */
.device-grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 16px;
  margin-top: 16px;
}
.device-card {
  background: var(--surface);
  border: 1px solid var(--border);
  border-left-width: 4px;
  border-radius: 10px;
  padding: 16px 18px;
}
.device-card.reader  { border-left-color: var(--c-reader); }
.device-card.display { border-left-color: var(--c-display); }
.device-card.writer  { border-left-color: var(--c-writer); }
.device-card h3 {
  margin: 0 0 4px;
  font-size: 1.05rem;
}
.device-card .summary { margin: 0 0 12px; color: var(--muted); font-size: 0.9rem; }
.device-card .role-label {
  display: inline-block;
  margin-top: 10px;
  margin-bottom: 6px;
  padding: 2px 8px;
  font-size: 0.7rem;
  font-weight: 600;
  letter-spacing: 0.05em;
  text-transform: uppercase;
  border-radius: 4px;
  background: rgba(255, 255, 255, 0.06);
  color: var(--text);
}
.device-card ul {
  margin: 0 0 6px;
  padding-left: 18px;
  font-size: 0.85rem;
}
.device-card li { margin-bottom: 2px; }
.device-card code {
  background: rgba(255, 255, 255, 0.06);
  padding: 1px 5px;
  border-radius: 3px;
  font-size: 0.8rem;
}
.device-card .source {
  margin-top: 12px;
  font-size: 0.75rem;
  color: var(--muted);
}

@media (max-width: 900px) {
  .device-grid { grid-template-columns: 1fr; }
}
```

- [ ] **Step 2: Replace the Device Reference section body**

Replace the existing `<section id="device-reference">…</section>` block with:

```html
  <section id="device-reference">
    <h2>Device Reference</h2>
    <p class="purpose">Each ESP32 firmware variant runs a server side and a client side. The cards below summarize what each device serves and what it connects out to.</p>

    <div class="device-grid">
      <article class="device-card reader">
        <h3>Serial Reader</h3>
        <p class="summary">Reads from a physical serial device (scale, sensor) and exposes the live stream to any subscriber.</p>

        <span class="role-label">Server</span>
        <ul>
          <li><code>GET/POST /rest/serial</code></li>
          <li><code>WS /ws/serial</code></li>
          <li>MQTT publish <code>weighsoft/serial/{id}/data</code></li>
          <li>BLE read+notify <code>12340000-…-1234</code></li>
        </ul>

        <span class="role-label">Client</span>
        <ul>
          <li><code>WeightForwarderService</code> HTTP POSTs lines to a remote URL (e.g. Node-RED).</li>
        </ul>

        <p class="source">src/examples/serial/SerialService.{h,cpp}<br>src/examples/weightforwarder/WeightForwarderService.{h,cpp}</p>
      </article>

      <article class="device-card display">
        <h3>Display</h3>
        <p class="summary">Drives a 16x2 I2C LCD. Accepts data from any source; can also pull a remote stream onto the screen.</p>

        <span class="role-label">Server</span>
        <ul>
          <li><code>GET/POST /rest/display</code></li>
          <li><code>WS /ws/display</code></li>
          <li>MQTT subscribe <code>weighsoft/display/{id}/set</code></li>
        </ul>

        <span class="role-label">Client</span>
        <ul>
          <li><code>DisplayService</code> Serial Bridge connects out to a remote reader's <code>/ws/serial</code>, MQTT topic, or BLE peripheral and mirrors data on the LCD.</li>
        </ul>

        <p class="source">src/examples/display/DisplayService.{h,cpp}</p>
      </article>

      <article class="device-card writer">
        <h3>Serial Writer</h3>
        <p class="summary">Writes to a physical serial port. Accepts pending lines from any source, or pulls them from a remote reader.</p>

        <span class="role-label">Server</span>
        <ul>
          <li><code>GET/POST /rest/serialWriter</code></li>
          <li><code>WS /ws/serialWriter</code></li>
          <li>MQTT subscribe <code>weighsoft/serialwriter/{id}/send</code></li>
          <li>MQTT publish status <code>weighsoft/serialwriter/{id}/status</code></li>
        </ul>

        <span class="role-label">Client</span>
        <ul>
          <li><code>SerialWriterForwarderService</code> HTTP-polls or WebSocket-subscribes to a remote reader and writes received lines to the UART (and optionally USB CDC).</li>
        </ul>

        <p class="source">src/examples/serialwriter/SerialWriterService.{h,cpp}<br>src/examples/serialwriter/SerialWriterForwarderService.{h,cpp}</p>
      </article>
    </div>
  </section>
```

- [ ] **Step 3: Open the file in the browser to verify**

Reload the page.
Expected:
- Three cards side-by-side under the Device Reference heading.
- Each card has a colored 4px left border (blue / green / amber).
- Cards collapse to a single column when the browser is narrowed below 900px.
- Server/Client role labels render as small uppercase pills with a subtle background.

- [ ] **Step 4: Commit**

```bash
git add docs/multi-device-architecture.html
git commit -m "docs(serialWriter): add device reference cards"
```

---

### Task 3: Add reusable diagram CSS (svg wrapper, device boxes, paths, dots, protocol pills)

**Files:**
- Modify: `docs/multi-device-architecture.html` (`<style>` block only)

- [ ] **Step 1: Add diagram CSS rules**

Insert these rules immediately before the `@media (max-width: 900px)` block in the `<style>` element:

```css
/* Diagram wrapper and SVG */
.diagram {
  margin-top: 8px;
  padding: 16px;
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 10px;
  overflow-x: auto;
}
.diagram svg {
  display: block;
  width: 100%;
  min-width: 600px;
  height: var(--diagram-h);
}

/* Device boxes drawn inside SVG */
.dev-box {
  fill: var(--surface);
  stroke-width: 3;
  rx: 8; ry: 8;
}
.dev-box.reader   { stroke: var(--c-reader); }
.dev-box.display  { stroke: var(--c-display); }
.dev-box.writer   { stroke: var(--c-writer); }
.dev-box.nodered  { stroke: var(--c-nodered); }
.dev-box.mqtt     { stroke: var(--c-mqtt); }
.dev-box.mobile   { stroke: var(--c-mobile); }
.dev-box.physical { stroke: var(--c-physical); }

.dev-name {
  fill: var(--text);
  font-size: 14px;
  font-weight: 600;
  text-anchor: middle;
}
.dev-role {
  fill: var(--muted);
  font-size: 11px;
  text-anchor: middle;
  font-weight: 500;
  letter-spacing: 0.03em;
  text-transform: uppercase;
}

/* Flow paths and animated dots */
.flow-path {
  fill: none;
  stroke-width: 2;
  stroke-linecap: round;
}
.flow-path.reader   { stroke: var(--c-reader); }
.flow-path.display  { stroke: var(--c-display); }
.flow-path.writer   { stroke: var(--c-writer); }
.flow-path.nodered  { stroke: var(--c-nodered); }
.flow-path.mqtt     { stroke: var(--c-mqtt); }
.flow-path.mobile   { stroke: var(--c-mobile); }
.flow-path.physical { stroke: var(--c-physical); }
.flow-path.dashed   { stroke-dasharray: 8 6; opacity: 0.6; }

.flow-dot { r: 4; }
.flow-dot.reader   { fill: var(--c-reader); }
.flow-dot.display  { fill: var(--c-display); }
.flow-dot.writer   { fill: var(--c-writer); }
.flow-dot.nodered  { fill: var(--c-nodered); }
.flow-dot.mqtt     { fill: var(--c-mqtt); }
.flow-dot.mobile   { fill: var(--c-mobile); }
.flow-dot.physical { fill: var(--c-physical); }

/* Protocol pill labels */
.proto-pill rect {
  fill: var(--bg);
  stroke: var(--border);
  stroke-width: 1;
  rx: 4; ry: 4;
}
.proto-pill text {
  fill: var(--text);
  font-size: 10px;
  font-weight: 600;
  text-anchor: middle;
  dominant-baseline: middle;
  letter-spacing: 0.04em;
}

/* Reduced motion */
body[data-reduced-motion] .flow-dot { display: none; }
```

- [ ] **Step 2: Open the browser to verify nothing broke**

Reload. Expected: page looks identical (no diagrams yet — these styles are unused so far). Console shows no errors.

- [ ] **Step 3: Commit**

```bash
git add docs/multi-device-architecture.html
git commit -m "docs(serialWriter): add reusable diagram CSS primitives"
```

---

### Task 4: Use Case A diagram — Multiple Readers → Node-RED

**Files:**
- Modify: `docs/multi-device-architecture.html` (Use Case A `<section>`)

**Diagram coordinates** (viewBox `0 0 900 360`):
- Three Reader boxes on the left at x=140, y=40 / 140 / 240 (size 140w × 60h).
- Three purple physical-device boxes at x=20, y=50 / 150 / 250 (size 90w × 40h).
- Node-RED at x=700, y=140 (size 140w × 60h).
- Three flow paths from each Reader's right edge (x=280, y=70/170/270) curving to Node-RED's left edge (x=700, y=170).

- [ ] **Step 1: Replace the Use Case A section content**

Replace the existing `<section id="use-case-a">…</section>` block with:

```html
  <section id="use-case-a">
    <h2>Use Case A — Multiple Readers → Node-RED</h2>
    <p class="purpose">Several Readers, each tapping its own serial device, all forward their stream to a central Node-RED instance.</p>

    <div class="diagram" role="img" aria-labelledby="diag-a-title">
      <svg viewBox="0 0 900 360" preserveAspectRatio="xMidYMid meet" xmlns="http://www.w3.org/2000/svg">
        <title id="diag-a-title">Three Serial Readers HTTP-POSTing to one Node-RED node.</title>

        <!-- Physical serial devices (purple) -->
        <rect class="dev-box physical" x="20"  y="50"  width="90" height="40"/>
        <text class="dev-name" x="65"  y="76">Scale 1</text>
        <rect class="dev-box physical" x="20"  y="150" width="90" height="40"/>
        <text class="dev-name" x="65"  y="176">Scale 2</text>
        <rect class="dev-box physical" x="20"  y="250" width="90" height="40"/>
        <text class="dev-name" x="65"  y="276">Scale 3</text>

        <!-- Reader boxes -->
        <text class="dev-role" x="210" y="32">Reader · Server</text>
        <rect class="dev-box reader" x="140" y="40"  width="140" height="60"/>
        <text class="dev-name" x="210" y="76">Reader 1</text>

        <text class="dev-role" x="210" y="132">Reader · Server</text>
        <rect class="dev-box reader" x="140" y="140" width="140" height="60"/>
        <text class="dev-name" x="210" y="176">Reader 2</text>

        <text class="dev-role" x="210" y="232">Reader · Server</text>
        <rect class="dev-box reader" x="140" y="240" width="140" height="60"/>
        <text class="dev-name" x="210" y="276">Reader 3</text>

        <!-- Serial wires (purple short lines) -->
        <line x1="110" y1="70"  x2="140" y2="70"  stroke="#8b5cf6" stroke-width="2"/>
        <line x1="110" y1="170" x2="140" y2="170" stroke="#8b5cf6" stroke-width="2"/>
        <line x1="110" y1="270" x2="140" y2="270" stroke="#8b5cf6" stroke-width="2"/>

        <!-- Node-RED -->
        <text class="dev-role" x="770" y="132">Node-RED</text>
        <rect class="dev-box nodered" x="700" y="140" width="140" height="60"/>
        <text class="dev-name" x="770" y="176">Node-RED</text>

        <!-- Flow paths Reader N → Node-RED, drawn as quadratic curves -->
        <path id="a-flow-1" class="flow-path reader" d="M 280 70  Q 490 70  700 165"/>
        <path id="a-flow-2" class="flow-path reader" d="M 280 170 L 700 170"/>
        <path id="a-flow-3" class="flow-path reader" d="M 280 270 Q 490 270 700 175"/>

        <!-- Protocol pills -->
        <g class="proto-pill"><rect x="455" y="55"  width="60" height="16"/><text x="485" y="63">HTTP POST</text></g>
        <g class="proto-pill"><rect x="455" y="158" width="60" height="16"/><text x="485" y="166">HTTP POST</text></g>
        <g class="proto-pill"><rect x="455" y="262" width="60" height="16"/><text x="485" y="270">HTTP POST</text></g>

        <!-- Animated dots on each path: 3 dots, stagger begin = i*dur/3 -->
        <circle class="flow-dot reader"><animateMotion dur="2.4s" begin="0s"   repeatCount="indefinite"><mpath href="#a-flow-1"/></animateMotion></circle>
        <circle class="flow-dot reader"><animateMotion dur="2.4s" begin="0.8s" repeatCount="indefinite"><mpath href="#a-flow-1"/></animateMotion></circle>
        <circle class="flow-dot reader"><animateMotion dur="2.4s" begin="1.6s" repeatCount="indefinite"><mpath href="#a-flow-1"/></animateMotion></circle>

        <circle class="flow-dot reader"><animateMotion dur="2.0s" begin="0s"      repeatCount="indefinite"><mpath href="#a-flow-2"/></animateMotion></circle>
        <circle class="flow-dot reader"><animateMotion dur="2.0s" begin="0.667s"  repeatCount="indefinite"><mpath href="#a-flow-2"/></animateMotion></circle>
        <circle class="flow-dot reader"><animateMotion dur="2.0s" begin="1.333s"  repeatCount="indefinite"><mpath href="#a-flow-2"/></animateMotion></circle>

        <circle class="flow-dot reader"><animateMotion dur="2.4s" begin="0s"   repeatCount="indefinite"><mpath href="#a-flow-3"/></animateMotion></circle>
        <circle class="flow-dot reader"><animateMotion dur="2.4s" begin="0.8s" repeatCount="indefinite"><mpath href="#a-flow-3"/></animateMotion></circle>
        <circle class="flow-dot reader"><animateMotion dur="2.4s" begin="1.6s" repeatCount="indefinite"><mpath href="#a-flow-3"/></animateMotion></circle>
      </svg>
    </div>

    <p class="caption"><strong>How it works:</strong> Each Reader runs its own <code>WeightForwarderService</code> with the Node-RED HTTP endpoint as its target. Readers don't talk to each other. Node-RED receives one POST per parsed line per reader.</p>
    <p class="example">Real-world example: three weighbridges across a yard, all forwarding to a central Node-RED dashboard.</p>
  </section>
```

- [ ] **Step 2: Open the browser and verify**

Reload. Click the "A" link in the nav.
Expected:
- Diagram renders inside a dark surface card.
- Three blue Reader boxes on the left, each with a purple Scale box to its left connected by a short serial wire.
- One red Node-RED box on the right.
- Three blue paths from Readers to Node-RED, each labeled `HTTP POST`.
- Blue dots travel left-to-right along each path continuously.

- [ ] **Step 3: Commit**

```bash
git add docs/multi-device-architecture.html
git commit -m "docs(serialWriter): add Use Case A diagram (multiple readers to Node-RED)"
```

---

### Task 5: Use Case B diagram — 1 Reader + Display + Writer

**Files:**
- Modify: `docs/multi-device-architecture.html` (Use Case B `<section>`)

**Diagram coordinates** (viewBox `0 0 900 360`):
- Scale at x=20, y=150 (90×40).
- Reader at x=180, y=140 (140×60).
- Display at x=620, y=40 (180×60).
- Writer at x=620, y=240 (180×60).
- Path Reader → Display: from (320, 150) curve up to (620, 70).
- Path Reader → Writer:  from (320, 190) curve down to (620, 270).

- [ ] **Step 1: Replace the Use Case B section content**

```html
  <section id="use-case-b">
    <h2>Use Case B — 1 Reader + Display + Writer</h2>
    <p class="purpose">One Reader at the center. A Display and a Writer both pull data from its server side.</p>

    <div class="diagram" role="img" aria-labelledby="diag-b-title">
      <svg viewBox="0 0 900 360" preserveAspectRatio="xMidYMid meet" xmlns="http://www.w3.org/2000/svg">
        <title id="diag-b-title">One Serial Reader serving a Display via WebSocket and a Writer via HTTP poll.</title>

        <rect class="dev-box physical" x="20" y="150" width="90" height="40"/>
        <text class="dev-name" x="65" y="176">Scale</text>
        <line x1="110" y1="170" x2="180" y2="170" stroke="#8b5cf6" stroke-width="2"/>

        <text class="dev-role" x="250" y="132">Reader · Server</text>
        <rect class="dev-box reader" x="180" y="140" width="140" height="60"/>
        <text class="dev-name" x="250" y="176">Reader</text>

        <text class="dev-role" x="710" y="32">Display · Client</text>
        <rect class="dev-box display" x="620" y="40" width="180" height="60"/>
        <text class="dev-name" x="710" y="76">Display</text>

        <text class="dev-role" x="710" y="232">Writer · Client</text>
        <rect class="dev-box writer" x="620" y="240" width="180" height="60"/>
        <text class="dev-name" x="710" y="276">Writer</text>

        <path id="b-flow-display" class="flow-path display" d="M 320 150 Q 470 70 620 70"/>
        <path id="b-flow-writer"  class="flow-path writer"  d="M 320 190 Q 470 270 620 270"/>

        <g class="proto-pill"><rect x="430" y="80"  width="40" height="16"/><text x="450" y="88">WS</text></g>
        <g class="proto-pill"><rect x="430" y="248" width="70" height="16"/><text x="465" y="256">HTTP poll</text></g>

        <circle class="flow-dot display"><animateMotion dur="2.2s" begin="0s"      repeatCount="indefinite"><mpath href="#b-flow-display"/></animateMotion></circle>
        <circle class="flow-dot display"><animateMotion dur="2.2s" begin="0.733s"  repeatCount="indefinite"><mpath href="#b-flow-display"/></animateMotion></circle>
        <circle class="flow-dot display"><animateMotion dur="2.2s" begin="1.467s"  repeatCount="indefinite"><mpath href="#b-flow-display"/></animateMotion></circle>

        <circle class="flow-dot writer"><animateMotion dur="2.4s" begin="0s"   repeatCount="indefinite"><mpath href="#b-flow-writer"/></animateMotion></circle>
        <circle class="flow-dot writer"><animateMotion dur="2.4s" begin="0.8s" repeatCount="indefinite"><mpath href="#b-flow-writer"/></animateMotion></circle>
        <circle class="flow-dot writer"><animateMotion dur="2.4s" begin="1.6s" repeatCount="indefinite"><mpath href="#b-flow-writer"/></animateMotion></circle>
      </svg>
    </div>

    <p class="caption"><strong>How it works:</strong> The Display's Serial Bridge connects out to the Reader's <code>/ws/serial</code>. The Writer's <code>SerialWriterForwarderService</code> HTTP-polls the Reader (the firmware default; WebSocket subscribe is also supported via the same service config).</p>
    <p class="example">Real-world example: one scale read by an ESP, mirrored to a floor LCD and echoed onto a printer's serial port.</p>
  </section>
```

- [ ] **Step 2: Open the browser and verify**

Reload, jump to "B".
Expected: Reader in center with Scale on its left. Green Display top-right, amber Writer bottom-right. Green dots travel up to Display, amber dots travel down to Writer. Pills label `WS` and `HTTP poll`.

- [ ] **Step 3: Commit**

```bash
git add docs/multi-device-architecture.html
git commit -m "docs(serialWriter): add Use Case B diagram (reader serving display and writer)"
```

---

### Task 6: Use Case C diagram — Reader as local monitor AND cloud forwarder

**Files:**
- Modify: `docs/multi-device-architecture.html` (Use Case C `<section>`)

- [ ] **Step 1: Replace the Use Case C section content**

```html
  <section id="use-case-c">
    <h2>Use Case C — Reader as local monitor AND cloud forwarder</h2>
    <p class="purpose">A single Reader's server serves a Display while its own client side (Forwarder) pushes the same stream to Node-RED. Both flows run simultaneously.</p>

    <div class="diagram" role="img" aria-labelledby="diag-c-title">
      <svg viewBox="0 0 900 360" preserveAspectRatio="xMidYMid meet" xmlns="http://www.w3.org/2000/svg">
        <title id="diag-c-title">One Reader simultaneously serving a Display via WebSocket and forwarding to Node-RED via HTTP POST.</title>

        <rect class="dev-box physical" x="20" y="150" width="90" height="40"/>
        <text class="dev-name" x="65" y="176">Scale</text>
        <line x1="110" y1="170" x2="180" y2="170" stroke="#8b5cf6" stroke-width="2"/>

        <text class="dev-role" x="250" y="132">Reader · Server + Client</text>
        <rect class="dev-box reader" x="180" y="140" width="140" height="60"/>
        <text class="dev-name" x="250" y="176">Reader</text>

        <text class="dev-role" x="710" y="32">Display · Client</text>
        <rect class="dev-box display" x="620" y="40" width="180" height="60"/>
        <text class="dev-name" x="710" y="76">Display</text>

        <text class="dev-role" x="710" y="232">Node-RED</text>
        <rect class="dev-box nodered" x="620" y="240" width="180" height="60"/>
        <text class="dev-name" x="710" y="276">Node-RED</text>

        <path id="c-flow-display" class="flow-path display" d="M 320 150 Q 470 70  620 70"/>
        <path id="c-flow-nodered" class="flow-path nodered" d="M 320 190 Q 470 270 620 270"/>

        <g class="proto-pill"><rect x="430" y="80"  width="40" height="16"/><text x="450" y="88">WS</text></g>
        <g class="proto-pill"><rect x="430" y="248" width="60" height="16"/><text x="460" y="256">HTTP POST</text></g>

        <circle class="flow-dot display"><animateMotion dur="2.2s" begin="0s"     repeatCount="indefinite"><mpath href="#c-flow-display"/></animateMotion></circle>
        <circle class="flow-dot display"><animateMotion dur="2.2s" begin="0.733s" repeatCount="indefinite"><mpath href="#c-flow-display"/></animateMotion></circle>
        <circle class="flow-dot display"><animateMotion dur="2.2s" begin="1.467s" repeatCount="indefinite"><mpath href="#c-flow-display"/></animateMotion></circle>

        <circle class="flow-dot nodered"><animateMotion dur="2.4s" begin="0s"   repeatCount="indefinite"><mpath href="#c-flow-nodered"/></animateMotion></circle>
        <circle class="flow-dot nodered"><animateMotion dur="2.4s" begin="0.8s" repeatCount="indefinite"><mpath href="#c-flow-nodered"/></animateMotion></circle>
        <circle class="flow-dot nodered"><animateMotion dur="2.4s" begin="1.6s" repeatCount="indefinite"><mpath href="#c-flow-nodered"/></animateMotion></circle>
      </svg>
    </div>

    <p class="caption"><strong>How it works:</strong> The Reader's <code>SerialService</code> serves the Display over <code>/ws/serial</code>. In the same firmware, <code>WeightForwarderService</code> independently HTTP-POSTs every parsed line to Node-RED. Server side and client side run in parallel inside the same ESP.</p>
    <p class="example">Real-world example: local LCD on the production line for the operator, cloud logging in parallel for the office.</p>
  </section>
```

- [ ] **Step 2: Verify in browser**

Reload, jump to "C". Expected: Reader center, Display upper-right (green flow), Node-RED lower-right (red flow). Both flows animate simultaneously.

- [ ] **Step 3: Commit**

```bash
git add docs/multi-device-architecture.html
git commit -m "docs(serialWriter): add Use Case C diagram (reader as monitor and forwarder)"
```

---

### Task 7: Use Case D diagram — Multiple Displays from one Reader

**Files:**
- Modify: `docs/multi-device-architecture.html` (Use Case D `<section>`)

- [ ] **Step 1: Replace the Use Case D section content**

```html
  <section id="use-case-d">
    <h2>Use Case D — Multiple Displays from one Reader</h2>
    <p class="purpose">One Reader, many Display ESPs all subscribed to its server. Every screen shows the same value live.</p>

    <div class="diagram" role="img" aria-labelledby="diag-d-title">
      <svg viewBox="0 0 900 360" preserveAspectRatio="xMidYMid meet" xmlns="http://www.w3.org/2000/svg">
        <title id="diag-d-title">One Serial Reader broadcasting to three Displays via WebSocket.</title>

        <rect class="dev-box physical" x="20" y="150" width="90" height="40"/>
        <text class="dev-name" x="65" y="176">Scale</text>
        <line x1="110" y1="170" x2="180" y2="170" stroke="#8b5cf6" stroke-width="2"/>

        <text class="dev-role" x="250" y="132">Reader · Server</text>
        <rect class="dev-box reader" x="180" y="140" width="140" height="60"/>
        <text class="dev-name" x="250" y="176">Reader</text>

        <text class="dev-role" x="710" y="32">Display · Client</text>
        <rect class="dev-box display" x="620" y="40"  width="180" height="60"/>
        <text class="dev-name" x="710" y="76">Display 1</text>

        <text class="dev-role" x="710" y="132">Display · Client</text>
        <rect class="dev-box display" x="620" y="140" width="180" height="60"/>
        <text class="dev-name" x="710" y="176">Display 2</text>

        <text class="dev-role" x="710" y="232">Display · Client</text>
        <rect class="dev-box display" x="620" y="240" width="180" height="60"/>
        <text class="dev-name" x="710" y="276">Display 3</text>

        <path id="d-flow-1" class="flow-path display" d="M 320 150 Q 470 70  620 70"/>
        <path id="d-flow-2" class="flow-path display" d="M 320 170 L 620 170"/>
        <path id="d-flow-3" class="flow-path display" d="M 320 190 Q 470 270 620 270"/>

        <g class="proto-pill"><rect x="455" y="80"  width="40" height="16"/><text x="475" y="88">WS</text></g>
        <g class="proto-pill"><rect x="455" y="158" width="40" height="16"/><text x="475" y="166">WS</text></g>
        <g class="proto-pill"><rect x="455" y="262" width="40" height="16"/><text x="475" y="270">WS</text></g>

        <circle class="flow-dot display"><animateMotion dur="2.2s" begin="0s"     repeatCount="indefinite"><mpath href="#d-flow-1"/></animateMotion></circle>
        <circle class="flow-dot display"><animateMotion dur="2.2s" begin="0.733s" repeatCount="indefinite"><mpath href="#d-flow-1"/></animateMotion></circle>
        <circle class="flow-dot display"><animateMotion dur="2.2s" begin="1.467s" repeatCount="indefinite"><mpath href="#d-flow-1"/></animateMotion></circle>

        <circle class="flow-dot display"><animateMotion dur="2.0s" begin="0s"      repeatCount="indefinite"><mpath href="#d-flow-2"/></animateMotion></circle>
        <circle class="flow-dot display"><animateMotion dur="2.0s" begin="0.667s"  repeatCount="indefinite"><mpath href="#d-flow-2"/></animateMotion></circle>
        <circle class="flow-dot display"><animateMotion dur="2.0s" begin="1.333s"  repeatCount="indefinite"><mpath href="#d-flow-2"/></animateMotion></circle>

        <circle class="flow-dot display"><animateMotion dur="2.2s" begin="0s"     repeatCount="indefinite"><mpath href="#d-flow-3"/></animateMotion></circle>
        <circle class="flow-dot display"><animateMotion dur="2.2s" begin="0.733s" repeatCount="indefinite"><mpath href="#d-flow-3"/></animateMotion></circle>
        <circle class="flow-dot display"><animateMotion dur="2.2s" begin="1.467s" repeatCount="indefinite"><mpath href="#d-flow-3"/></animateMotion></circle>
      </svg>
    </div>

    <p class="caption"><strong>How it works:</strong> Each Display's Serial Bridge connects out to the same Reader's <code>/ws/serial</code>. WebSocket fan-out is handled inside the Reader by the framework's <code>WebSocketTxRx</code> — every connected client receives the same broadcast.</p>
    <p class="example">Real-world example: one scale read by an ESP, the same value shown on every status board on the warehouse floor.</p>
  </section>
```

- [ ] **Step 2: Verify in browser**

Reload, jump to "D". Expected: Reader on left, three Displays stacked on right, three green flows animating.

- [ ] **Step 3: Commit**

```bash
git add docs/multi-device-architecture.html
git commit -m "docs(serialWriter): add Use Case D diagram (multi-display fan-out)"
```

---

### Task 8: Use Case E diagram — Node-RED commanding a Writer

**Files:**
- Modify: `docs/multi-device-architecture.html` (Use Case E `<section>`)

- [ ] **Step 1: Replace the Use Case E section content**

```html
  <section id="use-case-e">
    <h2>Use Case E — Node-RED commanding a Writer</h2>
    <p class="purpose">Reverse direction. Node-RED POSTs commands to a Writer; the Writer pushes them out a physical serial port.</p>

    <div class="diagram" role="img" aria-labelledby="diag-e-title">
      <svg viewBox="0 0 900 360" preserveAspectRatio="xMidYMid meet" xmlns="http://www.w3.org/2000/svg">
        <title id="diag-e-title">Node-RED HTTP-POSTing commands to a Writer that drives a physical serial device.</title>

        <text class="dev-role" x="150" y="132">Node-RED</text>
        <rect class="dev-box nodered" x="80" y="140" width="140" height="60"/>
        <text class="dev-name" x="150" y="176">Node-RED</text>

        <text class="dev-role" x="470" y="132">Writer · Server</text>
        <rect class="dev-box writer" x="400" y="140" width="140" height="60"/>
        <text class="dev-name" x="470" y="176">Writer</text>

        <line x1="540" y1="170" x2="610" y2="170" stroke="#8b5cf6" stroke-width="2"/>
        <rect class="dev-box physical" x="610" y="150" width="120" height="40"/>
        <text class="dev-name" x="670" y="176">Printer / PLC</text>

        <path id="e-flow" class="flow-path nodered" d="M 220 170 L 400 170"/>

        <g class="proto-pill"><rect x="265" y="148" width="90" height="16"/><text x="310" y="156">HTTP POST</text></g>

        <circle class="flow-dot nodered"><animateMotion dur="1.8s" begin="0s"   repeatCount="indefinite"><mpath href="#e-flow"/></animateMotion></circle>
        <circle class="flow-dot nodered"><animateMotion dur="1.8s" begin="0.6s" repeatCount="indefinite"><mpath href="#e-flow"/></animateMotion></circle>
        <circle class="flow-dot nodered"><animateMotion dur="1.8s" begin="1.2s" repeatCount="indefinite"><mpath href="#e-flow"/></animateMotion></circle>
      </svg>
    </div>

    <p class="caption"><strong>How it works:</strong> Node-RED POSTs <code>{"pending_line": "..."}</code> to the Writer's <code>/rest/serialWriter</code> endpoint. <code>SerialWriterService</code> picks up the queued line on its next loop and writes it out the UART (and optionally USB CDC).</p>
    <p class="example">Real-world example: Node-RED triggers a label printer or PLC by POSTing the command string to the Writer.</p>
  </section>
```

- [ ] **Step 2: Verify in browser**

Reload, jump to "E". Expected: Three boxes in a horizontal line — red Node-RED, amber Writer, purple Printer/PLC. Single red flow with `HTTP POST` pill.

- [ ] **Step 3: Commit**

```bash
git add docs/multi-device-architecture.html
git commit -m "docs(serialWriter): add Use Case E diagram (Node-RED commanding writer)"
```

---

### Task 9: Use Case F diagram — MQTT broker as hub

**Files:**
- Modify: `docs/multi-device-architecture.html` (Use Case F `<section>`)

- [ ] **Step 1: Replace the Use Case F section content**

```html
  <section id="use-case-f">
    <h2>Use Case F — MQTT broker as hub</h2>
    <p class="purpose">All ESPs talk to one MQTT broker instead of each other. The broker decouples publishers and subscribers.</p>

    <div class="diagram" role="img" aria-labelledby="diag-f-title">
      <svg viewBox="0 0 900 360" preserveAspectRatio="xMidYMid meet" xmlns="http://www.w3.org/2000/svg">
        <title id="diag-f-title">A Reader publishes to an MQTT broker; Display, Writer, and Node-RED all subscribe.</title>

        <rect class="dev-box physical" x="20" y="150" width="90" height="40"/>
        <text class="dev-name" x="65" y="176">Scale</text>
        <line x1="110" y1="170" x2="140" y2="170" stroke="#8b5cf6" stroke-width="2"/>

        <text class="dev-role" x="210" y="132">Reader · Publisher</text>
        <rect class="dev-box reader" x="140" y="140" width="140" height="60"/>
        <text class="dev-name" x="210" y="176">Reader</text>

        <text class="dev-role" x="450" y="132">MQTT Broker</text>
        <rect class="dev-box mqtt" x="400" y="140" width="100" height="60"/>
        <text class="dev-name" x="450" y="176">Broker</text>

        <text class="dev-role" x="690" y="32">Display · Subscriber</text>
        <rect class="dev-box display" x="620" y="40"  width="170" height="60"/>
        <text class="dev-name" x="705" y="76">Display</text>

        <text class="dev-role" x="690" y="132">Writer · Subscriber</text>
        <rect class="dev-box writer" x="620" y="140" width="170" height="60"/>
        <text class="dev-name" x="705" y="176">Writer</text>

        <text class="dev-role" x="690" y="232">Node-RED</text>
        <rect class="dev-box nodered" x="620" y="240" width="170" height="60"/>
        <text class="dev-name" x="705" y="276">Node-RED</text>

        <path id="f-pub"     class="flow-path reader"  d="M 280 170 L 400 170"/>
        <path id="f-display" class="flow-path display" d="M 500 160 Q 560 70  620 70"/>
        <path id="f-writer"  class="flow-path writer"  d="M 500 170 L 620 170"/>
        <path id="f-nodered" class="flow-path nodered" d="M 500 180 Q 560 270 620 270"/>

        <g class="proto-pill"><rect x="295" y="148" width="92" height="16"/><text x="341" y="156">serial/{id}/data</text></g>
        <g class="proto-pill"><rect x="510" y="80"  width="92" height="16"/><text x="556" y="88">display/{id}/set</text></g>
        <g class="proto-pill"><rect x="515" y="148" width="100" height="16"/><text x="565" y="156">serialwriter/{id}/send</text></g>
        <g class="proto-pill"><rect x="510" y="248" width="92" height="16"/><text x="556" y="256">subscribe topic</text></g>

        <circle class="flow-dot reader"><animateMotion dur="1.6s" begin="0s"      repeatCount="indefinite"><mpath href="#f-pub"/></animateMotion></circle>
        <circle class="flow-dot reader"><animateMotion dur="1.6s" begin="0.533s"  repeatCount="indefinite"><mpath href="#f-pub"/></animateMotion></circle>
        <circle class="flow-dot reader"><animateMotion dur="1.6s" begin="1.067s"  repeatCount="indefinite"><mpath href="#f-pub"/></animateMotion></circle>

        <circle class="flow-dot display"><animateMotion dur="2.0s" begin="0s"     repeatCount="indefinite"><mpath href="#f-display"/></animateMotion></circle>
        <circle class="flow-dot display"><animateMotion dur="2.0s" begin="0.667s" repeatCount="indefinite"><mpath href="#f-display"/></animateMotion></circle>
        <circle class="flow-dot display"><animateMotion dur="2.0s" begin="1.333s" repeatCount="indefinite"><mpath href="#f-display"/></animateMotion></circle>

        <circle class="flow-dot writer"><animateMotion dur="1.8s" begin="0s"   repeatCount="indefinite"><mpath href="#f-writer"/></animateMotion></circle>
        <circle class="flow-dot writer"><animateMotion dur="1.8s" begin="0.6s" repeatCount="indefinite"><mpath href="#f-writer"/></animateMotion></circle>
        <circle class="flow-dot writer"><animateMotion dur="1.8s" begin="1.2s" repeatCount="indefinite"><mpath href="#f-writer"/></animateMotion></circle>

        <circle class="flow-dot nodered"><animateMotion dur="2.0s" begin="0s"     repeatCount="indefinite"><mpath href="#f-nodered"/></animateMotion></circle>
        <circle class="flow-dot nodered"><animateMotion dur="2.0s" begin="0.667s" repeatCount="indefinite"><mpath href="#f-nodered"/></animateMotion></circle>
        <circle class="flow-dot nodered"><animateMotion dur="2.0s" begin="1.333s" repeatCount="indefinite"><mpath href="#f-nodered"/></animateMotion></circle>
      </svg>
    </div>

    <p class="caption"><strong>How it works:</strong> Reader publishes to <code>weighsoft/serial/{id}/data</code>. Display subscribes to <code>weighsoft/display/{id}/set</code>; in this topology the broker (or a Node-RED bridge node) republishes Reader data onto the Display topic. The Writer subscribes to <code>weighsoft/serialwriter/{id}/send</code> and publishes status on <code>/status</code>. Node-RED can subscribe to any of these.</p>
    <p class="example">Real-world example: all ESPs talk to one broker; replacing or adding a device requires no IP reconfiguration on the others.</p>
  </section>
```

- [ ] **Step 2: Verify in browser**

Reload, jump to "F". Expected: cyan Broker in the center, blue Reader on its left, three subscribers stacked on the right (green Display, amber Writer, red Node-RED), four flow paths animating.

- [ ] **Step 3: Commit**

```bash
git add docs/multi-device-architecture.html
git commit -m "docs(serialWriter): add Use Case F diagram (MQTT broker hub)"
```

---

### Task 10: Use Case G diagram — Serial WiFi bridge

**Files:**
- Modify: `docs/multi-device-architecture.html` (Use Case G `<section>`)

- [ ] **Step 1: Replace the Use Case G section content**

```html
  <section id="use-case-g">
    <h2>Use Case G — Serial WiFi bridge (Reader feeds Writer)</h2>
    <p class="purpose">Two physical serial ports linked across WiFi. Reader reads device A; Writer writes the same data to device B.</p>

    <div class="diagram" role="img" aria-labelledby="diag-g-title">
      <svg viewBox="0 0 900 360" preserveAspectRatio="xMidYMid meet" xmlns="http://www.w3.org/2000/svg">
        <title id="diag-g-title">A serial signal travels from device A through Reader, over WiFi to Writer, and out to device B.</title>

        <rect class="dev-box physical" x="20" y="150" width="100" height="40"/>
        <text class="dev-name" x="70" y="176">Device A</text>
        <line x1="120" y1="170" x2="180" y2="170" stroke="#8b5cf6" stroke-width="2"/>

        <text class="dev-role" x="250" y="132">Reader · Server</text>
        <rect class="dev-box reader" x="180" y="140" width="140" height="60"/>
        <text class="dev-name" x="250" y="176">Reader</text>

        <text class="dev-role" x="650" y="132">Writer · Client</text>
        <rect class="dev-box writer" x="580" y="140" width="140" height="60"/>
        <text class="dev-name" x="650" y="176">Writer</text>

        <line x1="720" y1="170" x2="780" y2="170" stroke="#8b5cf6" stroke-width="2"/>
        <rect class="dev-box physical" x="780" y="150" width="100" height="40"/>
        <text class="dev-name" x="830" y="176">Device B</text>

        <!-- Single end-to-end flow path. The middle segment is rendered by a
             second visible dashed path overlaid on the same trajectory. -->
        <path id="g-flow"        class="flow-path reader"          d="M 320 170 L 580 170"/>
        <path                    class="flow-path reader dashed"   d="M 320 170 L 580 170"/>

        <g class="proto-pill"><rect x="410" y="148" width="80" height="16"/><text x="450" y="156">WiFi · HTTP</text></g>

        <circle class="flow-dot reader"><animateMotion dur="2.2s" begin="0s"      repeatCount="indefinite"><mpath href="#g-flow"/></animateMotion></circle>
        <circle class="flow-dot reader"><animateMotion dur="2.2s" begin="0.733s"  repeatCount="indefinite"><mpath href="#g-flow"/></animateMotion></circle>
        <circle class="flow-dot reader"><animateMotion dur="2.2s" begin="1.467s"  repeatCount="indefinite"><mpath href="#g-flow"/></animateMotion></circle>
      </svg>
    </div>

    <p class="caption"><strong>How it works:</strong> The Writer's <code>SerialWriterForwarderService</code> is configured with the Reader as its source URL. The Writer pulls every line and re-emits it on its own UART. The dashed segment is the WiFi hop; dots traverse it continuously so the visual reads as one stream from device A to device B.</p>
    <p class="example">Real-world example: two pieces of legacy serial equipment that need to share data but are physically too far apart for an RS-232 cable.</p>
  </section>
```

- [ ] **Step 2: Verify in browser**

Reload, jump to "G". Expected: 5 boxes in a horizontal line with a single solid + dashed-overlay flow line and dots traveling end-to-end without stopping.

- [ ] **Step 3: Commit**

```bash
git add docs/multi-device-architecture.html
git commit -m "docs(serialWriter): add Use Case G diagram (serial WiFi bridge)"
```

---

### Task 11: Use Case H diagram — Mixed protocol fan-out

**Files:**
- Modify: `docs/multi-device-architecture.html` (Use Case H `<section>`)

- [ ] **Step 1: Replace the Use Case H section content**

```html
  <section id="use-case-h">
    <h2>Use Case H — Mixed protocol fan-out from one Reader</h2>
    <p class="purpose">One Reader, four consumers, four protocols — all running concurrently from the same source line.</p>

    <div class="diagram" role="img" aria-labelledby="diag-h-title">
      <svg viewBox="0 0 900 360" preserveAspectRatio="xMidYMid meet" xmlns="http://www.w3.org/2000/svg">
        <title id="diag-h-title">One Serial Reader serving a Display via WebSocket, a Writer via HTTP poll, Node-RED via HTTP POST, and a mobile device via BLE.</title>

        <rect class="dev-box physical" x="20" y="160" width="90" height="40"/>
        <text class="dev-name" x="65" y="186">Scale</text>
        <line x1="110" y1="180" x2="180" y2="180" stroke="#8b5cf6" stroke-width="2"/>

        <text class="dev-role" x="250" y="142">Reader · Server</text>
        <rect class="dev-box reader" x="180" y="150" width="140" height="60"/>
        <text class="dev-name" x="250" y="186">Reader</text>

        <text class="dev-role" x="710" y="22">Display · Client</text>
        <rect class="dev-box display" x="620" y="30"  width="180" height="50"/>
        <text class="dev-name" x="710" y="60">Display</text>

        <text class="dev-role" x="710" y="112">Writer · Client</text>
        <rect class="dev-box writer" x="620" y="120" width="180" height="50"/>
        <text class="dev-name" x="710" y="150">Writer</text>

        <text class="dev-role" x="710" y="202">Node-RED</text>
        <rect class="dev-box nodered" x="620" y="210" width="180" height="50"/>
        <text class="dev-name" x="710" y="240">Node-RED</text>

        <text class="dev-role" x="710" y="292">Mobile · BLE</text>
        <rect class="dev-box mobile" x="620" y="300" width="180" height="50"/>
        <text class="dev-name" x="710" y="330">Mobile</text>

        <path id="h-display" class="flow-path display" d="M 320 160 Q 470 60  620 55"/>
        <path id="h-writer"  class="flow-path writer"  d="M 320 170 Q 470 145 620 145"/>
        <path id="h-nodered" class="flow-path nodered" d="M 320 195 Q 470 235 620 235"/>
        <path id="h-mobile"  class="flow-path mobile"  d="M 320 205 Q 470 325 620 325"/>

        <g class="proto-pill"><rect x="445" y="55"  width="40" height="16"/><text x="465" y="63">WS</text></g>
        <g class="proto-pill"><rect x="430" y="120" width="70" height="16"/><text x="465" y="128">HTTP poll</text></g>
        <g class="proto-pill"><rect x="430" y="218" width="70" height="16"/><text x="465" y="226">HTTP POST</text></g>
        <g class="proto-pill"><rect x="445" y="300" width="40" height="16"/><text x="465" y="308">BLE</text></g>

        <circle class="flow-dot display"><animateMotion dur="2.4s" begin="0s"   repeatCount="indefinite"><mpath href="#h-display"/></animateMotion></circle>
        <circle class="flow-dot display"><animateMotion dur="2.4s" begin="0.8s" repeatCount="indefinite"><mpath href="#h-display"/></animateMotion></circle>
        <circle class="flow-dot display"><animateMotion dur="2.4s" begin="1.6s" repeatCount="indefinite"><mpath href="#h-display"/></animateMotion></circle>

        <circle class="flow-dot writer"><animateMotion dur="2.0s" begin="0s"     repeatCount="indefinite"><mpath href="#h-writer"/></animateMotion></circle>
        <circle class="flow-dot writer"><animateMotion dur="2.0s" begin="0.667s" repeatCount="indefinite"><mpath href="#h-writer"/></animateMotion></circle>
        <circle class="flow-dot writer"><animateMotion dur="2.0s" begin="1.333s" repeatCount="indefinite"><mpath href="#h-writer"/></animateMotion></circle>

        <circle class="flow-dot nodered"><animateMotion dur="2.0s" begin="0s"     repeatCount="indefinite"><mpath href="#h-nodered"/></animateMotion></circle>
        <circle class="flow-dot nodered"><animateMotion dur="2.0s" begin="0.667s" repeatCount="indefinite"><mpath href="#h-nodered"/></animateMotion></circle>
        <circle class="flow-dot nodered"><animateMotion dur="2.0s" begin="1.333s" repeatCount="indefinite"><mpath href="#h-nodered"/></animateMotion></circle>

        <circle class="flow-dot mobile"><animateMotion dur="2.4s" begin="0s"   repeatCount="indefinite"><mpath href="#h-mobile"/></animateMotion></circle>
        <circle class="flow-dot mobile"><animateMotion dur="2.4s" begin="0.8s" repeatCount="indefinite"><mpath href="#h-mobile"/></animateMotion></circle>
        <circle class="flow-dot mobile"><animateMotion dur="2.4s" begin="1.6s" repeatCount="indefinite"><mpath href="#h-mobile"/></animateMotion></circle>
      </svg>
    </div>

    <p class="caption"><strong>How it works:</strong> Each consumer uses the protocol best suited to it. Display subscribes via <code>/ws/serial</code>. Writer's Forwarder HTTP-polls the Reader. Node-RED gets HTTP POSTs from the Reader's <code>WeightForwarderService</code>. A mobile app reads the Reader's BLE notify characteristic. The pink Mobile/BLE consumer appears only here.</p>
    <p class="example">Real-world example: one reader, four consumers — a single source of truth feeding every part of the operation in the protocol that suits each consumer.</p>
  </section>
```

- [ ] **Step 2: Verify in browser**

Reload, jump to "H". Expected: Reader on left, four boxes stacked on right (green, amber, red, pink). Four flows in distinct colors animating. Mobile box has the pink border.

- [ ] **Step 3: Commit**

```bash
git add docs/multi-device-architecture.html
git commit -m "docs(serialWriter): add Use Case H diagram (mixed protocol fan-out)"
```

---

### Task 12: Add IntersectionObserver active nav highlighting

**Files:**
- Modify: `docs/multi-device-architecture.html` (add `<script>` at the end of `<body>`)

- [ ] **Step 1: Insert the script just before `</body>`**

Add this script block immediately before the closing `</body>` tag:

```html
<script>
(function () {
  const navLinks = document.querySelectorAll('nav#topnav a[href^="#"]');
  const linkById = new Map();
  navLinks.forEach(a => {
    const id = a.getAttribute('href').slice(1);
    linkById.set(id, a);
  });

  const sections = Array.from(linkById.keys())
    .map(id => document.getElementById(id))
    .filter(Boolean);

  if (!('IntersectionObserver' in window) || sections.length === 0) return;

  const setActive = (id) => {
    navLinks.forEach(a => a.classList.remove('active'));
    const a = linkById.get(id);
    if (a) a.classList.add('active');
  };

  const observer = new IntersectionObserver((entries) => {
    // Pick the entry whose top is closest to (but past) the sticky-nav line.
    const visible = entries
      .filter(e => e.isIntersecting)
      .sort((a, b) => a.boundingClientRect.top - b.boundingClientRect.top);
    if (visible.length > 0) {
      setActive(visible[0].target.id);
    }
  }, {
    rootMargin: '-80px 0px -60% 0px',
    threshold: 0,
  });

  sections.forEach(s => observer.observe(s));
})();
</script>
```

- [ ] **Step 2: Verify in browser**

Reload. Slowly scroll from top to bottom.
Expected:
- The corresponding nav link (A, B, C…) gains the `.active` class as each use case section enters the viewport — visible as a slightly lighter background and brighter text.
- Console shows no errors.

- [ ] **Step 3: Commit**

```bash
git add docs/multi-device-architecture.html
git commit -m "docs(serialWriter): highlight active nav link via IntersectionObserver"
```

---

### Task 13: Add prefers-reduced-motion handling

**Files:**
- Modify: `docs/multi-device-architecture.html` (extend the existing `<script>` block)

- [ ] **Step 1: Add a second IIFE inside the same `<script>` block**

Insert this block immediately above the closing `</script>` tag (i.e. after the IntersectionObserver IIFE):

```javascript
(function () {
  const mql = window.matchMedia('(prefers-reduced-motion: reduce)');

  const apply = (reduced) => {
    document.querySelectorAll('svg').forEach(svg => {
      if (typeof svg.pauseAnimations === 'function') {
        if (reduced) svg.pauseAnimations();
        else svg.unpauseAnimations();
      }
    });
    if (reduced) document.body.setAttribute('data-reduced-motion', '');
    else document.body.removeAttribute('data-reduced-motion');
  };

  apply(mql.matches);

  if (typeof mql.addEventListener === 'function') {
    mql.addEventListener('change', e => apply(e.matches));
  } else if (typeof mql.addListener === 'function') {
    // Older Safari fallback
    mql.addListener(e => apply(e.matches));
  }
})();
```

- [ ] **Step 2: Verify in browser**

Reload with normal motion settings. Expected: dots animate as before.

Now toggle the OS-level reduced-motion preference:
- **Windows:** Settings → Accessibility → Visual effects → Animation effects: Off.
- **macOS:** System Settings → Accessibility → Display → Reduce motion: On.

Reload (some browsers don't pick up the change live). Expected:
- All flow dots disappear (CSS `display: none` via `body[data-reduced-motion]`).
- Paths and device boxes still render.

Toggle the preference back off and reload — dots return.

- [ ] **Step 3: Commit**

```bash
git add docs/multi-device-architecture.html
git commit -m "docs(serialWriter): respect prefers-reduced-motion via SMIL pauseAnimations"
```

---

### Task 14: Final verification pass — accessibility, responsive, console

**Files:**
- Read-only: `docs/multi-device-architecture.html`

This task does not modify the file unless an issue is found. It is a structured verification pass. If any check fails, fix it inline and re-run all checks before committing.

- [ ] **Step 1: Console / network sanity**

Open the file with DevTools open. Reload.
Expected:
- No errors or warnings in the Console tab.
- No 404s in the Network tab (the file is fully self-contained — there should be zero external requests beyond the HTML itself).

- [ ] **Step 2: Heading structure**

In DevTools, run in the Console:

```js
[...document.querySelectorAll('h1,h2,h3')].map(h => h.tagName + ' ' + h.textContent.slice(0, 60))
```

Expected output: exactly one `H1` (page title), 9 `H2`s (Device Reference + 8 use cases), 3 `H3`s (the device cards). If the count is off, find and fix the structural mismatch.

- [ ] **Step 3: Anchor ID coverage**

Run:

```js
['device-reference','use-case-a','use-case-b','use-case-c','use-case-d','use-case-e','use-case-f','use-case-g','use-case-h']
  .map(id => [id, !!document.getElementById(id)])
```

Expected: every entry returns `true`.

- [ ] **Step 4: SVG titles for screen readers**

Run:

```js
[...document.querySelectorAll('section .diagram svg > title')].length
```

Expected: `8` (one `<title>` element per use case diagram).

- [ ] **Step 5: Responsive check at narrow widths**

Resize the browser to ~700px wide. Expected:
- Device Reference cards collapse to a single column at <900px.
- Diagrams remain at their natural width with a horizontal scrollbar inside the diagram card (because of `min-width: 600px; overflow-x: auto`).

Resize further to ~400px. Expected:
- Sticky nav `<ul>` wraps and the brand text stacks above the links (the existing `@media (max-width: 720px)` rule).
- The page is still readable; diagrams scroll horizontally rather than crushing.

- [ ] **Step 6: Smooth-scroll behavior**

Click each nav link A–H in turn. Expected: page smoothly scrolls (not snaps) to each section, and the corresponding nav link becomes `.active` while that section is in view.

- [ ] **Step 7: If everything passes, no commit needed**

This task is read-only when all checks pass. If a check failed and you fixed it inline, commit the fix:

```bash
git add docs/multi-device-architecture.html
git commit -m "docs(serialWriter): fix issues found in final verification pass"
```

If everything passed first try, no commit is created.

- [ ] **Step 8: Push the branch**

Once all verification passes:

```bash
git push origin serialWriter
```

---

## Self-Review Notes

This plan was self-reviewed against the spec at
`docs/superpowers/specs/2026-04-29-multi-device-architecture-design.md`:

- **Spec coverage:** All ten structure items mapped to tasks. Header → Task 1.
  Device Reference → Task 2. Use Cases A–H → Tasks 4–11. Sticky nav → Task 1.
  Active nav highlight → Task 12. Reduced motion → Task 13. Anchor IDs use the
  `#use-case-x` convention from the spec. Color palette and `--diagram-h`
  variable match the spec verbatim.
- **Animation:** Stagger formula `begin = (i * dur) / N` from the spec is
  applied per diagram with `N = 3` (most paths) or with rounded begin times
  matching the spec's intent.
- **Reduced motion:** Uses `svg.pauseAnimations()` + `data-reduced-motion`
  attribute as the spec specifies, not the CSS `animation-play-state`
  approach the spec called out as wrong.
- **Responsive:** Diagram wrappers use `overflow-x: auto` with `min-width:
  600px` per spec. Device grid collapses below 900px.
- **No placeholders:** Every step contains real code or a real verification
  step — no TBDs, no "implement appropriately".
