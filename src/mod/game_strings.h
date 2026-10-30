#pragma once

// Read-only access to the game's own string tables, so the mod can speak the
// exact text the game shows without copying any of it into this source.
//
// Bejeweled 3 loads properties/default.xml (and the framework properties)
// into two std::map members of the app object (see docs/re/menus.md):
//   gApp+0x7F4  std::map<int, std::wstring>          numeric ids, e.g. 287
//   gApp+0x750  std::map<std::string, std::wstring>  every id as text, e.g.
//                                                     "DIALOG_BUTTON_OK"
// The game's own lookup (FUN_00475e30, numeric) uses a custom register
// calling convention, so the mod walks the red-black tree itself with
// guarded reads instead of calling it. The maps are filled while properties
// load at startup and are not modified afterwards (static evidence only),
// so reading them from the game's window thread between frames is safe.
//
// Strings are returned raw (colour codes such as ^FFAACC^, &cr;, printf
// placeholders like %s / %d intact); pass them through CleanGameText before
// speaking, and fill placeholders only with values read from the game.

#include <cstdint>
#include <string>

// Text of numeric string id (the id="N" in default.xml). False if the table
// is not loaded yet, the id does not exist, or a read failed. When the id is
// missing the game itself falls back to the English literal compiled next
// to the lookup call; the mod has no copy of that literal, so it reports
// "unknown" instead.
bool GameString(int id, std::wstring* out);

// Text of a named property string such as "DIALOG_BUTTON_OK".
bool GameStringByKey(const char* key, std::wstring* out);

// Number of entries in the numeric table, or -1 if unreadable (diagnostics:
// lets the log show whether the table was loaded).
int GameStringCount();

// The game's own resource id for a loaded image (e.g. the button image of an
// image-only button), such as "IMAGE_MAIN_MENU_QUIT_BUTTON". Found the way
// the game's generated resource code does it: the pointer table at
// 0x008CF5C8 lists the address of every resource global, and
// GetStringIdById (0x006E8CA0) maps the same index to its name. False if the
// image is not a named resource or a read failed.
bool GameImageResourceName(uintptr_t image, std::string* out);
