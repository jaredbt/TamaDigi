"""Write every image embedded in a .glb to <out_dir>/<image name>.<ext>, including
ones no material references (Blender's importer skips those)."""
import json
import os
import struct
import sys

src, out = sys.argv[1], sys.argv[2]
b = open(src, 'rb').read()
jl = struct.unpack('<I', b[12:16])[0]
j = json.loads(b[20:20 + jl])
bin_start = 20 + jl + 8
os.makedirs(out, exist_ok=True)
for im in j.get('images', []):
    bv = j['bufferViews'][im['bufferView']]
    data = b[bin_start + bv.get('byteOffset', 0): bin_start + bv.get('byteOffset', 0) + bv['byteLength']]
    ext = 'png' if im.get('mimeType') == 'image/png' else 'jpg' if 'jpeg' in im.get('mimeType', '') else 'bin'
    with open(os.path.join(out, f"{im['name']}.{ext}"), 'wb') as f:
        f.write(data)
print(len(j.get('images', [])), 'images ->', out)
