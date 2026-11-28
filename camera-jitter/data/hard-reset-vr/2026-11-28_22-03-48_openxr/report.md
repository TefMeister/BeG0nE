# hard-reset-vr (openxr, VR, headset or simulator not recorded)

Log `hard-reset-vr_20261128-220156.jl`, checked 2026-11-28 22:04. `[measured]`: one VR session.

The head hardly turned, so this log cannot show jitter. Turn the head slowly and smoothly for a few seconds.

- quality: **no-movement**
- 112.6 s; 90.0 pictures, 90.0 pose reads, 2536.7 camera writes a second
- turning 0 s, jitter 0 s

## Controllers (Occlusion Drift BeG0nE)

- left: 90.0 reads a second; untracked 0.0% of the time, longest 0.0 s; 0% of the untracked reads came while the other controller was within 0.3 m
- right: 90.0 reads a second; untracked 0.0% of the time, longest 0.0 s; 0% of the untracked reads came while the other controller was within 0.3 m

![chart](chart.png)

## On the recording

- a recording is still running: once it stops, run `begone.py frames camera-jitter <run> attach`.
