#!/usr/bin/env python3
"""Reference sender: push weight to a Weighsoft Live Weight relay board.

This is the sender half of docs/WIFI-WEIGHT-DISCOVERY.md. Any LAN device may
send: a Pi indicator, an ESP scale bridge, a PC tool. Copy this file or port the
~40 lines of real logic - the contract, not the language, is what matters.

Usage:
  # one reading
  python scripts/send-weight.py --weight 12.34

  # pipe a real scale in; one reading per line, last number on the line wins
  cat /dev/ttyUSB0 | python scripts/send-weight.py --stdin
  python read_my_scale.py | python scripts/send-weight.py --stdin

  # skip discovery when you already know where the board is
  python scripts/send-weight.py --host 192.168.2.55 --weight 12.34

  # see what would be sent without touching the board
  python scripts/send-weight.py --weight 12.34 --dry-run

How the board is found, in order:
  1. --host, if given.
  2. mDNS hostname esp8266-relayboard.local.
  3. UDP announce on :4210 (the board shouts every 5s).
Whatever is found is checked against /rest/liveWeightDiscovery before any weight
is sent. A stale IP can be answered by a DIFFERENT device that pings fine and
serves nothing, so the board id (last 6 of its MAC) is the thing to trust.

Exit codes: 0 sent (or dry-run) - 2 board not found - 3 wrong device - 4 send failed.
"""

from __future__ import annotations

import argparse
import json
import re
import socket
import sys
import time
import urllib.error
import urllib.request

UDP_PORT = 4210
SVC = "weighsoft-lw"
DEFAULT_HOSTNAME = "esp8266-relayboard.local"
NUMBER = re.compile(r"[-+]?\d+(?:\.\d+)?")


def _get_json(url: str, timeout: float = 8.0) -> dict:
    with urllib.request.urlopen(url, timeout=timeout) as resp:
        return json.load(resp)


def _post_json(url: str, payload: dict, timeout: float = 8.0) -> tuple[int, str]:
    req = urllib.request.Request(
        url,
        data=json.dumps(payload).encode(),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return resp.status, resp.read().decode("utf-8", "replace")
    except urllib.error.HTTPError as exc:
        return exc.code, exc.read().decode("utf-8", "replace")


def discover_via_udp(seconds: float) -> str | None:
    """Wait for the board's UDP announce. Returns its IP, or None."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    except OSError:
        pass
    try:
        sock.bind(("", UDP_PORT))
    except OSError as exc:
        print(f"cannot listen on UDP {UDP_PORT}: {exc}", file=sys.stderr)
        return None
    sock.settimeout(0.5)
    deadline = time.time() + seconds
    try:
        while time.time() < deadline:
            try:
                data, addr = sock.recvfrom(512)
            except socket.timeout:
                continue
            try:
                msg = json.loads(data.decode("utf-8", "replace"))
            except ValueError:
                continue
            if msg.get("svc") == SVC:
                return str(msg.get("ip") or addr[0])
    finally:
        sock.close()
    return None


NOT_FOUND = "not_found"
WRONG_DEVICE = "wrong_device"


def _identify(base: str, expect_id: str) -> tuple[dict | None, str | None]:
    """Ask a candidate who it is. Returns (discovery_json, failure_reason)."""
    try:
        info = _get_json(base.rstrip("/") + "/rest/liveWeightDiscovery")
    except Exception:
        return None, NOT_FOUND
    if expect_id and str(info.get("id")) != expect_id:
        print(
            f"WRONG DEVICE at {base}: id={info.get('id')} expected {expect_id}",
            file=sys.stderr,
        )
        return None, WRONG_DEVICE
    return info, None


def find_board(host: str, udp_seconds: float, expect_id: str):
    """Return (base_url, discovery_json, None), or (None, None, reason).

    Reason matters: "did not answer" and "answered but is not our board" need
    different fixes, and the exit code has to tell them apart.
    """
    candidate = host or DEFAULT_HOSTNAME
    base = candidate if candidate.startswith("http") else "http://" + candidate
    info, reason = _identify(base, expect_id)
    if info:
        return base, info, None
    if reason == WRONG_DEVICE or host:
        # A wrong device is an answer, not a miss - hunting further would hide it.
        # An explicit --host that stayed quiet is also final; the caller chose it.
        return None, None, reason

    print(f"{DEFAULT_HOSTNAME} did not answer, listening for the announce...", file=sys.stderr)
    ip = discover_via_udp(udp_seconds)
    if not ip:
        return None, None, NOT_FOUND
    base = "http://" + ip
    info, reason = _identify(base, expect_id)
    if info:
        return base, info, None
    return None, None, reason


def send_weight(base: str, weight: float, last_line: str, count: int | None) -> tuple[int, str]:
    payload: dict = {"weight": weight, "last_line": last_line}
    if count is not None:
        payload["count"] = count
    return _post_json(base.rstrip("/") + "/rest/liveWeight", payload)


def parse_weight(line: str) -> float | None:
    """Last number on the line. Scale protocols vary; this covers the common ones.

    'ST,GS,   12.34kg' -> 12.34      '+00012.34 kg' -> 12.34
    """
    matches = NUMBER.findall(line)
    if not matches:
        return None
    try:
        return float(matches[-1])
    except ValueError:
        return None


def main() -> int:
    ap = argparse.ArgumentParser(description="Push weight to a Weighsoft relay board")
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--weight", type=float, help="Send this single reading")
    src.add_argument("--stdin", action="store_true", help="Read readings from stdin, one per line")
    ap.add_argument("--host", default="", help="Board host or URL. Skips discovery.")
    ap.add_argument("--expect-id", default="97cbc0", help='Board id to require. "" disables the check.')
    ap.add_argument("--count", type=int, default=None, help="Optional piece count")
    ap.add_argument("--label", default="send-weight.py", help="Sent as last_line")
    ap.add_argument("--udp-seconds", type=float, default=8.0, help="How long to wait for the announce")
    ap.add_argument("--min-interval", type=float, default=0.2, help="Rate limit for --stdin (seconds)")
    ap.add_argument("--dry-run", action="store_true", help="Show what would be sent; contact nothing")
    args = ap.parse_args()

    if args.dry_run:
        readings = [args.weight] if args.weight is not None else []
        print(f"DRY RUN target={args.host or DEFAULT_HOSTNAME} expect_id={args.expect_id or '(any)'}")
        for w in readings:
            print(f"  would POST /rest/liveWeight weight={w} last_line={args.label!r} count={args.count}")
        if args.stdin:
            print("  would POST one reading per stdin line")
        return 0

    base, info, reason = find_board(args.host, args.udp_seconds, args.expect_id)
    if not info:
        target = args.host or DEFAULT_HOSTNAME
        if reason == WRONG_DEVICE:
            print(f"{target} is answering, but it is not our board.", file=sys.stderr)
            print("Something else holds that address. Find the board by name, or check --expect-id.", file=sys.stderr)
            return 3
        print(f"board not found (tried {target})", file=sys.stderr)
        print("Try: --host <ip>, or check the board is on this network.", file=sys.stderr)
        return 2
    print(f"board {info.get('id')} at {info.get('ip')} ({base})", flush=True)

    if args.weight is not None:
        status, body = send_weight(base, args.weight, args.label, args.count)
        ok = status == 200
        print(f"{'sent' if ok else 'FAILED'} weight={args.weight} -> HTTP {status}", flush=True)
        if not ok:
            print(body[:200], file=sys.stderr)
        return 0 if ok else 4

    # Rate limiting must never drop the LAST reading - on a scale that is the
    # settled weight, the only one anybody cares about. So coalesce instead of
    # discarding: hold the newest reading and flush it when the interval passes,
    # then flush whatever is still pending at end of stream.
    sent = coalesced = unparsed = failed = 0
    last_at = 0.0
    pending: tuple[float, str] | None = None

    def flush() -> None:
        nonlocal pending, sent, failed, last_at
        if pending is None:
            return
        weight, label = pending
        pending = None
        last_at = time.time()
        status, _ = send_weight(base, weight, label, args.count)
        if status == 200:
            sent += 1
        else:
            failed += 1
            print(f"send failed HTTP {status} for {weight}", file=sys.stderr, flush=True)

    for raw in sys.stdin:
        line = raw.strip().lstrip("﻿")  # tolerate a BOM from piped input
        if not line:
            continue
        weight = parse_weight(line)
        if weight is None:
            unparsed += 1
            continue
        if pending is not None:
            coalesced += 1  # superseded before it could be sent
        pending = (weight, line[:64])
        if time.time() - last_at >= args.min_interval:
            flush()
    flush()  # the settled reading always goes

    print(f"sent={sent} coalesced={coalesced} unparsed={unparsed} failed={failed}", flush=True)
    return 0 if failed == 0 else 4


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(0)
