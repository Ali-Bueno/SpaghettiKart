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

#include <cstddef>
#include <cstdint>

extern "C" {
// High-level game state. Compare against RACING / ENDING / CREDITS_SEQUENCE.
extern int32_t gGamestate;
// Non-zero while the in-race pause menu is open (value = pausing player + 1).
extern uint16_t gIsGamePaused;
// Currently displayed front-end menu screen.
// Compare against MAIN_MENU / CHARACTER_SELECT_MENU / COURSE_SELECT_MENU, etc.
extern int32_t gMenuSelection;

// --- Main menu state machine ---
// Sub-state within MAIN_MENU (see MainMenuSelectionType in menus.h).
extern int8_t gMainMenuSelection;
// Highlighted player count (1-4) during MAIN_MENU_PLAYER_SELECT.
extern int8_t gPlayerCount;
// Highlighted mode column per player count; index with [gPlayerCount - 1].
extern int8_t gGameModeMenuColumn[4];
// Highlighted sub-mode (CC / Time Trials option); index [gPlayerCount-1][modeColumn].
extern int8_t gGameModeSubMenuColumn[4][3];
// Read-only: which game mode each (playerCount, column) maps to (GRAND_PRIX, ...).
extern const int32_t gGameModePlayerSelection[4][3];

// --- Character select ---
// Grid position per player (1-8, 0 = none). Index by player id.
extern int8_t gCharacterGridSelections[4];
extern int8_t gPlayerSelectMenuSelection;

// --- Course select ---
// Live resolved track id (updated as the player browses cups/courses).
extern int16_t gCurrentCourseId;
// Live cup index / name while browsing cups (gCupSelection is not updated live).
uint32_t GetCupIndex(void);
const char* GetCupName(void);
// Live cursor position (0-based) within the current World cup. Together with
// GetCupIndex() and gCupCourseOrder this resolves the highlighted course the same
// way the game's own course-select screen does (port/Game.cpp).
size_t GetCupCursorPosition(void);
// Per-cup course id table, rows: mushroom, flower, star, special, battle; columns:
// the four courses in that cup. Indexes into the spoken TRACKS[] names. (menus.c)
extern const int16_t gCupCourseOrder[5][4];

// --- Options / shared sub-menu cursor ---
extern int8_t gSubMenuSelection;
extern uint8_t gSoundMode; // SOUND_STEREO..SOUND_MONO
}
