# alan-wake-vr (openxr)

Log `alan-wake-vr_20261011-203358.jl`, checked 2026-10-11 21:08. `[measured]`: one VR session.

JITTER in 329 of 636 turning seconds. Why: the camera held still on some pictures although it was written and the pose was fresh: the value written did not change, so look at what sits between the pose and the camera (a smoothing step, a rounding, a once-per-tick game update).

- quality: **good**
- 1990.57 s; 72.0 pictures, 72.0 pose reads, 88.9 camera writes a second
- turning 636 s, jitter 329 s

## Why

- the camera held still on some pictures although it was written and the pose was fresh: the value written did not change, so look at what sits between the pose and the camera (a smoothing step, a rounding, a once-per-tick game update)

![chart](chart.png)
