#!/usr/bin/env python3
"""Small ISO9660 disc and mixed data/audio BIN and WAV CUE fixtures."""
import math
import pathlib
import struct
import sys
import wave

target = pathlib.Path(sys.argv[1])
target.mkdir(parents=True, exist_ok=True)
def both16(n): return struct.pack('<H', n) + struct.pack('>H', n)
def both32(n): return struct.pack('<I', n) + struct.pack('>I', n)
def record(name, sector, size, flags=0):
    out = bytearray(33 + len(name) + (not len(name) % 2))
    out[0] = len(out)
    out[2:10] = both32(sector)
    out[10:18] = both32(size)
    out[18:25] = bytes([126, 10, 3, 12, 0, 0, 0])
    out[25] = flags
    out[28:32] = both16(1)
    out[32] = len(name)
    out[33:33+len(name)] = name
    return out

iso = bytearray(32 * 2048)
pvd = bytearray(2048)
pvd[:7] = b'\x01CD001\x01'
pvd[8:40] = b'DOSBOX-X'.ljust(32, b' ')
pvd[40:72] = b'OPENSTEP_TEST'.ljust(32, b' ')
pvd[80:88] = both32(32)
pvd[120:124] = both16(1)
pvd[124:128] = both16(1)
pvd[128:132] = both16(2048)
pvd[132:140] = both32(10)
pvd[140:144] = struct.pack('<I', 18)
pvd[148:152] = struct.pack('>I', 19)
pvd[156:190] = record(b'\0', 20, 2048, 2)
pvd[881] = 1
iso[16*2048:17*2048] = pvd
iso[17*2048:17*2048+7] = b'\xffCD001\x01'
iso[18*2048:18*2048+10] = b'\x01\0' + struct.pack('<IH', 20, 1) + b'\0\0'
iso[19*2048:19*2048+10] = b'\x01\0' + struct.pack('>IH', 20, 1) + b'\0\0'
contents = b'OPENSTEP ISO9660 FILE\r\n'
directory = record(b'\0', 20, 2048, 2) + record(b'\1', 20, 2048, 2) + record(b'README.TXT;1', 21, len(contents))
iso[20*2048:20*2048+len(directory)] = directory
iso[21*2048:21*2048+len(contents)] = contents
(target/'fixture.iso').write_bytes(iso)
pcm = b''.join(struct.pack('<hh', int(8000*math.sin(i*2*math.pi*440/44100)),
                          int(6000*math.sin(i*2*math.pi*660/44100))) for i in range(88200))
with wave.open(str(target/'fixture.wav'), 'wb') as out:
    out.setparams((2, 2, 44100, 0, 'NONE', 'not compressed'))
    out.writeframes(pcm)
raw = bytearray()
for sector in range(32):
    minute, rest = divmod(sector + 150, 75 * 60)
    second, frame = divmod(rest, 75)
    bcd = lambda n: (n // 10) * 16 + n % 10
    raw += b'\0' + b'\xff'*10 + b'\0' + bytes([bcd(minute), bcd(second), bcd(frame), 1])
    raw += iso[sector*2048:(sector+1)*2048] + bytes(288)
(target/'fixture.bin').write_bytes(raw + pcm)
(target/'binary.cue').write_text('FILE "fixture.bin" BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00\nTRACK 02 AUDIO\nINDEX 01 00:00:32\n')
(target/'wave.cue').write_text('FILE "fixture.iso" BINARY\nTRACK 01 MODE1/2048\nINDEX 01 00:00:00\nFILE "fixture.wav" WAVE\nTRACK 02 AUDIO\nINDEX 01 00:00:00\n')
