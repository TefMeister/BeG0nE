# Hand Teleport BeG0nE — the hand never jumps; it travels along the controller's own path to the grab

Order: 17000
From: the ideas repo, `games/begone.md` (<https://github.com/TefMeister/mod-ideas/blob/main/games/begone.md>), copied 2026-10-03

`[raw]` · `[looks doable]` — ⚠️ **not checked against any real engine yet**; judged from how the Village grip
script already moves the hand.

> "hand teleport be gone - instead on a hand jumping on the gun, onto thd lever, it always wollows the
> motion controller's last trajectory to wherever it needs to grab on to, pick up, push, open, reload
> or turn"

A third cure for the set. Today, when a VR mod decides the hand is "on" something (the forestock, a
lever, a door handle, a magazine, a valve), the hand model snaps there in one frame. Tefa wants the
hand to **keep going the way the controller was already moving** and arrive at the grab point along
that line, so the eye reads one continuous motion instead of a cut. It covers every kind of grab:
pick up, push, open, reload, turn.

**What it'd take:** a short "approach" phase between *grab decided* and *grab held*: remember the
controller's last direction and speed, steer the hand model from where it is to the grab point over a
few frames (long enough to read as motion, short enough not to feel laggy, likely 80–150 ms), then hand
over to the held pose. In REFramework games that is a Lua pass in the same place the stacked grip
already runs. The release side is the same idea in reverse (see the all-games "canned animations"
idea, which also asks for no snap in or out).

Related: the [all-games canned-animation idea](all-games.md) asks for the same smooth hand-over when the
game takes the hands for an animation and gives them back.
