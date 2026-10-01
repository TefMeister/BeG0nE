> **Superseded in part, 2026-10-01 (Tefa):** the repo is now **`BeG0nE`**, a home for several cures, and this one is
> **Occlusion Drift BeG0nE** in `occlusion-drift/`. The exact-name rule below no longer applies; the design itself still does.

# Dr.BeGonE: design

**Full name:** VR Super Infinite OCCLUSION DRIFT BeGonE 3000XXL Turbo
**Short name:** Dr.BeGonE (Drift Be Gone: a cure for occlusion drift)
**Repo:** `TefMeister/VR-Super-Infinite-OCCLUSION-DRIFT-BeGonE-3000XXL-Turbo` (public). ⭐ Tefa, 2026-09-28: the repo
name is spelled **exactly** like that, capitals and all, and never shortened. Inside the repo, all writing may use
**Dr.BeGonE**. Tefa's reason: a name people remember; a plain "Occlusion Drift Remover" would be porridge with no
strawberry jam.
**Status:** design approved by Tefa 2026-09-28. Nothing built yet.

## 1. The problem it cures

In VR, most people hold a two-handed long weapon (rifle, shotgun) with the left hand stretched out in front of the
right one, the way you would in real life. Seen from the headset, the front hand hides the back one. Inside-out
tracking loses whichever controller it cannot see, and the weapon's aim drifts. This is **occlusion drift**.

The cure is to hold the weapon with the **left controller sitting directly above the right one**. Both controllers
stay in the headset cameras' view, so tracking never drops. The mod makes the game accept that hold as a proper
two-handed grip, and keeps the weapon pointing where the right hand points.

## 2. Who it is for

1. **Gamers** who just want the feature: download, copy into the game folder, play.
2. **Modders, human or AI**, who want to add it to another game: read the guide, look at how each earlier game did
   it, and reuse the code.
3. **Us.** Every VR mod we make gets it, and each game makes the guide better.

Nothing in the repo is tied to Claude. Everything is plain text and ordinary source code.

## 3. What is in the repo

```
dr-begone/
  README.md          what it is, the downloads, what each needs, credits
  guide/
    HOW-IT-WORKS.md  the idea in plain words, independent of any engine
    WHERE-TO-LOOK.md checklist for finding the grip code in a new game; grows with every game
    GAMES.md         one row per game: status, what it needs, link to its folder
  games/
    re-village/      Village: code, notes, build steps, test record
  shared/            code proven to be the same across games (empty until a second game proves something)
  CREDITS.md
  LICENSE
```

**The rule every game follows (engine-independent):**
- A **zone above the right controller** (Village first guesses: 3 to 35 cm up, within 12 cm sideways, 18 cm to let
  go so a wobble cannot flicker it).
- The **left grip button** takes the two-handed grip there, and letting go of the button releases it.
- While the hand is held there, the **weapon follows the right hand alone**. The line between the hands would point
  at the sky, so it is ignored.
- The drawn left hand stays on the forestock, so it still looks right.
- Every number is a named setting, never a bare number in the code.

## 4. Village, the first game

- **Built on:** REFramework by praydog (its built-in RE8 VR mode). Dr.BeGonE is an add-on to that VR mod, not a VR
  mod by itself, and every download says so at the top.
- **Where the code is today:** `re-village-scope-vr/dev-archive/reframework-patch/grip-no-throw.patch`
  (`RE8VR.cpp` / `.hpp`). Worn 2026-09-22: the stacked grip took and the rifle stayed straight
  `[reported 2026-09-22]`. The low shots that day belonged to the scope's aiming maths, not the grip.
- **The split:** the grip patch is currently tangled with the scope mod's other REFramework patches. The Dr.BeGonE
  download must be **stock REFramework plus the grip change only**, so a player without our scope mod gets exactly
  this feature. The grip code's home moves to `dr-begone/games/re-village/`; the scope repo points there instead of
  keeping its own copy (one source, no drift between copies).
- **What the Village download contains (Tefa, 2026-09-28):** the controller height adjustment only, i.e. the
  stacked grip. None of the scope mod.
- **Nice to have, not required:** live sliders in REFramework's in-game menu to move the zone while playing.

## 5. The release gate: what must pass before anything goes public

A game's download is published **only after Tefa has tested it and approved it for public download**.

For Village that means **every long weapon in the game**, each passing one test:

> **The left controller sits visibly above the right one, and the aim does not drift when aiming down the
> scope or the sights.**

Fine-tuning the zone numbers happens during that testing. Until the gate passes, the repo can be public, but it
holds the guide and code only, with no download.

## 6. Release rules for Dr.BeGonE

- Every download says clearly what it is, what it is not, and **which game and which VR mod it needs**.
- **No motion-sickness caution** on Dr.BeGonE downloads. Tefa's decision, 2026-09-28: it does not change the
  camera at all. (The standard caution still applies to our VR mods themselves.)
- CREDITS names everyone whose work it builds on (praydog / REFramework first), plus the standard line that anyone
  missing can email and will be added.
- No game files, ever. Only our own code and our own builds of open-source tools, with their licences kept.

## 7. On the front page

Dr.BeGonE gets **its own category** on the `TefMeister/TefMeister` profile README, separate from the video games
and the plugins (Tefa, 2026-09-28): one row per game it supports, with its status (in testing / approved and
downloadable). It also gets an entry in `INDEX.json` and a dated line in `ACTIVITY.md` whenever it moves.

## 8. How it grows

Each new game adds a folder under `games/`, a row in `GAMES.md`, and anything it taught to `WHERE-TO-LOOK.md`.
When a second game's code turns out to match Village's, that part moves into `shared/`, and not before.
The next likely candidate is the RE2 mod (Visceral), which carries the same socket rule in REFramework's
`FirstPerson.cpp` `[inferred-static 2026-09-22]`.

## 9. Not in scope

- A VR mod of its own, or any camera change.
- Guessing shared code before a second game exists.
- A download for any game Tefa has not approved.
