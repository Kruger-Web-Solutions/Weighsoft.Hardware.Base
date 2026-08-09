#!/usr/bin/env python3
"""Desk harness: listen for Weighsoft Live Weight board UDP announces (port 4210).

Usage:
  python scripts/listen-weighsoft-announce.py
  python scripts/listen-weighsoft-announce.py --seconds 20
  python scripts/listen-weighsoft-announce.py --rest http://192.168.2.67 --seconds 15

--rest signs in (admin/admin by default), GETs /rest/liveWeightDiscovery (triggers
broadcast + unicast poke to this PC), and still listens on UDP.

Manual IP fallback: point your sender at the board STA IP and POST
{weight, last_line} to http://IP/rest/liveWeight (authenticated).

See docs/WIFI-WEIGHT-DISCOVERY.md
"""

from __future__ import annotations

import argparse
import json
import socket
import sys
import threading
import time
import urllib.error
import urllib.request

PORT = 4210
SVC = "weighsoft-lw"


def sign_in(base: str, user: str, password: str) -> str:
    req = urllib.request.Request(
        base.rstrip("/") + "/rest/signIn",
        data=json.dumps({"username": user, "password": password}).encode(),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=10) as resp:
        return json.load(resp)["access_token"]


def probe_rest(base: str, token: str) -> dict:
    req = urllib.request.Request(
        base.rstrip("/") + "/rest/liveWeightDiscovery",
        headers={"Authorization": "Bearer " + token},
        method="GET",
    )
    with urllib.request.urlopen(req, timeout=10) as resp:
        return json.load(resp)


def main() -> int:
    parser = argparse.ArgumentParser(description="Listen for weighsoft-lw UDP announces")
    parser.add_argument("--seconds", type=float, default=0, help="Stop after N seconds (0 = until Ctrl+C)")
    parser.add_argument("--port", type=int, default=PORT)
    parser.add_argument("--bind", default="", help="Optional local IP to bind (e.g. 192.168.2.56)")
    parser.add_argument("--rest", default="", help="Board base URL to GET /rest/liveWeightDiscovery")
    parser.add_argument("--user", default="admin")
    parser.add_argument("--password", default="admin")
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    except OSError:
        pass
    sock.bind((args.bind, args.port))
    sock.settimeout(0.5)

    print(f"Listening for {SVC} on UDP :{args.port} bind={args.bind or '0.0.0.0'} (Ctrl+C to stop)", flush=True)
    seen = {}
    rest_info = None
    deadline = time.time() + args.seconds if args.seconds > 0 else None

    def do_rest_probe() -> None:
        nonlocal rest_info
        try:
            time.sleep(0.3)
            tok = sign_in(args.rest, args.user, args.password)
            rest_info = probe_rest(args.rest, tok)
            print(
                f"[REST] id={rest_info.get('id')} host={rest_info.get('host')} ip={rest_info.get('ip')} "
                f"udp_ready={rest_info.get('udp_ready')} last_send_ok={rest_info.get('last_send_ok')} "
                f"unicast_ok={rest_info.get('unicast_to_client_ok')}",
                flush=True,
            )
        except Exception as exc:  # noqa: BLE001 — desk harness
            print(f"[REST] probe failed: {exc}", file=sys.stderr, flush=True)

    if args.rest:
        threading.Thread(target=do_rest_probe, daemon=True).start()

    try:
        while True:
            if deadline is not None and time.time() >= deadline:
                break
            try:
                data, addr = sock.recvfrom(512)
            except socket.timeout:
                continue
            text = data.decode("utf-8", errors="replace").strip()
            try:
                payload = json.loads(text)
            except json.JSONDecodeError:
                print(f"from {addr[0]}: non-JSON {text[:80]!r}", flush=True)
                continue
            if payload.get("svc") != SVC or int(payload.get("v", 0)) != 1:
                continue
            board_id = payload.get("id") or "?"
            ip = payload.get("ip") or addr[0]
            host = payload.get("host") or ""
            rest = payload.get("rest") or "/rest/liveWeight"
            key = board_id
            first = key not in seen
            seen[key] = payload
            tag = "ADOPT" if first else "refresh"
            print(
                f"[{tag}] id={board_id} host={host} ip={ip} "
                f"POST http://{ip}{rest}  (from {addr[0]})",
                flush=True,
            )
            if first:
                print(
                    "  Manual IP fallback: configure this IP on the sender if auto-find is skipped.",
                    flush=True,
                )
    except KeyboardInterrupt:
        print("\nStopped.", flush=True)

    if seen:
        print(f"Boards seen via UDP: {len(seen)}")
        return 0
    if rest_info and rest_info.get("ip"):
        print(
            "No UDP heard (AP may filter broadcast). REST identity OK — use manual IP / REST adopt:",
            rest_info.get("ip"),
        )
        return 0
    print("No announce heard. Check board is on WiFi, same LAN, and firmware has discovery.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
