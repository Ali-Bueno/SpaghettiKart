#pragma once

#ifdef __cplusplus

#include "MenuNarrator.h"
#include "PauseNarrator.h"
#include "PostRaceNarrator.h"
#include "RaceNarrator.h"
#include "DriveAssist.h"
#include "ItemBoxBeacon.h"
#include "ShellTracker.h"
#include "BananaBeacon.h"
#include "ObstacleBeacon.h"

/**
 * Top-level orchestrator for accessibility features.
 *
 * A single Tick() is driven from the game loop. The manager decides, based on
 * the current game state and the user's CVar toggles, which accessibility
 * service should run this frame. All game-reading/speaking logic lives in the
 * services; this class only routes.
 */
class AccessibilityManager {
  public:
    static AccessibilityManager& Instance();

    void Tick();
    void Shutdown();

  private:
    AccessibilityManager() = default;

    bool Enabled() const;
    void EnsureScreenReaderInitialized();
    // Seed the recommended starting volumes / anticipation once on first run.
    void ApplyRecommendedDefaultsOnce();

    bool mScreenReaderInitTried = false;
    bool mDefaultsChecked = false;
    MenuNarrator mMenuNarrator;
    PauseNarrator mPauseNarrator;
    PostRaceNarrator mPostRaceNarrator;
    RaceNarrator mRaceNarrator;
    DriveAssist mDriveAssist;
    ItemBoxBeacon mItemBoxBeacon;
    ShellTracker mShellTracker;
    BananaBeacon mBananaBeacon;
    ObstacleBeacon mObstacleBeacon;
};

extern "C" {
#endif // __cplusplus

// C entry points, callable from the game loop (main.c).
void Accessibility_Tick(void);
void Accessibility_Shutdown(void);

#ifdef __cplusplus
}
#endif
