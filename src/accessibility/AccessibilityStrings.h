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

// Main menu game-mode options (indexed by gMainMenuSelection).
inline constexpr const char* MODE_GRAND_PRIX = "Grand Prix";
inline constexpr const char* MODE_TIME_TRIALS = "Time Trials";
inline constexpr const char* MODE_VERSUS = "Versus";
inline constexpr const char* MODE_BATTLE = "Battle";

} // namespace AccessibilityStrings
