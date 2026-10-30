#include "game_actions.h"

#include <windows.h>

#include "game_memory.h"
#include "log.h"
#include "sexy.h"

// Every address, offset and slot below is from static analysis of the Steam
// Bejeweled3.exe (SHA-256 d5516244...88c3); docs/re/game-actions.md has the
// call sites that show each calling convention. Virtual functions are
// __thiscall (ECX = this, arguments pushed right to left, callee pops); they
// are called through __fastcall pointers with a dummy EDX argument, the
// same technique as text_capture.cpp. The two WidgetManager helpers and the
// Zen select routine take their object in a register and need inline asm.

namespace game_actions {
namespace {

// --- Vtables (RTTI, private/ghidra-menus/vtables.txt) ----------------------
constexpr uintptr_t kVtCheckbox = 0x00876BDC;
constexpr uintptr_t kVtSlider = 0x00876D2C;
constexpr uintptr_t kVtListWidget = 0x008766F4;
constexpr uintptr_t kVtDropListWidget = 0x0086F3DC;
constexpr uintptr_t kVtZenOptionsDialog = 0x0086F55C;

// --- Widget virtual slots (byte offsets into the vtable) -------------------
constexpr int kSlotCheckboxSetChecked = 0x144;  // slot 81, (bool checked, bool tell), RET 8
constexpr int kSlotSliderSetValue = 0x144;      // slot 81, (double), RET 8
constexpr int kSlotMouseUp = 0xE4;              // slot 57, (int x, int y), RET 8
constexpr int kSlotDropListSetOpen = 0x154;     // slot 85, (bool open), RET 4

// --- Listener slots --------------------------------------------------------
// ButtonListener: 0 ButtonPress(id, clickCount) (default forwards to slot 1
// ButtonPress(id)), 2 ButtonDepress(id), 4 ButtonMouseEnter, 5 ...Leave.
constexpr int kSlotButtonPressClicks = 0x00;  // RET 8
constexpr int kSlotButtonDepress = 0x08;      // RET 4
// ListListener: 0 ListClicked(id, index, clickCount), RET 0xC.
constexpr int kSlotListClicked = 0x00;

// --- Field offsets ----------------------------------------------------------
constexpr int kWidgetManagerOfWidget = sexy::kWidgetManager;  // +0x20
constexpr int kWmOverWidget = 0xA8;     // WidgetManager::mOverWidget
constexpr int kCheckboxChecked = 0xC4;  // bool
constexpr int kCheckboxListener = 0xBC;
constexpr int kSliderListener = 0xBC;
constexpr int kListId = 0xC0;
constexpr int kListLinesBegin = 0xE0;  // std::vector<std::wstring>, element 0x1C
constexpr int kListLinesEnd = 0xE4;
constexpr int kListListener = 0x120;
constexpr int kListLineSize = 0x1C;
constexpr int kDropChildList = 0x130;
constexpr int kZenSections = 0xD8;  // ZenCategoryContainer*[4]
constexpr int kZenSelected = 0xE8;  // -1 none
constexpr int kZenSectionDropList = 0x144;

// --- Game routines with register arguments ---------------------------------
// WidgetManager EnterWidget / LeaveWidget: widget in ESI, no stack
// arguments, plain RET; they set/clear Widget::mIsOver, call
// MouseEnter/MouseLeave and ShowFinger. Callers (FUN_00585a10) update
// WidgetManager+0xA8 themselves.
constexpr uintptr_t kEnterWidget = 0x00585020;
constexpr uintptr_t kLeaveWidget = 0x00585050;
// ZenOptionsDialog select section: dialog in EDI, int id on the stack, RET 4.
constexpr uintptr_t kZenSelectSection = 0x00683410;

using Fn0 = void(__fastcall*)(uintptr_t self, uintptr_t edx);
using FnI = void(__fastcall*)(uintptr_t self, uintptr_t edx, int a);
using FnII = void(__fastcall*)(uintptr_t self, uintptr_t edx, int a, int b);
using FnIII = void(__fastcall*)(uintptr_t self, uintptr_t edx, int a, int b, int c);
using FnD = void(__fastcall*)(uintptr_t self, uintptr_t edx, double a);

bool InImage(uintptr_t p) { return p >= 0x00400000 && p < 0x00D92000; }

// Reads the function pointer at byte offset `slot` of an object's vtable;
// both the vtable and the function must lie in the executable image.
bool VirtualAt(uintptr_t object, int slot, uintptr_t* fn) {
    uint32_t vt, f;
    if (!ReadU32(object, &vt) || !InImage(vt) || !ReadU32(vt + slot, &f) || !InImage(f))
        return false;
    *fn = f;
    return true;
}

bool HasVtable(uintptr_t object, uintptr_t vtable) {
    uint32_t vt;
    return ReadU32(object, &vt) && vt == vtable;
}

// The checks the WidgetManager applies before it delivers mouse input: the
// widget is in a WidgetManager, visible and not disabled.
bool Reachable(uintptr_t widget, const char* what) {
    uint32_t wm;
    uint8_t visible, disabled;
    if (!ReadU32(widget + kWidgetManagerOfWidget, &wm) || !ReadU32(wm, &wm) ||
        wm != sexy::kVtWidgetManager) {
        Log("actions: %s %08X is not in a WidgetManager", what, widget);
        return false;
    }
    if (!ReadU8(widget + sexy::kVisible, &visible) || !ReadU8(widget + sexy::kDisabled, &disabled))
        return false;
    if (!visible || disabled) {
        Log("actions: %s %08X refused (visible %d, disabled %d)", what, widget, visible, disabled);
        return false;
    }
    return true;
}

// --- SEH-guarded calls. No C++ objects with destructors in these. ----------

bool Guarded0(uintptr_t fn, uintptr_t self, const char* what) {
    __try {
        reinterpret_cast<Fn0>(fn)(self, 0);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("actions: exception %08X in %s (fn %08X, this %08X)", GetExceptionCode(), what, fn, self);
        return false;
    }
}

bool GuardedI(uintptr_t fn, uintptr_t self, int a, const char* what) {
    __try {
        reinterpret_cast<FnI>(fn)(self, 0, a);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("actions: exception %08X in %s (fn %08X, this %08X)", GetExceptionCode(), what, fn, self);
        return false;
    }
}

bool GuardedII(uintptr_t fn, uintptr_t self, int a, int b, const char* what) {
    __try {
        reinterpret_cast<FnII>(fn)(self, 0, a, b);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("actions: exception %08X in %s (fn %08X, this %08X)", GetExceptionCode(), what, fn, self);
        return false;
    }
}

bool GuardedIII(uintptr_t fn, uintptr_t self, int a, int b, int c, const char* what) {
    __try {
        reinterpret_cast<FnIII>(fn)(self, 0, a, b, c);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("actions: exception %08X in %s (fn %08X, this %08X)", GetExceptionCode(), what, fn, self);
        return false;
    }
}

bool GuardedD(uintptr_t fn, uintptr_t self, double a, const char* what) {
    __try {
        reinterpret_cast<FnD>(fn)(self, 0, a);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("actions: exception %08X in %s (fn %08X, this %08X)", GetExceptionCode(), what, fn, self);
        return false;
    }
}

// Calls EnterWidget or LeaveWidget with the widget in ESI.
void CallWithEsi(uintptr_t fn, uintptr_t widget) {
    __asm {
        push esi
        mov esi, widget
        mov eax, fn
        call eax
        pop esi
    }
}

bool GuardedWithEsi(uintptr_t fn, uintptr_t widget, const char* what) {
    __try {
        CallWithEsi(fn, widget);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("actions: exception %08X in %s (widget %08X)", GetExceptionCode(), what, widget);
        return false;
    }
}

// Calls the Zen select routine: dialog in EDI, id pushed, callee pops.
void CallZenSelect(uintptr_t dialog, int id) {
    __asm {
        push edi
        mov edi, dialog
        push id
        mov eax, 0x00683410  ; kZenSelectSection (a literal: a C++ name here would be read as memory)
        call eax
        pop edi
    }
}

bool GuardedZenSelect(uintptr_t dialog, int id) {
    __try {
        CallZenSelect(dialog, id);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("actions: exception %08X in Zen select (dialog %08X, id %d)", GetExceptionCode(), dialog, id);
        return false;
    }
}

// ButtonWidget id and listener; the listener's vtable must be in the image.
bool ButtonParts(uintptr_t button, int32_t* id, uintptr_t* listener) {
    uint32_t l, lvt;
    if (!IsKindOf(button, "ButtonWidget")) {
        Log("actions: %08X is not a ButtonWidget (%s)", button, ClassNameOf(button).c_str());
        return false;
    }
    if (!ReadI32(button + sexy::kButtonId, id) || !ReadU32(button + sexy::kButtonListener, &l) ||
        !l || !ReadU32(l, &lvt) || !InImage(lvt)) {
        Log("actions: button %08X has no readable listener", button);
        return false;
    }
    *listener = l;
    return true;
}

}  // namespace

bool PressButton(uintptr_t button) {
    int32_t id;
    uintptr_t listener, press, depress;
    if (!ButtonParts(button, &id, &listener) || !Reachable(button, "button")) return false;
    if (!VirtualAt(listener, kSlotButtonPressClicks, &press) ||
        !VirtualAt(listener, kSlotButtonDepress, &depress))
        return false;
    Log("actions: press button %08X id %d listener %08X", button, id, listener);
    // Same pair as ButtonWidget's gamepad down/up handlers (FUN_0049d290 /
    // FUN_0049d320): ButtonPress(id, 1), then ButtonDepress(id).
    if (!GuardedII(press, listener, id, 1, "ButtonPress")) return false;
    // ButtonPress can close a screen. Re-read the button before the second
    // call; if it no longer looks like the same button, stop here.
    int32_t idNow;
    uintptr_t listenerNow;
    // Removing a widget clears its mWidgetManager (framework
    // WidgetRemovedHelper; hypothesis for this build), so a button whose
    // screen went away fails the WidgetManager check.
    uint32_t wmNow, wmVt;
    if (!ButtonParts(button, &idNow, &listenerNow) || idNow != id || listenerNow != listener ||
        !ReadU32(button + kWidgetManagerOfWidget, &wmNow) || !ReadU32(wmNow, &wmVt) ||
        wmVt != sexy::kVtWidgetManager) {
        Log("actions: button %08X changed after ButtonPress; ButtonDepress skipped", button);
        return true;
    }
    if (!VirtualAt(listener, kSlotButtonDepress, &depress)) return true;
    return GuardedI(depress, listener, id, "ButtonDepress");
}

bool HoverWidget(uintptr_t widget, bool enter) {
    uint32_t wm, wmVt, over;
    if (!ReadU32(widget + kWidgetManagerOfWidget, &wm) || !ReadU32(wm, &wmVt) ||
        wmVt != sexy::kVtWidgetManager || !ReadU32(wm + kWmOverWidget, &over)) {
        Log("actions: hover %08X: no WidgetManager", widget);
        return false;
    }
    uint32_t vt;
    if (!ReadU32(widget, &vt) || !InImage(vt)) return false;
    volatile uint32_t* overField = reinterpret_cast<volatile uint32_t*>(wm + kWmOverWidget);
    // Mirrors WidgetManager FUN_00585a10 (00585a51..00585a7c): clear
    // mOverWidget, leave the old widget, store the new one, enter it.
    if (enter) {
        if (!Reachable(widget, "hover target")) return false;
        if (over == widget) return true;
        *overField = 0;
        if (over && !GuardedWithEsi(kLeaveWidget, over, "LeaveWidget")) return false;
        *overField = static_cast<uint32_t>(widget);
        return GuardedWithEsi(kEnterWidget, widget, "EnterWidget");
    }
    if (over != widget) return true;  // not the game's over widget: nothing to leave
    *overField = 0;
    return GuardedWithEsi(kLeaveWidget, widget, "LeaveWidget");
}

bool ClickCheckbox(uintptr_t checkbox) {
    uint8_t checked;
    if (!HasVtable(checkbox, kVtCheckbox) || !ReadU8(checkbox + kCheckboxChecked, &checked)) {
        Log("actions: %08X is not a Checkbox (%s)", checkbox, ClassNameOf(checkbox).c_str());
        return false;
    }
    return SetCheckbox(checkbox, !checked);
}

bool SetCheckbox(uintptr_t checkbox, bool checked) {
    uint8_t current;
    uint32_t listener;
    uintptr_t fn;
    if (!HasVtable(checkbox, kVtCheckbox) || !ReadU8(checkbox + kCheckboxChecked, &current) ||
        !ReadU32(checkbox + kCheckboxListener, &listener)) {
        Log("actions: %08X is not a Checkbox (%s)", checkbox, ClassNameOf(checkbox).c_str());
        return false;
    }
    if (!Reachable(checkbox, "checkbox")) return false;
    if ((current != 0) == checked) return true;
    if (!VirtualAt(checkbox, kSlotCheckboxSetChecked, &fn)) return false;
    Log("actions: checkbox %08X -> %d (listener %08X)", checkbox, checked ? 1 : 0, listener);
    // Checkbox::SetChecked(checked, tellListener = true) FUN_005a15b0: stores
    // mChecked, calls CheckboxChecked(mId, checked), MarkDirty; the same
    // three steps as Checkbox::MouseDown FUN_005a1740.
    return GuardedII(fn, checkbox, checked ? 1 : 0, 1, "Checkbox::SetChecked");
}

bool SetSlider(uintptr_t slider, double value) {
    uint32_t listener;
    uintptr_t setValue, mouseUp;
    if (!HasVtable(slider, kVtSlider) || !ReadU32(slider + kSliderListener, &listener)) {
        Log("actions: %08X is not a Slider (%s)", slider, ClassNameOf(slider).c_str());
        return false;
    }
    if (!listener) {
        Log("actions: slider %08X has no listener", slider);  // MouseUp would crash
        return false;
    }
    if (!Reachable(slider, "slider")) return false;
    if (!(value >= 0.0)) value = 0.0;  // also catches NaN
    if (value > 1.0) value = 1.0;
    if (!VirtualAt(slider, kSlotSliderSetValue, &setValue) ||
        !VirtualAt(slider, kSlotMouseUp, &mouseUp))
        return false;
    Log("actions: slider %08X -> %.4f", slider, value);
    // Slider::SetValue FUN_005a2730 (stores and clamps mVal, MarkDirty; no
    // listener), then Slider::MouseUp FUN_005a2d50 (ends the drag: cursor
    // reset, listener slots 0 and 1 with (mId, mVal)). MouseUp ignores x, y.
    if (!GuardedD(setValue, slider, value, "Slider::SetValue")) return false;
    return GuardedII(mouseUp, slider, 0, 0, "Slider::MouseUp");
}

bool ChooseListRow(uintptr_t list, int index) {
    int32_t id;
    uint32_t begin, end, listener;
    uintptr_t fn;
    if (!HasVtable(list, kVtListWidget) || !ReadI32(list + kListId, &id) ||
        !ReadU32(list + kListLinesBegin, &begin) || !ReadU32(list + kListLinesEnd, &end) ||
        !ReadU32(list + kListListener, &listener)) {
        Log("actions: %08X is not a ListWidget (%s)", list, ClassNameOf(list).c_str());
        return false;
    }
    if (end < begin || (end - begin) % kListLineSize != 0) return false;
    int count = static_cast<int>((end - begin) / kListLineSize);
    if (index < 0 || index >= count) {
        Log("actions: list %08X row %d out of range (%d rows)", list, index, count);
        return false;
    }
    if (!listener) {
        Log("actions: list %08X has no listener", list);  // MouseDown ignores the click too
        return false;
    }
    if (!VirtualAt(listener, kSlotListClicked, &fn)) return false;
    Log("actions: list %08X id %d row %d listener %08X", list, id, index, listener);
    // What ListWidget::MouseDown FUN_0059d860 calls for the hilited row.
    return GuardedIII(fn, listener, id, index, 1, "ListClicked");
}

bool IsDropListOpen(uintptr_t dropList, bool* open) {
    uint32_t child;
    uint8_t visible;
    if (!HasVtable(dropList, kVtDropListWidget) || !ReadU32(dropList + kDropChildList, &child) ||
        !child || !ReadU8(child + sexy::kVisible, &visible))
        return false;
    *open = visible != 0;
    return true;
}

bool SetDropListOpen(uintptr_t dropList, bool open) {
    bool isOpen;
    uintptr_t fn;
    if (!IsDropListOpen(dropList, &isOpen)) {
        Log("actions: %08X is not a DropListWidget (%s)", dropList, ClassNameOf(dropList).c_str());
        return false;
    }
    if (!Reachable(dropList, "drop list")) return false;
    if (isOpen == open) return true;
    if (!VirtualAt(dropList, kSlotDropListSetOpen, &fn)) return false;
    Log("actions: drop list %08X open -> %d", dropList, open ? 1 : 0);
    // DropListWidget::SetOpen FUN_00673dc0 (what MouseDown FUN_00673300 calls
    // with !open): child list SetVisible, plus its scrollbar when needed.
    return GuardedI(fn, dropList, open ? 1 : 0, "DropListWidget::SetOpen");
}

bool ToggleDropList(uintptr_t dropList) {
    bool isOpen;
    if (!IsDropListOpen(dropList, &isOpen)) {
        Log("actions: %08X is not a DropListWidget (%s)", dropList, ClassNameOf(dropList).c_str());
        return false;
    }
    return SetDropListOpen(dropList, !isOpen);
}

bool ChooseDropListRow(uintptr_t dropList, int index) {
    uint32_t child;
    if (!HasVtable(dropList, kVtDropListWidget) || !ReadU32(dropList + kDropChildList, &child) ||
        !child) {
        Log("actions: %08X is not a DropListWidget (%s)", dropList, ClassNameOf(dropList).c_str());
        return false;
    }
    if (!Reachable(dropList, "drop list")) return false;
    // The child list's listener is the drop list's own ListListener
    // (FUN_006732b0): closes the list, SetSelect(index), stores it at
    // +0xCC and calls the owner's ListClicked(dropId, index, clickCount).
    return ChooseListRow(child, index);
}

bool ZenSectionSelectBlocked(uintptr_t zenDialog, bool* blocked) {
    int32_t selected;
    if (!HasVtable(zenDialog, kVtZenOptionsDialog) || !ReadI32(zenDialog + kZenSelected, &selected))
        return false;
    *blocked = false;
    if (selected < 0 || selected > 3) return true;
    uint32_t section, drop;
    if (!ReadU32(zenDialog + kZenSections + 4 * selected, &section) || !section ||
        !ReadU32(section + kZenSectionDropList, &drop))
        return false;
    if (!drop) return true;
    // Same test as FUN_00683410 at 0068346f..00683489.
    uint32_t child;
    uint8_t visible;
    if (!ReadU32(drop + kDropChildList, &child) || !child || !ReadU8(child + sexy::kVisible, &visible))
        return false;
    *blocked = visible != 0;
    return true;
}

bool SelectZenSection(uintptr_t zenDialog, int sectionId) {
    bool blocked;
    if (sectionId < 0 || sectionId > 3 || !ZenSectionSelectBlocked(zenDialog, &blocked)) {
        Log("actions: Zen select %d refused: %08X is %s", sectionId, zenDialog,
            ClassNameOf(zenDialog).c_str());
        return false;
    }
    for (int i = 0; i < 4; ++i) {
        uint32_t section;
        if (!ReadU32(zenDialog + kZenSections + 4 * i, &section) || !section) return false;
    }
    if (!Reachable(zenDialog, "Zen options dialog")) return false;
    if (blocked) {
        Log("actions: Zen select %d refused: drop-down list open", sectionId);
        return false;
    }
    Log("actions: Zen select section %d (dialog %08X)", sectionId, zenDialog);
    return GuardedZenSelect(zenDialog, sectionId);
}

}  // namespace game_actions
