# WiFi weight discovery (board announce)

**Product decision (locked):** Option 2 — the relay board **announces** itself on the LAN when it joins WiFi. Any sender on the network can **auto-adopt** the board. Senders may also use a **manual IP**. Weight is always **pushed TO** the board (REST/WebSocket). The board does not pull weight from a sender IP.

Sprint: `SPRINT-2026-08-09-HWB-C` · RT-039

## Who this is for

Authors of **any LAN sender**: ESP scale bridge, WOW Trade / Pi, PC tool, or future product. One open protocol — not tied to a single app.

## How senders find the board

### 1) Preferred — UDP announce (auto-adopt)

When STA has an IP, the board broadcasts a small JSON packet every **5 seconds** (and immediately on join / IP change).

| Field | Value |
|-------|--------|
| UDP port | **4210** |
| Destination | Subnet broadcast (fallback `255.255.255.255`) |
| Interval | 5000 ms |
| Service id | `weighsoft-lw` |

**Example payload** (one UDP datagram, UTF-8 JSON, typically &lt; 160 bytes):

```json
{
  "v": 1,
  "svc": "weighsoft-lw",
  "id": "0097cbc0",
  "host": "esp8266-e8db8497cbc0",
  "ip": "192.168.2.67",
  "http": 80,
  "rest": "/rest/liveWeight",
  "ws": "/ws/liveWeight"
}
```

| Field | Meaning |
|-------|---------|
| `v` | Protocol version (1) |
| `svc` | Service name — ignore packets where this ≠ `weighsoft-lw` |
| `id` | Stable-ish board id (chip id hex) |
| `host` | WiFi hostname |
| `ip` | IPv4 to push weight to |
| `http` | HTTP port (80) |
| `rest` | Path for `POST` weight |
| `ws` | WebSocket path for live weight |

**Sender algorithm (auto-adopt):**

1. Bind UDP listen on port **4210** (or use a raw socket that receives broadcasts on that port).
2. Parse JSON; require `v == 1` and `svc == "weighsoft-lw"`.
3. Adopt `ip` (and remember `rest` / `ws`). If several boards appear, pick by `id` / `host` or let the operator choose.
4. Push weight to `http://{ip}{rest}` (see below). Keep listening; refresh IP if announce changes.

Desk proof:

```text
python scripts/listen-weighsoft-announce.py --seconds 20
python scripts/listen-weighsoft-announce.py --rest http://BOARD_IP --seconds 15
```

`--rest` signs in, GETs `/rest/liveWeightDiscovery` (board identity + triggers broadcast/unicast poke), and still listens on UDP. If the AP filters broadcast, REST still returns `ip` / `host` for adopt; use **manual IP** on the sender.

### 2) Optional — mDNS

The board also registers:

- Service: **`_weighsoft-lw._tcp.local`**
- Port: **80**
- TXT: `path=/rest/liveWeight`, `ws=/ws/liveWeight`
- Host: WiFi hostname (e.g. `esp8266-….local`)

Browse with Avahi / `dns-sd` / OS mDNS APIs. UDP remains the lean primary path for ESP8266 heap.

### 3) REST identity (authenticated helper)

`GET /rest/liveWeightDiscovery` (same auth as other `/rest/*` APIs) returns board `ip`, `host`, `id`, UDP port, mDNS name, and push paths. Calling it also triggers a UDP broadcast and a **unicast** announce to the caller’s IP (useful when the AP filters broadcast).

This is a helper for senders/tools — not a second discovery server, and not “type sender IP on the board”.

### 4) Fallback — manual IP (sender-side only)

If auto-find fails (VLAN, broadcast filtered, etc.), the **sender** UI/config has a **manual IP** box. Point it at the board’s STA IP (shown on Tech → “How senders find me”, WiFi Status, or `GET /rest/liveWeightDiscovery`).

There is **no** “type sender IP here” discovery setting on the board.

## Push weight to the board

Data direction is unchanged.

**REST** (authenticated — same as the web UI; default factory admin user):

```http
POST /rest/liveWeight
Content-Type: application/json

{ "weight": "1.50", "last_line": "SCALE,1.50" }
```

**WebSocket:** connect to `/ws/liveWeight` and send the same JSON fields (framework WebSocketTxRx protocol).

Do **not** implement board-pulls-from-sender-IP.

## Security notes

- Announce is **open on the LAN** (presence + IP only). It does not include passwords.
- Weight push endpoints use the device **security manager** (login / JWT as for other `/rest/*` APIs).
- Prefer a trusted Page Home / private LAN.

## Heap / ESP8266 notes

- Small JSON payload, 5 s interval, no chatty loops.
- Discovery helper is not a second application stack on the board.
- Reference listener runs on a PC (or another host), not on the ESP8266 board.

## Related UI

**Live Weight → Tech** shows board IP, hostname, UDP port, mDNS service name, and a short “how senders find me” note. Manual IP is documented as sender-side only.
