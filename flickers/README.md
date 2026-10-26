# Flickers BeG0nE

A tool that catches and hides single-frame flickers in VR games, part of BeG0nE: tools that remove unwanted things
from video games, for modders and gamers alike.

## The problem

In some VR mods a part of the picture flashes wrong for one frame at a time: grass that blinks, a dark blot that
appears and is gone. Resident Evil 4 (2023) in VR with praydog's REFramework does it in tall grass, a few times a
second, with no rhythm. Every setting has been tried and none changes it. Because each flash lasts one frame, it is
almost impossible to say by eye what is wrong with it.

## The cure

**First, catch it.** Flickers BeG0nE sits between the game and the headset and keeps the last 72 pictures of each eye.
Every so often (or when asked) it saves them, together with where the head was and what the game sent for each
frame. A reader then compares every frame with the one before and the one after, marks the frames that differ from
both, and shows where in the picture the flash was.

**Then, hide it**, from what the catch shows. If the game sent the wrong picture or the wrong head position for a frame,
the frame before is shown in its place, at its own head position, so the headset moves it to where you are looking.
If only a patch of the picture flashes, only that patch is filled in from the frame before. A frame is never held
twice in a row, and no delay is added to the picture.

## Where it stands

Built 2026-10-26, step 1 of 3: the catcher and the reader. Tested on the dev PC with no game and no headset: a test
program drives the OpenXR Simulator, Flickers BeG0nE paints pink squares into known frames as fake flickers, and the
reader found all of them and nothing else. Both shapes work:

- **the RE4 fix**: one file, `flickers_begone.dll`, for REFramework's `reframework/plugins/` folder;
- **a layer for any OpenXR game**, used for testing.

Next: the first real catch in Resident Evil 4's grass, on the home PC. No download yet; the hiding part is built
only after the catch shows what the flash is.

## Trying the catcher (for testing)

1. Copy `flickers_begone.dll` into `reframework/plugins/` in the game folder.
2. In the game folder, make `reframework/data/flickers/flickers_begone.txt` with:

```
on = 2
exe = re4.exe
```

3. Play in VR. Every 20 seconds the last 72 frames of each eye are saved into `reframework/data/flickers/`. A file
   named `dump_now` in the same folder saves them straight away.
4. `python flickers/tools/flicker_read.py <saved folder>` writes the report, a contact sheet and a heat map.

`on = 0`, or no settings file, means the plugin does nothing at all.

## What it is, and what it is not

- **It is** a small add-on that catches flickers, and later hides them, in games that already run in VR.
- **It is not** a VR mod by itself.
- **It contains** no game files, only our own code.
