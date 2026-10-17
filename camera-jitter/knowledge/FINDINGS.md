# What the numbers say about jittery and smooth VR cameras

Rebuilt 2026-10-17 13:31 from 36 sessions boiled down to 5 patterns. A pattern is one way a mod behaved; sessions that behaved the same way count as one. A pattern is grouped as jittery or smooth by the checker's own verdict, unless the person wearing the headset said otherwise with `ride.py label`, which outranks it. Every line is `[measured]` on our own mods.

## Across every game, headset simulator

4 patterns: 4 jittery, 0 smooth, 0 contradictory, 4 unlabelled.

Nothing to compare yet: it needs at least one labelled jittery and one labelled smooth pattern.

## mad-max-vr, headset simulator

4 patterns: 4 jittery, 0 smooth, 0 contradictory, 4 unlabelled.

Nothing to compare yet: it needs at least one labelled jittery and one labelled smooth pattern.

## Across every game, VR, headset or simulator not recorded

1 patterns: 1 jittery, 0 smooth, 0 contradictory, 1 unlabelled.

Nothing to compare yet: it needs at least one labelled jittery and one labelled smooth pattern.

## alan-wake-vr, VR, headset or simulator not recorded

1 patterns: 1 jittery, 0 smooth, 0 contradictory, 1 unlabelled.

Nothing to compare yet: it needs at least one labelled jittery and one labelled smooth pattern.

## Every pattern

| id | label | where | seen | games | camera writes per picture | pose reads per picture | pictures that went out with an unchanged camera | pose reads identical to the one before | pictures whose camera matches the pose before last | age of the pose when the camera was written (ms) | pictures a second | checker said |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `1355201647` | unlabelled | headset simulator | 1 | mad-max-vr | 1 | 1.15 | 1 | 1 | 0 | 2 | 55 | JITTER |
| `13603247e5` | unlabelled | headset simulator | 1 | mad-max-vr | 1 | 1.05 | 1 | 1 | 0 | 2 | 60 | JITTER |
| `33a9744571` | unlabelled | headset simulator | 1 | mad-max-vr | 1 | 1 | 0 | 0 | 0 | 4 | 60 | JITTER |
| `bdbaf5b39c` | unlabelled | VR, headset or simulator not recorded | 1 | alan-wake-vr | 1.15 | 1 | 0.2 | 0 | 0.5 | 8 | 70 | JITTER |
| `cd363d8de5` | unlabelled | headset simulator | 1 | mad-max-vr | 1 | 1.05 | 1 | 1 | 0 | 2 | 55 | JITTER |
