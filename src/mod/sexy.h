#pragma once

// Memory layout of the Steam Bejeweled3.exe (SHA-256 d5516244...88c3, image
// base 0x00400000, no ASLR). Every value cites its evidence; see
// docs/reverse-engineering.md. "runtime" means confirmed in a live dump,
// "static" means read from Ghidra decompilation only.

#include <cstdint>

namespace sexy {

// Global SexyAppBase*/Bej3App* (static: Dialog ctor FUN_00583870 reads it;
// community: FatBrother1 deck-bot).
constexpr uintptr_t kGAppPtr = 0x008E1730;

// WidgetContainer (static: ctor FUN_00474450).
// std::list<Widget*> mWidgets at +0x04 (MSVC 2008 list, 0x1C bytes); its
// head node pointer is at +0x18 and size at +0x1C. Nodes: {next, prev, value}.
constexpr int kWidgetsHead = 0x18;
constexpr int kWidgetsSize = 0x1C;
constexpr int kWidgetManager = 0x20;
constexpr int kParent = 0x24;
constexpr int kX = 0x40;
constexpr int kY = 0x44;
constexpr int kWidth = 0x48;
constexpr int kHeight = 0x4C;

// Widget (static: ctor FUN_0046a500; bools in framework declaration order).
constexpr int kVisible = 0x64;
constexpr int kMouseVisible = 0x65;
constexpr int kDisabled = 0x66;
constexpr int kHasFocus = 0x67;
constexpr int kIsDown = 0x68;
constexpr int kIsOver = 0x69;

// ButtonWidget (static: ctor FUN_0049c490 stores the id and listener args).
constexpr int kButtonId = 0xBC;
constexpr int kButtonLabel = 0xC0;  // std::wstring
constexpr int kButtonListener = 0x13C;

// Dialog (static: ctor FUN_00583870; three std::wstring members).
constexpr int kDialogYesButton = 0xCC;
constexpr int kDialogNoButton = 0xD0;
constexpr int kDialogHeader = 0xD8;
constexpr int kDialogFooter = 0xF4;
constexpr int kDialogLines = 0x110;

// EditWidget (static: ctor FUN_0059dc10).
constexpr int kEditString = 0xC0;  // std::wstring mString

// WidgetManager (static: ctor FUN_00584a10; runtime 2026-10-30: +0xA0 held
// the focused EditWidget and +0xAC the open WelcomeDialog).
constexpr int kWmApp = 0x6C;
constexpr int kWmFocusWidget = 0xA0;
constexpr int kWmBaseModalWidget = 0xAC;
constexpr int kWmDumpFields[] = {0xA0, 0xA4, 0xA8, 0xAC, 0xB0};
// Mouse remap rectangles {x, y, w, h} (runtime: logical -102,0 1228x768 and
// client 0,0 2560x1440 in 2560x1440 fullscreen). Client = (logical - L.x) *
// C.w / L.w + C.x, likewise for y.
constexpr int kWmLogicalRect = 0xE8;
constexpr int kWmClientRect = 0xF8;

// MSVC 2008 std::wstring: +0 allocator/proxy, +4 inline buffer (8 wchar_t)
// or heap pointer when capacity > 7, +0x14 size, +0x18 capacity
// (static: ButtonWidget ctor sets capacity 7 and a 2-byte terminator).
constexpr int kStrBuf = 0x04;
constexpr int kStrSize = 0x14;
constexpr int kStrCap = 0x18;

// Vtables from RTTI (tools/rtti_vtables.py), used to identify objects.
constexpr uintptr_t kVtWidgetManager = 0x008751A4;

}  // namespace sexy
