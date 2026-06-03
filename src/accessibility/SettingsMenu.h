#pragma once

// Native, screen-reader-accessible settings menu.
//
// Hosted inside the game's native Options screen: each category (Accessibility,
// Sound, ...) is a row on the Options menu whose A-action opens that category
// here and parks gSubMenuSelection at the SUB_MENU_MOD_SETTINGS sentinel. While
// in that sentinel state, func_800A1FB0 (render) and options_menu_act (input) in
// the decompiled menu code delegate to the thin C API below. All option data,
// navigation, CVar binding and screen-reader narration live in the C++ module,
// keeping the decompiled-file footprint minimal.

#ifdef __cplusplus
extern "C" {
#endif

// Open a category (0 = Accessibility, 1 = Sound) and announce it.
void SettingsMenu_Open(int categoryId);

// Process one frame of input (the game's combined buttonPressed|stickPressed
// mask). Returns a bitfield for the C caller to drive menu SFX / state:
//   0x1 = exit back to the native Options list
//   0x2 = play cursor-move SFX (a cursor or value change happened)
//   0x8 = play go-back SFX (set together with 0x1 on exit)
int SettingsMenu_HandleInput(unsigned short buttons);

// Render accessors (called by func_800A1FB0). Rows are the visible window.
int SettingsMenu_RowCount(void);
int SettingsMenu_IsRowSelected(int i);
const char* SettingsMenu_Title(void);
// Writes the display text ("LABEL  VALUE") for visible row i into out.
void SettingsMenu_RowText(int i, char* out, int outSize);

// Category currently open (0 = Accessibility, 1 = Sound). Used on exit to land
// the native cursor back on this category's row.
int SettingsMenu_OpenCategoryId(void);

#ifdef __cplusplus
}
#endif
