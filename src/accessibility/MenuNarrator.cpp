#include "MenuNarrator.h"

#include "GameBridge.h"
#include "ScreenReaderService.h"
#include "AccessibilityStrings.h"

#include <string>

#include <defines.h>

using namespace AccessibilityStrings;

namespace {

// Mirrors enum MainMenuSelectionType in menus.h (the MAIN_MENU sub-state).
enum {
    MM_NONE = 0,
    MM_OPTION = 1,
    MM_DATA = 2,
    MM_PLAYER_SELECT = 3,
    MM_MODE_SELECT = 4,
    MM_MODE_SUB_SELECT = 5,
    MM_OK_SELECT = 6,
    MM_OK_SELECT_GO_BACK = 7,
    MM_MODE_SUB_SELECT_GO_BACK = 8,
};

// Mirrors the sub-menu values used on the course-select and options screens
// (enum in menus.h).
enum {
    SUB_MAP_CUP = 0x01,
    SUB_MAP_COURSE = 0x02,
    SUB_MAP_OK = 0x03,
    SUB_MAP_BATTLE_COURSE = 0x04,
    SUB_OPT_ACCESSIBILITY = 0x15,
    SUB_OPT_SOUND = 0x16,
    SUB_OPT_COPY_PAK = 0x17,
    SUB_OPT_ERASE_ALL = 0x18,
    SUB_OPT_RETURN = 0x19,
};

// Grid position (1-8) to character id. sCharacterGridOrder is static in
// menus.c, so the mapping is reproduced here.
const int kGridToCharacter[8] = { MARIO, LUIGI, PEACH, TOAD, YOSHI, DK, WARIO, BOWSER };

} // namespace

void MenuNarrator::Reset() {
    mLastScreen = -1;
    mLastAnnouncement.clear();
}

const char* MenuNarrator::ScreenName(int screen) const {
    switch (screen) {
        case HARBOUR_MASTERS_MENU:
            return MENU_HARBOUR_MASTERS;
        case LOGO_INTRO_MENU:
            return MENU_LOGO_INTRO;
        case START_MENU:
            return MENU_START;
        case MAIN_MENU:
            return MENU_MAIN;
        case CHARACTER_SELECT_MENU:
            return MENU_CHARACTER_SELECT;
        case COURSE_SELECT_MENU:
            return MENU_COURSE_SELECT;
        case OPTIONS_MENU:
            return MENU_OPTIONS;
        case DATA_MENU:
            return MENU_DATA;
        case COURSE_DATA_MENU:
            return MENU_COURSE_DATA;
        case CONTROLLER_PAK_MENU:
            return MENU_CONTROLLER_PAK;
        default:
            return nullptr;
    }
}

std::string MenuNarrator::BuildItemAnnouncement(int screen) const {
    switch (screen) {
        case MAIN_MENU: {
            const int sub = gMainMenuSelection;
            const int pc = gPlayerCount; // 1..4
            switch (sub) {
                case MM_PLAYER_SELECT:
                    if (pc >= 1 && pc <= 4) {
                        return std::to_string(pc) + (pc == 1 ? " player" : " players");
                    }
                    return "";
                case MM_MODE_SELECT: {
                    if (pc < 1 || pc > 4) {
                        return "";
                    }
                    const int col = gGameModeMenuColumn[pc - 1];
                    if (col < 0 || col > 2) {
                        return "";
                    }
                    const int mode = gGameModePlayerSelection[pc - 1][col];
                    if (mode >= 0 && mode < 4) {
                        return MODES[mode];
                    }
                    return "";
                }
                case MM_MODE_SUB_SELECT:
                case MM_MODE_SUB_SELECT_GO_BACK: {
                    if (pc < 1 || pc > 4) {
                        return "";
                    }
                    const int col = gGameModeMenuColumn[pc - 1];
                    if (col < 0 || col > 2) {
                        return "";
                    }
                    const int mode = gGameModePlayerSelection[pc - 1][col];
                    const int subValue = gGameModeSubMenuColumn[pc - 1][col];
                    if (mode == GRAND_PRIX || mode == VERSUS) {
                        if (subValue >= 0 && subValue < 4) {
                            return CC_CLASSES[subValue];
                        }
                    } else if (mode == TIME_TRIALS) {
                        return subValue == 0 ? "Begin" : "Data";
                    }
                    return "";
                }
                case MM_OK_SELECT:
                case MM_OK_SELECT_GO_BACK:
                    return "OK";
                case MM_OPTION:
                    return "Options";
                case MM_DATA:
                    return "Data";
                default:
                    return "";
            }
        }
        case CHARACTER_SELECT_MENU: {
            // Player 1's cursor (1P focus); 1..8, 0 = none.
            const int pos = gCharacterGridSelections[0];
            if (pos >= 1 && pos <= 8) {
                return CHARACTERS[kGridToCharacter[pos - 1]];
            }
            return "";
        }
        case COURSE_SELECT_MENU: {
            const int sub = gSubMenuSelection;
            if (sub == SUB_MAP_CUP) {
                // Cup browsing updates the World cup index, not gCupSelection.
                const int cup = static_cast<int>(GetCupIndex());
                if (cup >= 0 && cup < 5) {
                    return CUPS[cup];
                }
                const char* name = GetCupName();
                return name != nullptr ? std::string(name) : "";
            }
            if (sub == SUB_MAP_COURSE || sub == SUB_MAP_BATTLE_COURSE) {
                // gCurrentCourseId is the live resolved track id.
                const int track = gCurrentCourseId;
                if (track >= 0 && track < 20) {
                    return TRACKS[track];
                }
                return "";
            }
            if (sub == SUB_MAP_OK) {
                return "OK";
            }
            return "";
        }
        case OPTIONS_MENU: {
            // Native Options rows. The accessible settings categories (Accessibility,
            // Sound) park gSubMenuSelection at SUB_MENU_MOD_SETTINGS while open, which
            // has no case here, so MenuNarrator stays silent and the settings module is
            // the sole narrator inside a category.
            switch (gSubMenuSelection) {
                case SUB_OPT_ACCESSIBILITY:
                    return "Accessibility";
                case SUB_OPT_SOUND:
                    return "Sound";
                case SUB_OPT_COPY_PAK:
                    return "Copy Controller Pak";
                case SUB_OPT_ERASE_ALL:
                    return "Erase all data";
                case SUB_OPT_RETURN:
                    return "Return to game select";
                default:
                    return "";
            }
        }
        default:
            return "";
    }
}

void MenuNarrator::Tick(ScreenReaderService& reader) {
    const int screen = gMenuSelection;
    std::string item = BuildItemAnnouncement(screen);

    // Screen changed: announce the screen name and the current item.
    if (screen != mLastScreen) {
        mLastScreen = screen;
        mLastAnnouncement = item;

        const char* name = ScreenName(screen);
        std::string message = (name != nullptr) ? name : "";
        if (!item.empty()) {
            if (!message.empty()) {
                message += ". ";
            }
            message += item;
        }
        if (!message.empty()) {
            reader.Speak(message, true);
        }
        return;
    }

    // Same screen, highlighted item changed: announce just the item.
    if (item != mLastAnnouncement) {
        mLastAnnouncement = item;
        if (!item.empty()) {
            reader.Speak(item, true);
        }
    }
}
