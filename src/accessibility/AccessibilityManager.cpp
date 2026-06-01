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

    // Front-end contexts: narrate menu navigation.
    // Active gameplay (RACING) and the post-race sequences are handled by later
    // phases (race telemetry, drive assist); reset the narrator so the next menu
    // entry is announced fresh.
    if (gGamestate != RACING && gGamestate != ENDING && gGamestate != CREDITS_SEQUENCE) {
        if (CVarGetInteger(CVAR_ACCESS_MENU_NARRATION, CVAR_ACCESS_MENU_NARRATION_DEFAULT) != 0) {
            mMenuNarrator.Tick(reader);
        }
    } else {
        mMenuNarrator.Reset();
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
