# hard-reset-vr (openxr, VR, headset or simulator not recorded)

Log `hard-reset-vr_20261124-014800.jl`, checked 2026-11-24 01:50. `[measured]`: one VR session.

The head hardly turned, so this log cannot show jitter. Turn the head slowly and smoothly for a few seconds.

- quality: **no-movement**
- 129.81 s; 89.8 pictures, 89.8 pose reads, 2124.8 camera writes a second
- turning 0 s, jitter 0 s

## Controllers (Occlusion Drift BeG0nE)

- left: 89.8 reads a second; untracked 0.0% of the time, longest 0.0 s; 0% of the untracked reads came while the other controller was within 0.3 m
- right: 89.8 reads a second; untracked 0.0% of the time, longest 0.0 s; 0% of the untracked reads came while the other controller was within 0.3 m

![chart](chart.png)

## On the recording

- a recording is still running: once it stops, run `begone.py frames camera-jitter <run> attach`.
