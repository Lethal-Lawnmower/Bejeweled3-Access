#include "ui_names.h"

#include <cwctype>

#include "game_memory.h"
#include "game_strings.h"

namespace {

// ButtonWidget::mButtonImage (static: MainMenu FUN_006a3530 and PlayMenu
// FUN_00680760 store image resource globals at button+0xE4; framework
// order mLabel +0xC0, mLabelJustify +0xDC, mFont +0xE0, mButtonImage +0xE4).
// Candidate for sexy.h once runtime-verified.
constexpr int kButtonImage = 0xE4;

// PlayMenu description std::wstring (static: ButtonMouseEnter FUN_0068e0e0
// assigns it at listener+0x10C = object+0x1C8; PlayMenu::Draw FUN_00689d20
// draws this+0x1C8 centred).
constexpr int kPlayMenuDescription = 0x1C8;

struct WidgetTextEntry {
    const char* ownerClass;
    UiWidgetKind kind;
    int widgetId;
    int stringId;
    UiTextSource source;
    // Evidence: function and what it shows (details in docs/re/menus.md).
};

// Only entries with decompiled evidence linking the string to the widget.
// Image-only buttons whose handlers open no titled dialog and near which
// the game draws no text (PlayMenu 100, QuestMenu 0/1/2) are deliberately
// absent: use ButtonImageWords for them.
constexpr WidgetTextEntry kWidgetText[] = {
    // MainMenu::Draw (006a0ca0) draws id 288 at button 4's y while button 4
    // is hovered; button 4 (FUN_006a3530, invisible ButtonWidget) opens the
    // user selection dialog via FUN_0076fbd0.
    {"MainMenu", UiWidgetKind::Button, 4, 288, UiTextSource::DrawnNearWidget},
    // MainMenu ButtonDepress (006a2de0) case 3 -> Bej3App::AskQuit
    // (0076b900), whose dialog header is id 73. Same handler for PlayMenu
    // 101 (0069b360) and QuestMenu 3 (00697640). Image:
    // IMAGE_MAIN_MENU_QUIT_BUTTON.
    {"MainMenu", UiWidgetKind::Button, 3, 73, UiTextSource::OpensDialogTitled},
    {"PlayMenu", UiWidgetKind::Button, 101, 73, UiTextSource::OpensDialogTitled},
    {"QuestMenu", UiWidgetKind::Button, 3, 73, UiTextSource::OpensDialogTitled},
    // MainMenu ButtonDepress case 7 creates HelpDialog (ctor 00696100),
    // whose header is id 239. Image: IMAGE_MAIN_MENU_HELP_BUTTON.
    {"MainMenu", UiWidgetKind::Button, 7, 239, UiTextSource::OpensDialogTitled},

    // OptionsDialog::DrawAll (0068ed30) draws these labels beside the
    // widgets created in the ctor (00681370). Sliders 0,1,2 are created top
    // to bottom and labels 312,313,314 drawn top to bottom (order match).
    {"OptionsDialog", UiWidgetKind::Slider, 0, 312, UiTextSource::DrawnNearWidget},
    {"OptionsDialog", UiWidgetKind::Slider, 1, 313, UiTextSource::DrawnNearWidget},
    {"OptionsDialog", UiWidgetKind::Slider, 2, 314, UiTextSource::DrawnNearWidget},
    // Checkboxes 8/9/10 form the resolution radio group in CheckboxChecked
    // (00684ae0); 10 is refused below 1200-line screens -> 318.
    {"OptionsDialog", UiWidgetKind::Checkbox, 8, 316, UiTextSource::DrawnNearWidget},
    {"OptionsDialog", UiWidgetKind::Checkbox, 9, 317, UiTextSource::DrawnNearWidget},
    {"OptionsDialog", UiWidgetKind::Checkbox, 10, 318, UiTextSource::DrawnNearWidget},
    // CheckboxChecked: id 4 drives the 3D acceleration dialogs (325-333),
    // id 5 the windowed-mode dialog (322-324), id 7 the animated
    // background dialog (563-565). Id 6 is the remaining label (321) by
    // elimination: runtime check pending (docs/re/menus.md).
    {"OptionsDialog", UiWidgetKind::Checkbox, 4, 319, UiTextSource::DrawnNearWidget},
    {"OptionsDialog", UiWidgetKind::Checkbox, 5, 320, UiTextSource::DrawnNearWidget},
    {"OptionsDialog", UiWidgetKind::Checkbox, 6, 321, UiTextSource::DrawnNearWidget},
    {"OptionsDialog", UiWidgetKind::Checkbox, 7, 562, UiTextSource::DrawnNearWidget},
};

struct ScreenTitleEntry {
    const char* screenClass;
    int stringId;
};

// SecretMenu::Draw (00677580) draws id 477 as its heading.
constexpr ScreenTitleEntry kScreenTitles[] = {
    {"SecretMenu", 477},
};

}  // namespace

bool UiWidgetText(const std::string& ownerClass, UiWidgetKind kind, int widgetId,
                  std::wstring* out, UiTextSource* source) {
    for (const auto& e : kWidgetText) {
        if (e.kind == kind && e.widgetId == widgetId && ownerClass == e.ownerClass) {
            if (!GameString(e.stringId, out)) return false;
            if (source) *source = e.source;
            return true;
        }
    }
    return false;
}

bool UiScreenTitle(const std::string& screenClass, std::wstring* out) {
    for (const auto& e : kScreenTitles)
        if (screenClass == e.screenClass) return GameString(e.stringId, out);
    return false;
}

bool ButtonImageWords(uintptr_t button, std::wstring* out) {
    uint32_t image;
    std::string id;
    if (!ReadU32(button + kButtonImage, &image) || !GameImageResourceName(image, &id))
        return false;
    // "IMAGE_MAIN_MENU_QUIT_BUTTON" -> "main menu quit button".
    const std::string prefix = "IMAGE_";
    if (id.compare(0, prefix.size(), prefix) == 0) id.erase(0, prefix.size());
    out->clear();
    for (char c : id) {
        wchar_t w = static_cast<wchar_t>(static_cast<unsigned char>(c));
        out->push_back(w == L'_' ? L' ' : static_cast<wchar_t>(towlower(w)));
    }
    return !out->empty();
}

bool PlayMenuHoverDescription(uintptr_t playMenu, std::wstring* out) {
    return ReadWString(playMenu + kPlayMenuDescription, out) && !out->empty();
}
