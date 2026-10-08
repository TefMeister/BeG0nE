r"""begone_jitter.py -- Camera Jitter/Shake BeG0nE: read recordings of a modding session and say whether the view
steps (jitter), wobbles (shake), stalls (hitch) or stops (freeze), for the game window AND the headset's view.

    python begone_jitter.py scan <video.mp4> [--game <project>] [--headset <video.mp4>|none] [--note "<text>"]
                                  [--wait-headset]   (obs-rec.py stop: wait for the _second_ file to finish writing)
    python begone_jitter.py follow [--root <recordings folder>] [--once]
    python begone_jitter.py runs [--game <project>]
    python begone_jitter.py discard <run folder name> "<why>"
    python begone_jitter.py headset-window
    python begone_jitter.py selftest

scan       Checks one recording. A headset-view recording made beside it (obs-rec.py `--also`, saved as
           <label>_second_<date>.mp4) is found and checked too, and the two are compared.
follow     Rides along a session: watches the recordings folder and checks every new recording once it is
           finished. obs-rec.py `stop` already starts a scan by itself, so follow is for recordings made any other way.
runs       Lists every checked run, newest first, with its quality and verdict.
discard    Marks a run as not to be trusted (Tefa saw something the numbers could not: a crash, a menu), with the
           reason. Nothing is deleted; the run stays, marked.
headset-window
           Finds the window that shows what the headset shows (SteamVR's "VR View", or the OpenXR simulator's
           preview) and prints the `--also` value obs-rec.py needs to record it beside the game.

Every run is stored in camera-jitter/data/<project>/<date>_<time>_<label>/ as summary.json (all the numbers),
report.md (the plain verdict) and chart.png (lines and colours only, never game pictures), and gets a line in
camera-jitter/data/INDEX.csv. The videos themselves are never copied into the repo.
"""
import csv
import json
import os
import re
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import jitter_chart  # noqa: E402
import jitter_judge  # noqa: E402
import jitter_measure  # noqa: E402

TOOL_VERSION = "0.1.0"
DATA = os.path.join(os.path.dirname(HERE), "data")
INDEX = os.path.join(DATA, "INDEX.csv")
INDEX_FIELDS = ["run", "game", "date", "time", "label", "view", "seconds", "pictures_per_s", "travelling_s",
                "jitter_s", "shake_s", "freezes", "hitches", "quality", "verdict", "tool"]
REC_ROOTS = {"RTX": r"D:\MEGA transfer\Videos"}      # same choice as claude-memory/tools/obs-rec.py
REC_ROOT = REC_ROOTS.get(os.environ.get("COMPUTERNAME", ""), r"E:\OBS gameplay videos")
FOLLOW_STATE = os.path.join(os.path.expandvars("%LOCALAPPDATA%"), "begone-jitter-follow.json")
FOLLOW_POLL_S = 15            # how often follow looks for new recordings
SETTLE_S = 20                 # a recording counts as finished when it has not grown for this long
NAME_RE = re.compile(r"^(?P<label>.+?)(?P<second>_second)?_(?P<date>\d{4}-\d{2}-\d{2})_(?P<time>\d{2}-\d{2}-\d{2})\.(mp4|mkv|mov)$")
HEADSET_TITLES = ("VR View", "Headset Window", "OpenXR", "Simulator", "Preview")


def _say(msg):
    print(msg, flush=True)


def _progress(i, total):
    if total:
        print(f"  ... {100 * i // total}%", flush=True)


def _parse_name(path):
    m = NAME_RE.match(os.path.basename(path))
    if not m:
        stamp = time.strftime("%Y-%m-%d_%H-%M-%S", time.localtime(os.path.getmtime(path)))
        return os.path.splitext(os.path.basename(path))[0], stamp[:10], stamp[11:], False
    return m["label"], m["date"], m["time"], bool(m["second"])


def _find_headset_twin(path):
    """The `_second_` file obs-rec.py writes beside the game recording: same label, started within 5 s."""
    label, date, tm, is_second = _parse_name(path)
    if is_second:
        return None
    folder = os.path.dirname(path) or "."
    t0 = time.mktime(time.strptime(f"{date}_{tm}", "%Y-%m-%d_%H-%M-%S"))
    for f in os.listdir(folder):
        m = NAME_RE.match(f)
        if m and m["second"] and m["label"] == label:
            t1 = time.mktime(time.strptime(f"{m['date']}_{m['time']}", "%Y-%m-%d_%H-%M-%S"))
            if abs(t1 - t0) <= 5:
                return os.path.join(folder, f)
    return None


def _check(path, title):
    _say(f"checking the {title}: {os.path.basename(path)}")
    info, rows = jitter_measure.measure(path, progress=_progress)
    return info, rows, jitter_judge.judge(info, rows)


def _compare(game, headset):
    """What the two views together say that neither says alone."""
    g, h = game, headset
    out = []
    if g["freezes"] and not h["freezes"]:
        out.append("The game window froze while the headset view kept getting new pictures: the game itself stalled "
                   "or stopped drawing to its window, the VR side did not.")
    if h["freezes"] and not g["freezes"]:
        out.append("The headset view froze while the game window kept moving: the VR side stopped receiving pictures.")
    if g["freezes"] and h["freezes"]:
        out.append("Both views froze: the whole game stopped (or both were on a still screen).")
    gj, hj = g["seconds_jitter"], h["seconds_jitter"]
    if hj and not gj and g["quality"] == "good":
        out.append("The headset view jitters but the game window does not: the step happens on the way to the headset "
                   "(how the mod hands each eye's picture and pose over), not in the game's own camera.")
    if gj and hj:
        out.append("Both views jitter: the step is already in the game's camera before the picture reaches the headset.")
    if gj and not hj and h["quality"] == "good":
        out.append("The game window jitters but the headset view is smooth: the flat window is not what the player "
                   "sees; judge by the headset view.")
    return " ".join(out) or "The two views agree."


def _run_folder(game, label, date, tm):
    name = f"{date}_{tm}_{label}"
    return name, os.path.join(DATA, game, name)


def _append_index(entry):
    os.makedirs(DATA, exist_ok=True)
    new = not os.path.exists(INDEX)
    with open(INDEX, "a", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=INDEX_FIELDS)
        if new:
            w.writeheader()
        w.writerow(entry)


def _index_rows():
    if not os.path.exists(INDEX):
        return []
    with open(INDEX, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def _write_index(rows):
    with open(INDEX, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=INDEX_FIELDS)
        w.writeheader()
        w.writerows(rows)


def _report(run, game, label, note, parts, comparison):
    lines = [f"# {label} ({game})", "", f"Run `{run}`, checked {time.strftime('%Y-%m-%d %H:%M')} by BeG0nE jitter "
             f"tool {TOOL_VERSION}. Every claim below is `[measured]`: one recording, one scene.", ""]
    if note:
        lines += [f"**Note:** {note}", ""]
    for title, info, _rows, s in parts:
        lines += [f"## {title}", "", s["verdict"], "",
                  f"- quality: **{s['quality']}**{' (' + s['quality_reason'] + ')' if s['quality_reason'] else ''}",
                  f"- {s['seconds']} s, {s['size']}, {s['new_pictures_per_s']} new pictures a second",
                  f"- moving {s['seconds_travelling']} s: smooth {s['seconds_smooth']}, jitter {s['seconds_jitter']}, "
                  f"shake {s['seconds_shake']}; still {s['seconds_still']} s",
                  f"- freezes {len(s['freezes'])}, hitches {s['hitches']}"]
        if s["jitter_times_s"]:
            lines.append(f"- jitter at (s): {', '.join(str(int(t)) for t in s['jitter_times_s'])}")
        if s["shake_times_s"]:
            lines.append(f"- shake at (s): {', '.join(str(int(t)) for t in s['shake_times_s'])}")
        lines.append("")
    if comparison:
        lines += ["## The two views together", "", comparison, ""]
    lines += ["![chart](chart.png)", ""]
    return "\n".join(lines)


def _wait_settled(path, timeout_s=120):
    """Wait until a file has stopped growing (the recorder may still be closing it)."""
    last, t0 = -1, time.time()
    while time.time() - t0 < timeout_s:
        try:
            size = os.path.getsize(path)
        except OSError:
            size = -1
        if size > 0 and size == last:
            return
        last = size
        time.sleep(2)


def cmd_scan(path, game=None, headset=None, note="", wait_headset=False):
    if not os.path.exists(path):
        sys.exit(f"no such recording: {path}")
    _wait_settled(path)
    if wait_headset and headset is None:
        for _ in range(30):                      # the second file is closed a moment after the first
            headset = _find_headset_twin(path)
            if headset:
                _wait_settled(headset); break
            time.sleep(2)
    label, date, tm, is_second = _parse_name(path)
    if is_second:
        sys.exit("that is the headset-view file; scan the game recording beside it and both are checked")
    game = game or os.path.basename(os.path.dirname(os.path.abspath(path))) or "unknown"
    if headset is None:
        headset = _find_headset_twin(path)
    elif headset == "none":
        headset = None
    parts = [("game window",) + _check(path, "game window")]
    if headset:
        parts.append(("headset view",) + _check(headset, "headset view"))
    comparison = _compare(parts[0][3], parts[1][3]) if headset else ""
    run, folder = _run_folder(game, label, date, tm)
    os.makedirs(folder, exist_ok=True)
    summary = {"tool": TOOL_VERSION, "run": run, "game": game, "label": label, "note": note,
               "checked": time.strftime("%Y-%m-%d %H:%M"), "comparison": comparison,
               "views": {t: dict(s, file=os.path.basename(p)) for (t, _i, _r, s), p in zip(parts, [path, headset])}}
    with open(os.path.join(folder, "summary.json"), "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=1)
    with open(os.path.join(folder, "report.md"), "w", encoding="utf-8") as f:
        f.write(_report(run, game, label, note, parts, comparison))
    jitter_chart.draw(os.path.join(folder, "chart.png"), parts)
    rows = [r for r in _index_rows() if r["run"] != run]          # a re-scan replaces its old lines
    for title, _i, _r, s in parts:
        rows.append({"run": run, "game": game, "date": date, "time": tm, "label": label, "view": title,
                     "seconds": s["seconds"], "pictures_per_s": s["new_pictures_per_s"],
                     "travelling_s": s["seconds_travelling"], "jitter_s": s["seconds_jitter"],
                     "shake_s": s["seconds_shake"], "freezes": len(s["freezes"]), "hitches": s["hitches"],
                     "quality": s["quality"], "verdict": s["verdict"], "tool": TOOL_VERSION})
    os.makedirs(DATA, exist_ok=True)
    _write_index(rows)
    _say("")
    for title, _i, _r, s in parts:
        _say(f"{title.upper()}: {s['verdict']}")
    if comparison:
        _say(f"TOGETHER: {comparison}")
    _say(f"saved: camera-jitter/data/{game}/{run}/")
    return folder


def cmd_follow(root, once=False):
    state = {}
    try:
        with open(FOLLOW_STATE, encoding="utf-8") as f:
            state = json.load(f)
    except (OSError, ValueError):
        pass
    _say(f"following {root} (Ctrl+C to stop)")
    while True:
        for game in sorted(os.listdir(root)) if os.path.isdir(root) else []:
            folder = os.path.join(root, game)
            if not os.path.isdir(folder):
                continue
            for f in sorted(os.listdir(folder)):
                p = os.path.join(folder, f)
                m = NAME_RE.match(f)
                if not m or m["second"] or p in state:
                    continue
                if time.time() - os.path.getmtime(p) < SETTLE_S:
                    continue          # still being written
                try:
                    cmd_scan(p, game)
                    state[p] = "done"
                except Exception as e:  # one bad file must not stop the ride-along
                    state[p] = f"failed: {e}"
                    _say(f"could not check {f}: {e}")
                with open(FOLLOW_STATE, "w", encoding="utf-8") as fh:
                    json.dump(state, fh, indent=1)
        if once:
            return
        time.sleep(FOLLOW_POLL_S)


def cmd_runs(game=None):
    rows = [r for r in _index_rows() if not game or r["game"] == game]
    rows.sort(key=lambda r: (r["date"], r["time"]), reverse=True)
    if not rows:
        _say("no runs yet"); return
    for r in rows:
        _say(f"{r['date']} {r['time'][:5].replace('-', ':')}  {r['game']:26s} {r['label'][:22]:22s} {r['view']:12s} "
             f"{r['quality']:11s} {r['verdict'][:110]}")


def cmd_discard(run, why):
    rows = _index_rows()
    hit = [r for r in rows if r["run"] == run]
    if not hit:
        sys.exit(f"no run called {run}; `runs` lists them")
    for r in hit:
        r["quality"], r["verdict"] = "discarded", f"Discarded: {why}"
        p = os.path.join(DATA, r["game"], run, "summary.json")
        try:
            with open(p, encoding="utf-8") as f:
                s = json.load(f)
            s["discarded"] = why
            with open(p, "w", encoding="utf-8") as f:
                json.dump(s, f, indent=1)
        except OSError:
            pass
    _write_index(rows)
    _say(f"marked {run} as discarded: {why}")


def cmd_headset_window():
    """Lists windows that look like a headset view, and prints the obs-rec.py --also value for the best one."""
    import ctypes
    import ctypes.wintypes as W
    u, k = ctypes.windll.user32, ctypes.windll.kernel32
    found = []

    @ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
    def cb(h, _):
        if not u.IsWindowVisible(h):
            return True
        t = ctypes.create_unicode_buffer(256); c = ctypes.create_unicode_buffer(256)
        u.GetWindowTextW(h, t, 256); u.GetClassNameW(h, c, 256)
        if any(s.lower() in t.value.lower() for s in HEADSET_TITLES):
            pid = W.DWORD(); u.GetWindowThreadProcessId(h, ctypes.byref(pid))
            hp = k.OpenProcess(0x1000, False, pid.value)
            exe = ""
            if hp:
                buf = ctypes.create_unicode_buffer(520); n = W.DWORD(520)
                if k.QueryFullProcessImageNameW(hp, 0, buf, ctypes.byref(n)):
                    exe = os.path.basename(buf.value)
                k.CloseHandle(hp)
            found.append((t.value, c.value, exe))
        return True
    u.EnumWindows(cb, 0)
    if not found:
        _say("no headset-view window found. SteamVR: open the SteamVR menu -> Display VR View. "
             "The OpenXR simulator shows its preview by itself once the game has started VR.")
        return 1
    for title, cls, exe in found:
        _say(f"  {title!r}  class {cls!r}  in {exe}")
    title, cls, exe = found[0]
    _say(f"--also {exe}:{cls}")
    return 0


def main():
    a = sys.argv[1:]
    opt = lambda key, default=None: a[a.index(key) + 1] if key in a and a.index(key) + 1 < len(a) else default
    if not a:
        sys.exit(__doc__)
    if a[0] == "scan" and len(a) >= 2:
        cmd_scan(a[1], opt("--game"), opt("--headset"), opt("--note", ""), "--wait-headset" in a)
    elif a[0] == "follow":
        cmd_follow(opt("--root", REC_ROOT), "--once" in a)
    elif a[0] == "runs":
        cmd_runs(opt("--game"))
    elif a[0] == "discard" and len(a) >= 3:
        cmd_discard(a[1], a[2])
    elif a[0] == "headset-window":
        sys.exit(cmd_headset_window())
    elif a[0] == "selftest":
        import runpy
        sys.argv = [os.path.join(HERE, "tests", "test_synthetic.py")]
        runpy.run_path(sys.argv[0], run_name="__main__")
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
