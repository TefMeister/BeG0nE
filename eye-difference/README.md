# Eye Difference BeG0nE

A tool that finds everything that differs between the left and the right eye in VR, part of BeG0nE: tools that
remove unwanted things from video games, for modders and gamers alike.


## The problem

In a VR mod both eyes should show the same world, only seen from two places a few centimetres apart. Some mods get
this wrong, and it is very hard to say what is wrong by looking, because the brain mixes the two eyes into one
picture:

- **the light differs**: one eye darker, brighter or tinted (Resident Evil 2, 7 and Village under REFramework's
  Single Frame Multipass had a golden veil in one eye, since cured);
- **things in one eye only**: plants, bushes or objects drawn in one eye and missing from the other (Resident Evil 4
  with REFramework, 2026-10-31: *"some flora is missing from the right eye and is present in the left"*);
- **picture quality differs**: one eye jagged or pixelated, the other smooth (the same RE4 run: *"the left eye has
  more pixelated look ... right eye image quality is really nice"*);
- **the eyes out of line**: one eye's picture higher than the other, or the eyes pointing apart.


## How it works

It reads the pictures the **Flickers BeG0nE** catcher saves: both eyes of the same frames, with where each eye was
and how much it could see. For each frame:

1. The right eye's picture is turned into the left eye's view, using the two eyes' recorded directions and fields of
   view. After that, things far away sit in the same place in both, and nearer things are shifted sideways only,
   more the nearer they are. That shift is normal 3D.
2. Every small square of each eye looks for its partner in the other eye along that sideways line. A spot with no
   partner anywhere is something in one eye only. When both eyes have such a spot in the same place (a plant in one
   eye hides background the other eye shows), the eye that has the thing is the one whose spot looks unlike its own
   surroundings; when that cannot be told, the report says "unclear" and the picture sheet shows both.
3. Thin strips beside nearer things are normal: one eye sees a little way round their edge. They are counted apart
   and never reported as faults.
4. Light, tint, picture quality and line-up are measured on the part both eyes see.

It writes three files into the catcher's dump folder:

- `eyediff-report.txt`: one verdict line, then each finding in plain words: which eye, where in the picture (and how
  far across and down, in percent), in how many of the frames;
- `eyediff-sheet.jpg`: the worst frames, left eye, right eye and the findings marked, then close-ups of each finding
  in both eyes side by side;
- `eyediff-heat.png`: where the differences sat, over all frames read.


## Using it

1. Install the Flickers BeG0nE catcher (see [flickers/README.md](../flickers/README.md)): `flickers_begone.dll` in
   `reframework/plugins/`, or the OpenXR layer for other OpenXR games.
2. For eye differences only a few frames per catch are needed, so keep the catches small. In
   `reframework/data/flickers/flickers_begone.txt`:

```
on = 2
exe = re4.exe
ring_frames = 8
auto_dump_s = 10
max_dumps_auto = 20
```

   That saves the last 8 frames of each eye every 10 seconds, up to 20 times a launch (about 50 MB each at Quest 3
   size). A file named `dump_now` in the same folder saves a catch straight away.
3. Play, and look at the place where the difference shows.
4. Read the newest catch, or all of them:

```
python -I eye-difference/tools/eye_diff_read.py <game>/reframework/data/flickers
python -I eye-difference/tools/eye_diff_read.py <game>/reframework/data/flickers --all
```

Needs Python with `numpy` and `opencv-python`.


## Tested

On the dev PC, 2026-11-02, with no game and no headset (`tools/test_eye_diff_read.py`, 16 checks, all pass):
made-up eye pairs written as real catcher dumps and read back. It says the eyes match when they only differ by normal
3D (a near board, also with lopsided Quest-style views drawn ray by ray, and with one eye turned 8 degrees); it finds
a plant or a bush in one eye only and names the right eye, in both directions; it finds one eye 15 % darker, a patch
lit in one eye only, one eye blurred, one eye blocky, and the eyes 6 pixels out of line. It also read three real
catches from the catcher on the OpenXR Simulator (its test program draws flat bars, not a 3D scene, so those only
prove the reading works). Not yet used on a real game.


## What it is, and what it is not

- **It is** a reader for modders: it says what differs between the eyes and where, so the cause can be hunted.
- **It is not** a fix by itself, and not a VR mod. It changes nothing in the game.
- **It contains** no game files, only our own code.

Made with Tefa for the Resident Evil 4 plants seen in one eye only. Reports and fixes are welcome as GitHub issues on
this repo.
