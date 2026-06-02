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

} // namespace AccessibilityStrings
