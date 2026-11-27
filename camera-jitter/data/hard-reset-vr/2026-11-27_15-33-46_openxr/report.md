# hard-reset-vr (openxr, VR, headset or simulator not recorded)

Log `hard-reset-vr_20261127-153201.jl`, checked 2026-11-27 15:34. `[measured]`: one VR session.

JITTER in 2 of 2 turning seconds. Why: 99% of pose reads returned the same pose as the read before while the head was turning: the runtime handed a stale pose (the pose is read at the wrong moment, or from a cached copy, not once per frame after waiting for the frame).

- quality: **good**
- 104.53 s; 90.0 pictures, 90.0 pose reads, 3007.0 camera writes a second
- turning 2 s, jitter 2 s

## Why

- 99% of pose reads returned the same pose as the read before while the head was turning: the runtime handed a stale pose (the pose is read at the wrong moment, or from a cached copy, not once per frame after waiting for the frame)

## Controllers (Occlusion Drift BeG0nE)

- left: 90.0 reads a second; untracked 0.0% of the time, longest 0.0 s; 0% of the untracked reads came while the other controller was within 0.3 m
- right: 90.0 reads a second; untracked 0.0% of the time, longest 0.0 s; 0% of the untracked reads came while the other controller was within 0.3 m

![chart](chart.png)

## On the recording

- **OpenXR Simulator preview**: `begone_second_2026-11-27_15-32-19.mp4`; the log starts -17.46 s into it (log clock).
  - jitter 69-70s: 90 frames; on screen 0 pictures held, 0 jumped, in `<recordings>/hard-reset-vr/begone-frames/2026-11-27_15-33-46_openxr/jitter_69-70s_headset/` (sheet.jpg)
  - jitter 84-85s: 90 frames; on screen 0 pictures held, 0 jumped, in `<recordings>/hard-reset-vr/begone-frames/2026-11-27_15-33-46_openxr/jitter_84-85s_headset/` (sheet.jpg)
- **game window**: `begone_2026-11-27_15-32-18.mp4`; the log starts -17.46 s into it (log clock).
  - jitter 69-70s: 90 frames; on screen 2 pictures held, 0 jumped, in `<recordings>/hard-reset-vr/begone-frames/2026-11-27_15-33-46_openxr/jitter_69-70s_game/` (sheet.jpg)
  - jitter 84-85s: 90 frames; on screen 0 pictures held, 0 jumped, in `<recordings>/hard-reset-vr/begone-frames/2026-11-27_15-33-46_openxr/jitter_84-85s_game/` (sheet.jpg)
