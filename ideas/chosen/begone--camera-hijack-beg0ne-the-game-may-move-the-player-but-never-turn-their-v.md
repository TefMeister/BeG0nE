# Camera Hijack BeG0nE — the game may move the player, but never turn their view

Order: 17001
From: the ideas repo, `games/begone.md` (<https://github.com/TefMeister/mod-ideas/blob/main/games/begone.md>), copied 2026-11-03

`[settled]` (Tefa, 2026-11-03: build it, named Camera Hijack BeG0nE) · `[looks doable]` — ⚠️ **not checked against any real engine yet**; judged from what Camera Jitter BeG0nE
already records and from how the Village mods already reach the camera.

> "another BeG0nE tool we need - Camera Jack BeG0nE - for removing game taking over camera control in VR, it can be a
> neauseating experience and very much needed later in re8, so it can still move with the player movement - for
> example being knocked back by an explosion, but never moving left, right, up or down by itself, only the position
> changes."

In VR only the player's head (and their own turn stick) may turn the view. Whatever the game does to the camera's
direction by itself (recoil kick, flinch when hit, being turned to face an attacker or a scripted event, camera shake,
head bob, the look during a grab) gets thrown away. The game may still MOVE the player: knockback from an explosion,
being shoved, falling, crouching.

Two halves, BeG0nE style: first a recorder that catches every moment the game turns the view (how far, how long, what
was happening), then the cure per game, first in Resident Evil Village.

**What it'd take:** each frame, compare the direction the view should have (headset plus the player's own turning)
with the direction the game asked for; anything left over is the game taking the camera. Camera Jitter BeG0nE already
logs the headset and the camera we write, so part of the recorder may reuse it; the new part is reading the game's own
camera before our VR layer touches it. Named Camera Hijack BeG0nE (easier to find in a search than "Camera Jack").
Hard case to watch: grab attacks that turn you to face the enemy; with the turn removed you might be looking away from
the animation.

---
