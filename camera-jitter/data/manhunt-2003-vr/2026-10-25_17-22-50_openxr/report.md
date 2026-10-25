# manhunt-2003-vr (openxr, VR, headset or simulator not recorded)

Log `manhunt-2003-vr_20261025-171955.jl`, checked 2026-10-25 17:23. `[measured]`: one VR session.

JITTER in 127 of 127 turning seconds. Why: the camera is written 0.00 times per picture: it is renewed less often than pictures are drawn, so some pictures reuse the previous camera (the rate fault).

- quality: **good**
- 174.49 s; 71.8 pictures, 71.8 pose reads, 0.0 camera writes a second
- turning 127 s, jitter 127 s

## Why

- the camera is written 0.00 times per picture: it is renewed less often than pictures are drawn, so some pictures reuse the previous camera (the rate fault)

![chart](chart.png)

## On the recording

- **game window**: `begone_2026-10-25_17-20-04.mp4`; the log starts -8.73 s into it (log file made).
  - jitter 9-13s: 150 frames; on screen 21 pictures held, 12 jumped, in `<recordings>/manhunt-2003-vr/begone-frames/2026-10-25_17-22-50_openxr/jitter_9-13s_game/` (sheet.jpg)
  - jitter 18-24s: 150 frames; on screen 0 pictures held, 0 jumped, in `<recordings>/manhunt-2003-vr/begone-frames/2026-10-25_17-22-50_openxr/jitter_18-24s_game/` (sheet.jpg)
  - jitter 25-26s: 90 frames; on screen 0 pictures held, 0 jumped, in `<recordings>/manhunt-2003-vr/begone-frames/2026-10-25_17-22-50_openxr/jitter_25-26s_game/` (sheet.jpg)
