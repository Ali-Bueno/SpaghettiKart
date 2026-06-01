#pragma once

// Minimal bridge to the game's C globals consumed by the accessibility module.
//
// These are declared here (instead of pulling in the heavy, C-only game headers
// such as main.h / menus.h) so the C++ accessibility translation units stay small
// and isolated. The integer types below mirror the game's ultra64 types:
//   s32 -> int32_t, s8 -> int8_t. Linkage is by symbol name, so this is safe.
//
// Screen and game-state constants (RACING, MAIN_MENU, ...) live in <defines.h>,
// which is C++-safe, and are included directly where needed.

#include <cstdint>

extern "C" {
// High-level game state. Compare against RACING / ENDING / CREDITS_SEQUENCE.
extern int32_t gGamestate;
// Currently displayed front-end menu screen.
// Compare against MAIN_MENU / CHARACTER_SELECT_MENU / COURSE_SELECT_MENU, etc.
extern int32_t gMenuSelection;
// Per-screen cursor / option selection variables.
extern int8_t gMainMenuSelection;
extern int8_t gPlayerSelectMenuSelection;
extern int8_t gSubMenuSelection;
}
