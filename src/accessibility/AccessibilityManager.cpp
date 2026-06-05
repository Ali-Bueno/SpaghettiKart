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
// Race sub-state (RACE_IN_PROGRESS while the live race / a replay is running).
extern "C" int32_t gRaceState;
// Non-zero during the title-screen attract demo (a race the game plays by itself).
extern "C" uint16_t gDemoMode;

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

void AccessibilityManager::ApplyRecommendedDefaultsOnce() {
    if (mDefaultsChecked) {
        return;
    }
    mDefaultsChecked = true;

    // Seed the recommended starting values only once ever. The sentinel persists, so a
    // fresh install (or an existing one being updated) gets these defaults a single time
    // and any later change the player makes is preserved. Some testers reported they
    // follow the audio cues better with the music and rival karts quieter and a longer
    // steering-guide anticipation.
    if (CVarGetInteger(CVAR_ACCESS_DEFAULTS_APPLIED, 0) != 0) {
        return;
    }
    CVarSetFloat(CVAR_MAIN_MUSIC_VOLUME, CVAR_ACCESS_RECOMMENDED_MUSIC_VOLUME);
    CVarSetFloat(CVAR_ACCESS_RIVAL_VOLUME, CVAR_ACCESS_RECOMMENDED_RIVAL_VOLUME);
    CVarSetInteger(CVAR_ACCESS_DRIVE_LOOKAHEAD, CVAR_ACCESS_DRIVE_LOOKAHEAD_DEFAULT);
    CVarSetInteger(CVAR_ACCESS_DEFAULTS_APPLIED, 1);
    CVarSave();
}

void AccessibilityManager::Tick() {
    // Seed the recommended starting defaults the very first time we run.
    ApplyRecommendedDefaultsOnce();

    // Advance any running Help cue demo (independent of the narration state).
    SettingsMenu_TickDemo();

    // Keep the streamed (HMAS) music at the user's saved gMainMusicVolume. The
    // title/menu track can start before the config is loaded, so it would otherwise
    // play at full volume until the slider is touched. This is idempotent and
    // re-applies the current base volume (so the game's own ducks/fades still work).
    HMAS_RefreshMusicVolume();

    // Keep the rival-kart volume scale in sync with the saved setting (applied to
    // rival kart sounds during races; persists via the CVar, applied live here).
    Accessibility_SetRivalKartVolume(
        CVarGetFloat(CVAR_ACCESS_RIVAL_VOLUME, CVAR_ACCESS_RECOMMENDED_RIVAL_VOLUME));

    if (!Enabled()) {
        mDriveAssist.Reset(); // recenter game audio if disabled mid-race
        mItemBoxBeacon.Reset();
        return;
    }

    EnsureScreenReaderInitialized();

    // Do NOT bail out when the screen reader is unavailable: Speak() safely no-ops in
    // that case, but the non-speech aids (drive assist, item-box beacon, audio cues)
    // don't need a screen reader and should still run. This matters when PRISM can't
    // attach to the active reader (e.g. some JAWS setups) - the player still gets the
    // driving cues even if narration is silent.
    ScreenReaderService& reader = ScreenReaderService::Instance();

    if (gGamestate == RACING) {
        mMenuNarrator.Reset();
        mPostRaceNarrator.Reset();
        if (gDemoMode != DEMO_MODE_INACTIVE) {
            // Title-screen attract demo: the game plays a race by itself behind the
            // intro / "Press Start" screen. It is not the player's race, so stay fully
            // silent - no race narration, no driving cues. (A time-trial replay the
            // player asked for runs with gDemoMode inactive, so it still narrates.)
            mPauseNarrator.Reset();
            mRaceNarrator.Reset();
            mDriveAssist.Reset();
            mItemBoxBeacon.Reset();
        } else if (mPauseNarrator.MenuActive()) {
            // An in-race overlay menu is up (pause, or the end-course/replay menu):
            // silence the driving cues and narrate the menu.
            mRaceNarrator.Reset();
            mDriveAssist.Reset();
            mItemBoxBeacon.Reset();
            if (CVarGetInteger(CVAR_ACCESS_MENU_NARRATION, CVAR_ACCESS_MENU_NARRATION_DEFAULT) != 0) {
                mPauseNarrator.Tick(reader);
            }
        } else {
            // Active race: narrate live race state and drive the blind drive assist.
            mPauseNarrator.Reset();
            if (CVarGetInteger(CVAR_ACCESS_RACE_NARRATION, CVAR_ACCESS_RACE_NARRATION_DEFAULT) != 0) {
                mRaceNarrator.Tick(reader);
            }
            // The driving cues (steering guide + item-box beacon) must only play while
            // the race is actually being driven: after "Go!" and before the finish line
            // (and during a replay) - all of which run with gRaceState == RACE_IN_PROGRESS.
            // During the start intro/countdown and after crossing the line the kart still
            // exists but is not being raced, so the cues stay silent.
            if (gRaceState == RACE_IN_PROGRESS) {
                if (CVarGetInteger(CVAR_ACCESS_DRIVE_ASSIST, CVAR_ACCESS_DRIVE_ASSIST_DEFAULT) != 0) {
                    mDriveAssist.Tick(reader);
                } else {
                    mDriveAssist.Reset();
                }
                mItemBoxBeacon.Tick(); // has its own toggle (CVAR_ACCESS_ITEMBOX_CUE)
            } else {
                mDriveAssist.Reset();
                mItemBoxBeacon.Reset();
            }
        }
    } else if (gGamestate != ENDING && gGamestate != CREDITS_SEQUENCE) {
        // Front-end contexts: narrate menu navigation.
        mPauseNarrator.Reset();
        mPostRaceNarrator.Reset();
        mRaceNarrator.Reset();
        if (!SettingsMenu_DemoActive()) {
            mDriveAssist.Reset(); // skip while a cue demo plays so its held edge tone survives
            mItemBoxBeacon.Reset();
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
        mItemBoxBeacon.Reset();
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
