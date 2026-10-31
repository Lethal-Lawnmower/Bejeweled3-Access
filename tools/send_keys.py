"""Post key presses to the Bejeweled 3 window for testing mod hotkeys.

Usage: python -I send_keys.py KEY [KEY...]   e.g. tab return f12
Each key gets WM_KEYDOWN and WM_KEYUP, then a 0.4 s pause. No WM_CHAR is
posted: the game's own TranslateMessage call generates it from the key-down
(posting one too typed every letter twice in testing).
"""
import ctypes
import sys
import time

VK = {"tab": 0x09, "return": 0x0D, "space": 0x20, "up": 0x26, "down": 0x28,
      "home": 0x24, "end": 0x23, "left": 0x25, "right": 0x27, "f5": 0x74, "f6": 0x75, "f12": 0x7B, "back": 0x08,
      "escape": 0x1B}

u = ctypes.windll.user32
hwnd = u.FindWindowW("MainWindow", None)
if not hwnd:
    sys.exit("Bejeweled 3 window not found")
for name in sys.argv[1:]:
    vk = VK[name.lower()] if name.lower() in VK else ord(name.upper())
    u.PostMessageW(hwnd, 0x100, vk, 1)
    u.PostMessageW(hwnd, 0x101, vk, 0xC0000001)
    time.sleep(0.4)
