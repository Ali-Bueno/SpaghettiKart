#include "PauseNarrator.h"

#include "ScreenReaderService.h"
#include "AccessibilityStrings.h"

#include <cstdint>
#include <string>

extern "C" {
#include <defines.h> // GRAND_PRIX, TIME_TRIALS, VERSUS, BATTLE
}

// In-race menu game state. Declared locally (like RaceNarrator's externs) to keep
// the C++ side free of the heavy C-only menu headers; linkage is by symbol name.
extern "C" {
// Non-zero while the in-race pause menu is open (value = pausing player + 1).
extern uint16_t gIsGamePaused;
// Active game mode (GRAND_PRIX=0, TIME_TRIALS=1, VERSUS=2, BATTLE=3).
extern int32_t gModeSelection;
// Base cursor value of the pause MenuItem per game mode, indexed by gModeSelection.
extern const int8_t D_800F0B50[];

// Minimal mirror of MenuItem (menu_items.h): we only read .state, which sits at
// the same offset. Reading it through this leading-fields struct is safe because
// linkage is by symbol name (same convention as GameBridge.h).
typedef struct {
    int32_t type;
    int32_t state;
} AccessMenuItem;
extern AccessMenuItem* find_menu_items(int32_t type);
}

using namespace AccessibilityStrings;

namespace {

// MenuItem type ids (menu_items.h).
constexpr int32_t kMenuItemPause = 0xC7;        // MENU_ITEM_PAUSE
constexpr int32_t kMenuItemEndCourse = 0xBD;    // MENU_ITEM_END_COURSE_OPTION
constexpr int32_t kMenuItemTimeTrialEnd = 0xBA; // MENU_ITEM_TYPE_0BA (time trial finish menu)
constexpr int32_t kMenuItemGpResults = 0xAC;    // MENU_ITEM_TYPE_0AC (GP finished: Retry / Quit)

// The Grand Prix "finished" menu (0xAC, func_800ACA14) shows two options drawn from
// gTextPauseButton (Retry / Quit). The cursor lives in .state: 0x0B = Retry, 0x0C = Quit.
constexpr int kGpResultRetry = 0x0B;
constexpr int kGpResultQuit = 0x0C;

// Both finish menus share the same option layout (gTextPauseButton[idx + 1] = Retry,
// Course change, Driver change, Quit, Replay, Save ghost) but the cursor's base state
// value differs; below the range the item is in its intro/animation phase:
//   - Time trial finish menu (0xBA): cursor state 0x05..0x0A (selected = state - 5)
//   - End-course option menu (0xBD): cursor state 0x0B..0x10 (selected = state - 0x0B)
constexpr int kEndCourseMin = 0x0B;
constexpr int kEndCourseMax = 0x10;
constexpr int kTimeTrialEndMin = 0x05;
constexpr int kTimeTrialEndMax = 0x0A;

// Save-ghost sub-states of the time-trial finish menu (0xBA), reached after choosing
// "Save ghost": two save slots (the cursor lives in .state), then a Yes/No overwrite
// confirmation. These were unread, so the player couldn't pick a slot.
constexpr int kGhostSlot1 = 0x11;
constexpr int kGhostSlot2 = 0x12;
constexpr int kGhostOverwriteNo = 0x14;
constexpr int kGhostOverwriteYes = 0x15;

inline bool IsGhostSaveState(int state) {
    return state == kGhostSlot1 || state == kGhostSlot2 || state == kGhostOverwriteNo ||
           state == kGhostOverwriteYes;
}

// gTextPauseButton indices (TEXT_MENU_ID in menu_items.h), used to index
// PAUSE_OPTIONS[].
enum {
    OPT_CONTINUE = 0,
    OPT_RETRY = 1,
    OPT_COURSE_CHANGE = 2,
    OPT_DRIVER_CHANGE = 3,
    OPT_QUIT = 4,
};

} // namespace

void PauseNarrator::Reset() {
    mLastKind = 0;
    mLastAnnouncement.clear();
}

std::string PauseNarrator::PauseOption() const {
    const AccessMenuItem* item = find_menu_items(kMenuItemPause);
    if (item == nullptr) {
        return "";
    }

    const int mode = gModeSelection;
    if (mode < 0 || mode > 3) {
        return "";
    }

    // Item index within this mode's option list (0-based). The pause MenuItem
    // briefly holds state 0 before the game seeds it to the mode's base value.
    const int idx = item->state - D_800F0B50[mode];
    if (idx < 0) {
        return "";
    }

    // Map the per-mode option index to a PAUSE_OPTIONS entry. The layouts mirror
    // the render_pause_menu_* functions in menu_items.c.
    int optionId = -1;
    switch (mode) {
        case GRAND_PRIX: {
            static const int kOptions[] = { OPT_CONTINUE, OPT_QUIT };
            if (idx < 2) {
                optionId = kOptions[idx];
            }
            break;
        }
        case TIME_TRIALS: {
            static const int kOptions[] = { OPT_CONTINUE, OPT_RETRY, OPT_COURSE_CHANGE, OPT_DRIVER_CHANGE, OPT_QUIT };
            if (idx < 5) {
                optionId = kOptions[idx];
            }
            break;
        }
        case VERSUS:
        case BATTLE: {
            static const int kOptions[] = { OPT_CONTINUE, OPT_COURSE_CHANGE, OPT_DRIVER_CHANGE, OPT_QUIT };
            if (idx < 4) {
                optionId = kOptions[idx];
            }
            break;
        }
        default:
            break;
    }

    if (optionId < 0) {
        return "";
    }
    return PAUSE_OPTIONS[optionId];
}

std::string PauseNarrator::EndCourseOption() const {
    // Time trial finish menu (0xBA): func_800A3E60 highlights option (state - 5) and
    // draws gTextPauseButton[option + 1].
    const AccessMenuItem* tt = find_menu_items(kMenuItemTimeTrialEnd);
    if (tt != nullptr && tt->state >= kTimeTrialEndMin && tt->state <= kTimeTrialEndMax) {
        const int optionId = (tt->state - kTimeTrialEndMin) + 1;
        if (optionId >= 0 && optionId < 7) {
            return PAUSE_OPTIONS[optionId];
        }
    }
    // Save-ghost sub-menu (same 0xBA item): the save-slot picker and overwrite confirmation.
    if (tt != nullptr) {
        switch (tt->state) {
            case kGhostSlot1: return std::string(GHOST_SLOT_PREFIX) + "1";
            case kGhostSlot2: return std::string(GHOST_SLOT_PREFIX) + "2";
            case kGhostOverwriteNo: return GHOST_OVERWRITE_NO;
            case kGhostOverwriteYes: return GHOST_OVERWRITE_YES;
            default: break;
        }
    }
    // End-course option menu (0xBD): render_menu_item_end_course_option draws
    // gTextPauseButton[idx + 1] for idx = state - 0x0B.
    const AccessMenuItem* ec = find_menu_items(kMenuItemEndCourse);
    if (ec != nullptr && ec->state >= kEndCourseMin && ec->state <= kEndCourseMax) {
        const int optionId = (ec->state - kEndCourseMin) + 1;
        if (optionId >= 0 && optionId < 7) {
            return PAUSE_OPTIONS[optionId];
        }
    }
    // Grand Prix finished menu (0xAC): Retry / Quit.
    const AccessMenuItem* gp = find_menu_items(kMenuItemGpResults);
    if (gp != nullptr) {
        if (gp->state == kGpResultRetry) {
            return PAUSE_OPTIONS[OPT_RETRY];
        }
        if (gp->state == kGpResultQuit) {
            return PAUSE_OPTIONS[OPT_QUIT];
        }
    }
    return "";
}

bool PauseNarrator::MenuActive() const {
    if (gIsGamePaused != 0) {
        return true;
    }
    const AccessMenuItem* tt = find_menu_items(kMenuItemTimeTrialEnd);
    if (tt != nullptr &&
        ((tt->state >= kTimeTrialEndMin && tt->state <= kTimeTrialEndMax) || IsGhostSaveState(tt->state))) {
        return true;
    }
    const AccessMenuItem* ec = find_menu_items(kMenuItemEndCourse);
    if (ec != nullptr && ec->state >= kEndCourseMin && ec->state <= kEndCourseMax) {
        return true;
    }
    const AccessMenuItem* gp = find_menu_items(kMenuItemGpResults);
    return gp != nullptr && (gp->state == kGpResultRetry || gp->state == kGpResultQuit);
}

std::string PauseNarrator::CurrentOption(int* kind) const {
    if (gIsGamePaused != 0) {
        *kind = 1;
        return PauseOption();
    }
    const std::string endOption = EndCourseOption();
    if (!endOption.empty()) {
        *kind = 2;
        return endOption;
    }
    *kind = 0;
    return "";
}

void PauseNarrator::Tick(ScreenReaderService& reader) {
    int kind = 0;
    const std::string option = CurrentOption(&kind);

    // Menu just opened or switched: announce the screen name plus the current item.
    if (kind != mLastKind) {
        mLastKind = kind;
        mLastAnnouncement = option;
        if (kind == 0) {
            return; // menu closed
        }

        std::string message = (kind == 1) ? PAUSE_MENU : PAUSE_END_MENU;
        if (!option.empty()) {
            message += ". ";
            message += option;
        }
        reader.Speak(message, true);
        return;
    }

    // Same menu, highlighted option changed: announce just the new item.
    if (option != mLastAnnouncement) {
        mLastAnnouncement = option;
        if (!option.empty()) {
            reader.Speak(option, true);
        }
    }
}
