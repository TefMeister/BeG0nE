# Camera Jitter/Shake BeG0nE

**Started 2026-10-01. The measuring tool exists since 2026-10-08; nothing cures anything yet.** A cure for the
jittery camera that VR mods written with AI code so often have. First find out why. Then a tool for modders. Then a
jitter-free camera for specific games.

## The problem

Many VR mods written with AI help share one fault: when you move slowly and smoothly, the camera does not. It
steps. It teleports a little way, holds, and steps again. Players call it jitter or shake. It is not the
headset's frame rate, and it is not reprojection; those look different and a VR player tells them apart. It
happens across different games and different engines, which suggests the cause is in how the mods are written,
not in any one game.

## What is here

### `tool/` — reads a recording and says what the view did

It rides along a modding session. Every recording the session makes (the game window, and the headset's own view
beside it when there is one) is checked the moment it is saved, and the verdict is kept in `data/`.

Four words, in the vocabulary of the person who wears the headset:

| Word | What the tool saw | What it points at |
| --- | --- | --- |
| **JITTER** | new pictures kept arriving, but during a slow smooth movement some of them did not move at all; the view stepped | the camera is renewed **less often** than the picture is drawn: a rate fault in the mod |
| **SHAKE** | the view moved back against its own direction, or wobbled while the head was still | a pose or geometry fault: something attached to the wrong thing |
| **hitch** | no new picture for a few frames | frame rate, not the camera |
| **freeze** | no new picture for a second or more | the game stopped, or a still screen (menu, loading); the second view tells which |

It also notices a **jump** (one snap of the view in an otherwise still second: a nudge, a snap turn, a respawn)
and a **cut** (scene change) and keeps both out of the jitter verdict, and it says when the view **barely
moved**, because a recording with no movement cannot show jitter.

How it measures: each frame is shrunk, greyed, and compared with the last one. A frame that differs is a **new
picture**. For every new picture it reads how far the whole view slid since the previous new picture (phase
correlation on the middle 70% of the picture, so HUDs and borders stay out). A second of recording in which the
view travels, but a fifth or more of its new pictures did not move, is a JITTER second. Two eyes side by side
are recognised (the left half is found inside the right half) and only the left eye is measured.

```
python camera-jitter/tool/begone_jitter.py scan <recording.mp4>         # check one, plus its headset twin
python camera-jitter/tool/begone_jitter.py follow                       # watch the recordings folder
python camera-jitter/tool/begone_jitter.py runs                         # every run, newest first
python camera-jitter/tool/begone_jitter.py discard <run> "<why>"        # mark a run as not to be trusted
python camera-jitter/tool/begone_jitter.py headset-window               # the --also value to record the headset view
python camera-jitter/tool/begone_jitter.py selftest                     # nine made-up recordings, known answers
```

Needs Python 3 with `opencv-python` and `numpy`. The self-test needs `ffmpeg` on the path to encode its clips
the way OBS does; without it the clips are checked unencoded.

**What its numbers are worth.** Everything it prints is `[measured]`: one recording, one scene. It judges only
what reached the recording. A capture that got 30 pictures a second from a game drawing 90 cannot see a step
that lasts one game frame, and the report says so when the rate is low. It cannot tell a jump of the view from
the player snapping the camera on purpose; that is what the **note** on a run and `discard` are for. The
thresholds live at the top of `tool/jitter_judge.py` and `tool/jitter_measure.py`, each with its name and the
measurement that set it.

### `data/` — every run, kept

One folder per checked recording: `summary.json` (all the numbers, per second), `report.md` (the plain verdict)
and `chart.png` (coloured bands and a line, never a picture from the game). `data/INDEX.csv` lists them all.
Runs are never deleted; a run the numbers got wrong is marked **discarded** with the reason and stays, so the
next reading of the data knows to skip it.

The data is what the cause will be found from: the same mod, before and after a change; a mod that jitters
beside one that does not; the game window beside the headset view of the same seconds.

## The plan

1. **Find out why.** Compare mods that jitter with mods that do not, in games we can measure. The working
   suspicion is a rate fault: the camera's position is renewed less often than the headset's movement is
   sampled, so each frame shows a pose that is a step behind. That is a hypothesis until it is measured.
   `[hypothesis]`
2. **A tool for modders**, human or AI: how to tell jitter from the other faults, where to look in a mod for
   the cause, and the code shape that avoids it. Goes in `guide/`.
3. **A jitter-free camera for specific games**, as a drop-in fix beside the game's VR mod, once the cause is
   understood. Goes in `games/<game>/`.
