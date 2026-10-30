#include "game_memory.h"

#include <windows.h>

#include <cstring>

#include "sexy.h"

namespace {

// Only the executable's image is a valid home for vtables and RTTI.
bool InImage(uintptr_t p) { return p >= 0x00400000 && p < 0x00D92000; }

bool SafeCopy(void* dst, uintptr_t src, size_t len) {
    if (src < 0x10000) return false;
    __try {
        memcpy(dst, reinterpret_cast<const void*>(src), len);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Copies the decorated RTTI name (".?AVButtonWidget@Sexy@@") of a type descriptor.
bool TypeName(uintptr_t typeDescriptor, char* out, size_t cap) {
    if (!InImage(typeDescriptor)) return false;
    if (!SafeCopy(out, typeDescriptor + 8, cap - 1)) return false;
    out[cap - 1] = '\0';
    return strncmp(out, ".?AV", 4) == 0 || strncmp(out, ".?AU", 4) == 0;
}

// ".?AVButtonWidget@Sexy@@" -> "ButtonWidget"
std::string Undecorate(const char* name) {
    const char* start = name + 4;
    const char* at = strchr(start, '@');
    return at ? std::string(start, at) : std::string(start);
}

// Complete object locator: {sig, offset, cdOffset, pTypeDescriptor, pHierarchy}.
bool ObjectLocator(uintptr_t object, uint32_t col[5]) {
    uint32_t vtable, colAddr;
    if (!ReadU32(object, &vtable) || !InImage(vtable)) return false;
    if (!ReadU32(vtable - 4, &colAddr) || !InImage(colAddr)) return false;
    return SafeCopy(col, colAddr, 5 * sizeof(uint32_t)) && col[0] == 0;
}

}  // namespace

bool ReadU32(uintptr_t addr, uint32_t* out) { return SafeCopy(out, addr, 4); }
bool ReadI32(uintptr_t addr, int32_t* out) { return SafeCopy(out, addr, 4); }
bool ReadU8(uintptr_t addr, uint8_t* out) { return SafeCopy(out, addr, 1); }

bool ReadWString(uintptr_t addr, std::wstring* out) {
    uint32_t size, cap;
    if (!ReadU32(addr + sexy::kStrSize, &size) || !ReadU32(addr + sexy::kStrCap, &cap))
        return false;
    if (size > 4096 || cap < size || cap < 7) return false;
    uintptr_t data = addr + sexy::kStrBuf;
    if (cap > 7) {
        uint32_t heap;
        if (!ReadU32(data, &heap)) return false;
        data = heap;
    }
    out->resize(size);
    return size == 0 || SafeCopy(&(*out)[0], data, size * sizeof(wchar_t));
}

std::string ClassNameOf(uintptr_t object) {
    uint32_t col[5];
    char name[128];
    if (!ObjectLocator(object, col) || !TypeName(col[3], name, sizeof name)) return {};
    return Undecorate(name);
}

bool IsKindOf(uintptr_t object, const char* baseName) {
    // Class hierarchy descriptor: {sig, attrs, numBases, pBaseClassArray};
    // each base class descriptor starts with its pTypeDescriptor.
    uint32_t col[5], chd[4];
    if (!ObjectLocator(object, col) || !InImage(col[4])) return false;
    if (!SafeCopy(chd, col[4], sizeof chd) || chd[2] > 64 || !InImage(chd[3])) return false;
    std::string want = std::string(".?AV") + baseName + "@";
    for (uint32_t i = 0; i < chd[2]; ++i) {
        uint32_t bcd, td;
        char name[128];
        if (!ReadU32(chd[3] + 4 * i, &bcd) || !ReadU32(bcd, &td)) return false;
        if (TypeName(td, name, sizeof name) && strncmp(name, want.c_str(), want.size()) == 0)
            return true;
    }
    return false;
}
