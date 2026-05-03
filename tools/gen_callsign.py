#!/usr/bin/env python3
"""Generate my-callsigns.txt for the Flipper SD card.

Usage:
    python3 gen_callsign.py LU3ARN
    python3 gen_callsign.py LU3ARN-14
"""

import sys


def aprs_passcode(call):
    call = call.split("-")[0].upper()
    h = 0x73E2
    for i in range(0, len(call), 2):
        h ^= ord(call[i]) << 8
        if i + 1 < len(call):
            h ^= ord(call[i + 1])
    return h & 0x7FFF


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} CALLSIGN[-SSID]")
        sys.exit(1)

    call = sys.argv[1].upper()
    passcode = aprs_passcode(call)

    print(f"Callsign: {call}")
    print(f"Passcode: {passcode}")
    print(f"\nCopy this file to /ext/ham/my-callsigns.txt on the Flipper SD card:")
    print(f"\n  {call},{passcode}")
