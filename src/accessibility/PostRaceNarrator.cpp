#include "PostRaceNarrator.h"

#include "ScreenReaderService.h"
#include "AccessibilityStrings.h"

#include <libultraship.h> // pull C++ headers (templates) before the extern "C" block

#include <cstdint>
#include <string>

extern "C" {
#include <common_structs.h> // Player
#include <defines.h>        // GRAND_PRIX, VERSUS, BATTLE
}

// Post-race game state. Declared locally (like the other narrators' externs) to
// keep this C++ TU free of the heavy C-only menu headers; linkage is by symbol.
extern "C" {
extern int32_t gModeSelection;
extern Player* gPlayerOne;

// Minimal mirror of MenuItem (menu_items.h): we only read .state. Reading it via
// this leading-fields struct is safe because linkage is by symbol name.
typedef struct {
    int32_t type;
    int32_t state;
} AccessMenuItem;
extern AccessMenuItem* find_menu_items(int32_t type);
}

using namespace AccessibilityStrings;

namespace {

// MenuItem type ids (menu_items.h).
constexpr int32_t kMenuItemVsBattle = 0xB0;  // MENU_ITEM_TYPE_0B0 (VS/Battle ranking + menu)
constexpr int32_t kMenuItemGpResults = 0xAA; // MENU_ITEM_TYPE_0AA (GP race results)

// The Versus/Battle menu cursor lives in MenuItem.state over this inclusive range
// (4 options); below it the screen is still animating in.
constexpr int kVbMenuMin = 0x0A;
constexpr int kVbMenuMax = 0x0D;

// gTextPauseButton index of the first VS/Battle option (Retry). The render draws
// gTextPauseButton[(state - 0x0A) + 1] = Retry, Course change, Driver change, Quit.
constexpr int kVbFirstOption = 1;

} // namespace

void PostRaceNarrator::Reset() {
    mLastKind = 0;
    mLastAnnouncement.clear();
    mLastGpRank = -1;
}

std::string PostRaceNarrator::VersusBattleOption() const {
    const AccessMenuItem* item = find_menu_items(kMenuItemVsBattle);
    if (item == nullptr || item->state < kVbMenuMin || item->state > kVbMenuMax) {
        return "";
    }
    const int optionId = (item->state - kVbMenuMin) + kVbFirstOption;
    if (optionId < 0 || optionId >= 7) {
        return "";
    }
    return PAUSE_OPTIONS[optionId];
}

void PostRaceNarrator::Tick(ScreenReaderService& reader) {
    const int mode = gModeSelection;

    // --- Versus / Battle: the selectable results menu --------------------------
    if (mode == VERSUS || mode == BATTLE) {
        const std::string option = VersusBattleOption();
        if (option.empty()) {
            // Menu not selectable yet (still animating): nothing to say.
            if (mLastKind == 1) {
                mLastKind = 0;
                mLastAnnouncement.clear();
            }
            return;
        }
        if (mLastKind != 1) {
            mLastKind = 1;
            mLastAnnouncement = option;
            reader.Speak(std::string(PAUSE_END_MENU) + ". " + option, true);
            return;
        }
        if (option != mLastAnnouncement) {
            mLastAnnouncement = option;
            reader.Speak(option, true);
        }
        return;
    }

    // --- Grand Prix: announce the finishing position once the results show -----
    if (mode == GRAND_PRIX) {
        const AccessMenuItem* results = find_menu_items(kMenuItemGpResults);
        // state >= 2 means the results have animated in and are on screen.
        if (results == nullptr || results->state < 2) {
            return;
        }
        const Player* player = gPlayerOne;
        if (player == nullptr) {
            return;
        }
        const int rank = player->currentRank; // 0-based: 0 = 1st
        if (rank == mLastGpRank) {
            return;
        }
        mLastGpRank = rank;
        mLastKind = 2;
        if (rank >= 0 && rank < 8) {
            reader.Speak(std::string(RACE_OVER) + ". " + POSITIONS[rank], true);
        }
    }
}
