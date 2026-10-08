# Checked recordings

One folder per recording, under the game's name: `<date>_<time>_<label>/` with `summary.json`, `report.md` and
`chart.png`. `INDEX.csv` has one line per view (game window, headset view) of every run.

- Nothing in here is a picture from a game. The recordings themselves stay on the PC that made them.
- Nothing in here is deleted. A run that cannot be trusted is marked `discarded` in `INDEX.csv` and in its
  `summary.json`, with the reason.
- `quality` says how much a run is worth: `good`, `no-movement` (the view hardly moved, so no jitter verdict),
  `faulty` (too short, or frozen for most of its length), `discarded`.
- Every verdict is `[measured]`: one recording, one scene, judged by the tool version in the `tool` column.
