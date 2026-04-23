---
name: Testing-only soak plan
overview: "Amended from the original debug plan: validate the **current** tree with **no code or config edits**—build, flash to COM10, and run the supervised WebSocket soak with logging and greps. This work is best delegated to the **shell** subagent (terminal-heavy pipeline); use **generalPurpose** only if failures need broader diagnosis without changing files."
todos:
  - id: snapshot-readonly
    content: Record git status and HEAD hash; note dirty paths for traceability only (no fixes).
    status: pending
  - id: build-no-edit
    content: Run `python -m platformio run -e esp32wroom32d`; abort task on failure (no source changes in this session).
    status: pending
  - id: flash-com10
    content: Free COM10 handles per platformio-upload rule; upload with `python -m platformio run -e esp32wroom32d -t upload`.
    status: pending
  - id: smoke-then-soak
    content: 10–15 min smoke (scale + WS peer), then supervised ~2h soak with Serial + peer logs and periodic failure greps.
    status: pending
  - id: chat-summary
    content: Deliver soak summary in chat (metrics, anomalies, zero code/doc commits unless user asks separately).
    status: pending
isProject: false
---

# Amended plan: testing only (no code changes)

## Agent choice

| Agent | Fit for this task |
|--------|-------------------|
| **shell** (recommended) | Best default. The session becomes a **command pipeline**: `platformio run`, upload to COM10, serial capture, periodic greps, optional `git` snapshot. No codebase edits; minimal file reads unless a command fails. |
| **generalPurpose** | Use if the **shell** run hits errors you need to **interpret** (upload denied, link errors, ambiguous serial output) while still **not** modifying source—e.g. correlating log lines with existing decision logs. |
| **explore** | Optional **read-only** pre-read of [`platformio.ini`](platformio.ini) / upload rule if you want the agent to confirm env name and `COM10` without running anything yet. |

**Not ideal here:** **best-of-n-runner** (parallel code experiments), **cursor-guide** (product docs).

Plan the work assuming a **shell**-style execution list: ordered commands, Windows/PowerShell notes from [`.cursor/rules/platformio-upload.mdc`](.cursor/rules/platformio-upload.mdc), and explicit “stop if build fails” gates.

## Scope change vs [`.cursor/plans/debug_chat_optimization_dcbd6066.plan.md`](.cursor/plans/debug_chat_optimization_dcbd6066.plan.md)

- **Removed:** incremental code optimizations, `platformio.ini` tunables, instrumentation gates, any edits to [`SerialService`](src/examples/serial/SerialService.cpp), [`WeightForwarderService`](src/examples/weightforwarder/WeightForwarderService.cpp), [`WebSocketTxRx`](lib/framework/WebSocketTxRx.h), [`main.cpp`](src/main.cpp), or related UI/types.
- **In scope:** prove behavior of **whatever is already at `HEAD` + working tree** for `esp32wroom32d`—build, flash, soak, observe.
- **Documentation:** Original plan required appending to three draft decision logs. Under **testing-only**, treat that as **out of scope** unless you explicitly want markdown updates later (those are still file edits, not “code,” but they expand scope).

## Critical path (unchanged understanding, read-only)

Scale → Serial → Weight forwarder → LAN WebSocket client; loop order in `main.cpp` remains the **behavior under test**, not something to change.

```mermaid
flowchart LR
  scale[Scale_UART]
  serialSvc[SerialService]
  wfLoop[WeightForwarderService_loop]
  wsClient[WebSocketsClient_LAN]
  scale --> serialSvc
  serialSvc --> wfLoop
  wfLoop --> wsClient
```

## Execution sequence (for shell agent)

1. **Snapshot (readonly):** `git status`, `git rev-parse HEAD` (and note dirty files only for your own record—no fixes).
2. **Free COM10:** Close Serial Monitor / other handles; on Windows, stop stuck Python processes per upload rule if needed.
3. **Build:** `python -m platformio run -e esp32wroom32d` — **no** follow-up “fix compile” unless you later open a separate coding task.
4. **Upload:** `python -m platformio run -e esp32wroom32d -t upload` to **COM10** (per env in [`platformio.ini`](platformio.ini)).
5. **Smoke (10–15 min):** scale + WS peer; confirm basic forward before long soak.
6. **Supervised soak (~2h):** Serial0 log to file; peer message log; periodic greps for failure tokens (`ws_tx_FAIL`, `abort`, `Guru`, `WDT`, etc.) as in the original plan’s checklist; spot-check REST/UI if reachable.
7. **Report:** Summarize in chat (counts, disconnects, worst-case log lines, min heap if present in logs)—**no** required repo edits.

## Rollback / safety

- If the **binary** on device is wrong or bad: re-flash a **known-good artifact** (your backup firmware or prior CI build)—still testing/deployment, not source edits.
- Duplicate `src\` vs `src/` paths in the working tree: **do not “normalize” files** in this task; only avoid editing until a separate cleanup task exists.

## When to switch agents

- Stays **shell** while commands succeed and logs are mechanical.
- Escalate to **generalPurpose** if you need someone to read multiple repo files and cross-check behavior **without** applying patches.
