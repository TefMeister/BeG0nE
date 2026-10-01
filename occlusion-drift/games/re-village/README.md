# Dr.BeGonE for Resident Evil Village

**Status: in testing. No download yet.** A download comes only after every long weapon passes the test in
the main README.

## What is here

`reframework/autorun/re8_vrz_begone_grip.lua`: the stacked two-handed grip, for REFramework's built-in RE8 VR
mode by praydog. Hold the left grip button with the left controller 3 to 35 cm above the right one. The weapon
then aims with the right hand alone, and the drawn left hand stays on the forestock.

It is VR only. On a flat screen it loads and does nothing.

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
- **The split has not been worn in a headset yet.**

## Test

`re-village-scope-vr` staging: `scripts/tests/grip_split_test.lua` loads this file and the scope's half
separately and checks that each one starts on its own and answers its words.
