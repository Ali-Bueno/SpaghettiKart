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

// Grand Prix overall standings (the points table read after each race).
inline constexpr const char* STANDINGS_PREFIX = "Points. ";
inline constexpr const char* STANDINGS_SELF = "You"; // the player's own row

// Cup completion trophies, indexed by the save value (0 none, 1 bronze, 2 silver,
// 3 gold). Index 0 is empty (cup not won yet).
inline constexpr const char* TROPHIES[4] = { "", "bronze trophy", "silver trophy", "gold trophy" };

// Save-ghost sub-menu (after a Time Trial): the save-slot picker and the overwrite
// confirmation. These are sub-states of the same time-trial finish menu (0xBA), which
// otherwise went unread, so the player couldn't tell which slot was highlighted.
inline constexpr const char* GHOST_SLOT_PREFIX = "Save ghost, slot ";
inline constexpr const char* GHOST_OVERWRITE_NO = "Overwrite ghost? No";
inline constexpr const char* GHOST_OVERWRITE_YES = "Overwrite ghost? Yes";

// Time Trial data / records screen (the "Data" menu). Option cursor, indexed by
// gCourseRecordsMenuSelection (0 Return, 1 Erase records, 2 Erase ghost).
inline constexpr const char* COURSE_DATA_OPTIONS[3] = { "Return", "Erase records", "Erase ghost" };
inline constexpr const char* RECORD_BEST_TIME = "Best time, ";
inline constexpr const char* RECORD_BEST_LAP = "Best lap, ";
inline constexpr const char* RECORD_NONE = "no record"; // shown as dashes for an unset record
inline constexpr const char* RECORD_BY = ", by "; // joins a time to the character who set it

// Time Trial finish results (the lap times spoken when the finish menu opens).
inline constexpr const char* TT_RESULT_LAP = "Lap "; // + "1, " + time
inline constexpr const char* TT_RESULT_TOTAL = "Total, ";

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
// Spoken when the player crosses the finish line, followed by a POSITIONS[] entry
// (e.g. "You finished 3rd").
inline constexpr const char* RACE_FINISH_PREFIX = "You finished ";
inline constexpr const char* RACE_OFF_ROAD = "Off road";
inline constexpr const char* RACE_ON_ROAD = "On road";

// Drive assist curve announcements. The spoken call is composed as
// <severity prefix> + <direction> + <optional "long">, e.g. "Right", "Hard left",
// "Hairpin right", "Easy left long" - a compact rally-style pacenote graded from the
// curve's geometry (radius vs track width and total heading change).
inline constexpr const char* TURN_LEFT = "Left";
inline constexpr const char* TURN_RIGHT = "Right";
// Severity prefixes (prepended to the direction). Hairpin = tightest (near U-turn),
// Hard = tight/closed, Easy = open/gentle; a normal curve has no prefix.
inline constexpr const char* TURN_PREFIX_HAIRPIN = "Hairpin ";
inline constexpr const char* TURN_PREFIX_HARD = "Hard ";
inline constexpr const char* TURN_PREFIX_EASY = "Easy ";
// Appended for a sustained (long) curve.
inline constexpr const char* TURN_SUFFIX_LONG = " long";
// Joins back-to-back curves with no straight between them into one call, e.g.
// "Hard left then easy right".
inline constexpr const char* TURN_CHAIN = " then ";

// Multi-path fork (Yoshi Valley): an advance heads-up spoken once per lap a little before the
// kart reaches the forking section, where the track splits into four routes. It is a plain
// warning - the player then navigates the split with the normal engine-pan steering guide.
inline constexpr const char* MULTIPATH_FORK = "Fork ahead, four routes";

// Shortcut beacon: spoken once per lap as a shortcut's fork comes into range, with the side it
// leaves the road on (relative to the kart's heading at that moment), so the player knows where
// to enter. The entrance beep then marks the exact spot and the chord rings as guidance latches.
inline constexpr const char* SHORTCUT_AHEAD = "Shortcut ahead";
inline constexpr const char* SHORTCUT_SIDE_LEFT = ", left";
inline constexpr const char* SHORTCUT_SIDE_RIGHT = ", right";

} // namespace AccessibilityStrings
