# Camera Jitter/Shake BeG0nE

**Started 2026-10-01. Nothing built yet.** A cure for the jittery camera that VR mods written with AI code so
often have.

## The problem

Many VR mods written with AI help share one fault: when you move slowly and smoothly, the camera does not. It
steps. It teleports a little way, holds, and steps again. Players call it jitter or shake. It is not the
headset's frame rate, and it is not reprojection; those look different and a VR player tells them apart. It
happens across different games and different engines, which suggests the cause is in how the mods are written,
not in any one game.

## The plan

1. **Find out why.** Compare mods that jitter with mods that do not, in games we can measure. The working
   suspicion is a rate fault: the camera's position is renewed less often than the headset's movement is
   sampled, so each frame shows a pose that is a step behind. That is a hypothesis until it is measured.
2. **A tool for modders**, human or AI: how to tell jitter from the other faults, where to look in a mod for
   the cause, and the code shape that avoids it.
3. **A jitter-free camera for specific games**, as a drop-in fix beside the game's VR mod, once the cause is
   understood.

## What is here

Nothing yet. This folder gets a `guide/` and `games/<game>/` the same way the other cures do, as soon as there
is something that has been measured rather than guessed.
