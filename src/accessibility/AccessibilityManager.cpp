#include "AccessibilityManager.h"

#include "GameBridge.h"
#include "ScreenReaderService.h"
#include "AccessibilityCVars.h"

#include <libultraship.h>

extern "C" {
#include <defines.h>
}

AccessibilityManager& AccessibilityManager::Instance() {
    static AccessibilityManager instance;
    return instance;
}

bool AccessibilityManager::Enabled() const {
    return CVarGetInteger(CVAR_ACCESS_ENABLED, CVAR_ACCESS_ENABLED_DEFAULT) != 0;
}

void AccessibilityManager::EnsureScreenReaderInitialized() {
    if (mScreenReaderInitTried) {
        return;
    }
    mScreenReaderInitTried = true;

    if (CVarGetInteger(CVAR_ACCESS_SCREEN_READER, CVAR_ACCESS_SCREEN_READER_DEFAULT) != 0) {
        ScreenReaderService::Instance().Initialize();
    }
}

void AccessibilityManager::Tick() {
    if (!Enabled()) {
        return;
    }

    EnsureScreenReaderInitialized();

    ScreenReaderService& reader = ScreenReaderService::Instance();
    if (!reader.IsAvailable()) {
        return;
    }

    if (gGamestate == RACING) {
        // Active race: narrate live race state.
        mMenuNarrator.Reset();
        if (CVarGetInteger(CVAR_ACCESS_RACE_NARRATION, CVAR_ACCESS_RACE_NARRATION_DEFAULT) != 0) {
            mRaceNarrator.Tick(reader);
        }
    } else if (gGamestate != ENDING && gGamestate != CREDITS_SEQUENCE) {
        // Front-end contexts: narrate menu navigation.
        mRaceNarrator.Reset();
        if (CVarGetInteger(CVAR_ACCESS_MENU_NARRATION, CVAR_ACCESS_MENU_NARRATION_DEFAULT) != 0) {
            mMenuNarrator.Tick(reader);
        }
    } else {
        // Post-race sequences (ENDING / CREDITS): nothing yet.
        mMenuNarrator.Reset();
        mRaceNarrator.Reset();
    }
}

void AccessibilityManager::Shutdown() {
    ScreenReaderService::Instance().Shutdown();
}

extern "C" void Accessibility_Tick(void) {
    AccessibilityManager::Instance().Tick();
}

extern "C" void Accessibility_Shutdown(void) {
    AccessibilityManager::Instance().Shutdown();
}
