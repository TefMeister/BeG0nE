# What the numbers say about jittery and smooth VR cameras

Rebuilt 2026-10-25 17:40 from 62 sessions boiled down to 13 patterns. A pattern is one way a mod behaved; sessions that behaved the same way count as one. A pattern is grouped as jittery or smooth by the checker's own verdict, unless the person wearing the headset said otherwise with `ride.py label`, which outranks it. Every line is `[measured]` on our own mods.

## Across every game, headset simulator

4 patterns: 4 jittery, 0 smooth, 0 contradictory, 4 unlabelled.

Nothing to compare yet: it needs at least one labelled jittery and one labelled smooth pattern.

## mad-max-vr, headset simulator

4 patterns: 4 jittery, 0 smooth, 0 contradictory, 4 unlabelled.

Nothing to compare yet: it needs at least one labelled jittery and one labelled smooth pattern.

## Across every game, VR, headset or simulator not recorded

9 patterns: 9 jittery, 0 smooth, 0 contradictory, 9 unlabelled.

Nothing to compare yet: it needs at least one labelled jittery and one labelled smooth pattern.

## alan-wake-vr, VR, headset or simulator not recorded

1 patterns: 1 jittery, 0 smooth, 0 contradictory, 1 unlabelled.

Nothing to compare yet: it needs at least one labelled jittery and one labelled smooth pattern.

## hard-reset-vr, VR, headset or simulator not recorded

8 patterns: 8 jittery, 0 smooth, 0 contradictory, 8 unlabelled.

Nothing to compare yet: it needs at least one labelled jittery and one labelled smooth pattern.

## Every pattern

| id | label | where | seen | games | camera writes per picture | pose reads per picture | pictures that went out with an unchanged camera | pose reads identical to the one before | pictures whose camera matches the pose before last | age of the pose when the camera was written (ms) | pictures a second | checker said |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `1355201647` | unlabelled | headset simulator | 1 | mad-max-vr | 1 | 1.15 | 1 | 1 | 0 | 2 | 55 | JITTER |
| `13603247e5` | unlabelled | headset simulator | 1 | mad-max-vr | 1 | 1.05 | 1 | 1 | 0 | 2 | 60 | JITTER |
| `14fbae90c7` | unlabelled | VR, headset or simulator not recorded | 1 | hard-reset-vr | 1.45 | 1 | 0.75 | 0.75 | 0 | 8 | 60 | JITTER |
| `18732c3fcd` | unlabelled | VR, headset or simulator not recorded | 1 | hard-reset-vr | 1.9 | 1 | 0.1 | 0.05 | 0 | 12 | 65 | JITTER |
| `1c8918554c` | unlabelled | VR, headset or simulator not recorded | 1 | hard-reset-vr | 2 | 1 | 0.1 | 0.05 | 0 | 16 | 60 | JITTER |
| `33a9744571` | unlabelled | headset simulator | 1 | mad-max-vr | 1 | 1 | 0 | 0 | 0 | 4 | 60 | JITTER |
| `4fd84f808f` | unlabelled | VR, headset or simulator not recorded | 1 | hard-reset-vr | 2 | 1 | 1 | 1 | 0 | 4 | 60 | JITTER |
| `7edfd2257f` | unlabelled | VR, headset or simulator not recorded | 1 | hard-reset-vr | 2 | 1 | 0.05 | 0.05 | 0 | 4 | 60 | JITTER |
| `bdbaf5b39c` | unlabelled | VR, headset or simulator not recorded | 1 | alan-wake-vr | 1.15 | 1 | 0.2 | 0 | 0.5 | 8 | 70 | JITTER |
| `cc0583d319` | unlabelled | VR, headset or simulator not recorded | 1 | hard-reset-vr | 1.15 | 1 | 1 | 1 | 1 | 8 | 60 | JITTER |
| `cd363d8de5` | unlabelled | headset simulator | 1 | mad-max-vr | 1 | 1.05 | 1 | 1 | 0 | 2 | 55 | JITTER |
| `e5a43b867f` | unlabelled | VR, headset or simulator not recorded | 1 | hard-reset-vr | 1.4 | 1 | 0.8 | 0.8 | 0.9 | 6 | 60 | JITTER |
| `f273af45fa` | unlabelled | VR, headset or simulator not recorded | 1 | hard-reset-vr | 1.6 | 1 | 0.2 | 0.1 | 0 | 10 | 65 | JITTER |
