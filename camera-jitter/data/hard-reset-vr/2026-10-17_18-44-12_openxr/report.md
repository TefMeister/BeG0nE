# hard-reset-vr (openxr, VR, headset or simulator not recorded)

Log `hard-reset-vr_20261017-181240.jl`, checked 2026-10-17 18:47. `[measured]`: one VR session.

The head hardly turned, so this log cannot show jitter. Turn the head slowly and smoothly for a few seconds.

- quality: **no-movement**
- 1891.76 s; 60.6 pictures, 60.6 pose reads, 117.3 camera writes a second
- turning 0 s, jitter 0 s

## Controllers (Occlusion Drift BeG0nE)

- left: 60.6 reads a second; untracked 0.0% of the time, longest 0.0 s; 0% of the untracked reads came while the other controller was within 0.3 m
- right: 60.6 reads a second; untracked 0.0% of the time, longest 0.0 s; 0% of the untracked reads came while the other controller was within 0.3 m

![chart](chart.png)

## On the recording

- **OpenXR Simulator preview**: `begone_second_2026-10-17_18-17-57.mp4`; the log starts -317.27 s into it (log clock).
- **OpenXR Simulator preview**: `begone_second_2026-10-17_18-26-22.mp4`; the log starts -821.66 s into it (log clock).
- **game window**: `begone_2026-10-17_18-17-57.mp4`; the log starts -317.27 s into it (log clock).
- **game window**: `begone_2026-10-17_18-26-21.mp4`; the log starts -821.66 s into it (log clock).
- ⚠️ the recording broke at 822 s of the log: OBS closed (restarted).
