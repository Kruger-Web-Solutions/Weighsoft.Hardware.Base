# Node-RED: serialReader WebSocket monitor (debug)

This documents the **serialReader WS monitor** tab used to compare **laptop → reader** HTTP/WebSocket behaviour with the **ESP serialWriter forwarder** (independent clients).

## What was broken (symptoms)

- **http request** node: `JSON parse error`
- **function** node: `No access_token in response: ""`

**Cause:** The inject node sent **`payloadType: date`** (timestamp), not credentials. The **http request** node had **`paytoqs: ignore`**, so **no POST body** was sent to `/rest/signIn`. The reader responds with **HTTP 400** and an **empty or non-JSON** body; Node-RED then fails when **Return** is set to a parsed JSON object.

## Fix applied (live Node-RED on this machine)

1. **`inj_signin`:** `payload` is now a JSON object `{"username":"admin","password":"admin"}` with **`payloadType: json`** (edit password to match your reader).
2. **`http_signin`:** **`paytoqs` set to `body`** so `msg.payload` is sent as the **request body** (with `Content-Type: application/json` header already configured).
3. **Deploy** via Admin API **v2** (`POST /flows` with `{ flows, rev }` and `If-Match: ETag`).

## Re-deploy from this repository

The patch script lives in the Weighsoft repo (it does **not** live inside the NutSync Node-RED project folder):

```powershell
cd C:\Projects\Weighsoft.Hardware.Base
python scripts/patch_nodered_serialreader_flows.py
```

Requirements:

- Node-RED reachable at **`http://127.0.0.1:3000`** (change `BASE` in the script if your port differs).
- No admin auth on the Admin API (default local install). If you enable HTTP Node auth for the editor, extend the script with credentials.

The script also runs an optional **smoke test** `POST http://10.45.71.5/rest/signIn` with `admin`/`admin` to confirm the reader is reachable from the laptop.

## Manual flow test (UI)

1. Open **http://localhost:3000** (or your editor URL).
2. Open tab **serialReader WS monitor**.
3. Click **Deploy** if you changed credentials.
4. Click **`1 signIn get token`**.
5. In **Debug**, confirm **`SIGNIN_OK`** and copy the **`ws://...access_token=...`** line from **debug FULL ws URL**.
6. Double-click the **websocket-client** config used by **reader /ws/serial**, paste the full URL into **Path**, **Deploy**.
7. When the reader streams JSON, **debug frame summary** and **debug raw payload** should show traffic.

## Interpreting results vs ESP writer

| Observation | Meaning |
|-------------|---------|
| Node-RED **signIn 200** but ESP **`http_code=-1`** | Writer-side WiFi/lwIP or routing; laptop path is fine. |
| Both **fail** signIn | Reader down, wrong IP, or wrong password / `FT_SECURITY` mismatch. |
| Node-RED WS **drops on same cadence** as ESP (~8 s idle) | Likely **reader** idle policy or **AP** behaviour — inspect reader logs and WebSocket close codes (see [SERIAL-WRITER-EXAMPLE.md](./SERIAL-WRITER-EXAMPLE.md) periodic disconnect section). |
| Node-RED WS **stable**, ESP **drops** | Focus on **writer** (dual client, soft-AP, `WiFi.reconnect` path). |

## Next steps (debugging plan)

For a full **ordered** checklist (writer UI, NR, PC `wscat`, log table, close-code interpretation, **serialReader** handoff), use **[SERIAL-FORWARDER-DISCONNECT-RUNBOOK.md](./SERIAL-FORWARDER-DISCONNECT-RUNBOOK.md)**.

1. **Baseline:** With Node-RED connected to `/ws/serial`, leave the tab running and log **timestamps** of any gap in **debug frame summary**; correlate with ESP writer UART `[ws-event] DISCONNECTED` times.
2. **Reader UART:** Enable logging on the **serialReader** device for `/ws/serial` accept/close reasons (close code, heap, client count).
3. **Close payload:** Add short **hex dump** on `DISCONNECTED` in `SerialWriterForwarderService` (optional firmware follow-up) to map RFC 6455 codes.
4. **Writer UI:** Close all browser tabs to the **writer** while testing; note whether **`[WS] Client connected`** on the writer still appears (see SERIAL-WRITER-EXAMPLE).
5. **Credentials:** If you change the reader password, update the **inject** JSON in Node-RED and re-run the patch script or edit the flow manually.

## NutSync project note

Node-RED for this workspace ran from **`NutSyncNodeRed`** (`http://localhost:3000`). The **deployed runtime** was updated by the script above. To keep the NutSync git project in sync, **export flows** from the editor or copy `flows.json` from that project after verifying the tab, then commit inside the **NutSync** repository separately from Weighsoft.Hardware.Base.
