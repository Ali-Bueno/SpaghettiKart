#pragma once

/**
 * Collision-warning cue for dynamic track obstacles.
 *
 * While the kart is within collision range of a hazard you can crash into - oncoming
 * traffic (cars, trucks, buses), falling rocks, the train, paddle boats, cows, piranha
 * plants - a blip repeats (~200 ms), panned toward the hazard and pitch-shifted as it
 * falls behind (the same Doppler cue as the other beacons), so a blind player can hear
 * a hazard closing in and which side it is on, and steer clear. It stops once nothing
 * is in range.
 *
 * Only collidable hazards are considered: the same live + can-collide actor flags the
 * game's own collision pass uses, so despawned / respawning / non-collidable obstacles
 * are ignored.
 *
 * It only reads game state and delegates the sound to AudioCueService. Like the rest of
 * the drive assist it must only run during the live race; the AccessibilityManager gates
 * that and calls Reset() otherwise.
 */
class ObstacleBeacon {
  public:
    // Silence the cue (call when leaving the live race).
    void Reset();
    // Find the nearest in-range hazard and drive the warning blip. Once per frame.
    void Tick();

  private:
    int mBlipTimer = 0; // counts down between blips (the ~200 ms pulse)
};
