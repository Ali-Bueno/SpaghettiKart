#include "MenuNarrator.h"

#include "GameBridge.h"
#include "ScreenReaderService.h"
#include "AccessibilityStrings.h"

#include <string>

extern "C" {
#include <defines.h>
}

using namespace AccessibilityStrings;

void MenuNarrator::Reset() {
    mLastScreen = -1;
    mLastSelection = kNoSelection;
}

const char* MenuNarrator::ScreenName(int menuScreen) const {
    switch (menuScreen) {
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

int MenuNarrator::CurrentSelection(int menuScreen) const {
    switch (menuScreen) {
        case MAIN_MENU:
            return gMainMenuSelection;
        case CHARACTER_SELECT_MENU:
            return gPlayerSelectMenuSelection;
        case OPTIONS_MENU:
        case DATA_MENU:
            return gSubMenuSelection;
        default:
            // Screens whose cursor variable is not yet mapped: announce the
            // screen on entry but do not track per-option movement.
            return kNoSelection;
    }
}

const char* MenuNarrator::OptionName(int menuScreen, int selection) const {
    if (menuScreen == MAIN_MENU) {
        switch (selection) {
            case GRAND_PRIX:
                return MODE_GRAND_PRIX;
            case TIME_TRIALS:
                return MODE_TIME_TRIALS;
            case VERSUS:
                return MODE_VERSUS;
            case BATTLE:
                return MODE_BATTLE;
            default:
                break;
        }
    }
    return nullptr;
}

void MenuNarrator::Tick(ScreenReaderService& reader) {
    const int screen = gMenuSelection;
    const int selection = CurrentSelection(screen);

    // Screen changed: announce the screen, plus the currently highlighted option.
    if (screen != mLastScreen) {
        mLastScreen = screen;
        mLastSelection = selection;

        const char* name = ScreenName(screen);
        std::string message = (name != nullptr) ? name : MENU_GENERIC;

        if (selection != kNoSelection) {
            const char* option = OptionName(screen, selection);
            if (option != nullptr) {
                message += ". ";
                message += option;
            }
        }
        reader.Speak(message, true);
        return;
    }

    // Same screen, cursor moved: announce the new option.
    if (selection != kNoSelection && selection != mLastSelection) {
        mLastSelection = selection;
        const char* option = OptionName(screen, selection);
        if (option != nullptr) {
            reader.Speak(option, true);
        } else {
            reader.Speak("Option " + std::to_string(selection + 1), true);
        }
    }
}
