#pragma once

// Centralized CVar names for the accessibility module.
// All accessibility features must be individually toggleable (project convention).

#define CVAR_ACCESS_ENABLED "gAccessibility.Enabled"
#define CVAR_ACCESS_SCREEN_READER "gAccessibility.ScreenReader"
#define CVAR_ACCESS_MENU_NARRATION "gAccessibility.MenuNarration"
#define CVAR_ACCESS_RACE_NARRATION "gAccessibility.RaceNarration"
#define CVAR_ACCESS_OFFROAD_CUE "gAccessibility.OffRoadCue"

// Default values used both by the logic (CVarGetInteger fallbacks) and the UI
// checkboxes, so the menu state and behaviour always agree.
#define CVAR_ACCESS_ENABLED_DEFAULT 1
#define CVAR_ACCESS_SCREEN_READER_DEFAULT 1
#define CVAR_ACCESS_MENU_NARRATION_DEFAULT 1
#define CVAR_ACCESS_RACE_NARRATION_DEFAULT 1
#define CVAR_ACCESS_OFFROAD_CUE_DEFAULT 1
