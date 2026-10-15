# mad-max-vr (openxr-openxr-simulator-runtime)

Log `mad-max-vr_20261015-184231.jl`, checked 2026-10-15 18:48. `[measured]`: one VR session.

JITTER in 4 of 4 turning seconds. Why: 98% of pose reads returned the same pose as the read before while the head was turning: the runtime handed a stale pose (the pose is read at the wrong moment, or from a cached copy, not once per frame after waiting for the frame).

- quality: **good**
- 336.04 s; 56.0 pictures, 60.0 pose reads, 10.5 camera writes a second
- turning 4 s, jitter 4 s

## Why

- 98% of pose reads returned the same pose as the read before while the head was turning: the runtime handed a stale pose (the pose is read at the wrong moment, or from a cached copy, not once per frame after waiting for the frame)

![chart](chart.png)
