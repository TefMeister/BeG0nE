# Dr.BeGonE for Resident Evil Village

**Status: in testing. No download yet.** A download comes only after every long weapon passes the test in
the main README.

## What is here

`reframework/autorun/re8_vrz_begone_grip.lua`: the stacked two-handed grip, for REFramework's built-in RE8 VR
mode by praydog. Hold the left grip button with the left controller 3 to 35 cm above the right one. The weapon
then aims with the right hand alone, and the drawn left hand stays on the forestock.

It is VR only. On a flat screen it loads and does nothing.

Since the evening of 2026-10-01 it also carries two fixes for praydog's own two-handed grip, so they no longer
need a patched loader:

- **No left grip button, no grip.** Praydog docks the left hand the moment it comes within 10 cm of the weapon's
  grip spot. This file undoes any dock made without the left grip button held, so the button is the switch.
- **No jump on the first shot.** Praydog steers the weapon against the grip spot the game's animation gives him
  each frame, and the firing pose moves that spot by about five degrees, so the first shot after taking the grip
  jerked the rifle left. The grip spot is now frozen at the moment the grip is taken; the animation may move its
  own hand, the weapon does not follow.

Both run right after praydog's hand update, using only REFramework's public Lua API, and check themselves: the
file recomputes the controller poses the way praydog's C++ does and compares its right hand with his on every
pass. The log says `pose check: … the recompute is right` once that has agreed 300 times, or `POSE CHECK FAILED`
if it ever does not. Harness words: `grip button 0|1`, `grip freeze 0|1` (either fix off or on, live, for an
A/B), `grip check` (the figures), `grip` (the state line).

## Where it came from

On 2026-10-01 it was split out of the RE Village VR Scope mod (`re8_vrz_scope_left_grip.lua`, shipped in its
v1.0.0 and v1.0.1). The grip code itself was not changed. The scope mod keeps its weapon-click silencing and
left-menu-button fix in its own file.

## Not done yet

- **It still needs the scope mod.** It only acts while the scope's rifle camera exists, so today it works with
  the sniper rifle only. Standing alone for every long weapon needs a way to tell which weapon is held, read
  from the game while it runs.
- **It still uses the scope's settings file** for the captured hand spot (`reframework/data/re_scope_left_grip.txt`),
  so a spot captured before the split keeps working.
- **Neither the split nor the two fixes have been worn in a headset yet.** Installed on the home PC on
  2026-10-01; the first wear reads the pose check and judges the first shot and the button-only dock.
- **Praydog has not been sent the grip rules upstream yet.**

## Test

`re-village-scope-vr` staging: `scripts/tests/grip_split_test.lua` loads this file and the scope's half
separately and checks that each one starts on its own and answers its words; `scripts/tests/begone_grip_fixes_test.lua`
checks the two fixes are on by default, answer their words, and that the steering maths turns the right way.
