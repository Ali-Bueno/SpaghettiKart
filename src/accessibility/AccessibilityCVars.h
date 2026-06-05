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
// Engine pan strength 0-100%: scales how far the audio leans. Lower = gentler.
#define CVAR_ACCESS_DRIVE_PAN_STRENGTH "gAccessibility.DriveAssistPanStrength"
// Steering Guide look-ahead ("anticipation"), in path points: how far ahead to aim.
// Lower = tighter centering / reacts sooner; higher = smoother / leans earlier.
#define CVAR_ACCESS_DRIVE_LOOKAHEAD "gAccessibility.DriveAssistLookAhead"
// Short panned beep when the kart drifts close to a track edge (pre off-road warning).
#define CVAR_ACCESS_EDGE_CUE "gAccessibility.EdgeCue"
// Edge cue sensitivity 0-100: how early (how far from the edge) the cue starts. The
// cue stays silent while you are centered; higher = it begins sooner / from further
// in, lower = it stays silent until you are closer to the edge.
#define CVAR_ACCESS_EDGE_SENSITIVITY "gAccessibility.EdgeSensitivity"
// Rival-kart engine volume scale (0..1): lowered by default so a blind player can
// pick out their own kart over the pack. Applied live in src/audio/external.c.
#define CVAR_ACCESS_RIVAL_VOLUME "gAccessibility.RivalKartVolume"
// Port-wide background-music volume (0..1). Not accessibility-namespaced, but seeded
// to a quieter recommended default below for the same reason.
#define CVAR_MAIN_MUSIC_VOLUME "gMainMusicVolume"
// Sentinel: set once the recommended starting defaults below have been seeded, so a
// fresh install gets them but the player's later changes are never overwritten.
#define CVAR_ACCESS_DEFAULTS_APPLIED "gAccessibility.DefaultsApplied"

// Default values used both by the logic (CVarGetInteger fallbacks) and the UI
// checkboxes, so the menu state and behaviour always agree.
#define CVAR_ACCESS_ENABLED_DEFAULT 1
#define CVAR_ACCESS_SCREEN_READER_DEFAULT 1
#define CVAR_ACCESS_MENU_NARRATION_DEFAULT 1
#define CVAR_ACCESS_RACE_NARRATION_DEFAULT 1
#define CVAR_ACCESS_OFFROAD_CUE_DEFAULT 1
#define CVAR_ACCESS_DRIVE_ASSIST_DEFAULT 1
#define CVAR_ACCESS_DRIVE_INVERT_DEFAULT 0
#define CVAR_ACCESS_DRIVE_PAN_STRENGTH_DEFAULT 60
#define CVAR_ACCESS_DRIVE_LOOKAHEAD_DEFAULT 12
#define CVAR_ACCESS_EDGE_CUE_DEFAULT 1
#define CVAR_ACCESS_EDGE_SENSITIVITY_DEFAULT 50

// Recommended starting volumes (0..1) seeded once on first run. Testers reported they
// follow the audio cues better with the music and rival karts a little quieter.
#define CVAR_ACCESS_RECOMMENDED_MUSIC_VOLUME 0.60f
#define CVAR_ACCESS_RECOMMENDED_RIVAL_VOLUME 0.60f
