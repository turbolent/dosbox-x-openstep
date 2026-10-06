"""Run locally or natively: python3 openstep/tests/package_test.py."""
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.check_app import check_app, FRAMEWORKS


def executable(names, flags=1, command=12):
    commands = []
    for name in names:
        raw = name.encode() + b'\0'
        raw += b'\0' * (-len(raw) % 4)
        commands.append(struct.pack('<6I', command, 24 + len(raw), 24, 0, 0, 0) + raw)
    payload = b''.join(commands)
    return struct.pack('<7I', 0xfeedface, 7, 3, 2, len(commands), len(payload), flags) + payload


good = executable(sorted(FRAMEWORKS))
assert set(check_app(good)) == FRAMEWORKS
for bad in (
    executable(sorted(FRAMEWORKS) + ['/usr/local/lib/libiconv.dylib']),
    executable(sorted(FRAMEWORKS), flags=0),
    executable(sorted(FRAMEWORKS), command=0x80000018),
    executable([]), good[:24], good[:-1],
    good[:32] + struct.pack('<I', 0) + good[36:],
):
    try:
        check_app(bad)
    except ValueError:
        pass
    else:
        raise AssertionError('Invalid or non-standalone executable accepted')
print('Standalone package audit: system-only dependencies accepted; external/invalid binaries rejected')
