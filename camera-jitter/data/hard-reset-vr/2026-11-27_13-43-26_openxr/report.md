# hard-reset-vr (openxr, VR, headset or simulator not recorded)

Log `hard-reset-vr_20261127-133749.jl`, checked 2026-11-27 13:44. `[measured]`: one VR session.

The head hardly turned, so this log cannot show jitter. Turn the head slowly and smoothly for a few seconds.

- quality: **no-movement**
- 336.98 s; 90.0 pictures, 90.0 pose reads, 1192.7 camera writes a second
- turning 1 s, jitter 1 s

## Controllers (Occlusion Drift BeG0nE)

- left: 90.0 reads a second; untracked 0.0% of the time, longest 0.0 s; 0% of the untracked reads came while the other controller was within 0.3 m
- right: 90.0 reads a second; untracked 0.0% of the time, longest 0.0 s; 0% of the untracked reads came while the other controller was within 0.3 m

![chart](chart.png)

## On the recording

- **OpenXR Simulator preview**: `begone_second_2026-11-27_13-38-41.mp4`; the log starts -52.54 s into it (log clock).
  - jitter 250-251s: 90 frames; on screen 1 pictures held, 0 jumped, in `<recordings>/hard-reset-vr/begone-frames/2026-11-27_13-43-26_openxr/jitter_250-251s_headset/` (sheet.jpg)
- **game window**: `begone_2026-11-27_13-38-41.mp4`; the log starts -52.54 s into it (log clock).
  - jitter 250-251s: 90 frames; on screen 1 pictures held, 1 jumped, in `<recordings>/hard-reset-vr/begone-frames/2026-11-27_13-43-26_openxr/jitter_250-251s_game/` (sheet.jpg)
