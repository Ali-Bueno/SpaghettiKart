# Changelog — Super Blind Kart

## Changes on 05/06/2026

### New
- **Spinning shell cue.** A looping sound (with a Doppler "it passed you" drop) plays while a
  shell — green, red or blue — is flying across the track, whether you or a rival threw it, so
  you can react to an incoming threat. Toggle and demo in the accessibility menu.
- **Banana hazard cue.** A sound plays for a banana lying on the track, panned toward it (with
  the same Doppler), so you can steer clear. Toggle, range slider and demo in the menu.
- **Graded curve calls.** Curves are now announced with how tight they are — Easy, Normal, Hard
  or Hairpin, plus "Long" for long curves — and a run of curves with no straight between them is
  announced together (for example "Hard left then easy right").
- **More post-race narration.** Your final Grand Prix placement, the full points standings after
  each race, the trophy already won for each cup on the cup-select screen, the Retry/Quit menu
  when you finish low, and the Time-Trial save-ghost menu are all read now.
- **More sound controls.** Separate volume, tone and loop-speed sliders for the driving cues, in
  the accessibility menu.

### Improvements
- **Reworked curve detection** so curve calls are more accurate and consistent from lap to lap.
- **Softer, clearer audio cues**, with a gentler track-limit tone and distinct sounds for the
  approach, curve and edge cues.

### Fixes
- **JAWS now works.** The mod was picking a screen reader that was not running (NVDA) ahead of
  the one actually in use, which left JAWS users with no speech at all. It now always uses the
  screen reader that is actually running.

## Changes on 04/06/2026

### New
- **3D positional cue for item boxes.** A blip pans toward the nearest item box and grows
  louder as you close in, so you can steer onto it. If you drive past it, it keeps sounding
  from behind at a lower pitch (a Doppler "you passed it" cue), and it stops as soon as you
  pick up an item. It can be toggled and its range adjusted in the accessibility menu, with
  a demo on the Z button.
- **Your finishing position is announced** at the end of a race (Grand Prix and Versus).

### Fixes
- Driving audio cues now play **only during the actual race** (after the start, and during
  replays). They no longer fire during the intro/countdown, the title-screen demo, while
  paused, in menus, or after crossing the finish line.
- **Time Trials:** track names now refresh correctly when you switch cups (they used to keep
  the previous cup's tracks until you went back to the main menu).
- **More robust with screen readers:** the audio cues now work even when the screen reader
  fails to attach, and screen-reader detection was improved (with more diagnostic info in
  the log).

### Default settings (based on user feedback)
- Music and rival-kart volume set to **60%**.
- Drive-assist anticipation set to **12**.
