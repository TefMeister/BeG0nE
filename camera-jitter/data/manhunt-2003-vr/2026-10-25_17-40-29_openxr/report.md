# manhunt-2003-vr (openxr, VR, headset or simulator not recorded)

Log `manhunt-2003-vr_20261025-173910.jl`, checked 2026-10-25 17:40. `[measured]`: one VR session.

JITTER in 45 of 45 turning seconds. Why: the camera is written 0.00 times per picture: it is renewed less often than pictures are drawn, so some pictures reuse the previous camera (the rate fault).

- quality: **good**
- 78.75 s; 71.8 pictures, 71.8 pose reads, 0.0 camera writes a second
- turning 45 s, jitter 45 s

## Why

- the camera is written 0.00 times per picture: it is renewed less often than pictures are drawn, so some pictures reuse the previous camera (the rate fault)

![chart](chart.png)

## On the recording

- **game window**: `begone_2026-10-25_17-39-22.mp4`; the log starts -11.81 s into it (log file made).
  - jitter 13-15s: 150 frames; on screen 12 pictures held, 4 jumped, in `<recordings>/manhunt-2003-vr/begone-frames/2026-10-25_17-40-29_openxr/jitter_13-15s_game/` (sheet.jpg)
  - jitter 17-18s: 90 frames; on screen 6 pictures held, 1 jumped, in `<recordings>/manhunt-2003-vr/begone-frames/2026-10-25_17-40-29_openxr/jitter_17-18s_game/` (sheet.jpg)
  - jitter 19-25s: 150 frames; on screen 5 pictures held, 0 jumped, in `<recordings>/manhunt-2003-vr/begone-frames/2026-10-25_17-40-29_openxr/jitter_19-25s_game/` (sheet.jpg)
