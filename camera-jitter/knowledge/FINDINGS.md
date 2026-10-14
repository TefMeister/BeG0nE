# What the numbers say about jittery and smooth VR cameras

Rebuilt 2026-10-14 17:57 from 4 sessions boiled down to 1 patterns. A pattern is one way a mod behaved; sessions that behaved the same way count as one. A pattern is grouped as jittery or smooth by the checker's own verdict, unless the person wearing the headset said otherwise with `ride.py label`, which outranks it. Every line is `[measured]` on our own mods.

## Across every game

1 patterns: 1 jittery, 0 smooth, 0 contradictory, 1 unlabelled.

Nothing to compare yet: it needs at least one labelled jittery and one labelled smooth pattern.

## alan-wake-vr

1 patterns: 1 jittery, 0 smooth, 0 contradictory, 1 unlabelled.

Nothing to compare yet: it needs at least one labelled jittery and one labelled smooth pattern.

## Every pattern

| id | label | seen | games | camera writes per picture | pose reads per picture | pictures that went out with an unchanged camera | pose reads identical to the one before | pictures whose camera matches the pose before last | age of the pose when the camera was written (ms) | pictures a second | checker said |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `650cf56b9d` | unlabelled | 1 | alan-wake-vr | 1.15 | 1 | 0.2 | 0 | 0.5 | 8 | 70 | JITTER |
