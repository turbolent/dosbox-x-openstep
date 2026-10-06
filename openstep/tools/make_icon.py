"""Convert the upstream icon to the associated-alpha TIFF used by OPENSTEP.

Run locally with Pillow; the generated TIFF is included for native builds.
"""
from pathlib import Path
import struct
from PIL import Image

root = Path(__file__).resolve().parents[2]
target = root / 'openstep' / 'DOSBox-X.tiff'
with Image.open(root / 'contrib' / 'icons' / 'dosbox-x.png') as original:
    # Premultiply color channels before declaring associated alpha.
    picture = Image.frombytes('RGBA', original.size, original.convert('RGBa').tobytes())
    picture.save(target, compression='raw')

# Pillow's RGBA writer always declares unassociated alpha; fix that one tag.
data = bytearray(target.read_bytes())
endian = '<' if data[:2] == b'II' else '>'
directory = struct.unpack_from(endian + 'I', data, 4)[0]
count = struct.unpack_from(endian + 'H', data, directory)[0]
for index in range(count):
    offset = directory + 2 + 12 * index
    tag, kind, length = struct.unpack_from(endian + 'HHI', data, offset)
    if tag == 338:
        assert kind == 3 and length == 1
        struct.pack_into(endian + 'H', data, offset + 8, 1)
        break
else:
    raise RuntimeError('TIFF writer omitted the alpha tag')
target.write_bytes(data)
with Image.open(target) as result:
    assert result.tag_v2[338] == (1,)
