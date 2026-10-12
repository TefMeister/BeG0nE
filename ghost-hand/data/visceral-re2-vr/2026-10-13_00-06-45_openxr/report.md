# visceral-re2-vr (openxr) - Ghost Hand

Log `visceral-re2-vr_20261013-000403.ghl`, checked 2026-10-13 00:06. `[measured]`: one VR session.

rack: 33 of 36 finished; most misses: the gun hand moved back with the pull (1 of 3); the game's left hand trails the controller by 2 frames (27.9 ms); the game's left hand wanders up to 21.8 cm from its usual place; the game's right hand trails the controller by 2 frames (27.9 ms); the game's hand jumped 21 times while the controller was calm; a controller jumped 54 times (tracking glitches); tracking lost 2 times, 2 with the hands stacked (occlusion).

- quality: **good**
- 162.07 s, 71.8 frames a second
- left hand: usual gap 12.3 cm, wanders 21.8 cm (95%), lag 2 frames
- right hand: usual gap 10.1 cm, wanders 0.3 cm (95%), lag 2 frames

## Misses

- #1 rack at 30.602 s: pull too short: 0.4 cm of 5.0 cm
- #17 rack at 74.216 s: the gun hand moved back with the pull: the left hand travelled 9.3 cm but the hands only moved 4.9 cm apart (5.0 needed; gun moved 4.2 cm)
- #36 rack at 148.113 s: the hands moved 13.3 cm apart, enough for the 5.0 cm needed, but the mod measured less (0.80)
