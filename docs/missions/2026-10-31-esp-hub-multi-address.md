STATUS: IN-FLIGHT

# Ticket: ESP accepts a multi-address Hub pairing code (S5, 2026-10-31)

Parent mission: Weighsoft.Indicator.Edge docs/missions/2026-10-31-hub-pairing-all-addresses.md
(Ratchet section, S5). Branch claude/esp-hub-multi-address, cut from claude/project-thread-21cbr9.

## INTEL PACKET

schema: weighsoft-intel/2

QUESTION: What must change so hub_code_parse reads the Hub's new "hub_urls" list (LAN entries
only, old codes unchanged) and the box fails over between those addresses at runtime?

TERRAIN:
- lib/wsoft_core/src/hub_pairing.h:31 HubPairing has one hub_url; no list, no failover type.
- lib/wsoft_core/src/hub_pairing.cpp:94 hub_code_parse; reads hub_url at lib/wsoft_core/src/hub_pairing.cpp:111.
- lib/wsoft_core/src/hub_pairing.cpp:23 json_string reads plain string values only (no arrays).
- lib/wsoft_esp/src/hub.cpp:48 load_pairing copies hub_url into a String; heartbeat at lib/wsoft_esp/src/hub.cpp:215, clock at lib/wsoft_esp/src/hub.cpp:250.
- lib/wsoft_esp/src/hub.cpp:401 hub_address() returns the String; comment at lib/wsoft_esp/src/hub.cpp:394 says it never changes at runtime.
- lib/wsoft_esp/src/hub.cpp:422 hub_save_code puts a HubPairing on the web task stack.
- src/examples/wsoft/WSoftHubService.cpp:36 status JSON; src/examples/wsoft/WSoftHubService.cpp:57 a second HubPairing on the web task stack.
- interface/src/project/wsoft/WSoftHub.tsx:107 shows one "Hub address"; type at interface/src/project/wsoft/types.ts:42.
- test/test_hub_pairing/test_main.cpp:146 medic's red S5 tests (compile-red on the new API).

PATTERNS: fixed-size buffers only (no heap growth); parse never logs; secrets zeroed after use;
host-tested pure code in lib/wsoft_core, Arduino glue in lib/wsoft_esp; Tailscale / loopback
classification already exists in hub_url_hint (lib/wsoft_core/src/hub_pairing.cpp:157).

OWNING MODULE: lib/wsoft_core/src/hub_pairing.cpp:94 (hub_code_parse). Invariants: returns OK
only with an http(s) hub_url, key_id and a 32-byte secret; out untouched-or-cleared on failure.

PRIOR ART:
- lib/wsoft_core/src/hub_pairing.cpp:157 hub_url_hint (reuse to keep LAN entries only).
NONE FOUND for a list parser or failover after:
SEARCH: pattern="hub_failover|hub_urls" paths="lib,src,interface/src" hits=0

RISKS:
- Blast radius: lib/wsoft_esp/src/hub.cpp (heartbeat, clock, web getters),
  src/examples/wsoft/WSoftHubService.cpp (status + pair JSON), interface/src/project/wsoft/WSoftHub.tsx.
- HubPairing grows ~650 B; three copies on the async web task stack could overflow it.
- hub_address() becomes runtime-changing; a String reassigned under the web task would race.
- USB "HUB PAIR" calls load_pairing, which rewrites the live address while the hub task runs.

UNKNOWNS: async web task stack size on this core (not measured; mitigated by static buffers).

Write set: lib/wsoft_core/src/hub_pairing.h, lib/wsoft_core/src/hub_pairing.cpp,
lib/wsoft_esp/src/hub.h, lib/wsoft_esp/src/hub.cpp, src/examples/wsoft/WSoftHubService.cpp,
interface/src/project/wsoft/WSoftHub.tsx, interface/src/project/wsoft/types.ts, this file.

## SITREP (builder)
