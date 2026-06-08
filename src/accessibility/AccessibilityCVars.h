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
// Audio-cue tone trims (0-100, 50 = unchanged): shift each cue family's pitch down
// (deeper/softer) or up. Purely cosmetic - they only change how a cue sounds, never the
// guidance - so they are safe to expose. The edge warning is deep by default so it is not
// piercing; these let a player tune any cue to taste.
#define CVAR_ACCESS_CUE_PITCH_APPROACH "gAccessibility.CuePitchApproach"
#define CVAR_ACCESS_CUE_PITCH_CURVE "gAccessibility.CuePitchCurve"
#define CVAR_ACCESS_CUE_PITCH_EDGE "gAccessibility.CuePitchEdge"
// Per-cue volume (0-100%): how loud each cue family plays, so a player can balance them
// against the music and each other. Approach = curve countdown beeps, Curve = the
// entry/middle/exit beeps, Edge = the track-limit beeps and held tone.
#define CVAR_ACCESS_CUE_VOL_APPROACH "gAccessibility.CueVolApproach"
#define CVAR_ACCESS_CUE_VOL_CURVE "gAccessibility.CueVolCurve"
#define CVAR_ACCESS_CUE_VOL_EDGE "gAccessibility.CueVolEdge"
// Short panned beep when the kart drifts close to a track edge (pre off-road warning).
#define CVAR_ACCESS_EDGE_CUE "gAccessibility.EdgeCue"
// Edge cue sensitivity 0-100: how early (how far from the edge) the cue starts. The
// cue stays silent while you are centered; higher = it begins sooner / from further
// in, lower = it stays silent until you are closer to the edge.
#define CVAR_ACCESS_EDGE_SENSITIVITY "gAccessibility.EdgeSensitivity"
// 3D proximity beacon for item boxes: a panned blip toward the nearest item box,
// louder as you close in, so the player can steer onto it. Stops once an item is held.
#define CVAR_ACCESS_ITEMBOX_CUE "gAccessibility.ItemBoxCue"
// Item-box beacon range 0-100: how far away the beacon starts guiding you to a box.
// Lower = only when close; higher = warns from further out.
#define CVAR_ACCESS_ITEMBOX_RANGE "gAccessibility.ItemBoxRange"
// Item-box beacon loop time in milliseconds: how often the blip repeats while a box is in
// range (the pulse rate). Default ~600 ms.
#define CVAR_ACCESS_ITEMBOX_INTERVAL "gAccessibility.ItemBoxInterval"
// Looping whoosh while a shell (green / red / blue) is in flight across the track - thrown
// by you or a rival - panned toward it and pitch-shifted as it falls behind (Doppler), so a
// blind player can hear an incoming shell. Stops when no shell is moving.
#define CVAR_ACCESS_SHELL_CUE "gAccessibility.ShellCue"
// Hazard blip toward the nearest banana resting on the track, louder as you near it (same
// Doppler as the item-box beacon), so a blind player can steer clear. Sounds whether or not
// an item is held - a banana is a hazard, not a pickup.
#define CVAR_ACCESS_BANANA_CUE "gAccessibility.BananaCue"
// Banana cue range 0-100: how far away the blip starts warning of a banana.
// Lower = only when close; higher = warns from further out.
#define CVAR_ACCESS_BANANA_RANGE "gAccessibility.BananaRange"
// Banana cue loop time in milliseconds: how often the blip repeats while a banana is in
// range (the pulse rate). Default ~600 ms.
#define CVAR_ACCESS_BANANA_INTERVAL "gAccessibility.BananaInterval"
// Collision-warning blip toward the nearest dynamic obstacle you can crash into - oncoming
// traffic (cars / trucks / buses), falling rocks, the train, paddle boats, cows, piranha
// plants - panned toward it and dropping in pitch once it is behind you, so a blind player
// can hear a hazard closing in and steer clear. Pulses ~200 ms while one is in range.
#define CVAR_ACCESS_OBSTACLE_CUE "gAccessibility.ObstacleCue"
// Obstacle warning range 0-100: how close a hazard must be before the cue starts. Lower =
// only warns when nearly on top of it; higher = warns from further out.
#define CVAR_ACCESS_OBSTACLE_RANGE "gAccessibility.ObstacleRange"
// Obstacle cue loop time in milliseconds: how often the blip repeats while a collision is
// imminent (the pulse rate). Lower = faster / more urgent. Default 100 ms.
#define CVAR_ACCESS_OBSTACLE_INTERVAL "gAccessibility.ObstacleInterval"
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
#define CVAR_ACCESS_CUE_PITCH_APPROACH_DEFAULT 50
#define CVAR_ACCESS_CUE_PITCH_CURVE_DEFAULT 50
#define CVAR_ACCESS_CUE_PITCH_EDGE_DEFAULT 50
// Cue volumes default to 55% (= the long-time fixed cue loudness), so existing behaviour is
// unchanged until the player moves a slider.
#define CVAR_ACCESS_CUE_VOL_APPROACH_DEFAULT 55
#define CVAR_ACCESS_CUE_VOL_CURVE_DEFAULT 55
#define CVAR_ACCESS_CUE_VOL_EDGE_DEFAULT 55
#define CVAR_ACCESS_ITEMBOX_CUE_DEFAULT 1
#define CVAR_ACCESS_ITEMBOX_RANGE_DEFAULT 50
#define CVAR_ACCESS_ITEMBOX_INTERVAL_DEFAULT 600
#define CVAR_ACCESS_SHELL_CUE_DEFAULT 1
#define CVAR_ACCESS_BANANA_CUE_DEFAULT 1
#define CVAR_ACCESS_BANANA_RANGE_DEFAULT 50
#define CVAR_ACCESS_BANANA_INTERVAL_DEFAULT 600
#define CVAR_ACCESS_OBSTACLE_CUE_DEFAULT 1
#define CVAR_ACCESS_OBSTACLE_RANGE_DEFAULT 50
#define CVAR_ACCESS_OBSTACLE_INTERVAL_DEFAULT 100

// Recommended starting volumes (0..1) seeded once on first run. Testers reported they
// follow the audio cues better with the music and rival karts a little quieter.
#define CVAR_ACCESS_RECOMMENDED_MUSIC_VOLUME 0.60f
#define CVAR_ACCESS_RECOMMENDED_RIVAL_VOLUME 0.60f
