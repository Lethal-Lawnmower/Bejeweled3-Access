"""List or extract files from a PopCap .pak archive (XOR 0xF7 obfuscation).

Usage:
  python -I pak_list.py main.pak                 # list entries
  python -I pak_list.py main.pak OUTDIR [regex]  # extract matching entries
Extracted game content is proprietary: keep it under private/ (git-ignored).
"""
import os
import re
import struct
import sys


def read_index(data):
    pos = 8  # magic + version
    entries = []
    while True:
        flags = data[pos]; pos += 1
        if flags & 0x80:
            break
        nlen = data[pos]; pos += 1
        name = data[pos:pos + nlen].decode("latin-1"); pos += nlen
        size = struct.unpack_from("<I", data, pos)[0]; pos += 4 + 8
        entries.append((name, size))
    return pos, entries


def main():
    raw = open(sys.argv[1], "rb").read()
    data = bytes(b ^ 0xF7 for b in raw) if raw[:4] != b"\xc0\x4a\xc0\xba" else raw
    assert data[:4] == b"\xc0\x4a\xc0\xba", "not a PopCap pak"
    pos, entries = read_index(data)
    if len(sys.argv) < 3:
        for name, size in entries:
            print(f"{size:10d} {name}")
        return
    out = sys.argv[2]
    pat = re.compile(sys.argv[3], re.I) if len(sys.argv) > 3 else None
    for name, size in entries:
        if pat is None or pat.search(name):
            path = os.path.join(out, name.replace("\\", os.sep))
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "wb") as f:
                f.write(data[pos:pos + size])
        pos += size


main()
