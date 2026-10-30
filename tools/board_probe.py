"""Read-only probe of the live Bejeweled 3 board (research tool, not part of the mod).

Usage: python -I tools/board_probe.py [--grid]
Opens the running game with read-only access and prints the board fields
documented in docs/re/board-*.md, so static findings can be checked live.
Writes nothing to the game.
"""
import ctypes
import struct
import subprocess
import sys

k32 = ctypes.windll.kernel32
PROCESS_VM_READ = 0x10


def game_pid():
    out = subprocess.check_output(
        'tasklist /FI "IMAGENAME eq Bejeweled3.exe" /FO CSV /NH', text=True)
    rows = [l for l in out.splitlines() if l.startswith('"')]
    if not rows:
        sys.exit("Bejeweled 3 is not running")
    return int(rows[0].split(",")[1].strip('"'))


handle = k32.OpenProcess(PROCESS_VM_READ, 0, game_pid())


def rd(addr, n):
    buf = ctypes.create_string_buffer(n)
    got = ctypes.c_size_t()
    if not k32.ReadProcessMemory(handle, ctypes.c_void_p(addr), buf, n, ctypes.byref(got)):
        return None
    return buf.raw


def u32(a):
    b = rd(a, 4)
    return struct.unpack("<I", b)[0] if b else None


def i32(a):
    b = rd(a, 4)
    return struct.unpack("<i", b)[0] if b else None


def f32(a):
    b = rd(a, 4)
    return struct.unpack("<f", b)[0] if b else None


def u8(a):
    b = rd(a, 1)
    return b[0] if b else None


def class_name(obj):
    vt = u32(obj)
    col = u32(vt - 4) if vt else None
    td = u32(col + 12) if col else None
    raw = rd(td + 8, 64) if td else None
    if not raw or not raw.startswith(b".?AV"):
        return "?"
    return raw[4:raw.index(b"@")].decode()


FLAGS = {1: "flame", 2: "hypercube", 4: "star", 0x10: "mult", 0x80: "butterfly",
         0x20000: "time", 0x10000: "ground", 0x1000: "scrambler"}

app = u32(0x008E1730)
board = u32(app + 0xBE8) if app else 0
print(f"gApp {app:08X}  board {board or 0:08X}  {class_name(board) if board else 'no board'}")
if not board:
    sys.exit()
print(f"score {i32(board + 0xD24)}  level0 {i32(board + 0xE04)}  points {i32(board + 0xE00)}"
      f"  mult {i32(board + 0xE0C)}")
print(f"cursor col {i32(board + 0x2C44)} row {i32(board + 0x2C48)}  stillTicks {i32(board + 0xE3C)}"
      f"  refillCountdown {i32(board + 0xE40)}  pauseFade {f32(board + 0x12C8):.2f}"
      f"  blocked1410 {u8(board + 0x1410)}  timeUp {u8(board + 0xE28)}")
print(f"swapsInProgress vec {u32(board + 0xC30):08X}  movesResolving vec {u32(board + 0xC48):08X}"
      f".. {u32(board + 0xC4C):08X}  hintCooldown {i32(board + 0x26A0)}  idle {i32(board + 0x26A4)}")

letters = "RWGYPOB"
bad = []
rows = []
specials = []
for r in range(8):
    line = []
    for c in range(8):
        p = u32(board + 0xF8 + 4 * (c + 8 * r))
        if not p:
            line.append(" .")
            continue
        pid, pc, pr = i32(p + 4), i32(p + 8), i32(p + 0xC)
        x, y, fall = f32(p + 0x10), f32(p + 0x14), f32(p + 0x1C)
        colour, flags, counter, sel = i32(p + 0x220), u32(p + 0x228), i32(p + 0x244), u8(p + 0x23C)
        if (pc, pr) != (c, r):
            bad.append(f"cell {c},{r} piece says {pc},{pr}")
        ch = letters[colour] if 0 <= colour <= 6 else "-"
        mark = "*" if flags else " "
        line.append(ch + mark)
        if flags or sel:
            names = [n for b, n in FLAGS.items() if flags & b]
            specials.append(f"  {c},{r} id {pid} colour {colour} flags {flags:#x} {'+'.join(names)}"
                            f" counter {counter} selected {sel}")
        if "--grid" in sys.argv:
            print(f"  {c},{r} id {pid} pos {x:.0f},{y:.0f} fall {fall:.2f} colour {colour} flags {flags:#x}")
    rows.append(" ".join(line))
print("grid (row 0 at top; letter = colour id 0..6 as R W G Y P O B, * = special):")
for r, line in enumerate(rows):
    print(f"  {r} {line}")
print("specials/selected:" if specials else "no special or selected gems")
for s in specials:
    print(s)
print("grid/piece position mismatches:", bad if bad else "none")
