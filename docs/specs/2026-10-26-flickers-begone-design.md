# Flickers BeG0nE: design

**Status:** written 2026-10-26, waiting for Tefa's approval. Nothing built yet.
**First target:** the grass flickering in Resident Evil 4 (2023 remake) in VR under REFramework.

## 1. What it is, and why

Tefa, 2026-10-26: *"please create a tool called Flickers BeG0nE ... masking it by showing the last frame when the
flicker happens, instead of the flickered frame image. we need to fix RE4 flora flickering with this, if at all
possible"*.

A cure for flicker that does not hunt the cause: notice the picture (or the part of it) that went wrong for one
frame, and show the last good one in its place. Like every BeG0nE cure it comes in two shapes: a tool for modders
that works on any game, and a drop-in fix per game.

## 2. What we know about the RE4 flicker

- **VR only, and it does not respond to settings.** Tefa tried every setting; it is *"something to do with
  reframework itself"*; flat play is not thought to flicker `[reported 2026-10-26]`.
- **Read from Tefa's clip** (`re4 flora.mp4`: 668 frames, 1440x720, 60 fps, the desktop mirror of one eye),
  with the camera motion removed frame by frame `[measured 2026-10-26, one clip]`:
  - the back-and-forth changes sit in the tall grass, about **3x** as often as on the rock beside it;
  - each one is a **single frame**: a patch of grass changes and the next frame is back to normal (frame 64 shows a
    dark blot in the grass for exactly one frame);
  - they come in small bursts every 10-20 frames with **no fixed rhythm** (frame-to-frame correlation 0.14 or less),
    so it is not a steady every-other-frame shimmer.
- ⚠️ The clip is the desktop mirror, recorded at 60 fps; the headset runs faster and the mirror can skip or repeat
  frames. The frames caught inside the game (section 5) are the real evidence; the clip only says where to look.

## 3. The lesson from Village (why the catcher comes first)

The Village scope had a one-frame flicker too. Holding the last picture was built (`hold 2`) and worn, and it did
**not** hide it: the detector never saw the bad frames (their change was 0.026, under every threshold), so it held
the wrong ones `[verified-live 2026-09-21]`. What cured it was catching frames by hand (the wearer pulled the trigger
on each flicker, the last 90 pictures went to disk), which showed the bad frame had been built with **the other
eye's projection**; rejecting that one reading fixed it `[verified-live 2026-09-22]`. Code:
`staging/re-village-scope-vr/plugin/src/flicker_ring.cpp`, `hold_math.h`, `eye_lock.h`, `tools/ring_view.py`.

So: **the detector is the hard part, and it is designed from real caught frames, not guessed.** And: the catcher
records which eye and which pose every frame was submitted with, because if RE4's bad frames turn out to carry the
other eye's data the way Village's did, the cure becomes a cheap swap with no picture analysis at all.

## 4. Goals, non-goals, anti-goals

**Goals**
1. A catcher that captures the bad frames from inside the VR pipeline, for each eye, on a button press.
2. A cure that hides them so Tefa stops noticing the grass flicker.
3. Works for any game that draws VR through OpenXR (REFramework, UEVR, our own mods), RE4 first.

**Non-goals**
- Finding the root cause inside REFramework (welcome if the catcher hands it to us, never required).
- Flat-screen play. Public release (private zip only, per the no-public-releases rule).

**Anti-goals: it has failed, even if it "works", if:**
- it adds a frame of delay to what the headset shows (waiting for the next frame to decide is not allowed: delay in
  VR is sickness);
- the picture stalls: a frame is never held twice in a row, and head turns must not judder;
- it touches game files or needs REFramework to be rebuilt.

## 5. How it works

### 5.1 Where it sits: an OpenXR layer

A small OpenXR API layer, the same kind as VR Bench's layer (`vr-bench/layer/`), between the game and the OpenXR
runtime. REFramework already runs RE2, Village and RE4 over OpenXR on the home PC (Virtual Desktop)
`[verified-live 2026-09-10 for RE2]`. The layer sees every eye picture the game hands to the headset, with the pose
and field of view it was drawn for, so it works the same for every OpenXR game. Direct3D 12 first (RE Engine);
Direct3D 11 later, when a game needs it.

It watches:
- `xrCreateSession`: the D3D12 device and queue (from the graphics binding);
- `xrCreateSwapchain` / `xrEnumerateSwapchainImages`: the images the game draws into;
- `xrReleaseSwapchainImage`: the moment an eye picture is finished, the last point it can still be copied or replaced;
- `xrEndFrame`: which image, which part of it, which pose and which field of view each eye was submitted with.

Installed the VR Bench way: registered as an implicit layer with a `disable_environment` switch, and it does nothing
unless a settings file next to it names the game's exe. Its own on/off entry in Lanes `features.py`.

### 5.2 Part 1: the catcher

- Keeps the last **72 frames per eye** (about a second) on the graphics card at **half size** (a setting; full size
  at 90 frames x 2 eyes would cost about 3 GB).
- On a trigger it writes them to disk, oldest first, as PNGs plus `frames.txt`: one line per frame with frame number,
  eye, the pose and field of view submitted, the swapchain image index, display time, and whether the picture was
  identical to that eye's previous one (this shows whether REFramework repeats or alternates eyes).
- Trigger, automatic first (BeG0nE runs itself, Tefa 2026-10-17): **auto mode** dumps every N seconds (default 20)
  while a session stands the player in the grass and turns the head slowly with the OpenXR Simulator or VR Bench, and
  the reader finds the odd frames by itself (it found them in Tefa's clip the same way). By hand only if the automatic
  catch keeps coming back empty: a controller chord held for half a second (the VR Bench "look at this" chord: both
  grips and the left trigger; the layer only watches it, the game still gets the buttons), a key, or a control file.
- At most 4 dumps per launch by hand, 12 in auto mode (settings).
- A reader, built from this session's clip analysis and Village's `ring_view.py`: removes the head motion between
  frames, marks the odd frame out, and writes a contact sheet and a heat map of where the flicker sits. It also
  compares each odd frame's pose and field of view with its neighbours: **other-eye data on the odd frame = the
  Village cause**.

### 5.3 Part 2: the cure (designed after the catch)

The detector is chosen from what the caught frames show. Two shapes are ready:

- **A. Wrong data** (the bad frame carries the other eye's pose or field of view, or a repeated stale picture): the
  layer swaps in that eye's previous picture **and submits that picture's own old pose**. The headset runtime already
  turns a picture to match the current head position from the pose it was submitted with, so a held picture still sits
  still in the world while the head turns. No picture analysis.
- **B. Only the grass is wrong** (pose and data look normal): a small GPU check on a reduced-size copy of each eye
  compares the new picture with that eye's previous one, after turning the old one to the new head angle, tile by tile.
  A tile that changes far more than the recent average is replaced by the turned old tile; the rule is Village's
  `hold_math.h` (never twice in a row, a real fast move goes straight through). All on the graphics card, decided and
  applied in the same frame, so no delay and no wait. Only the flickering tiles change, which gives "just the plants"
  without knowing what a plant is.

Both log every hold, so a run can be checked against Tefa's own count.

### 5.4 Testing without the game

The layer gets a **fake flicker** knob: it spoils a patch of one eye for one frame at random, with known frame numbers.
On the dev PC (no RE4, no headset) the OpenXR Simulator and a D3D12 OpenXR test app run it: the catcher must catch
those exact frames, and the cure must hold exactly them (and nothing during a steady pan). Only then does it go to the
home PC.

## 6. Order of work and gates

| Step | What | Gate |
| --- | --- | --- |
| 1 | Layer skeleton (hooks, eye bookkeeping, logs), fake flicker, catcher, reader; self-test with the simulator | PD / FLAT, dev PC |
| 2 | Auto catch in RE4: the session stands in the grass and turns the head with the simulator or VR Bench; the reader marks the odd frames. Only if that stays empty: Tefa holds the chord on each flicker | VR CLAUDE, home PC (VR USER only as the fallback) |
| 3 | Read the caught frames; choose A or B; build it; fake-flicker test again | PD |
| 4 | Tefa wears the cure: grass flicker gone, no stutter? | VR USER, home PC |
| 5 | RE4 drop-in package, guide for modders | PD |

## 7. Where things live

- Layer and reader code: `lanes-plugin/begone/flickers/` (BeG0nE's tools ship in Lanes).
- `BeG0nE/flickers/`: README (the fault and the cure in plain words), `guide/`, `games/re4/`, caught data in `data/`.
- Board rows on `claude-memory/status/BeG0nE.md`.

## 8. Calls made by the builder (Claude), open to change

- An OpenXR layer instead of a REFramework plugin: it works in every OpenXR game and needs no REFramework patch.
- 72 frames per eye at half size in the catcher; 4 dumps per launch.
- The VR Bench chord as the catch button.
- Code shape: files under 800 lines, every number named in one settings table.
- Settled in step 1 by logging, not guessed: whether REFramework hands the runtime D3D12 or D3D11 pictures, and
  whether it submits both eyes every frame or one eye per frame. The layer supports whichever it finds; D3D11 support
  moves up if that is what RE4 uses.
- Dev PC test app: the OpenXR SDK's `hello_xr` in D3D12 mode, with the OpenXR Simulator.
- ⚠️ Open risk: the flicker may not appear with the simulator standing in for the headset (it may need the real
  runtime timing). Then step 2 falls back to the chord, with Tefa.
