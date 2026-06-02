#include "RaceNarrator.h"

#include "ScreenReaderService.h"
#include "AccessibilityStrings.h"
#include "AccessibilityCVars.h"

#include <libultraship.h>

#include <string>

extern "C" {
#include <common_structs.h> // Player
#include <defines.h>        // ITEM_*, RACE_*
}

// Player one pointer and the race sub-state, defined in the game.
extern "C" {
extern Player* gPlayerOne;
extern int32_t gRaceState;
}

using namespace AccessibilityStrings;

namespace {

// Off-road surface ids (see enum SURFACE_TYPE in mk64.h). On these surfaces the
// kart is off the racing line and slowed down.
bool IsOffRoadSurface(int surface) {
    switch (surface) {
        case 0x07: // SAND_OFFROAD
        case 0x08: // GRASS
        case 0x0B: // SNOW_OFFROAD
        case 0x0D: // DIRT_OFFROAD
        case 0xFD: // OUT_OF_BOUNDS
            return true;
        default:
            return false;
    }
}

} // namespace

void RaceNarrator::Reset() {
    mLastRank = -1;
    mLastLap = -1;
    mLastItem = -1;
    mLastRaceState = -1;
    mWasOffRoad = false;
}

void RaceNarrator::Tick(ScreenReaderService& reader) {
    const Player* player = gPlayerOne;
    if (player == nullptr) {
        return;
    }

    // Start signal: announce "Go!" when the race begins.
    const int raceState = gRaceState;
    if (raceState != mLastRaceState) {
        if (mLastRaceState != -1 && raceState == RACE_IN_PROGRESS) {
            reader.Speak(RACE_GO, true);
        }
        mLastRaceState = raceState;
    }

    // Position (currentRank is 0-based: 0 = 1st).
    const int rank = player->currentRank;
    if (rank != mLastRank) {
        mLastRank = rank;
        if (rank >= 0 && rank < 8) {
            reader.Speak(POSITIONS[rank], true);
        }
    }

    // Lap (lapCount is 0-based: 0 = lap 1).
    const int lap = player->lapCount;
    if (lap != mLastLap) {
        const int previous = mLastLap;
        mLastLap = lap;
        // Only announce forward progress into a valid lap (avoid setup noise).
        if (previous != -1 && lap > previous && lap >= 1 && lap <= 2) {
            reader.Speak("Lap " + std::to_string(lap + 1), true);
        }
    }

    // Item obtained: announce when it changes to a real item.
    const int item = player->currentItemCopy;
    if (item != mLastItem) {
        mLastItem = item;
        if (item > ITEM_NONE && item < ITEM_MAX) {
            reader.Speak(ITEM_NAMES[item], false);
        }
    }

    // Off-road feedback (toggleable separately to avoid verbosity).
    if (CVarGetInteger(CVAR_ACCESS_OFFROAD_CUE, CVAR_ACCESS_OFFROAD_CUE_DEFAULT) != 0) {
        const bool offRoad = IsOffRoadSurface(player->surfaceType);
        if (offRoad != mWasOffRoad) {
            mWasOffRoad = offRoad;
            reader.Speak(offRoad ? RACE_OFF_ROAD : RACE_ON_ROAD, true);
        }
    }
}
