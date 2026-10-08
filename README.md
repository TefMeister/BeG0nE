# BeG0nE

**Tools and mods that remove unwanted features and faults from video games.** Each cure comes in two shapes: a
toolkit for modders, human or AI, that says where to look in a new game and shows the exact code that worked in
each earlier one, and a drop-in fix for gamers, per game, that just needs copying into the game folder.

| Cure | What it removes | Where it stands |
| --- | --- | --- |
| [**Occlusion Drift BeG0nE**](occlusion-drift/) | In VR, a two-handed weapon's aim drifting because the front hand hides the back controller from the headset. The cure: hold the weapon with the left controller just above the right, and the game accepts that as a proper grip. | In testing on Resident Evil Village. No download yet. |
| [**Camera Jitter/Shake BeG0nE**](camera-jitter/) | The stepped, jittery camera that VR mods written with AI code so often have. First job: find out why. Then a tool for modders, and a jitter-free camera for specific games. | Since 2026-10-08 a tool reads every session recording (game window and headset view) and says whether the view stepped, shook, hitched or froze; the runs collect in `camera-jitter/data/`. No cure yet. |

## How this repo is organised

One folder per cure. Inside each: a `README.md` that explains the fault and the cure in plain words, a
`guide/` for modders, and `games/<game>/` with the code and install notes for each game it has been done in.
`docs/specs/` holds the design notes, `ideas/` the ideas filed for this repo.

## Downloads

A game gets a download only after the cure has been tested in that game and passes its own test, written in
the cure's README. Every download says which game and, where it applies, which VR mod it needs.

## What this is, and what it is not

- **It is** a set of small add-ons for games, and for games that already have a VR mod. Each one changes one
  thing and nothing else.
- **It is not** a VR mod by itself. Nothing here makes a flat game run in VR.
- **It contains** no game files, only our own code. Every add-on needs a legitimately owned copy of the game.
- Non-commercial fan work. Rights holders can ask for a correction or removal at any time and it will be done.

## Credits

See [CREDITS.md](CREDITS.md).

## History

Started on 2026-09-28 as *VR Super Infinite OCCLUSION DRIFT BeGonE 3000XXL Turbo*, a kit for one fault only.
Renamed BeG0nE on 2026-10-01 when the second cure was added and the repo became the home for all of them.
