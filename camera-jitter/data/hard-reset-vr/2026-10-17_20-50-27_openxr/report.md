# hard-reset-vr (openxr, VR, headset or simulator not recorded)

Log `hard-reset-vr_20261017-202339.jl`, checked 2026-10-17 20:53. `[measured]`: one VR session.

The head hardly turned, so this log cannot show jitter. Turn the head slowly and smoothly for a few seconds.

- quality: **no-movement**
- 1608.19 s; 60.5 pictures, 60.5 pose reads, 163.2 camera writes a second
- turning 0 s, jitter 0 s

## Controllers (Occlusion Drift BeG0nE)

- left: 60.5 reads a second; untracked 0.0% of the time, longest 0.0 s; 0% of the untracked reads came while the other controller was within 0.3 m
- right: 60.5 reads a second; untracked 0.0% of the time, longest 0.0 s; 0% of the untracked reads came while the other controller was within 0.3 m

![chart](chart.png)

## On the recording

- **OpenXR Simulator preview**: `begone_second_2026-10-17_20-29-52.mp4`; the log starts -373.95 s into it (log clock).
- **game window**: `begone_2026-10-17_20-29-52.mp4`; the log starts -373.95 s into it (log clock).
- ⚠️ the recording broke at 374 s of the log: OBS showed "OBS has crashed!" (restarted).
