# Multi-Device Architecture HTML Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a single self-contained `docs/multi-device-architecture.html` page that visually documents how the three Weighsoft ESP32 firmware variants (Serial Reader, Display, Serial Writer) compose into larger systems via 8 use case diagrams.

**Architecture:** One static HTML file. All CSS in a single `<style>` block, one inline `<script>` for `IntersectionObserver` nav highlighting / path-draw reveal and `prefers-reduced-motion` handling. Diagrams are inline SVG with `<animateMotion>` driving small dots along data flow paths. No build step, no external dependencies, no CDN. Two self-hosted woff2 fonts live alongside the HTML for offline-capable distinctive typography.

**Tech Stack:** Plain HTML5, CSS (custom properties, grid, sticky positioning), inline SVG with SMIL `<animateMotion>`, vanilla JS (`IntersectionObserver`, `matchMedia`). Self-hosted JetBrains Mono Bold + IBM Plex Sans Condensed Regular.

**Visual direction — Blueprint Cyanotype:**

This page is documentation about industrial serial hardware. The visual language is an engineering schematic / cyanotype blueprint, not a generic dashboard. The Blueprint direction *supersedes* the color palette listed in the spec — keep the spec's structure and content intent, replace the surface palette with the Blueprint values below.

- **Background:** deep oxide-blue `#08111d` with a faint `#1c3a5e` 24px grid (CSS repeating-linear-gradient at ~8% opacity) and a 1px noise grain overlay at ~3% opacity. A fixed title-block lives in the bottom-right corner of the viewport.
- **Ink palette:** signature cyan `#7fdbff` (= "Reader" channel and section accents). Other channels use desaturated technical hues so the page reads as one coherent drawing rather than a Tailwind rainbow:
  - Display:  `#6ae8b3` · Writer: `#f0b86a` · Node-RED: `#f08383` · MQTT broker: `#b78dff` · Mobile/BLE: `#ff9ec7` · Physical device: `#8ca0c0`.
  - Body text `#cbe7ff`, muted `#6b89a8`, hairlines `#2a4a6e`.
- **Geometry:** sharp 90° corners everywhere (no `border-radius` on any element). Device boxes use a 1px ink stroke with a translucent fill so they read as wet ink on paper. Each device card and diagram wrapper has L-shaped corner brackets (top-left / bottom-right) drawn via two pseudo-elements.
- **Typography:** **JetBrains Mono Bold** for headings, designation codes, and the title-block (uppercase, letter-spaced). **IBM Plex Sans Condensed Regular** for body and captions. No system fonts.
- **Motion:** dots travel as 3-dot "packet trains" with decreasing opacity (1.0 → 0.55 → 0.25) so each packet leaves a phosphor trail. On first scroll into a section, paths trace themselves via `stroke-dasharray` reveal before dots start flowing.

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

Color palette (Blueprint Cyanotype — overrides the spec's surface colors):

```css
--bg: #08111d;
--grid: #1c3a5e;
--paper: rgba(127, 219, 255, 0.025);
--ink-rule: #2a4a6e;
--ink-1: #7fdbff;
--text: #cbe7ff;
--muted: #6b89a8;
--c-reader:   #7fdbff;
--c-display:  #6ae8b3;
--c-writer:   #f0b86a;
--c-nodered:  #f08383;
--c-mqtt:     #b78dff;
--c-mobile:   #ff9ec7;
--c-physical: #8ca0c0;
--diagram-h: 380px;
```

---

### Task 0: Download self-hosted fonts

**Files:**
- Create: `docs/assets/fonts/JetBrainsMono-Bold.woff2`
- Create: `docs/assets/fonts/IBMPlexSansCondensed-Regular.woff2`

The page is dark, technical, offline-capable. Two distinctive fonts replace the system stack: JetBrains Mono Bold for headings/designation codes/title-block, IBM Plex Sans Condensed Regular for body. Both are open source (OFL).

- [ ] **Step 1: Create the fonts directory**

```bash
mkdir -p docs/assets/fonts
```

- [ ] **Step 2: Download JetBrains Mono Bold woff2**

```bash
curl -L -o docs/assets/fonts/JetBrainsMono-Bold.woff2 \
  https://github.com/JetBrains/JetBrainsMono/raw/v2.304/fonts/webfonts/JetBrainsMono-Bold.woff2
```

Expected: file exists, ~80KB, non-empty. Verify:

```bash
ls -lh docs/assets/fonts/JetBrainsMono-Bold.woff2
```

- [ ] **Step 3: Download IBM Plex Sans Condensed Regular woff2**

```bash
curl -L -o docs/assets/fonts/IBMPlexSansCondensed-Regular.woff2 \
  https://github.com/IBM/plex/raw/v6.4.0/IBM-Plex-Sans-Condensed/fonts/complete/woff2/IBMPlexSansCondensed-Regular.woff2
```

Expected: file exists, ~50KB, non-empty. Verify:

```bash
ls -lh docs/assets/fonts/IBMPlexSansCondensed-Regular.woff2
```

If either curl returns an HTML error page instead of a woff2 file (check the file size — under 5KB is suspect), find the current release tag at https://github.com/JetBrains/JetBrainsMono/releases or https://github.com/IBM/plex/releases and substitute the version into the URL above. Do not fall back to a CDN — the page must render offline.

- [ ] **Step 4: Commit the fonts**

```bash
git add docs/assets/fonts/JetBrainsMono-Bold.woff2 docs/assets/fonts/IBMPlexSansCondensed-Regular.woff2
git commit -m "docs(serialWriter): self-host JetBrains Mono Bold and IBM Plex Sans Condensed for blueprint typography"
```

---

### Task 1: Create page skeleton with sticky nav and CSS foundation

**Files:**
- Create: `docs/multi-device-architecture.html`

- [ ] **Step 1: Create the file with the Blueprint skeleton**

Write the entire file content below. The page will render with a deep oxide-blue background, a faint 24px blueprint grid, a fixed engineering-drawing title-block in the bottom-right corner, a sticky monospace nav, and 9 empty sections (the device reference + 8 use case anchors). No diagrams yet — they're added in later tasks.

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>WHB-ARCH-001 — Weighsoft Multi-Device Architecture</title>
  <style>
    /* ---------- Self-hosted fonts ---------- */
    @font-face {
      font-family: 'JetBrains Mono';
      font-style: normal;
      font-weight: 700;
      font-display: swap;
      src: url('assets/fonts/JetBrainsMono-Bold.woff2') format('woff2');
    }
    @font-face {
      font-family: 'IBM Plex Sans Condensed';
      font-style: normal;
      font-weight: 400;
      font-display: swap;
      src: url('assets/fonts/IBMPlexSansCondensed-Regular.woff2') format('woff2');
    }

    /* ---------- Tokens ---------- */
    :root {
      --bg: #08111d;
      --grid: #1c3a5e;
      --paper: rgba(127, 219, 255, 0.025);
      --ink-rule: #2a4a6e;
      --ink-1: #7fdbff;
      --text: #cbe7ff;
      --muted: #6b89a8;

      --c-reader:   #7fdbff;
      --c-display:  #6ae8b3;
      --c-writer:   #f0b86a;
      --c-nodered:  #f08383;
      --c-mqtt:     #b78dff;
      --c-mobile:   #ff9ec7;
      --c-physical: #8ca0c0;

      --diagram-h: 380px;

      --font-mono: 'JetBrains Mono', ui-monospace, Menlo, monospace;
      --font-sans: 'IBM Plex Sans Condensed', 'IBM Plex Sans', sans-serif;
    }

    * { box-sizing: border-box; }
    html { scroll-behavior: smooth; }

    body {
      margin: 0;
      background-color: var(--bg);
      background-image:
        repeating-linear-gradient(0deg,  var(--grid) 0 1px, transparent 1px 24px),
        repeating-linear-gradient(90deg, var(--grid) 0 1px, transparent 1px 24px);
      background-size: 24px 24px;
      background-attachment: fixed;
      color: var(--text);
      font-family: var(--font-sans);
      font-size: 15px;
      line-height: 1.55;
      letter-spacing: 0.005em;
    }

    /* Faint film-grain overlay so the page reads as printed paper */
    body::before {
      content: '';
      position: fixed;
      inset: 0;
      pointer-events: none;
      z-index: 1;
      opacity: 0.035;
      mix-blend-mode: screen;
      background-image: url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='160' height='160'><filter id='n'><feTurbulence type='fractalNoise' baseFrequency='0.9' numOctaves='2' stitchTiles='stitch'/><feColorMatrix values='0 0 0 0 1  0 0 0 0 1  0 0 0 0 1  0 0 0 0.6 0'/></filter><rect width='100%25' height='100%25' filter='url(%23n)'/></svg>");
    }

    /* ---------- Sticky nav ---------- */
    nav#topnav {
      position: sticky;
      top: 0;
      z-index: 10;
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 14px 28px;
      background: rgba(8, 17, 29, 0.82);
      backdrop-filter: blur(6px);
      -webkit-backdrop-filter: blur(6px);
      border-bottom: 1px solid var(--ink-rule);
    }
    nav#topnav .brand {
      font-family: var(--font-mono);
      font-weight: 700;
      font-size: 0.78rem;
      letter-spacing: 0.18em;
      text-transform: uppercase;
      color: var(--ink-1);
    }
    nav#topnav .brand .dim { color: var(--muted); margin-right: 10px; }
    nav#topnav ul {
      list-style: none;
      margin: 0;
      padding: 0;
      display: flex;
      gap: 0;
      border: 1px solid var(--ink-rule);
    }
    nav#topnav li + li { border-left: 1px solid var(--ink-rule); }
    nav#topnav a {
      display: inline-block;
      padding: 6px 12px;
      font-family: var(--font-mono);
      font-weight: 700;
      font-size: 0.78rem;
      letter-spacing: 0.12em;
      text-transform: uppercase;
      color: var(--muted);
      text-decoration: none;
      transition: color 0.15s, background 0.15s;
    }
    nav#topnav a:hover  { color: var(--ink-1); background: rgba(127, 219, 255, 0.06); }
    nav#topnav a.active { color: var(--bg); background: var(--ink-1); }

    /* ---------- Page shell ---------- */
    main {
      position: relative;
      z-index: 2;
      max-width: 1180px;
      margin: 0 auto;
      padding: 48px 28px 96px;
    }

    /* ---------- Header ---------- */
    header#page-header {
      position: relative;
      padding: 24px 0 36px;
      margin-bottom: 8px;
      border-bottom: 1px solid var(--ink-rule);
    }
    header#page-header .stamp {
      display: inline-block;
      padding: 4px 10px;
      margin-bottom: 18px;
      border: 1px solid var(--ink-1);
      font-family: var(--font-mono);
      font-weight: 700;
      font-size: 0.7rem;
      letter-spacing: 0.22em;
      text-transform: uppercase;
      color: var(--ink-1);
    }
    header#page-header h1 {
      margin: 0 0 12px;
      font-family: var(--font-mono);
      font-weight: 700;
      font-size: 2.4rem;
      letter-spacing: -0.01em;
      line-height: 1.1;
      color: var(--text);
    }
    header#page-header h1 .ink { color: var(--ink-1); }
    header#page-header p {
      margin: 0;
      max-width: 64ch;
      color: var(--muted);
      font-size: 1.05rem;
    }

    /* ---------- Sections ---------- */
    section {
      position: relative;
      padding: 40px 0 36px;
      border-top: 1px solid var(--ink-rule);
    }
    section .designation {
      display: inline-block;
      margin-bottom: 8px;
      padding: 2px 8px;
      border: 1px solid var(--ink-rule);
      font-family: var(--font-mono);
      font-weight: 700;
      font-size: 0.7rem;
      letter-spacing: 0.18em;
      text-transform: uppercase;
      color: var(--muted);
    }
    section h2 {
      margin: 0 0 6px;
      font-family: var(--font-mono);
      font-weight: 700;
      font-size: 1.35rem;
      letter-spacing: 0.01em;
      color: var(--text);
    }
    section h2 .em { color: var(--ink-1); }
    section .purpose {
      margin: 0 0 22px;
      max-width: 72ch;
      color: var(--muted);
    }
    section .caption {
      margin: 18px 0 6px;
      max-width: 72ch;
      color: var(--text);
      font-size: 0.95rem;
    }
    section .caption strong {
      font-family: var(--font-mono);
      font-weight: 700;
      letter-spacing: 0.06em;
      text-transform: uppercase;
      font-size: 0.78rem;
      color: var(--ink-1);
      margin-right: 8px;
    }
    section .example {
      margin: 0;
      max-width: 72ch;
      color: var(--muted);
      font-size: 0.88rem;
      font-style: italic;
    }
    section code {
      font-family: var(--font-mono);
      font-size: 0.85em;
      padding: 1px 4px;
      background: var(--paper);
      border: 1px solid var(--ink-rule);
      color: var(--ink-1);
    }

    /* ---------- Title block (bottom-right, like an engineering drawing) ---------- */
    aside#titleblock {
      position: fixed;
      right: 18px;
      bottom: 18px;
      z-index: 5;
      width: 232px;
      padding: 10px 12px;
      background: rgba(8, 17, 29, 0.78);
      border: 1px solid var(--ink-1);
      font-family: var(--font-mono);
      font-size: 0.66rem;
      letter-spacing: 0.12em;
      text-transform: uppercase;
      color: var(--muted);
      backdrop-filter: blur(4px);
      -webkit-backdrop-filter: blur(4px);
      pointer-events: none;
    }
    aside#titleblock .row {
      display: flex;
      justify-content: space-between;
      align-items: baseline;
      padding: 2px 0;
      border-bottom: 1px dashed var(--ink-rule);
    }
    aside#titleblock .row:last-child { border-bottom: none; }
    aside#titleblock .row .k { color: var(--muted); }
    aside#titleblock .row .v { color: var(--ink-1); font-weight: 700; }
    @media (max-width: 720px) { aside#titleblock { display: none; } }

    /* ---------- Responsive ---------- */
    @media (max-width: 720px) {
      nav#topnav { flex-direction: column; align-items: flex-start; gap: 10px; }
      nav#topnav ul { flex-wrap: wrap; }
      header#page-header h1 { font-size: 1.7rem; }
    }
  </style>
</head>
<body>

<nav id="topnav" aria-label="Page navigation">
  <span class="brand"><span class="dim">WHB-ARCH-001 //</span> Multi-Device Architecture</span>
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
    <span class="stamp">Drawing · Rev A · NTS</span>
    <h1>Multi-Device <span class="ink">Architecture</span></h1>
    <p>How the Serial Reader, Display, and Serial Writer ESP32 firmware variants compose into larger systems. Each ESP runs a server side and a client side; the diagrams below trace the wires between them.</p>
  </header>

  <section id="device-reference">
    <span class="designation">Sheet 00 · Device Reference</span>
    <h2>Device <span class="em">Reference</span></h2>
    <p class="purpose">Each ESP32 firmware variant runs a server side and a client side. The cards below summarize what each device serves and what it connects out to.</p>
  </section>

  <section id="use-case-a"><span class="designation">Sheet A · 01</span><h2>Multiple Readers → <span class="em">Node-RED</span></h2></section>
  <section id="use-case-b"><span class="designation">Sheet B · 02</span><h2>1 Reader + Display + <span class="em">Writer</span></h2></section>
  <section id="use-case-c"><span class="designation">Sheet C · 03</span><h2>Reader as local monitor + <span class="em">cloud forwarder</span></h2></section>
  <section id="use-case-d"><span class="designation">Sheet D · 04</span><h2>Multiple Displays from <span class="em">one Reader</span></h2></section>
  <section id="use-case-e"><span class="designation">Sheet E · 05</span><h2>Node-RED commanding a <span class="em">Writer</span></h2></section>
  <section id="use-case-f"><span class="designation">Sheet F · 06</span><h2>MQTT broker <span class="em">as hub</span></h2></section>
  <section id="use-case-g"><span class="designation">Sheet G · 07</span><h2>Serial WiFi <span class="em">bridge</span></h2></section>
  <section id="use-case-h"><span class="designation">Sheet H · 08</span><h2>Mixed protocol <span class="em">fan-out</span></h2></section>
</main>

<aside id="titleblock" aria-hidden="true">
  <div class="row"><span class="k">Drawing</span><span class="v">WHB-ARCH-001</span></div>
  <div class="row"><span class="k">Rev</span><span class="v">A</span></div>
  <div class="row"><span class="k">Date</span><span class="v">2026-04-29</span></div>
  <div class="row"><span class="k">Scale</span><span class="v">NTS</span></div>
  <div class="row"><span class="k">Project</span><span class="v">Weighsoft</span></div>
</aside>

</body>
</html>
```

- [ ] **Step 2: Open the file in a browser to verify**

Open `docs/multi-device-architecture.html` directly (Windows: double-click or `start docs/multi-device-architecture.html`).
Expected:
- Deep oxide-blue background with a faint 24px grid covering the whole viewport. Subtle grain texture visible.
- Both fonts loaded (DevTools → Network → font requests for both woff2 files succeed). The nav and headings render in JetBrains Mono Bold; body text in IBM Plex Sans Condensed.
- Sticky nav at top with `WHB-ARCH-001 //  Multi-Device Architecture` on the left and a row of bordered A–H letters on the right. Clicking a letter smoothly scrolls to its section.
- Each section shows a designation badge (`Sheet A · 01`, etc.) above its heading.
- A fixed engineering title-block hovers in the bottom-right corner with `Drawing / Rev / Date / Scale / Project` rows.
- DevTools Console is clean.

- [ ] **Step 3: Commit**

```bash
git add docs/multi-device-architecture.html
git commit -m "docs(serialWriter): scaffold multi-device-architecture.html with blueprint cyanotype shell"
```

---

### Task 2: Add Device Reference cards (Reader, Display, Writer)

**Files:**
- Modify: `docs/multi-device-architecture.html` (CSS + Device Reference section body)

- [ ] **Step 1: Add device-card CSS rules to the `<style>` block**

The cards are styled like schematic component data-sheet entries: sharp 90° corners, a 1px ink border on the device's channel color, a designation code in the top-right (R-01 / D-01 / W-01), and L-shaped corner brackets in two diagonal corners drawn with two pseudo-elements.

Insert these rules immediately before the `@media (max-width: 720px)` block at the end of the `<style>` element:

```css
/* Device reference grid */
.device-grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 20px;
  margin-top: 20px;
}
.device-card {
  position: relative;
  padding: 20px 22px 22px;
  background: var(--paper);
  border: 1px solid var(--ink-rule);
}
.device-card::before,
.device-card::after {
  content: '';
  position: absolute;
  width: 12px;
  height: 12px;
}
.device-card::before {
  top: -1px; left: -1px;
  border-top: 1px solid var(--ink);
  border-left: 1px solid var(--ink);
}
.device-card::after {
  bottom: -1px; right: -1px;
  border-bottom: 1px solid var(--ink);
  border-right: 1px solid var(--ink);
}
.device-card.reader  { --ink: var(--c-reader); }
.device-card.reader  { border-color: rgba(127, 219, 255, 0.55); }
.device-card.display { --ink: var(--c-display); }
.device-card.display { border-color: rgba(106, 232, 179, 0.55); }
.device-card.writer  { --ink: var(--c-writer); }
.device-card.writer  { border-color: rgba(240, 184, 106, 0.55); }

.device-card .designation-code {
  position: absolute;
  top: 8px;
  right: 12px;
  font-family: var(--font-mono);
  font-weight: 700;
  font-size: 0.7rem;
  letter-spacing: 0.18em;
  color: var(--ink);
}

.device-card h3 {
  margin: 0 0 4px;
  font-family: var(--font-mono);
  font-weight: 700;
  font-size: 0.95rem;
  letter-spacing: 0.1em;
  text-transform: uppercase;
  color: var(--ink);
}
.device-card .summary {
  margin: 0 0 14px;
  padding-bottom: 12px;
  border-bottom: 1px dashed var(--ink-rule);
  color: var(--muted);
  font-size: 0.92rem;
}

.device-card .role-label {
  display: inline-block;
  margin: 12px 0 6px;
  padding: 2px 8px;
  font-family: var(--font-mono);
  font-weight: 700;
  font-size: 0.65rem;
  letter-spacing: 0.22em;
  text-transform: uppercase;
  color: var(--bg);
  background: var(--ink);
}

.device-card ul {
  margin: 0 0 6px;
  padding-left: 14px;
  list-style: none;
  font-size: 0.88rem;
}
.device-card li {
  margin-bottom: 4px;
  position: relative;
}
.device-card li::before {
  content: '─';
  position: absolute;
  left: -14px;
  color: var(--ink-rule);
}
.device-card code {
  font-family: var(--font-mono);
  font-size: 0.78rem;
  padding: 1px 5px;
  background: rgba(127, 219, 255, 0.05);
  border: 1px solid var(--ink-rule);
  color: var(--ink);
}
.device-card .source {
  margin-top: 14px;
  padding-top: 10px;
  border-top: 1px dashed var(--ink-rule);
  font-family: var(--font-mono);
  font-size: 0.72rem;
  letter-spacing: 0.04em;
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
    <span class="designation">Sheet 00 · Device Reference</span>
    <h2>Device <span class="em">Reference</span></h2>
    <p class="purpose">Each ESP32 firmware variant runs a server side and a client side. The cards below summarize what each device serves and what it connects out to.</p>

    <div class="device-grid">
      <article class="device-card reader">
        <span class="designation-code">R-01</span>
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
        <span class="designation-code">D-01</span>
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
        <span class="designation-code">W-01</span>
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
- Three cards side-by-side, each with sharp 90° corners and a hairline ink border in its channel color (cyan / mint / amber).
- Each card shows a designation code in its top-right (`R-01`, `D-01`, `W-01`) in mono caps.
- Each card has a top-left and bottom-right L-shaped corner bracket (the registration marks drawn with ::before / ::after pseudo-elements).
- Server/Client role labels render as filled mono-uppercase chips in the channel color.
- List items use `─` dash bullets, code chips use the mono font with hairline borders.
- Cards collapse to a single column when the browser is narrowed below 900px.

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

These primitives apply uniformly to every diagram in Tasks 4–11. The eight diagrams keep their topology and coordinates; they inherit this visual language. Key choices:

- **Sharp corners** (`rx: 0; ry: 0`).
- **Ink stroke** at 1.5px (1px feels too fine on dark, 3px feels chunky).
- **Translucent fill** (`var(--paper)`) so the box reads as wet ink on paper rather than a Material card.
- **Mono device names** (uppercase, letter-spaced) and tiny mono role labels.
- **Callout protocol pills** — no rounded chip background; just mono text bracketed by `⌜ ⌟` corner-bracket characters with a hairline underline.
- **Packet trains** — three dots per train with opacity 1.0 / 0.55 / 0.25 so each "packet" leaves a brief phosphor trail.
- **Path-draw reveal** — paths start with `stroke-dashoffset` set to their length; the IntersectionObserver in Task 12 zeroes that offset on first entry, animating the line drawing itself.

Insert these rules immediately before the `@media (max-width: 900px)` block in the `<style>` element:

```css
/* ---------- Diagram wrapper ---------- */
.diagram {
  position: relative;
  margin-top: 12px;
  padding: 18px 18px 14px;
  background: var(--paper);
  border: 1px solid var(--ink-rule);
  overflow-x: auto;
}
.diagram::before,
.diagram::after {
  content: '';
  position: absolute;
  width: 14px; height: 14px;
  pointer-events: none;
}
.diagram::before {
  top: -1px; left: -1px;
  border-top: 1px solid var(--ink-1);
  border-left: 1px solid var(--ink-1);
}
.diagram::after {
  bottom: -1px; right: -1px;
  border-bottom: 1px solid var(--ink-1);
  border-right: 1px solid var(--ink-1);
}
.diagram svg {
  display: block;
  width: 100%;
  min-width: 600px;
  height: var(--diagram-h);
  font-family: var(--font-mono);
}

/* ---------- Device boxes drawn inside SVG ---------- */
.dev-box {
  fill: var(--paper);
  stroke-width: 1.5;
  rx: 0; ry: 0;
}
.dev-box.reader   { stroke: var(--c-reader); }
.dev-box.display  { stroke: var(--c-display); }
.dev-box.writer   { stroke: var(--c-writer); }
.dev-box.nodered  { stroke: var(--c-nodered); }
.dev-box.mqtt     { stroke: var(--c-mqtt); }
.dev-box.mobile   { stroke: var(--c-mobile); }
.dev-box.physical { stroke: var(--c-physical); stroke-dasharray: 4 3; }

.dev-name {
  fill: var(--text);
  font-size: 12px;
  font-weight: 700;
  letter-spacing: 0.14em;
  text-transform: uppercase;
  text-anchor: middle;
  font-family: var(--font-mono);
}
.dev-role {
  fill: var(--muted);
  font-size: 9.5px;
  font-weight: 700;
  letter-spacing: 0.22em;
  text-transform: uppercase;
  text-anchor: middle;
  font-family: var(--font-mono);
}

/* ---------- Flow paths ---------- */
.flow-path {
  fill: none;
  stroke-width: 1.5;
  stroke-linecap: square;
  /* Path-draw reveal: dasharray = path length is set per-path inline.
     The Task 12 script zeroes --draw-offset when the section enters view. */
  stroke-dashoffset: var(--draw-offset, 0);
  transition: stroke-dashoffset 800ms cubic-bezier(0.65, 0, 0.35, 1);
}
.flow-path.reader   { stroke: var(--c-reader); }
.flow-path.display  { stroke: var(--c-display); }
.flow-path.writer   { stroke: var(--c-writer); }
.flow-path.nodered  { stroke: var(--c-nodered); }
.flow-path.mqtt     { stroke: var(--c-mqtt); }
.flow-path.mobile   { stroke: var(--c-mobile); }
.flow-path.physical { stroke: var(--c-physical); }
.flow-path.dashed   { stroke-dasharray: 6 5 !important; opacity: 0.55; }

/* ---------- Animated dots (packet trains: 3 dots, decreasing opacity) ---------- */
.flow-dot { r: 3.5; }
.flow-dot.lead   { opacity: 1; }
.flow-dot.mid    { opacity: 0.55; r: 2.8; }
.flow-dot.trail  { opacity: 0.25; r: 2.2; }
.flow-dot.reader   { fill: var(--c-reader); }
.flow-dot.display  { fill: var(--c-display); }
.flow-dot.writer   { fill: var(--c-writer); }
.flow-dot.nodered  { fill: var(--c-nodered); }
.flow-dot.mqtt     { fill: var(--c-mqtt); }
.flow-dot.mobile   { fill: var(--c-mobile); }
.flow-dot.physical { fill: var(--c-physical); }

/* ---------- Protocol pills as schematic callouts ----------
   Existing diagram markup uses <g class="proto-pill"><rect/><text/></g>.
   We restyle that rect with sharp corners + paper fill + ink hairline so
   it reads as a callout box on a schematic, not a Material chip.        */
.proto-pill rect {
  fill: var(--bg);
  stroke: var(--ink-rule);
  stroke-width: 1;
  rx: 0; ry: 0;
}
.proto-pill text {
  fill: var(--ink-1);
  font-size: 9.5px;
  font-weight: 700;
  font-family: var(--font-mono);
  letter-spacing: 0.18em;
  text-transform: uppercase;
  text-anchor: middle;
  dominant-baseline: middle;
}

/* ---------- Reduced motion ---------- */
body[data-reduced-motion] .flow-dot { display: none; }
body[data-reduced-motion] .flow-path { stroke-dashoffset: 0; transition: none; }
```

**Note for Tasks 4–11:** No diagram-markup changes are needed. The diagrams in Tasks 4–11 declare 3 dots per path with begins staggered across the full duration (`0`, `dur/3`, `2·dur/3`) — that produces three independent packets continuously circling the path, which is exactly what we want for the Blueprint look. The packet-trail effect (decreasing opacity behind each dot) is added at runtime by the script in Task 12, which clones each `<circle class="flow-dot">` into a 3-dot train (`lead` / `mid` / `trail`) with begins offset by `-0.08s` / `-0.16s` from the original. The cloning approach keeps the eight diagram markup blocks untouched.

- [ ] **Step 2: Open the browser to verify nothing broke**

Reload. Expected: page looks identical to the end of Task 2 (no diagrams yet — these styles are unused so far). Console shows no errors. The new CSS rules don't apply visibly until Task 4 inserts the first SVG.

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

### Task 12: Add IntersectionObserver — active nav highlight, path-draw reveal, packet-train dots

**Files:**
- Modify: `docs/multi-device-architecture.html` (add `<script>` at the end of `<body>`)

This task wires up three behaviors in a single script block:

1. **Active nav highlight** — IntersectionObserver toggles `.active` on the nav link whose section is in view.
2. **Path-draw reveal** — on first entry, each `.flow-path` in the entering section animates `stroke-dashoffset` from its full path length down to 0 over 800ms, then the dots become visible.
3. **Packet-train cloning** — at startup, each `<circle class="flow-dot">` is replaced with a 3-circle train (`lead` / `mid` / `trail`) so each animated dot drags a fading phosphor trail.

- [ ] **Step 1: Insert the script just before `</body>`**

```html
<script>
/* ---- Packet-train cloning: turn every flow-dot into a 3-circle phosphor train ---- */
(function () {
  document.querySelectorAll('svg').forEach(svg => {
    const dots = Array.from(svg.querySelectorAll('circle.flow-dot'));
    dots.forEach(dot => {
      // Mark the original as the lead dot.
      dot.classList.add('lead');
      const motion = dot.querySelector('animateMotion');
      if (!motion) return;

      const beginAttr = motion.getAttribute('begin') || '0s';
      const beginSec = parseFloat(beginAttr);
      if (Number.isNaN(beginSec)) return;

      const dur = motion.getAttribute('dur') || '2s';
      const mpath = motion.querySelector('mpath');
      const pathHref = mpath ? mpath.getAttribute('href') : null;
      if (!pathHref) return;

      // Clone two trailing dots offset slightly behind the lead.
      [['mid', 0.08], ['trail', 0.16]].forEach(([cls, delta]) => {
        const clone = dot.cloneNode(false);
        clone.classList.remove('lead');
        clone.classList.add(cls);
        const am = document.createElementNS('http://www.w3.org/2000/svg', 'animateMotion');
        am.setAttribute('dur', dur);
        am.setAttribute('begin', (beginSec - delta).toFixed(3) + 's');
        am.setAttribute('repeatCount', 'indefinite');
        const mp = document.createElementNS('http://www.w3.org/2000/svg', 'mpath');
        mp.setAttribute('href', pathHref);
        am.appendChild(mp);
        clone.appendChild(am);
        dot.parentNode.insertBefore(clone, dot);
      });
    });
  });
})();

/* ---- Path-draw reveal preparation: set stroke-dasharray = path length, hide ---- */
(function () {
  document.querySelectorAll('.flow-path:not(.dashed)').forEach(p => {
    try {
      const len = p.getTotalLength();
      if (!len) return;
      p.style.strokeDasharray = len + ' ' + len;
      p.style.setProperty('--draw-offset', len + 'px');
      p.dataset.drawLength = String(len);
    } catch (_) { /* getTotalLength fails on detached SVGs in some browsers — ignore */ }
  });
})();

/* ---- IntersectionObserver: active nav + first-time path-draw reveal ---- */
(function () {
  const navLinks = document.querySelectorAll('nav#topnav a[href^="#"]');
  const linkById = new Map();
  navLinks.forEach(a => linkById.set(a.getAttribute('href').slice(1), a));

  const sections = Array.from(linkById.keys())
    .map(id => document.getElementById(id))
    .filter(Boolean);

  if (!('IntersectionObserver' in window) || sections.length === 0) return;

  const drawn = new WeakSet();

  const setActive = (id) => {
    navLinks.forEach(a => a.classList.remove('active'));
    const a = linkById.get(id);
    if (a) a.classList.add('active');
  };

  const drawPaths = (section) => {
    if (drawn.has(section)) return;
    drawn.add(section);
    section.querySelectorAll('.flow-path:not(.dashed)').forEach(p => {
      // Trigger transition by zeroing the stroke-dashoffset.
      requestAnimationFrame(() => p.style.setProperty('--draw-offset', '0px'));
    });
  };

  const observer = new IntersectionObserver((entries) => {
    // Active-nav: pick the topmost intersecting section.
    const visible = entries
      .filter(e => e.isIntersecting)
      .sort((a, b) => a.boundingClientRect.top - b.boundingClientRect.top);
    if (visible.length > 0) setActive(visible[0].target.id);

    // Path-draw: any entry that just became intersecting gets its paths drawn.
    entries.forEach(e => {
      if (e.isIntersecting) drawPaths(e.target);
    });
  }, {
    rootMargin: '-80px 0px -50% 0px',
    threshold: 0,
  });

  sections.forEach(s => observer.observe(s));
})();
</script>
```

- [ ] **Step 2: Verify in browser**

Reload. Scroll slowly from top to bottom.
Expected:
- The first time each use case section enters the viewport, its data-flow paths trace themselves over ~0.8s (paths visibly draw in from source to destination) and then the animated dot trains start running.
- Each dot is now followed by a fainter mid dot and a faintest trail dot — packet-train phosphor effect.
- The corresponding nav letter (A–H) gains the active style (cyan-on-dark inverted) as each section scrolls into view.
- Console shows no errors. (If `getTotalLength()` warns on Firefox for off-screen paths, the catch block silences it harmlessly.)

- [ ] **Step 3: Commit**

```bash
git add docs/multi-device-architecture.html
git commit -m "docs(serialWriter): add path-draw reveal, packet-train dots, active nav highlight"
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

- [ ] **Step 1: Console / network sanity + font loading**

Open the file with DevTools open. Reload.
Expected:
- No errors or warnings in the Console tab.
- Network tab shows exactly two external font requests, both 200 OK:
  - `assets/fonts/JetBrainsMono-Bold.woff2`
  - `assets/fonts/IBMPlexSansCondensed-Regular.woff2`
- No 404s. No CDN requests, no Google Fonts requests, no other external resources.

Verify the page is using the loaded fonts (not falling back to system mono):

```js
getComputedStyle(document.querySelector('h1')).fontFamily
```

Expected output: starts with `"JetBrains Mono"`. If it's falling back to `ui-monospace` or `Menlo`, the font failed to load — check the path and the woff2 files from Task 0.

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

- [ ] **Step 6: Smooth-scroll behavior + first-time path reveal**

Hard-refresh (Ctrl+Shift+R) so the IntersectionObserver state is reset. Click each nav link A–H in turn. Expected:
- Page smoothly scrolls (not snaps) to each section.
- The first time a section enters view, its diagram's flow paths visibly trace themselves before the dots start flowing.
- After all sections have been visited, scrolling back over them does NOT re-trigger the path-draw (the `WeakSet` in the script ensures one-shot reveal).
- The corresponding nav link becomes `.active` (cyan-on-dark inverted) while that section is in view.

- [ ] **Step 7: Title-block presence**

Verify the engineering title-block is fixed in the bottom-right corner of the viewport:

```js
const tb = document.getElementById('titleblock');
[getComputedStyle(tb).position, tb.offsetWidth, tb.querySelectorAll('.row').length]
```

Expected: `["fixed", 232, 5]`. Title-block hides itself below 720px viewport width — verify by resizing.

- [ ] **Step 8: Packet-train dot count**

After the script runs, every original `.flow-dot` has been cloned into 3. Total dot count should be a multiple of 3:

```js
const dots = document.querySelectorAll('.flow-dot');
[dots.length, dots.length % 3 === 0]
```

Expected: `[N, true]` where N is the total. Also verify the three classes are present:

```js
['lead', 'mid', 'trail'].map(c => document.querySelectorAll('.flow-dot.' + c).length)
```

Expected: three equal numbers (one third of total each).

- [ ] **Step 9: If everything passes, no commit needed**

This task is read-only when all checks pass. If a check failed and you fixed it inline, commit the fix:

```bash
git add docs/multi-device-architecture.html
git commit -m "docs(serialWriter): fix issues found in final verification pass"
```

If everything passed first try, no commit is created.

- [ ] **Step 10: Push the branch**

Once all verification passes:

```bash
git push origin serialWriter
```

---

## Self-Review Notes

This plan was self-reviewed against the spec at
`docs/superpowers/specs/2026-04-29-multi-device-architecture-design.md` and
the Blueprint Cyanotype visual direction agreed in the design review:

- **Spec coverage:** All ten structure items mapped to tasks. Fonts → Task 0.
  Header → Task 1. Device Reference → Task 2. Diagram primitives → Task 3.
  Use Cases A–H → Tasks 4–11. Sticky nav → Task 1. Active nav highlight +
  path-draw reveal + packet-train dots → Task 12. Reduced motion → Task 13.
  Anchor IDs use the `#use-case-x` convention from the spec.
- **Visual direction supersedes spec palette:** the Blueprint Cyanotype
  palette replaces the spec's Tailwind-default colors. Structure, content,
  and topology of the spec are preserved; only the surface aesthetic
  changes. This is called out at the top of the plan under "Visual
  direction".
- **Animation:** Stagger formula `begin = (i * dur) / N` from the spec is
  applied per diagram with `N = 3`. The packet-train cloning in Task 12
  layers two trailing dots at `-0.08s` and `-0.16s` for the phosphor effect
  on top of the spec's stagger, without changing the spec's intent.
- **Reduced motion:** Uses `svg.pauseAnimations()` + `data-reduced-motion`
  attribute as the spec specifies, not the CSS `animation-play-state`
  approach the spec called out as wrong. Reduced motion also disables the
  path-draw transition so paths render fully drawn immediately.
- **Responsive:** Diagram wrappers use `overflow-x: auto` with `min-width:
  600px` per spec. Device grid collapses below 900px. Title-block hides
  below 720px.
- **Self-contained:** Two woff2 fonts live in `docs/assets/fonts/`. No CDN,
  no Google Fonts, no external CSS or JS. The page renders fully offline.
- **No placeholders:** Every step contains real code or a real verification
  step — no TBDs, no "implement appropriately".
