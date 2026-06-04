#include "AccessibilityManager.h"

#include "GameBridge.h"
#include "ScreenReaderService.h"
#include "AccessibilityCVars.h"
#include "SettingsMenu.h"

#include <libultraship.h>

extern "C" {
#include <defines.h>
}

// Re-applies the streamed-music volume from gMainMusicVolume (port/audio/HMAS.cpp).
extern "C" void HMAS_RefreshMusicVolume(void);
// Sets the rival-kart audio volume scale (src/audio/external.c).
extern "C" void Accessibility_SetRivalKartVolume(float volume);

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
    // Advance any running Help cue demo (independent of the narration state).
    SettingsMenu_TickDemo();

    // Keep the streamed (HMAS) music at the user's saved gMainMusicVolume. The
    // title/menu track can start before the config is loaded, so it would otherwise
    // play at full volume until the slider is touched. This is idempotent and
    // re-applies the current base volume (so the game's own ducks/fades still work).
    HMAS_RefreshMusicVolume();

    // Keep the rival-kart volume scale in sync with the saved setting (applied to
    // rival kart sounds during races; persists via the CVar, applied live here).
    Accessibility_SetRivalKartVolume(CVarGetFloat("gAccessibility.RivalKartVolume", 1.0f));

    if (!Enabled()) {
        mDriveAssist.Reset(); // recenter game audio if disabled mid-race
        return;
    }

    EnsureScreenReaderInitialized();

    ScreenReaderService& reader = ScreenReaderService::Instance();
    if (!reader.IsAvailable()) {
        mDriveAssist.Reset();
        return;
    }

    if (gGamestate == RACING) {
        mMenuNarrator.Reset();
        mPostRaceNarrator.Reset();
        if (mPauseNarrator.MenuActive()) {
            // An in-race overlay menu is up (pause, or the end-course/replay menu):
            // silence the driving cues and narrate the menu.
            mRaceNarrator.Reset();
            mDriveAssist.Reset();
            if (CVarGetInteger(CVAR_ACCESS_MENU_NARRATION, CVAR_ACCESS_MENU_NARRATION_DEFAULT) != 0) {
                mPauseNarrator.Tick(reader);
            }
        } else {
            // Active race: narrate live race state and drive the blind drive assist.
            mPauseNarrator.Reset();
            if (CVarGetInteger(CVAR_ACCESS_RACE_NARRATION, CVAR_ACCESS_RACE_NARRATION_DEFAULT) != 0) {
                mRaceNarrator.Tick(reader);
            }
            if (CVarGetInteger(CVAR_ACCESS_DRIVE_ASSIST, CVAR_ACCESS_DRIVE_ASSIST_DEFAULT) != 0) {
                mDriveAssist.Tick(reader);
            } else {
                mDriveAssist.Reset();
            }
        }
    } else if (gGamestate != ENDING && gGamestate != CREDITS_SEQUENCE) {
        // Front-end contexts: narrate menu navigation.
        mPauseNarrator.Reset();
        mPostRaceNarrator.Reset();
        mRaceNarrator.Reset();
        if (!SettingsMenu_DemoActive()) {
            mDriveAssist.Reset(); // skip while a cue demo plays so its held edge tone survives
        }
        if (CVarGetInteger(CVAR_ACCESS_MENU_NARRATION, CVAR_ACCESS_MENU_NARRATION_DEFAULT) != 0) {
            mMenuNarrator.Tick(reader);
        }
    } else {
        // Post-race sequences (ENDING / CREDITS).
        mMenuNarrator.Reset();
        mPauseNarrator.Reset();
        mRaceNarrator.Reset();
        mDriveAssist.Reset();
        if (gGamestate == ENDING &&
            CVarGetInteger(CVAR_ACCESS_MENU_NARRATION, CVAR_ACCESS_MENU_NARRATION_DEFAULT) != 0) {
            mPostRaceNarrator.Tick(reader);
        } else {
            mPostRaceNarrator.Reset();
        }
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
