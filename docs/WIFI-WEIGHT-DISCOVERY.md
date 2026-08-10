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
| `id` | Stable board id (chip id hex) — **use this to confirm you found the right board**, it survives DHCP moves |
| `host` | WiFi hostname — also reachable as `esp8266-relayboard.local` |
| `ip` | IPv4 to push weight to. **Changes over time** (example above is illustrative) — re-read it from the announce rather than hardcoding, and never trust a remembered IP: another device can take it, answer ping, and serve nothing |
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

**Windows firewall:** If the PC hear script gets nothing on UDP **4210**, check Windows Defender Firewall (or third-party firewall) for an inbound **UDP 4210** allow rule for Python / the script.

**Telling firewall from AP broadcast filtering apart** — they look identical from the script, but the fix is different:

1. Run with `--rest`. That makes the board send a **unicast** poke straight back to this PC, on top of the broadcast.
2. If the REST line prints `unicast_ok=True` but **nothing is heard**, the packet reached your machine and was dropped locally → **firewall on the PC**.
3. If broadcast is silent but unicast arrives → **AP broadcast filtering / client isolation** → use `--rest` or manual IP on the sender.

Confirmed on the dev PC 2026-08-10: WiFi adapter classified **Public**, Public profile **on**, no inbound rule for `python.exe` or port 4210 → both broadcast and unicast silent while the board reported `udp_ready / last_send_ok / unicast_ok` all true.

Opening the port is a **security setting and the machine owner's call** — it is not done by any script in this repo. On that PC, as administrator:

```powershell
New-NetFirewallRule -DisplayName "Weighsoft LW announce (UDP 4210)" -Direction Inbound -Protocol UDP -LocalPort 4210 -Action Allow -Profile Private
```

Prefer marking the workshop WiFi **Private** over allowing the rule on **Public** — a Public profile rule opens the port on untrusted networks such as customer sites.

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

**REST** — `POST /rest/liveWeight` takes **no credentials**:

```http
POST /rest/liveWeight
Content-Type: application/json

{ "weight": "1.50", "last_line": "SCALE,1.50" }
```

> Corrected 2026-08-10. This section previously said the endpoint was
> authenticated. It is not — verified against the running board, which accepts
> the POST with no `Authorization` header. That matches the locked product
> decision that **any LAN device may send weight** (RT-050): a sender should not
> need to hold admin credentials just to report a number. The consequence is
> real and worth stating plainly: **anyone on the same network can set the
> displayed weight.** Config endpoints stay admin-only, so a stranger can move
> the number but cannot change relay maps, the product catalog, or the printer.
> On a trusted workshop LAN that is the intended trade-off. It is not suitable
> for an untrusted network without putting the board behind its own VLAN.

**WebSocket:** connect to `/ws/liveWeight` and send the same JSON fields (framework WebSocketTxRx protocol).

Do **not** implement board-pulls-from-sender-IP.

## Reference sender

`scripts/send-weight.py` is a working sender for **any** device — Pi indicator,
ESP bridge, or PC tool. Copy it or port its ~40 lines of real logic; the
contract is what matters, not the language.

```bash
# one reading
python scripts/send-weight.py --weight 12.34

# a real scale, piped in - one reading per line, last number on the line wins
cat /dev/ttyUSB0 | python scripts/send-weight.py --stdin

# skip discovery when you already know where the board is
python scripts/send-weight.py --host 192.168.2.55 --weight 12.34
```

It finds the board by name, falls back to the UDP announce, and **checks the
board id before sending anything**. Exit codes: `0` sent · `2` board not found ·
`3` something answered but it is not our board · `4` send failed.

### Two behaviours worth copying into your own sender

**Never drop the last reading.** Rate limiting protects the ESP8266, but a naive
limiter discards the newest value — and on a scale the settled reading is the
only one that matters. `send-weight.py` *coalesces* instead: it holds the most
recent reading, sends it when the interval passes, and always flushes whatever
is pending at end of stream.

**Confirm identity, not reachability.** A stale IP can be answered by a totally
different device that pings fine and serves nothing. Check `id` from
`/rest/liveWeightDiscovery` before trusting an address.

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
