#pragma once

// Interactive SexyAppFramework controls other than plain buttons: Checkbox,
// Slider, ListWidget, DropListWidget, ScrollbarWidget, and the tab-style
// button groups of RecordsScreen / HighScoresTab.
//
// This module only reads game memory and computes plans; it never posts
// input or calls game code. The navigator executes the returned mouse steps
// with the same hover-then-click path it uses for buttons.
//
// Every offset below is static evidence from Ghidra decompilation of the
// Steam Bejeweled3.exe (SHA-256 d5516244...88c3). Function addresses and
// what they show are recorded in docs/re/controls.md. None of these offsets
// has been confirmed at runtime yet; the lead's runtime checklist is in that
// document.

#include <cstdint>
#include <string>
#include <vector>

namespace controls {

// ---------------------------------------------------------------------------
// Offsets (static). Kept here rather than in sexy.h until runtime-verified.
// ---------------------------------------------------------------------------
namespace off {
// Checkbox (ctor FUN_005a14e0, SetChecked FUN_005a15b0, IsChecked FUN_005a15f0).
constexpr int kCheckboxListener = 0xBC;  // CheckboxListener*
constexpr int kCheckboxId = 0xC0;        // int
constexpr int kCheckboxChecked = 0xC4;   // bool

// Slider (ctor FUN_005a26a0, SetValue FUN_005a2730, MouseDown FUN_005a2aa0,
// MouseDrag FUN_005a2c60, MouseUp FUN_005a2d50, Draw FUN_005a2780).
constexpr int kSliderListener = 0xBC;    // SliderListener*
constexpr int kSliderValue = 0xC0;       // double, clamped to [0, 1]
constexpr int kSliderId = 0xC8;          // int
constexpr int kSliderTrackImage = 0xCC;  // Image*
constexpr int kSliderThumbImage = 0xD0;  // Image*
constexpr int kSliderDragging = 0xD4;    // bool
constexpr int kSliderRelX = 0xD8;        // int
constexpr int kSliderRelY = 0xDC;        // int
constexpr int kSliderHorizontal = 0xE0;  // bool, ctor default 1

// Image (accessors FUN_0045b1b0/1c0/1d0/1e0).
constexpr int kImageWidth = 0x2C;
constexpr int kImageHeight = 0x30;
constexpr int kImageNumRows = 0x34;
constexpr int kImageNumCols = 0x38;

// ListWidget (ctor FUN_0059bc40, MouseMove FUN_0059d740, MouseDown
// FUN_0059d860, SetHilite FUN_0059d710, SetSelect FUN_0059d900,
// GetLineCount FUN_0059cb90, GetStringAt FUN_0059c520).
constexpr int kListId = 0xC0;             // int (first ListClicked argument)
constexpr int kListFont = 0xC4;           // Font*
constexpr int kListScrollbar = 0xC8;      // ScrollbarWidget*
constexpr int kListWheelStep = 0xD0;      // int, lines per wheel notch (FUN_0059d940)
constexpr int kListLinesBegin = 0xE0;     // std::wstring* (std::vector<SexyString>)
constexpr int kListLinesEnd = 0xE4;
constexpr int kListLineSize = 0x1C;       // sizeof(std::wstring), MSVC 2008
constexpr int kListPosition = 0x108;      // double, first visible line
constexpr int kListPageSize = 0x110;      // double
constexpr int kListHilite = 0x118;        // int, -1 none
constexpr int kListSelect = 0x11C;        // int, -1 none
constexpr int kListListener = 0x120;      // ListListener*
constexpr int kListItemHeight = 0x134;    // int, -1 means "use font height"
constexpr int kListTopInset = 4;          // MouseMove subtracts 4 from y

// DropListWidget (ctor FUN_0067a1f0, MouseDown FUN_00673300, its
// ListListener::ListClicked FUN_006732b0).
constexpr int kDropId = 0xC0;
constexpr int kDropMaxRows = 0xC8;        // int, ctor 6
constexpr int kDropSelected = 0xCC;       // int, -1 none
constexpr int kDropList = 0x130;          // ListWidget* (child; visible = open)
constexpr int kDropListener = 0x134;      // ListListener*

// ScrollbarWidget (ctor FUN_0059ffe0, SetValue FUN_005a0500,
// ButtonPress FUN_005a0c40).
constexpr int kScrollUpButton = 0xC0;     // ScrollbuttonWidget*
constexpr int kScrollDownButton = 0xC4;   // ScrollbuttonWidget*
constexpr int kScrollId = 0xCC;
constexpr int kScrollValue = 0xD0;        // double
constexpr int kScrollMax = 0xD8;          // double
constexpr int kScrollPageSize = 0xE0;     // double
constexpr int kScrollHorizontal = 0xE8;   // bool
constexpr int kScrollListener = 0x114;    // ScrollListener*

// OptionsDialog (ctor FUN_00681370; CheckboxChecked FUN_00684ae0 with
// this = dialog + 0x22C; SliderVal FUN_00673a40; slider release FUN_006740a0).
constexpr int kOptCheckNormalRes = 0x230;   // id 8
constexpr int kOptCheckHighRes = 0x234;     // id 9
constexpr int kOptCheckUltraRes = 0x238;    // id 10
constexpr int kOptCheck3D = 0x23C;          // id 4
constexpr int kOptCheckFullscreen = 0x240;  // id 5
constexpr int kOptCheckCursors = 0x244;     // id 6
constexpr int kOptCheckAnimBg = 0x248;      // id 7
constexpr int kOptHowToPlayButton = 0x24C;
constexpr int kOptSliderMusic = 0x250;      // id 0
constexpr int kOptSliderSound = 0x254;      // id 1
constexpr int kOptSliderVoices = 0x258;     // id 2

// RecordsScreen (ctor FUN_006a05c0, tab switch FUN_0069c760).
constexpr int kRecordsTabPages = 0xC4;     // 3 x TabPage*: HighScores, Badges, Stats
constexpr int kRecordsTabButtons = 0xD0;   // 3 x Bej3Button*, ids 0..2

// HighScoresTab (ctor FUN_0069e320, mode select FUN_0069c800,
// Update FUN_0069cb10, mode buttons FUN_0069d440).
constexpr int kHsModeButtons = 0xC0;       // 6 x Bej3Button*, ids 0..5
constexpr int kHsBoards = 0xD8;            // 6 x score source objects
constexpr int kHsEntryWidgets = 0xF0;      // 10 x ScoreEntryWidget*
constexpr int kHsEntryCount = 10;
constexpr int kHsCurrentMode = 0x118;      // int 0..5

// ScoreEntryWidget (ctor FUN_0069bfb0, Draw FUN_0069c230) and its entry.
constexpr int kScoreEntryData = 0xC4;      // entry object*, 0 = empty row
constexpr int kEntryName = 0x04;           // std::wstring
constexpr int kEntryRank = 0x20;           // int, drawn with string 448 "%d."
constexpr int kEntryScore = 0x24;          // int, drawn comma-separated
}  // namespace off

// ---------------------------------------------------------------------------
// Classification
// ---------------------------------------------------------------------------
enum class Kind {
    None,
    Checkbox,
    Slider,
    List,        // ListWidget (also the child list of an open DropListWidget)
    DropList,    // DropListWidget
    Scrollbar,   // ScrollbarWidget
    TabButton,   // RecordsScreen tab button or HighScoresTab mode button
};

// Uses RTTI (IsKindOf). Button-derived widgets return TabButton only when
// their parent is a RecordsScreen or HighScoresTab; otherwise None (the
// navigator handles ordinary buttons).
Kind Classify(uintptr_t widget);

// ---------------------------------------------------------------------------
// State readers. All return false when any needed read fails; callers must
// then announce "unknown", never a default.
// ---------------------------------------------------------------------------
struct CheckboxState {
    int id = -1;
    bool checked = false;
};
bool ReadCheckbox(uintptr_t widget, CheckboxState* out);

struct SliderState {
    int id = -1;
    double value = 0.0;        // 0..1
    bool horizontal = true;
    bool dragging = false;
    int width = 0, height = 0; // widget size (logical)
    int thumbCelW = 0, thumbCelH = 0;  // 0 if the slider has no thumb image
};
bool ReadSlider(uintptr_t widget, SliderState* out);

// Rounded percentage for speech ("40 percent"); value is clamped to 0..1.
int SliderPercent(double value);

struct ListState {
    int id = -1;
    int count = 0;
    int hilite = -1;
    int select = -1;
    double position = 0.0;
    double pageSize = 0.0;
    int itemHeight = -1;       // -1: font height (not readable without a call)
    int width = 0, height = 0;
};
bool ReadList(uintptr_t widget, ListState* out);
// Line text (raw, may contain ^RRGGBB^ colour codes). False if unreadable.
bool ReadListLine(uintptr_t widget, int index, std::wstring* out);

struct DropListState {
    int id = -1;
    int selected = -1;
    bool open = false;         // child list visible
    uintptr_t list = 0;        // child ListWidget
};
bool ReadDropList(uintptr_t widget, DropListState* out);

struct ScrollbarState {
    int id = -1;
    double value = 0.0, maxValue = 0.0, pageSize = 0.0;
    bool horizontal = false;
    uintptr_t upButton = 0, downButton = 0;
};
bool ReadScrollbar(uintptr_t widget, ScrollbarState* out);

// Tab groups. The game marks the current tab by disabling its button
// (FUN_0069c760 / FUN_0069c800), so the navigator must not skip disabled
// TabButtons; it should speak them as "selected".
struct TabButtonState {
    int index = -1;            // button id (0-based position in its group)
    int count = 0;             // 3 (records tabs) or 6 (high score modes)
    bool selected = false;     // button disabled = current tab
    int labelStringId = -1;    // string-table id for records tabs, else -1
};
bool ReadTabButton(uintptr_t widget, TabButtonState* out);

// Current tab of a RecordsScreen (0 high scores, 1 badges, 2 stats) by the
// visible tab page; -1 if unknown.
int RecordsCurrentTab(uintptr_t recordsScreen);
// Current game mode of a HighScoresTab (0..5), -1 if unknown.
int HighScoresCurrentMode(uintptr_t highScoresTab);

struct ScoreRow {
    bool present = false;      // false: the row has no entry (empty)
    int rank = 0;
    int score = 0;
    std::wstring name;
};
// Reads one ScoreEntryWidget. False if a read failed.
bool ReadScoreEntry(uintptr_t scoreEntryWidget, ScoreRow* out);
// Reads the 10 rows of a HighScoresTab in display order.
bool ReadHighScores(uintptr_t highScoresTab, std::vector<ScoreRow>* out);

// ---------------------------------------------------------------------------
// Labels: string-table ids from properties/default.xml for controls whose
// label the dialog draws itself. -1 when unknown. dialogClass is the RTTI
// class name of the owning dialog ("OptionsDialog" or "ZenOptionsDialog").
// ---------------------------------------------------------------------------
int LabelStringId(const std::string& dialogClass, Kind kind, int controlId);
// Group heading drawn above a control (OptionsDialog string 315 for the
// three resolution checkboxes; ZenOptionsDialog category titles). -1 if none.
int GroupStringId(const std::string& dialogClass, Kind kind, int controlId);
// True for the OptionsDialog resolution checkboxes (ids 8, 9, 10), which act
// as one radio group: the game keeps exactly one checked.
bool IsRadioCheckbox(const std::string& dialogClass, int controlId);

// ---------------------------------------------------------------------------
// Action plans, in absolute logical coordinates (the snapshot's absX/absY).
// ---------------------------------------------------------------------------
struct MouseStep {
    enum Type { Move, Down, Up } type;
    int x, y;                  // absolute logical coordinates
    // Move only: wait until this widget reports mIsOver (+0x69) before the
    // next step, as the navigator already does for buttons. 0 = no wait.
    uintptr_t waitHover;
};

// Checkbox: Checkbox::MouseDown toggles on button DOWN and notifies the
// listener (FUN_005a1740). One click at the centre toggles exactly once.
std::vector<MouseStep> PlanCheckboxToggle(uintptr_t widget, int absX, int absY,
                                          int width, int height);

// Slider: plans the mouse path that makes the game itself set the value.
// Click on the track (outside the thumb) sets value = localX / width; a
// press on the thumb starts a drag where value = (localX - relX) /
// (width - thumbW). The plan uses a track click when that reaches the target
// and a drag otherwise (target 1.0, or a target under the current thumb).
// predicted receives the value the game should end with. False if the
// geometry is unusable.
bool PlanSliderSet(uintptr_t widget, const SliderState& s, int absX, int absY,
                   double target, std::vector<MouseStep>* steps, double* predicted);

// List: click on row index (must be on the visible page). False when the row
// is not visible or the item height is unknown; then scroll first.
bool PlanListClick(uintptr_t widget, const ListState& s, int absX, int absY, int index,
                   std::vector<MouseStep>* steps);

// Pure helpers exposed for checking the maths.
int SliderThumbPos(double value, int trackLen, int thumbLen);  // ftol(value*(len-thumb))
int ListRowAt(int localY, int itemHeight, double position);    // -1 if y above rows

// ---------------------------------------------------------------------------
// Coordinate mapping. widgets.h LogicalToClient truncates, which can land one
// client pixel before the logical pixel when the client is scaled up; this
// maps to the centre of the logical pixel instead. False if unreadable.
// ---------------------------------------------------------------------------
bool LogicalToClientCentre(uintptr_t widgetManager, int lx, int ly, int* cx, int* cy);

}  // namespace controls
