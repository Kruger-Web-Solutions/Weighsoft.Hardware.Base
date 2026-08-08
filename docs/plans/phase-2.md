# Phase 2 plan — Operator UI

**Goal:** Tabs Live | Target & Relays | Product | Tech; hero dial; navy styling; types match P1 firmware.  
**Branch:** `feat/lw-p2-operator-ui`  
**Depends:** P1 merged into `RelayBoardEspBuildIn` ✓

## Tasks
1. Extend `types.ts` for range/relays/PLU/DI/printer/job fields — DONE
2. `LiveWeight.tsx` routes/tabs — DONE
3. Slim `LiveWeightScreen` (Live only) — DONE
4. New `LiveWeightTarget.tsx`, `LiveWeightProduct.tsx` — DONE
5. Rename Setup → Tech; hide RS-485 as working — DONE
6. `liveWeight.css` hero dial ≥480px — DONE
7. `npm run build` — DONE (compiled; unrelated led/wifi lint warnings remain)
