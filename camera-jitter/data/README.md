# Checked VR sessions

One folder per VR session a mod logged, under the game's name: `<date>_<time>_<runtime>/` with `summary.json`
(all the numbers, per second), `report.md` (the plain verdict and WHY) and `chart.png` (coloured bands only).
`INDEX.csv` has one line per session.

- The rider (`ride-along/ride.py`) files these by itself the moment a VR session ends. The log it read is
  kept on the PC that made it (`%LOCALAPPDATA%\BeG0nE\jitter\done\`), never in the repo.
- Nothing in here is a picture from a game, and nothing in here is deleted.
- `quality`: `good`, `no-movement` (the head hardly turned, so no jitter verdict), `faulty`.
- Every verdict is `[measured]`: one VR session of one mod build. Say which build beside it when it is quoted.
