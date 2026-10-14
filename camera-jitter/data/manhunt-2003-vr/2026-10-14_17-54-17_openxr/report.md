# manhunt-2003-vr (openxr)

Log `manhunt-2003-vr_20261014-175156.jl`, checked 2026-10-14 17:54. `[measured]`: one VR session.

JITTER in 2 of 2 turning seconds. Why: the camera is written 0.00 times per picture: it is renewed less often than pictures are drawn, so some pictures reuse the previous camera (the rate fault); 98% of pose reads returned the same pose as the read before while the head was turning: the runtime handed a stale pose (the pose is read at the wrong moment, or from a cached copy, not once per frame after waiting for the frame).

- quality: **good**
- 141.07 s; 60.1 pictures, 60.1 pose reads, 0.0 camera writes a second
- turning 2 s, jitter 2 s

## Why

- the camera is written 0.00 times per picture: it is renewed less often than pictures are drawn, so some pictures reuse the previous camera (the rate fault)
- 98% of pose reads returned the same pose as the read before while the head was turning: the runtime handed a stale pose (the pose is read at the wrong moment, or from a cached copy, not once per frame after waiting for the frame)

![chart](chart.png)
