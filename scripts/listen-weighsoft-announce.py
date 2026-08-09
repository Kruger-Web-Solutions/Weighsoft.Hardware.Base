#!/usr/bin/env python3
"""Desk harness: listen for Weighsoft Live Weight board UDP announces (port 4210).

Usage:
  python scripts/listen-weighsoft-announce.py
  python scripts/listen-weighsoft-announce.py --seconds 20

Manual IP fallback (no announce needed): point your sender at the board STA IP
and POST {weight, last_line} to http://IP/rest/liveWeight (authenticated).

See docs/WIFI-WEIGHT-DISCOVERY.md
"""

from __future__ import annotations

import argparse
import json
import socket
import sys
import time

PORT = 4210
SVC = "weighsoft-lw"


def main() -> int:
    parser = argparse.ArgumentParser(description="Listen for weighsoft-lw UDP announces")
    parser.add_argument("--seconds", type=float, default=0, help="Stop after N seconds (0 = until Ctrl+C)")
    parser.add_argument("--port", type=int, default=PORT)
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    except OSError:
        pass
    sock.bind(("", args.port))
    sock.settimeout(1.0)

    print(f"Listening for {SVC} on UDP :{args.port} (Ctrl+C to stop)", flush=True)
    seen = {}
    deadline = time.time() + args.seconds if args.seconds > 0 else None

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

    if not seen:
        print("No announce heard. Check board is on WiFi, same LAN, and firmware has discovery.", file=sys.stderr)
        return 1
    print(f"Boards seen: {len(seen)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
