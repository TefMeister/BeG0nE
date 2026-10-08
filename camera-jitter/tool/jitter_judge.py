"""jitter_judge.py -- turn the per-frame numbers from jitter_measure.py into plain verdicts.

Vocabulary (Tefa's, claude-memory PREFERENCES.md "what jittery means"):
  JITTER  stepped, "a quick teleport", seen under a slow smooth movement  -> the camera renewed less often than the
          picture: new pictures arrive, but some of them did not move while the ones around them did
  SHAKE   smooth but wrong, wobbling                                       -> the view moves back against its own
          direction of travel, or wobbles while the head is meant to be still
  HITCH   no new picture for a moment                                      -> frame rate, not a camera fault
  FREEZE  no new picture for a second or more                              -> the game stopped, or a fully still
          screen (menu, loading); a second recording (the headset view) tells which
Motion is measured in percent of the picture's width, so a 1280-wide game window and a 1728-wide headset preview
read on the same scale.
"""
import math
import numpy as np

# --- settings: every number with a name ---
WINDOW_S = 1.0               # verdicts are made per one-second piece of the recording
MOVING_PCT_S = 3.0           # the view counts as travelling when it moves at least 3% of the width per second
MIN_MOVING_PICTURES = 5      # ... and at least this many pictures in the second actually moved. Fewer means one or
                             # two JUMPS in an otherwise still second (a nudge, a snap turn, a respawn), which is not
                             # a smooth movement and says nothing about jitter (Hard Reset recording, 2026-10-08)
HELD_SHARE_OF_MEDIAN = 0.25  # a new picture that moved under 25% of the second's average step counts as HELD
JITTER_HELD_SHARE = 0.20     # a travelling second with 20%+ held pictures is JITTER
JITTER_UNEVEN = 0.60         # ... or with step sizes this uneven (spread / typical) and alternating big-small
REVERSE_SHARE = 0.15         # a travelling second with 15%+ pictures moving backwards is SHAKE
WOBBLE_PCT = 0.25            # a still second whose pictures wobble more than 0.25% of the width each is SHAKE
HITCH_GAP_FACTOR = 2.5       # a gap 2.5x the usual frame-to-frame gap (and at least 3 frames) is a HITCH
CUT_PCT = 15.0               # one picture moving more than 15% of the width is a CUT (scene change, menu, teleport,
                             # respawn): counted on its own and left out of the jitter verdict, or one cut in a still
                             # second reads as "moved once, held the rest" (seen 2026-10-08 on a Hard Reset recording)
MIN_RESPONSE_STEP = 0.15     # a step whose motion reading is this unsure is not trusted
FREEZE_S = 1.0               # no new picture for this long is a FREEZE (or a fully still screen)
LOW_PICTURE_RATE = 45.0      # below this many new pictures a second, uneven steps may be the capture, not the game
MIN_USABLE_S = 5.0           # a recording shorter than this is not judged
MIN_TRAVEL_S = 2.0           # fewer travelling seconds than this: no jitter verdict can be given (nothing moved)


def _per_picture(info, rows):
    """One entry per NEW picture: time, gap (frames), velocity vx, vy in % width per second, trusted?,
    and the step itself in % of the width."""
    fps, w = info["fps"], info["width"]
    out = []
    for r in rows:
        if not r["new"]:
            continue
        ok = not (math.isnan(r["dx"]) or math.isnan(r["dy"])) and r["resp"] >= MIN_RESPONSE_STEP
        k = 100.0 / w * fps / r["gap"]
        step = math.hypot(r["dx"], r["dy"]) * 100.0 / w if ok else 0.0
        if step > CUT_PCT:
            ok = False
            cut = True
        else:
            cut = False
        out.append((r["t"], r["gap"], r["dx"] * k if ok else 0.0, r["dy"] * k if ok else 0.0, ok, step, cut))
    return out


def _alternation(speeds):
    """Lag-one correlation of step sizes: strongly negative means big-small-big-small (a cadence)."""
    if len(speeds) < 6 or np.std(speeds) == 0:
        return 0.0
    a = np.asarray(speeds) - np.mean(speeds)
    return float(np.sum(a[1:] * a[:-1]) / np.sum(a * a))


def judge_window(pics):
    """Verdict for one second of new pictures. Returns a dict."""
    good = [p for p in pics if p[4]]
    if len(good) < 3:
        return {"kind": "too few pictures", "pictures": len(pics)}
    v = np.array([[p[2], p[3]] for p in good])
    mean = v.mean(axis=0)
    travel = float(np.hypot(*mean))
    speeds = np.hypot(v[:, 0], v[:, 1])
    res = {"pictures": len(pics), "travel_pct_s": round(travel, 2)}
    if travel >= MOVING_PCT_S:
        direction = mean / travel
        along = v @ direction
        typical = travel   # the step every picture would take if the camera were renewed evenly
        held = float(np.mean(np.abs(along) < HELD_SHARE_OF_MEDIAN * typical))
        backwards = float(np.mean(along < -HELD_SHARE_OF_MEDIAN * typical))
        uneven = float(np.std(along) / typical) if typical else 0.0
        alt = _alternation(along)
        res.update(held=round(held, 2), backwards=round(backwards, 2), uneven=round(uneven, 2), alternation=round(alt, 2))
        if (1.0 - held) * len(good) < MIN_MOVING_PICTURES:
            res["kind"] = "jump"
        elif backwards >= REVERSE_SHARE:
            res["kind"] = "SHAKE"
        elif held >= JITTER_HELD_SHARE or (uneven >= JITTER_UNEVEN and alt < -0.3):
            res["kind"] = "JITTER"
        else:
            res["kind"] = "smooth"
    else:
        per_pic = float(np.sqrt(np.mean([p[5] ** 2 for p in good])))
        res.update(wobble_pct=round(per_pic, 3))
        res["kind"] = "SHAKE" if per_pic >= WOBBLE_PCT else "still"
    return res


def judge(info, rows):
    """Whole-recording verdict. Returns a dict that becomes summary.json."""
    fps = info["fps"]
    pics = _per_picture(info, rows)
    gaps = np.array([p[1] for p in pics]) if pics else np.array([1])
    usual_gap = float(np.median(gaps))
    hitch_gap = max(3, HITCH_GAP_FACTOR * usual_gap)
    hitches = [round(p[0], 2) for p in pics if p[1] >= hitch_gap and p[1] < FREEZE_S * fps]
    freezes = [{"at_s": round(p[0] - p[1] / fps, 2), "for_s": round(p[1] / fps, 2)} for p in pics if p[1] >= FREEZE_S * fps]
    # a freeze still running when the recording ends
    if rows:
        tail = rows[-1]["gap"] if not rows[-1]["new"] else 0
        if tail >= FREEZE_S * fps:
            freezes.append({"at_s": round(rows[-1]["t"] - tail / fps, 2), "for_s": round(tail / fps, 2), "until_end": True})
    windows = []
    n_win = int(info["seconds"] // WINDOW_S)
    for k in range(n_win):
        lo, hi = k * WINDOW_S, (k + 1) * WINDOW_S
        w = judge_window([p for p in pics if lo <= p[0] < hi])
        w["at_s"] = lo
        windows.append(w)
    count = lambda kind: sum(1 for w in windows if w.get("kind") == kind)
    travelling = count("smooth") + count("JITTER") + sum(1 for w in windows if w.get("kind") == "SHAKE" and "held" in w)
    jumps = count("jump")
    live_s = info["seconds"] - sum(f["for_s"] for f in freezes)   # the picture rate while the game was drawing
    rate = len(pics) / live_s if live_s > 0 else 0.0
    s = {
        "seconds": info["seconds"], "video_fps": fps, "size": f'{info["width"]}x{info["height"]}',
        "new_pictures_per_s": round(rate, 1), "usual_gap_frames": usual_gap,
        "cuts": sum(1 for p in pics if p[6]), "hitches": len(hitches), "hitch_times_s": hitches[:50], "freezes": freezes,
        "seconds_travelling": travelling, "seconds_smooth": count("smooth"), "seconds_jitter": count("JITTER"),
        "seconds_shake": count("SHAKE"), "seconds_still": count("still"), "seconds_jump": jumps,
        "jitter_times_s": [w["at_s"] for w in windows if w.get("kind") == "JITTER"][:60],
        "shake_times_s": [w["at_s"] for w in windows if w.get("kind") == "SHAKE"][:60],
        "windows": windows,
    }
    s["quality"], s["quality_reason"] = _quality(s)
    s["verdict"] = _verdict(s)
    return s


def _quality(s):
    """Is this recording usable as evidence? 'faulty' runs are kept but never counted (Tefa, 2026-10-04)."""
    if s["seconds"] < MIN_USABLE_S:
        return "faulty", f"only {s['seconds']} s long"
    frozen = sum(f["for_s"] for f in s["freezes"])
    if frozen > 0.5 * s["seconds"]:
        return "faulty", f"no new picture for {frozen:.0f} of {s['seconds']:.0f} s: frozen, loading or a menu"
    if s["seconds_travelling"] < MIN_TRAVEL_S:
        return "no-movement", "the view hardly moved, so jitter cannot be judged (freezes and hitches still count)"
    return "good", ""


def _verdict(s):
    """One or two plain sentences, Tefa's words."""
    if s["quality"] == "faulty":
        return f"Not usable: {s['quality_reason']}."
    parts = []
    if s["quality"] == "no-movement":
        parts.append("The view barely moved, so this recording cannot show jitter.")
    else:
        t = s["seconds_travelling"]
        j, sh = s["seconds_jitter"], s["seconds_shake"]
        if j == 0 and sh == 0:
            parts.append(f"Smooth: in {t} seconds of movement the picture never stepped.")
        else:
            if j:
                parts.append(f"JITTER in {j} of {t} moving seconds: new pictures arrived but the view held still in "
                             f"some of them, so the camera is renewed less often than the picture (a rate fault).")
            if sh:
                parts.append(f"SHAKE in {sh} seconds: the view moved back against itself or wobbled.")
    if s.get("seconds_jump"):
        parts.append(f"{s['seconds_jump']} second(s) with a single jump of the view (a nudge or snap, not a movement).")
    if s["freezes"]:
        parts.append(f"{len(s['freezes'])} freeze(s) of a second or more.")
    if s["hitches"]:
        parts.append(f"{s['hitches']} short hitches (frame rate, not the camera).")
    if s["new_pictures_per_s"] < LOW_PICTURE_RATE:
        parts.append(f"Only {s['new_pictures_per_s']} new pictures a second reached the recording, so small "
                     f"unevenness may come from the capture, not the game.")
    return " ".join(parts)
