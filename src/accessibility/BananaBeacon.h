#pragma once

/**
 * Hazard cue for bananas lying on the track.
 *
 * Each tick it finds the nearest banana resting on the ground (dropped by the player or a
 * rival) and plays a short blip (~every 600 ms) panned toward it and louder the closer it
 * is, with the same Doppler "it is behind you" pitch drop as the item-box beacon - so a
 * blind player can hear where a banana is and steer clear of it. Unlike the item-box
 * beacon it sounds whether or not an item is held: a banana is a hazard to avoid, not a
 * pickup. The blip stops once no banana is on the ground within range.
 *
 * It only reads game state and delegates the sound to AudioCueService. Like the rest of
 * the drive assist it must only run during the live race; the AccessibilityManager gates
 * that and calls Reset() otherwise.
 */
class BananaBeacon {
  public:
    // Silence the beacon (call when leaving the live race).
    void Reset();
    // Find the nearest grounded banana and emit the timed beacon blip. Once per frame.
    void Tick();

  private:
    int mBlipTimer = 0; // ticks until the next blip
};
