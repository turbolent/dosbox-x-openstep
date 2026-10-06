"""Reject native releases that need non-system shared libraries."""
from pathlib import Path
import struct
import sys

FRAMEWORKS = {
    '/NextLibrary/Frameworks/AppKit.framework/Versions/B/AppKit',
    '/NextLibrary/Frameworks/Foundation.framework/Versions/B/Foundation',
    '/NextLibrary/Frameworks/SoundKit.framework/Versions/A/SoundKit',
    '/NextLibrary/PrivateFrameworks/Interceptor.framework/Versions/A/Interceptor',
    '/NextLibrary/Frameworks/System.framework/Versions/A/System',
}


def check_app(data):
    if len(data) < 28:
        raise ValueError('Truncated Mach-O header')
    magic, cpu, subtype, kind, count, size, flags = struct.unpack_from('<7I', data)
    if (magic, cpu, subtype, kind) != (0xfeedface, 7, 3, 2) or not flags & 1:
        raise ValueError('Expected a generic i386 executable with no undefined symbols')
    offset, end = 28, 28 + size
    if end > len(data):
        raise ValueError('Truncated Mach-O load commands')
    dependencies = []
    for _ in range(count):
        if offset + 8 > end:
            raise ValueError('Missing Mach-O load command')
        command, length = struct.unpack_from('<2I', data, offset)
        if length < 8 or offset + length > end:
            raise ValueError('Invalid Mach-O load command size')
        if command in (0x6, 0x80000018, 0x8000001f, 0x80000023):
            raise ValueError('Unsupported shared-library load command')
        if command == 12:  # LC_LOAD_DYLIB
            if length < 24:
                raise ValueError('Truncated shared-library command')
            name = struct.unpack_from('<I', data, offset + 8)[0]
            if not 24 <= name < length:
                raise ValueError('Invalid shared-library name offset')
            raw = data[offset + name:offset + length]
            if b'\0' not in raw:
                raise ValueError('Unterminated shared-library name')
            dependencies.append(raw.split(b'\0')[0].decode('ascii'))
        offset += length
    if offset != end or set(dependencies) != FRAMEWORKS:
        raise ValueError('Unexpected runtime dependencies: ' + repr(dependencies))
    return dependencies


if __name__ == '__main__':
    for dependency in check_app(Path(sys.argv[1]).read_bytes()):
        print(dependency)
