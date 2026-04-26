"""Patch Node-RED flows for serialReader signIn (POST body) and redeploy via Admin API."""
from __future__ import annotations

import json
import sys
import urllib.error
import urllib.request

BASE = "http://127.0.0.1:3000"
API_V2 = {"Node-RED-API-Version": "v2"}


def main() -> int:
    etag_req = urllib.request.Request(f"{BASE}/flows", method="GET", headers=API_V2)
    etag_resp = urllib.request.urlopen(etag_req, timeout=30)
    etag = etag_resp.headers.get("ETag", "").strip()
    raw = etag_resp.read().decode("utf-8")
    doc = json.loads(raw)
    if isinstance(doc, dict) and "flows" in doc:
        flows = doc["flows"]
        rev = doc.get("rev")
    elif isinstance(doc, list):
        flows = doc
        rev = None
    else:
        print("Unexpected /flows shape", file=sys.stderr)
        return 2
    if not isinstance(flows, list):
        print("flows is not a list", file=sys.stderr)
        return 2

    changed = False
    for n in flows:
        if n.get("id") == "inj_signin":
            n["payload"] = {"username": "admin", "password": "admin"}
            n["payloadType"] = "json"
            n["props"] = [{"p": "payload"}]
            changed = True
        if n.get("id") == "http_signin":
            n["paytoqs"] = "body"
            n["method"] = "POST"
            n["ret"] = "obj"
            changed = True
        if n.get("id") == "tab_serialreader":
            n["info"] = (
                "1) Double-click **SignIn to reader** and set reader URL if IP changed.\n"
                "2) Edit inject JSON credentials: **payload** username/password (default admin/admin).\n"
                "3) **Deploy**, click **1 signIn get token**.\n"
                "4) Copy `ws://...` from **debug FULL ws URL**, open **websocket-client** (under reader /ws/serial), paste Path, **Deploy**.\n"
                "5) Watch Debug. **paytoqs=body** sends POST JSON (fixes empty body / JSON parse error).\n"
                "Reader must allow HTTP from this machine (same LAN)."
            )
            changed = True

    if not changed:
        print("No serialReader nodes patched (missing ids?)", file=sys.stderr)
        return 3

    out_doc: dict = {"flows": flows}
    if rev is not None:
        out_doc["rev"] = rev
    body = json.dumps(out_doc).encode("utf-8")
    post = urllib.request.Request(
        f"{BASE}/flows",
        data=body,
        method="POST",
        headers={
            "Content-Type": "application/json",
            "Node-RED-Deployment-Type": "full",
            **API_V2,
            **({"If-Match": etag} if etag else {}),
        },
    )
    try:
        out = urllib.request.urlopen(post, timeout=60)
        print("POST /flows:", out.status, out.read().decode("utf-8", errors="replace")[:500])
    except urllib.error.HTTPError as e:
        print("Deploy failed:", e.code, e.read().decode("utf-8", errors="replace"), file=sys.stderr)
        return e.code

    # Smoke test signIn from this machine (reader may be unreachable in CI)
    test_body = json.dumps({"username": "admin", "password": "admin"}).encode("utf-8")
    t_req = urllib.request.Request(
        "http://10.45.71.5/rest/signIn",
        data=test_body,
        method="POST",
        headers={"Content-Type": "application/json"},
    )
    try:
        tr = urllib.request.urlopen(t_req, timeout=5)
        print("Reader signIn smoke:", tr.status, tr.read().decode("utf-8", errors="replace")[:200])
    except Exception as ex:
        print("Reader signIn smoke skipped/failed:", ex)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
