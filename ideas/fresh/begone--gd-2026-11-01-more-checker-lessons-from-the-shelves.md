# More lessons for the checkers, from the shelves (Gold Digger, 2026-11-01)

Order: 17101
From: `/gd` inside `/fullscan` on 2026-11-01, 148 shelved items (9 jitter reports, 108 Ghost Hand reports, logs).
Not one of Tefa's ideas. Adds to `begone--gd-2026-10-26-checker-lessons-from-the-shelves.md`; does not repeat it.

1. **The picture rate says headset or simulator when the report does not.** Every report reads "headset or simulator
   not recorded", yet the dev PC's simulator runs sit at 60.0-60.9 pictures a second (Psychonauts 10-29, Hard Reset
   10-17/23) and Tefa's headset runs at 69-72 (Manhunt and Hard Reset 10-30, Village, RE2 10-31) `[measured, n=40+]`.
   Suggest: record the runtime name (`xrGetInstanceProperties`) in the log header; until then, guess from the rate and
   say it is a guess. The five Psychonauts 10-29 runs (dev PC, no headset) are more simulator "98 % same pose" runs.
2. **Many camera writes per picture hide a held camera.** Hard Reset logs 2-170 writes per picture (12,203 a second on
   10-30); "held still although written" cannot be judged then. Suggest: when writes per picture are well over 1, say
   so and judge from the last write per picture only.
3. **The magazine verdict the 10-31 idea warned about is now live.** Village sends `mag_need` (16 cm), and 7 reports
   (10-30 to 11-01) read "the hands moved 49 cm apart, enough for the 16.0 cm needed, but the mod measured less", which
   is the apart-gesture rule applied to a come-within gesture. `begone--ghost-hand-judge-a-magazine-miss-by-closest-vs-
   need.md` is the fix; until it lands, those misses are not evidence against the mod.
4. **Shells are mostly "not judged"** (Village 10-31 22:20: 20 of 21 misses). Same shape as the magazine: a shell must
   come within the port. Likely the same fix.
5. **A frame-rate drop inflates "a controller jumped"**: Silent Hill 2 went from 50 to 30 frames a second on 10-31, and
   the count went from 2-9 to 66-196 per run. Suggest: scale the jump threshold by the time between reads.
