#!/usr/bin/env bash
# Serial data streamer for /dev/ttyUSB0 (Raspbian/Linux)
# Simulates scale output: WN0000.04kg (same format as stream-com12.ps1)
#
# Usage: ./stream-ttyUSB0.sh [port] [baud] [delay_ms]
#   port     default /dev/ttyUSB0
#   baud     default 9600
#   delay_ms default 500
#
# Requires: Python 3, pyserial (pip install pyserial)
# Make executable: chmod +x stream-ttyUSB0.sh
# Raspbian: ensure user is in dialout for serial access: sudo usermod -a -G dialout $USER

set -e

PORT="${1:-/dev/ttyUSB0}"
BAUD="${2:-9600}"
DELAY_MS="${3:-500}"

if ! python3 -c "import serial" 2>/dev/null; then
    echo "ERROR: pyserial is not installed."
    echo "Install with: pip3 install pyserial"
    echo "Or:          sudo apt install python3-serial"
    exit 1
fi

echo "=== Serial Streamer (Raspbian) ==="
echo "Port: $PORT at $BAUD baud (delay ${DELAY_MS}ms)"
echo ""

python3 - <<PY
import serial
import time
import sys
from datetime import datetime

port = "$PORT"
baud = int("$BAUD")
delay_sec = float("$DELAY_MS") / 1000.0

try:
    ser = serial.Serial(port, baud)
    print("Streaming data on {}...".format(port))
    print("")
    counter = 0
    while True:
        weight = 0.01 * (counter % 1000)
        weight_str = "{:.2f}".format(weight)
        padded = weight_str.zfill(7)
        line = "WN{}kg".format(padded)
        ser.write((line + "\r\n").encode("ascii"))
        print("[{}] {}".format(datetime.now().strftime("%H:%M:%S"), line))
        counter += 1
        time.sleep(delay_sec)
except serial.SerialException as e:
    print("ERROR: {}".format(e), file=sys.stderr)
    sys.exit(1)
except KeyboardInterrupt:
    pass
finally:
    if 'ser' in dir() and ser.is_open:
        ser.close()
PY
