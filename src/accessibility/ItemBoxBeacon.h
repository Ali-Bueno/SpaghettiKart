#pragma once

/**
 * 3D proximity beacon that guides a blind player onto item boxes.
 *
 * Each tick it finds the nearest available item box ahead of the player's kart and,
 * while no item is held, plays a short blip (~every 600 ms) panned toward the box and
 * louder the closer it is - so the player can steer "toward the sound" and drive over
 * it. The blip stops as soon as the player grabs a box (and so holds an item).
 *
 * It only reads game state and delegates the sound to AudioCueService. Like the rest
 * of the drive assist it must only run during the live race; the AccessibilityManager
 * gates that and calls Reset() otherwise.
 */
class ItemBoxBeacon {
  public:
    // Silence the beacon (call when leaving the live race / collecting an item).
    void Reset();
    // Find the nearest item box and emit the timed beacon blip. Once per frame.
    void Tick();

  private:
    int mBlipTimer = 0; // ticks until the next blip
};
