#include "MultiPathGuide.h"

#include "AudioCueService.h"
#include "ScreenReaderService.h"
#include "AccessibilityStrings.h"
#include "AccessibilityCVars.h"

#include <libultraship.h>

extern "C" {
#include <common_structs.h> // Player
#include <waypoints.h>      // gPathIndexByPlayerId, gNearestPathPointByPlayerId, bInMultiPathSection
}

extern "C" {
extern Player* gPlayerOne;
// Per-track identity (port/Game.h, declared inside its extern "C" block so it has C linkage).
bool IsYoshiValley();
}

using namespace AccessibilityStrings;

namespace {

// The human player is always player 0; the path globals are indexed by player id.
constexpr int kPlayerId = 0;

// The engine flags the forking section (bInMultiPathSection) once the kart's nearest waypoint
// reaches 0x6D on the approach path; we warn a little before that so the player has time to
// react. Before the fork the kart is on the single main path (index 0).
constexpr int kForkEntryPoint = 0x6D;   // 109
constexpr int kWarnLeadPoints = 30;     // warn this many path points before the entrance (~2 s)

} // namespace

void MultiPathGuide::Reset() {
    mWarned = false;
    mLastLap = -1;
}

void MultiPathGuide::Tick(ScreenReaderService& reader) {
    if (CVarGetInteger(CVAR_ACCESS_MULTIPATH_CUE, CVAR_ACCESS_MULTIPATH_CUE_DEFAULT) == 0) {
        Reset();
        return;
    }
    if (!IsYoshiValley()) {
        Reset();
        return;
    }
    const Player* player = gPlayerOne;
    if (player == nullptr) {
        Reset();
        return;
    }

    // Re-arm once per lap, so the fork is announced on every lap's approach.
    const int lap = player->lapCount;
    if (lap != mLastLap) {
        mWarned = false;
        mLastLap = lap;
    }
    if (mWarned) {
        return;
    }

    // Once already inside the fork, stop trying to warn "ahead" for this lap (the approach
    // window was either spoken or skipped past).
    if (bInMultiPathSection[kPlayerId] != 0) {
        mWarned = true;
        return;
    }

    // On the approach the kart is on the single main path (0). Warn while it is within the
    // lead window just before the fork entrance.
    const int pathIndex = gPathIndexByPlayerId[kPlayerId];
    const int nearest = gNearestPathPointByPlayerId[kPlayerId];
    if (pathIndex == 0 && nearest >= (kForkEntryPoint - kWarnLeadPoints) && nearest < kForkEntryPoint) {
        // Centered alert first - it cuts through engine/race noise even if the speech is masked -
        // then the spoken detail (queued, so it never cuts a curve call mid-word).
        AudioCueService::Instance().PlayForkAlert();
        reader.Speak(MULTIPATH_FORK, false);
        mWarned = true;
    }
}
