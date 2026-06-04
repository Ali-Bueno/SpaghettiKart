#pragma once

// Centralized spoken strings for the accessibility module.
// Keeping every announced text here keeps patches free of hardcoded strings and
// makes future translation a single-file change.

namespace AccessibilityStrings {

// Front-end menu screen names. Returned by MenuNarrator::ScreenName().
inline constexpr const char* MENU_HARBOUR_MASTERS = "Harbour Masters menu";
inline constexpr const char* MENU_LOGO_INTRO = "Intro";
inline constexpr const char* MENU_START = "Start menu";
inline constexpr const char* MENU_MAIN = "Main menu";
inline constexpr const char* MENU_CHARACTER_SELECT = "Character select";
inline constexpr const char* MENU_COURSE_SELECT = "Course select";
inline constexpr const char* MENU_OPTIONS = "Options";
inline constexpr const char* MENU_DATA = "Data";
inline constexpr const char* MENU_COURSE_DATA = "Course data";
inline constexpr const char* MENU_CONTROLLER_PAK = "Controller Pak";
inline constexpr const char* MENU_GENERIC = "Menu";

// Game modes, indexed by mode id (GRAND_PRIX=0, TIME_TRIALS, VERSUS, BATTLE).
inline constexpr const char* MODES[4] = { "Grand Prix", "Time Trials", "Versus", "Battle" };

// Engine class, indexed by CC id (CC_50=0, CC_100, CC_150, CC_EXTRA).
inline constexpr const char* CC_CLASSES[4] = { "50 c c", "100 c c", "150 c c", "Extra" };

// Characters, indexed by character id (MARIO=0 .. BOWSER=7).
inline constexpr const char* CHARACTERS[8] = { "Mario",  "Luigi", "Yoshi", "Toad",
                                               "Donkey Kong", "Wario", "Peach", "Bowser" };

// Cups, indexed by cup id (MUSHROOM_CUP=0 .. BATTLE_CUP=4).
inline constexpr const char* CUPS[5] = { "Mushroom Cup", "Flower Cup", "Star Cup", "Special Cup",
                                         "Battle Cup" };

// Tracks, indexed by track id (see TRACK_* in mk64.h, 0..19).
inline constexpr const char* TRACKS[20] = {
    "Mario Raceway",     "Choco Mountain",  "Bowser's Castle", "Banshee Boardwalk",
    "Yoshi Valley",      "Frappe Snowland", "Koopa Troopa Beach", "Royal Raceway",
    "Luigi Raceway",     "Moo Moo Farm",    "Toad's Turnpike", "Kalimari Desert",
    "Sherbet Land",      "Rainbow Road",    "Wario Stadium",   "Block Fort",
    "Skyscraper",        "Double Deck",     "DK's Jungle Parkway", "Big Donut"
};

// Sound modes, indexed by gSoundMode (SOUND_STEREO=0 .. SOUND_MONO=3).
inline constexpr const char* SOUND_MODES[4] = { "Stereo", "Headphones", "Surround", "Mono" };

// Items, indexed by item id (see enum ITEMS in defines.h, 0..15).
inline constexpr const char* ITEM_NAMES[16] = {
    "",                  // ITEM_NONE
    "Banana",            "Banana bunch",      "Green shell",  "Triple green shell",
    "Red shell",         "Triple red shell",  "Blue shell",   "Lightning",
    "Fake item box",     "Star",              "Boo",          "Mushroom",
    "Double mushroom",   "Triple mushroom",   "Super mushroom"
};

// Race finishing positions, indexed by rank (0-based: index 0 = 1st).
inline constexpr const char* POSITIONS[8] = { "1st", "2nd", "3rd", "4th",
                                              "5th", "6th", "7th", "8th" };

// Pause menu.
inline constexpr const char* PAUSE_MENU = "Paused";
// End-of-course / replay options menu (after a Time Trial, and replay pause).
inline constexpr const char* PAUSE_END_MENU = "Options";
// Pause options, indexed by TEXT_MENU_ID (see menu_items.h: CONTINUE_GAME=0 ..
// SAVE_GHOST=6). Nicely cased instead of the game's ALL-CAPS so screen readers
// don't spell them out.
inline constexpr const char* PAUSE_OPTIONS[7] = {
    "Continue", "Retry", "Course change", "Driver change", "Quit", "Replay", "Save ghost"
};

// Race events.
inline constexpr const char* RACE_GO = "Go!";
inline constexpr const char* RACE_OVER = "Race finished";
inline constexpr const char* RACE_OFF_ROAD = "Off road";
inline constexpr const char* RACE_ON_ROAD = "On road";

// Drive assist curve announcements.
inline constexpr const char* TURN_LEFT = "Left";
inline constexpr const char* TURN_RIGHT = "Right";
inline constexpr const char* TURN_HARD_LEFT = "Hard left";
inline constexpr const char* TURN_HARD_RIGHT = "Hard right";

} // namespace AccessibilityStrings
