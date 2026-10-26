# Lessons for the two checkers, from reading every shelved report (Gold Digger, 2026-10-26)

Order: 17100
From: `/gd` on 2026-10-26, reading 218 shelved BeG0nE reports across 8 games. Not one of Tefa's ideas: four places
where a report says something its numbers do not support. `[measured 2026-10-26]` from the reports named.

## Camera Jitter

1. **The simulator's head moves in jumps, and the checker calls that JITTER.** Mad Max (2026-10-15, labelled
   simulator), Alice (2026-10-26 12:00) and Manhunt (2026-10-14 17:51) all read "98 % of pose reads returned the same
   pose while turning" over only 2-4 turning seconds in runs of 3-12 minutes. That is a head put somewhere by a
   command, not a slow turn. FINDINGS.md counts all 4 simulator patterns as jittery. Suggest: when turning seconds are
   a handful of isolated seconds and ~98 % of reads are identical, say "stepped head (simulator or VR Bench), cannot
   judge" and leave the run out of FINDINGS. Hard Reset's board already asks VR Bench to mark its instant head moves.
2. **"0.00 camera writes" is a missing hook, not a rate fault.** Manhunt has read 0.0 camera writes in all 6 logs,
   in every mode, and every verdict blames the rate. Suggest: 0 camera writes in a whole run means "the mod does not
   log camera writes; wire `jl_camera`", never JITTER.

## Ghost Hand

3. **A gap of 100 m or more is a space mix-up, not wander.** Village has 16 of 107 runs with the game hand ~156 m
   from the controller (world position vs room position), and its lag then sits at the 12-frame search limit. Those
   runs still say "quality: good". Suggest: a gap over a few metres marks the frames "spaces differ" and leaves them
   out of jumps, wander and lag; a lag equal to the search limit says "could not line up".
4. **A gap of exactly 0.0 cm with lag "?" means the game hand IS the controller** (Hard Reset, every run): "followed
   cleanly" is then true but proves nothing. Suggest saying so in the report.
