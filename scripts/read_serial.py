"""Quick serial reader for diagnosing boot/restart issues on a flashed ESP.

Usage: python scripts/read_serial.py COM31 [seconds]

Prints raw bytes from the port for the requested duration (default 12 s),
replacing non-UTF8 chunks rather than crashing on weird control bytes.
"""

import sys
import time
import serial

port = sys.argv[1] if len(sys.argv) > 1 else "COM31"
duration = float(sys.argv[2]) if len(sys.argv) > 2 else 12.0

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

print(f"[reader] opening {port} @ 115200 for {duration:.0f}s...", flush=True)
with serial.Serial(port, 115200, timeout=0.2) as s:
    deadline = time.time() + duration
    while time.time() < deadline:
        chunk = s.read(512)
        if not chunk:
            continue
        try:
            sys.stdout.write(chunk.decode("utf-8", errors="replace"))
        except Exception as e:
            sys.stdout.write(f"<<decode error: {e}>>")
        sys.stdout.flush()
print("\n[reader] done", flush=True)
