#include "game_strings.h"

#include <cstdint>
#include <cstring>

#include "game_memory.h"
#include "sexy.h"

// Layouts (static evidence, docs/re/menus.md "String tables"):
//
// MSVC 2008 std::map as used by the game: the head (sentinel) node pointer
// is at map+0x18 and the element count at map+0x1C; head->_Parent (+4) is
// the root. Tree node: +0 _Left, +4 _Parent, +8 _Right, +0xC value
// (pair<const Key, std::wstring>), then the colour and _Isnil bytes.
//
// Numeric map (FUN_00476450 = find): key int at +0xC, value wstring at
// +0x10, _Isnil at +0x2D.
// Named map (FUN_00409830 / FUN_00403a50 = find / lower_bound): key
// std::string at +0xC (inline buffer +0x10, size +0x20, capacity +0x24),
// value wstring at +0x28, _Isnil at +0x45.

namespace {

constexpr int kAppNumericStrings = 0x7F4;  // std::map<int, std::wstring>
constexpr int kAppNamedStrings = 0x750;    // std::map<std::string, std::wstring>
constexpr int kMapHead = 0x18;
constexpr int kMapSize = 0x1C;
constexpr int kNodeLeft = 0x0;
constexpr int kNodeParent = 0x4;
constexpr int kNodeRight = 0x8;
constexpr int kNodeKey = 0xC;

constexpr int kIntNodeValue = 0x10;
constexpr int kIntNodeIsNil = 0x2D;

constexpr int kStrNodeValue = 0x28;
constexpr int kStrNodeIsNil = 0x45;

// A balanced tree of a few thousand entries is far shallower than this;
// the bound only stops a walk through a corrupt or half-built tree.
constexpr int kMaxDepth = 64;

bool AppPointer(uintptr_t* app) {
    uint32_t p;
    if (!ReadU32(sexy::kGAppPtr, &p) || p == 0) return false;
    *app = p;
    return true;
}

bool IsNil(uintptr_t node, int isNilOffset, bool* nil) {
    uint8_t b;
    if (node == 0 || !ReadU8(node + isNilOffset, &b)) return false;
    *nil = b != 0;
    return true;
}

// Reads an MSVC 2008 std::string (narrow) at addr: +4 inline buffer of 16
// chars or heap pointer when capacity > 15, +0x14 size, +0x18 capacity.
bool ReadNarrowString(uintptr_t addr, std::string* out) {
    uint32_t size, cap;
    if (!ReadU32(addr + 0x14, &size) || !ReadU32(addr + 0x18, &cap)) return false;
    if (size > 256 || cap < size || cap < 15) return false;
    uintptr_t data = addr + 4;
    if (cap > 15) {
        uint32_t heap;
        if (!ReadU32(data, &heap)) return false;
        data = heap;
    }
    out->resize(size);
    for (uint32_t i = 0; i < size; ++i) {
        uint8_t c;
        if (!ReadU8(data + i, &c)) return false;
        (*out)[i] = static_cast<char>(c);
    }
    return true;
}

// std::less<std::string> ordering (byte-wise compare, shorter prefix first).
int CompareKeys(const std::string& a, const char* b, size_t bLen) {
    size_t n = a.size() < bLen ? a.size() : bLen;
    int c = n ? memcmp(a.data(), b, n) : 0;
    if (c != 0) return c;
    return a.size() < bLen ? -1 : (a.size() > bLen ? 1 : 0);
}

bool RootOf(uintptr_t map, uintptr_t* head, uintptr_t* root) {
    uint32_t h, r;
    if (!ReadU32(map + kMapHead, &h) || h == 0) return false;
    if (!ReadU32(h + kNodeParent, &r)) return false;
    *head = h;
    *root = r;
    return true;
}

}  // namespace

bool GameString(int id, std::wstring* out) {
    uintptr_t app, head, node;
    if (!AppPointer(&app) || !RootOf(app + kAppNumericStrings, &head, &node)) return false;
    for (int depth = 0; depth < kMaxDepth; ++depth) {
        bool nil;
        if (!IsNil(node, kIntNodeIsNil, &nil)) return false;
        if (nil || node == head) return false;  // not found
        int32_t key;
        if (!ReadI32(node + kNodeKey, &key)) return false;
        if (key == id) return ReadWString(node + kIntNodeValue, out);
        uint32_t next;
        if (!ReadU32(node + (id < key ? kNodeLeft : kNodeRight), &next)) return false;
        node = next;
    }
    return false;
}

bool GameStringByKey(const char* key, std::wstring* out) {
    if (key == nullptr) return false;
    size_t keyLen = strlen(key);
    uintptr_t app, head, node;
    if (!AppPointer(&app) || !RootOf(app + kAppNamedStrings, &head, &node)) return false;
    for (int depth = 0; depth < kMaxDepth; ++depth) {
        bool nil;
        if (!IsNil(node, kStrNodeIsNil, &nil)) return false;
        if (nil || node == head) return false;
        std::string nodeKey;
        if (!ReadNarrowString(node + kNodeKey, &nodeKey)) return false;
        int c = CompareKeys(nodeKey, key, keyLen);
        if (c == 0) return ReadWString(node + kStrNodeValue, out);
        uint32_t next;
        // nodeKey > key: go left.
        if (!ReadU32(node + (c > 0 ? kNodeLeft : kNodeRight), &next)) return false;
        node = next;
    }
    return false;
}

namespace {

// Generated resource code (static: FUN_006efb20 loops index 0..0xA4B over
// both; validated offline: all 2636 jump targets are "MOV EAX, name; RET").
constexpr uintptr_t kResourceGlobals = 0x008CF5C8;  // void* gResources[0xA4C]
constexpr uintptr_t kResourceNameJumpTable = 0x006ECA80;
constexpr int kResourceCount = 0xA4C;

bool ResourceNameByIndex(int index, std::string* out) {
    uint32_t target;
    uint8_t mov, ret;
    if (!ReadU32(kResourceNameJumpTable + 4 * index, &target)) return false;
    if (!ReadU8(target, &mov) || !ReadU8(target + 5, &ret) || mov != 0xB8 || ret != 0xC3)
        return false;
    uint32_t name;
    if (!ReadU32(target + 1, &name)) return false;
    out->clear();
    for (int i = 0; i < 96; ++i) {
        uint8_t c;
        if (!ReadU8(name + i, &c)) return false;
        if (c == 0) return !out->empty();
        out->push_back(static_cast<char>(c));
    }
    return false;
}

}  // namespace

bool GameImageResourceName(uintptr_t image, std::string* out) {
    if (image == 0) return false;
    for (int i = 0; i < kResourceCount; ++i) {
        uint32_t global, value;
        if (!ReadU32(kResourceGlobals + 4 * i, &global)) return false;
        if (global == 0) return false;  // table end
        if (ReadU32(global, &value) && value == image) return ResourceNameByIndex(i, out);
    }
    return false;
}

int GameStringCount() {
    uintptr_t app;
    uint32_t size;
    if (!AppPointer(&app) || !ReadU32(app + kAppNumericStrings + kMapSize, &size)) return -1;
    return static_cast<int>(size);
}
