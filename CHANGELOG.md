# Changelog — Super Blind Kart

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
