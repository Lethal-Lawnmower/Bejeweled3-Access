#pragma once

// Crash-safe reads of game objects from inside the game process. Every read
// is guarded, so a stale pointer yields "unknown" instead of a crash; callers
// must treat a failed read as unknown state, never as a default value.

#include <cstdint>
#include <string>

bool ReadU32(uintptr_t addr, uint32_t* out);
bool ReadU8(uintptr_t addr, uint8_t* out);
bool ReadI32(uintptr_t addr, int32_t* out);

// Reads an MSVC 2008 std::wstring located at addr (bounded to 4096 chars).
bool ReadWString(uintptr_t addr, std::wstring* out);

// MSVC RTTI: class name of a polymorphic object (e.g. "ButtonWidget"),
// without the ".?AV" prefix and "@Sexy@@" suffix. Empty if unreadable.
std::string ClassNameOf(uintptr_t object);

// True if the object's class is, or derives from, baseName (by RTTI
// class hierarchy, e.g. IsKindOf(p, "ButtonWidget")).
bool IsKindOf(uintptr_t object, const char* baseName);
