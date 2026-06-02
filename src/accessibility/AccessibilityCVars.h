#pragma once

// Centralized CVar names for the accessibility module.
// All accessibility features must be individually toggleable (project convention).

#define CVAR_ACCESS_ENABLED "gAccessibility.Enabled"
#define CVAR_ACCESS_SCREEN_READER "gAccessibility.ScreenReader"
#define CVAR_ACCESS_MENU_NARRATION "gAccessibility.MenuNarration"
#define CVAR_ACCESS_RACE_NARRATION "gAccessibility.RaceNarration"
#define CVAR_ACCESS_OFFROAD_CUE "gAccessibility.OffRoadCue"
#define CVAR_ACCESS_DRIVE_ASSIST "gAccessibility.DriveAssist"
#define CVAR_ACCESS_DRIVE_INVERT "gAccessibility.DriveAssistInvert"
// Engine pan model: 0 = lateral lane position (Top Speed style), 1 = heading error.
#define CVAR_ACCESS_DRIVE_PAN_MODE "gAccessibility.DriveAssistPanMode"
// Engine pan strength 0-100%: scales how far the audio leans. Lower = gentler.
#define CVAR_ACCESS_DRIVE_PAN_STRENGTH "gAccessibility.DriveAssistPanStrength"
// Short panned beep when the kart drifts close to a track edge (pre off-road warning).
#define CVAR_ACCESS_EDGE_CUE "gAccessibility.EdgeCue"

// Default values used both by the logic (CVarGetInteger fallbacks) and the UI
// checkboxes, so the menu state and behaviour always agree.
#define CVAR_ACCESS_ENABLED_DEFAULT 1
#define CVAR_ACCESS_SCREEN_READER_DEFAULT 1
#define CVAR_ACCESS_MENU_NARRATION_DEFAULT 1
#define CVAR_ACCESS_RACE_NARRATION_DEFAULT 1
#define CVAR_ACCESS_OFFROAD_CUE_DEFAULT 1
#define CVAR_ACCESS_DRIVE_ASSIST_DEFAULT 1
#define CVAR_ACCESS_DRIVE_INVERT_DEFAULT 0
#define CVAR_ACCESS_DRIVE_PAN_MODE_DEFAULT 0
#define CVAR_ACCESS_DRIVE_PAN_STRENGTH_DEFAULT 60
#define CVAR_ACCESS_EDGE_CUE_DEFAULT 1
