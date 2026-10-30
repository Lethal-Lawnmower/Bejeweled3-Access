"""List MSVC RTTI vtables in a 32-bit PE: prints vtable address, class name, slot count.

Usage: python -I rtti_vtables.py Bejeweled3.exe [filter]
Reads the executable only; writes nothing.
"""
import struct
import sys


def load_sections(data):
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    optsize = struct.unpack_from("<H", data, pe + 20)[0]
    base = struct.unpack_from("<I", data, pe + 24 + 28)[0]
    secs = []
    off = pe + 24 + optsize
    for i in range(nsec):
        name = data[off:off + 8].rstrip(b"\0").decode()
        vsize, va, rsize, raw = struct.unpack_from("<IIII", data, off + 8)
        secs.append((name, base + va, max(vsize, rsize), raw, rsize))
        off += 40
    return base, secs


def main():
    data = open(sys.argv[1], "rb").read()
    filt = sys.argv[2] if len(sys.argv) > 2 else None
    base, secs = load_sections(data)

    def to_off(va):
        for name, sva, size, raw, rsize in secs:
            if sva <= va < sva + rsize:
                return raw + va - sva
        return None

    def u32(va):
        o = to_off(va)
        return None if o is None else struct.unpack_from("<I", data, o)[0]

    text = next(s for s in secs if s[0] == ".text")
    rdata = next(s for s in secs if s[0] == ".rdata")
    # Candidate COLs: dword sig 0, pTypeDescriptor pointing at ".?AV" name - 8
    results = []
    for name, sva, size, raw, rsize in [rdata]:
        for o in range(raw, raw + rsize - 4, 4):
            col = sva + o - raw
            sig, offs, cd, ptd, pch = struct.unpack_from("<IIIII", data, o)
            if sig != 0:
                continue
            tdo = to_off(ptd + 8) if ptd else None
            if tdo is None or data[tdo:tdo + 4] not in (b".?AV", b".?AU"):
                continue
            cname = data[tdo:data.index(b"\0", tdo)].decode(errors="replace")
            results.append((col, offs, cname))
    cols = {c: (offs, n) for c, offs, n in results}
    # vtables: dword in rdata equal to a COL address; vtable starts right after it
    for o in range(rdata[3], rdata[3] + rdata[4] - 4, 4):
        v = struct.unpack_from("<I", data, o)[0]
        if v in cols:
            vt = rdata[1] + o - rdata[3] + 4
            n = 0
            while True:
                f = u32(vt + 4 * n)
                if f is None or not (text[1] <= f < text[1] + text[2]):
                    break
                n += 1
            offs, cname = cols[v]
            if filt is None or filt in cname:
                print(f"{vt:08X} off={offs:<4X} slots={n:<4} {cname}")


main()
