# Weighsoft.Hardware.Base — open questions

**Repo:** `Weighsoft.Hardware.Base`  
**Branch:** `RelayBoardEspBuildIn`  
**Updated:** 2026-08-09  
**Living list:** PC `rhynoTodoList` items with `kind: question` / `Q:` prefix  
**Sprint:** `SPRINT-2026-08-09-HWB-E`

Durable store so product/tech questions are not lost in chat. Status: `open` | `answered`.

## Open

| Id | Question | Asked | Status | Notes |
|----|----------|-------|--------|-------|
| Q-001 / RT-047 | Does Windows firewall block UDP **4210** for `scripts/listen-weighsoft-announce.py` on the desk PC? | 2026-08-09 | open | Field risk for discovery proof; allow inbound UDP 4210 if listen hears nothing |
| Q-002 / RT-048 | Which **first real sender** product should we integrate next (ESP bridge, WOW Trade / Pi, PC tool, other)? | 2026-08-09 | open | Sender = any LAN device; pick priority product after field tests |

## Answered

| Id | Question | Asked | Answered | Answer |
|----|----------|-------|----------|--------|
| Q-003 / RT-049 | Discovery shape — Option 1 vs Option 2? | 2026-08-09 | 2026-08-09 | **Option 2** — board announces; senders auto-adopt + manual IP |
| Q-004 / RT-050 | Who may send weight to the board? | 2026-08-09 | 2026-08-09 | **Any network / LAN device** (not one fixed product) |
| Q-005 / RT-051 | Is count per PLU or job-wide? | 2026-08-09 | 2026-08-09 | **Job-wide** shared count until **RT-040** (later, not approved) |
| Q-006 / RT-052 | Where is printer IP/port configured? | 2026-08-09 | 2026-08-09 | **Target & Relays** (not Tech); Tech may jump/link only |
| Q-007 / RT-053 | When do we flash again? | 2026-08-09 | 2026-08-09 | **After software work on the list is cleared** — then Jurien tests |
| Q-008 / RT-060 | Make **Live Weight** the post-login home (not Twin) to save heap? | 2026-08-09 | 2026-08-09 | **Yes** — Live Weight home + public operator (weigh/PLU/tx/DI-DO); settings need login. Sprint E. |

## How to update

1. Add/change rows here with date + status.  
2. Mirror open/answered on PC `list.yaml` (`kind: question`, title `Q: …`).  
3. When answered: set status, fill Answer, move list item to `complete` with `how_done`.  
4. Do not leave decisions only in chat.
