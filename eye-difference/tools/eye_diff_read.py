"""eye_diff_read.py -- the Eye Difference BeG0nE reader: what differs between the left and the right eye in VR.

    python -I eye_diff_read.py <flickers-dump-N folder | folder holding them> [--all] [--frames 12]

Reads a Flickers BeG0nE dump (both eyes' pictures of the same frames, with where each eye was and what it could see)
and writes into it eyediff-report.txt, eyediff-sheet.jpg (each eye side by side, differences marked, close-ups) and
eyediff-heat.png (where the differences sat, over all frames read), then prints the report.

What it looks for, frame by frame, between the two eyes of the SAME frame:
  1. LIGHT: one eye brighter or darker, or tinted, over the whole picture or one part of it.
  2. THINGS IN ONE EYE ONLY: a plant, an object or a patch that one eye shows and the other does not.
  3. PICTURE QUALITY: one eye more jagged (pixel-level edge noise) or blurrier than the other.
  4. LINE-UP: the eyes out of line vertically, or their positions / views odd.

How "one eye only" is told apart from normal 3D: the right eye's picture is first turned into the left eye's view
for things far away (from the two eyes' recorded directions and fields of view), so what is left between them is the
normal stereo shift, sideways only, bigger for nearer things. Every small square of each eye then looks for its
partner in the other eye along that sideways line. A square with no partner anywhere is a difference. Thin strips
beside a nearer object are normal (one eye sees round its edge) and are counted apart, not as faults.
Made 2026-11-02 for the RE4 plants seen in one eye only (Tefa), on top of the Flickers catcher's dumps.
"""
import argparse
import math
import os
import re
import sys

import cv2
import numpy as np

# ---- named numbers -----------------------------------------------------------------------------------------------
WORK_W = 480              # pictures are compared at this width (the sheet and the quality check use more)
QUALITY_W = 960           # width the picture-quality check runs at
BLOCK = 9                 # side of the squares that look for their partner (work pixels, odd)
MAX_SHIFT_SHARE = 0.12    # the biggest sideways stereo shift searched, as a share of the width (things ~25 cm away)
SLACK_PX = 3              # a little shift the wrong way allowed (recorded directions are not perfect)
TEX_FLOOR = 3.0           # grey levels: below this a square counts as flat
MATCHED = 0.15            # a square whose best partner differs by less than this (0 same, ~0.7 unrelated) is matched
COLOUR_DIFF = 40.0        # a pixel differs from its partner when some colour channel is this far off (0-255)
COLOUR_BINS = 4           # colours sorted into 4 x 4 x 4 boxes when comparing a place with its surroundings
OWN_MIN = 0.15            # an eye has the thing when that place differs this much from its own surroundings ...
OWN_RATIO = 1.3           # ... and this many times more than the other eye's place does ...
OWN_GAP = 0.1             # ... and by at least this much more
LOCAL_LIGHT = 15          # side of the area whose own brightness is taken out before shapes are compared
DENSE_SHARE = 0.2         # ... and it counts when at least this share of the pixels around it differ too
LIGHT_DIFF = 18.0         # grey levels a square's brightness may differ from its partner's before it counts
MIN_AREA_SHARE = 0.0015   # a difference smaller than this share of the picture is ignored
EDGE_MARGIN_PX = BLOCK + 2  # a strip this much wider than the stereo step beside it is not "round an edge" any more
#                            (the step itself, plus the square's spread on each side)
WHOLE_LIGHT = 0.03        # whole-picture brightness differing by more than 3 % is reported
PART_LIGHT = 0.06         # one ninth of the picture differing by more than 6 % is reported
TINT = 0.04               # one colour channel differing by more than 4 % against the others is a tint
QUALITY_RATIO = 1.15      # one eye with 15 % more fine edge energy than the other is reported
VERTICAL_PX = 1.0         # eyes out of line vertically by more than this (full-size pixels) is reported
TURN_DEG = 1.0            # eyes pointing apart by more than this is reported
GRID_MIN_PX = 50          # a ninth of the picture needs this many shared pixels to be judged for light
EDGE_BAND_PX = 8          # at least this wide a band each side of a find is read for the stereo step beside it
EDGE_MIN_STEP = 2         # a step smaller than this (work pixels) is no nearer thing, so no edge
LINEUP_MIN_PX = 1000      # the line-up check needs this many shared pixels
STEADY_SHARE = 0.5        # a frame counts toward "every frame" when its light differs by half the reported amount
JAGGED_COARSE = 0.9       # more fine edges without as many coarse ones means jagged, not simply sharper
EYE_GAP_MM = (40, 80)     # an eye gap outside this range is reported as odd
CROP_PX = 160             # side of each close-up on the sheet
QUALITY_TRIM = 7          # pixels trimmed off the shared part's edge before measuring quality
COARSE_SIGMAS = (1.5, 4.0)  # blurs whose difference measures coarser edges (sharpness, not jaggies)
OUTSIDE = -1000.0         # what a pixel turned in from outside the other eye's picture reads as
JPEG_QUALITY = 85
DEFAULT_FRAMES = 12       # frames compared per dump, spread over it
CROP_PAD = 4              # extra pixels round each close-up
HEAT_BLEND = (0.75, 0.5)  # heat map over the picture: heat weight, picture weight
MARK = {0: (255, 140, 0), 1: (0, 200, 255), None: (255, 255, 0), "edge": (128, 128, 128)}   # orange L, blue R, yellow ?
SHEET_ROWS = 4
CROPS = 6
CELL_W = 360
HEADER = "frame eye px py pz qx qy qz qw fovL fovR fovU fovD image rect_x rect_y rect_w rect_h same fake".split()
EYE = {0: "LEFT", 1: "RIGHT", None: "unclear"}
SHIFTABLE = np.ones((BLOCK, BLOCK), np.uint8)


# ---- reading a dump ----------------------------------------------------------------------------------------------
def read_rgb(path):
    img = cv2.imread(path, cv2.IMREAD_COLOR)
    if img is None:
        raise IOError("cannot read " + path)
    return cv2.cvtColor(img, cv2.COLOR_BGR2RGB)


def load_pairs(folder):
    """[(left record, right record)] of frames both eyes have, oldest first; a record holds the frames.txt numbers
    and 'path' (its picture). Pictures are read later, only for the frames used."""
    eyes = {0: [], 1: []}
    with open(os.path.join(folder, "frames.txt")) as f:
        rows = [ln.split() for ln in f if ln.strip() and not ln.startswith("#") and not ln.startswith("frame ")]
    for r in rows:
        rec = {k: float(v) for k, v in zip(HEADER, r)}
        rec["frame"], rec["eye"] = int(r[0]), int(r[1])
        rec["path"] = os.path.join(folder, "frame-%d-%02d.ppm" % (rec["eye"], len(eyes[rec["eye"]])))
        eyes[rec["eye"]].append(rec)
    right = {r["frame"]: r for r in eyes[1]}
    return [(l, right[l["frame"]]) for l in eyes[0] if l["frame"] in right]


# ---- geometry ----------------------------------------------------------------------------------------------------
def quat_mat(x, y, z, w):
    n = math.sqrt(x * x + y * y + z * z + w * w) or 1.0
    x, y, z, w = x / n, y / n, z / n, w / n
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def lens(rec, w, h):
    """3x3: an eye-space direction (tan x, tan y, 1) to that eye's picture pixel (OpenXR: -Z ahead, +Y up)."""
    tl, tr, tu, td = (math.tan(rec[k]) for k in ("fovL", "fovR", "fovU", "fovD"))
    if tr - tl < 1e-6 or tu - td < 1e-6:          # no field of view recorded: assume 90 degrees each way
        tl, tr, tu, td = -1.0, 1.0, 1.0, -1.0
    return np.array([[w / (tr - tl), 0, -tl * w / (tr - tl)], [0, -h / (tu - td), tu * h / (tu - td)], [0, 0, 1]])


def far_view(lrec, rrec, lsize, rsize):
    """(3x3 that moves a right-eye pixel to where something far away sits in the left eye, the right eye's
    position seen from the left eye, the angle between the two eyes' directions in degrees)."""
    rl = quat_mat(*(lrec[k] for k in ("qx", "qy", "qz", "qw")))
    rr = quat_mat(*(rrec[k] for k in ("qx", "qy", "qz", "qw")))
    rel = rl.T @ rr
    flip = np.diag([1.0, 1.0, -1.0])
    hom = lens(lrec, *lsize) @ flip @ rel @ flip @ np.linalg.inv(lens(rrec, *rsize))
    gap = rl.T @ np.array([rrec[k] - lrec[k] for k in ("px", "py", "pz")])
    turn = math.degrees(math.acos(max(-1.0, min(1.0, (np.trace(rel) - 1) / 2))))
    return hom / hom[2, 2], gap, turn


# ---- comparing ---------------------------------------------------------------------------------------------------
def grey(rgb):
    return cv2.cvtColor(rgb, cv2.COLOR_RGB2GRAY).astype(np.float32)


def box(a):
    return cv2.blur(a, (BLOCK, BLOCK), borderType=cv2.BORDER_REFLECT)


def shift_x(a, d, fill):
    """a moved d pixels to the right (out[u] = a[u - d]); what comes in from outside is `fill`."""
    out = np.full_like(a, fill)
    if d > 0:
        out[:, d:] = a[:, :-d]
    elif d < 0:
        out[:, :d] = a[:, -d:]
    else:
        out[:] = a
    return out


def partners(a, b, ok_a, ok_b, shifts):
    """For each square of `a`, its best partner in `b` among the sideways shifts (b's content at u - d).
    Returns (no-partner score, shift used, light difference at it, a's own detail)."""
    ma, mb = box(a), box(b)
    za, zb = a - ma, b - mb
    det_a, det_b = box(np.abs(za)), box(np.abs(zb))
    best = np.full(a.shape, np.inf, np.float32)
    best_d = np.zeros(a.shape, np.float32)
    best_light = np.zeros(a.shape, np.float32)
    for d in shifts:
        sz, sm, sok = shift_x(zb, d, 0.0), shift_x(mb, d, 0.0), shift_x(ok_b.astype(np.uint8), d, 0) > 0
        # 0 = the same detail; about 0.7 = two unrelated textures; near 1 = detail against a flat area
        cost = box(np.abs(za - sz)) / (det_a + shift_x(det_b, d, 0.0) + 2 * TEX_FLOOR)
        cost[~sok] = np.inf
        # each pixel keeps the best square that holds it: a square half on a near thing and half on the background
        # behind it has no single partner, but a square beside it, wholly on one of them, does
        cost = cv2.erode(cost, SHIFTABLE, borderType=cv2.BORDER_REPLICATE)
        lit = cv2.erode(np.abs(ma - sm), SHIFTABLE, borderType=cv2.BORDER_REPLICATE)
        take = cost < best
        best[take], best_d[take] = cost[take], d
        best_light[take] = lit[take]
    score = best
    score[~ok_a | ~np.isfinite(best)] = 0.0
    best_light[~ok_a | ~np.isfinite(best)] = 0.0
    return score, best_d, best_light, det_a


def compare_frame(lrgb, rrgb, lrec, rrec):
    """Everything found in one frame pair, at work size, plus the pictures used for the sheet."""
    lh, lw = lrgb.shape[:2]
    rh, rw = rrgb.shape[:2]
    hom, gap, turn = far_view(lrec, rrec, (lw, lh), (rw, rh))
    r_in_l = cv2.warpPerspective(rrgb, hom, (lw, lh), flags=cv2.INTER_LINEAR)
    cover = cv2.warpPerspective(np.full((rh, rw), 255, np.uint8), hom, (lw, lh), flags=cv2.INTER_NEAREST) > 0
    scale = WORK_W / float(lw) if lw > WORK_W else 1.0
    ws = (int(round(lw * scale)), int(round(lh * scale)))
    L = cv2.resize(lrgb, ws, interpolation=cv2.INTER_AREA)
    R = cv2.resize(r_in_l, ws, interpolation=cv2.INTER_AREA)
    ok = cv2.erode(cv2.resize(cover.astype(np.uint8), ws, interpolation=cv2.INTER_NEAREST), np.ones((3, 3), np.uint8)) > 0
    gl, gr = grey(L), grey(R)

    out = {"L": L, "R": R, "ok": ok, "gap": gap, "turn": turn, "size": (lw, lh)}
    out["light"] = light(L, R, ok)
    gain = out["light"]["whole_l"] / max(out["light"]["whole_r"], 1e-3)
    gr_even = np.clip(gr * gain, 0, 255)                  # whole-picture brightness taken out before matching shapes
    sign = 1 if gap[0] >= 0 else -1                       # the right eye sits to the right: its near things move left,
    #                                                       so a left square at u finds its partner at u - d, d >= 0
    far = int(MAX_SHIFT_SHARE * ws[0])
    shifts = [sign * d for d in range(-SLACK_PX, far + 1)]
    s_l, d_l, light_l, det_l = partners(gl, gr_even, ok, ok, shifts)
    s_r, d_r, light_r, det_r = partners(gr_even, gl, ok, ok, [-d for d in shifts])
    ch_gain = [out["light"]["chan_l"][c] / max(out["light"]["chan_r"][c], 1e-3) for c in range(3)]
    R_even = np.clip(R.astype(np.float32) * np.array(ch_gain, np.float32), 0, 255)
    L_f = L.astype(np.float32)
    only_l = unmatched(L_f, R_even, d_l, ok)
    only_r = unmatched(R_even, L_f, d_r, ok)
    near_thing = cv2.dilate((only_l | only_r).astype(np.uint8), np.ones((BLOCK, BLOCK), np.uint8)) > 0
    lit = ((light_l > LIGHT_DIFF) | (light_r > LIGHT_DIFF)) & ~near_thing
    depth = np.where((s_l < MATCHED) & (det_l > TEX_FLOOR) & ~near_thing, np.abs(d_l), np.nan)
    out["regions"] = merge_same_place(regions(only_l, only_r, lit, depth, ws), L, R_even, ok)
    out["mask"] = (only_l | only_r | lit).astype(np.uint8)
    out["vertical"] = vertical(gl, gr_even, ok) / scale
    things = cv2.resize(cv2.dilate((only_l | only_r).astype(np.uint8), np.ones((BLOCK, BLOCK), np.uint8)), (lw, lh),
                        interpolation=cv2.INTER_NEAREST) > 0
    out["quality"] = quality(lrgb, r_in_l, cover & ~things)   # a thing in one eye only would count as extra edges
    return out


def unmatched(a, b, d, ok):
    """Pixels of `a` whose colour differs from their partner in `b` (b's pixel at u - d, d from the best square), where
    enough of the pixels around differ too: something that is in a and not in b."""
    h, w = d.shape
    xs, ys = np.meshgrid(np.arange(w, dtype=np.float32), np.arange(h, dtype=np.float32))
    mx = xs - d.astype(np.float32)
    at = cv2.remap(b, mx, ys, cv2.INTER_LINEAR, borderMode=cv2.BORDER_CONSTANT, borderValue=(OUTSIDE,) * 3)
    seen = (at[..., 0] > OUTSIDE / 2) & ok
    # shapes only: each side's local brightness taken out first, so a patch lit differently is not a "thing"
    flat_a = a - cv2.blur(a, (LOCAL_LIGHT, LOCAL_LIGHT), borderType=cv2.BORDER_REFLECT)
    flat_at = at - cv2.blur(np.where(seen[..., None], at, a), (LOCAL_LIGHT, LOCAL_LIGHT), borderType=cv2.BORDER_REFLECT)
    off = (np.abs(flat_a - flat_at).max(axis=2) > COLOUR_DIFF) & seen
    dense = box(off.astype(np.float32)) > DENSE_SHARE
    return dense & seen


def light(L, R, ok):
    """Brightness and tint of each eye over the part both see, whole and in a 3x3 grid."""
    res = {"whole_l": float(grey(L)[ok].mean()) if ok.any() else 0.0, "whole_r": float(grey(R)[ok].mean()) if ok.any() else 0.0}
    res["chan_l"] = [float(L[..., c][ok].mean()) if ok.any() else 0.0 for c in range(3)]
    res["chan_r"] = [float(R[..., c][ok].mean()) if ok.any() else 0.0 for c in range(3)]
    h, w = ok.shape
    grid = []
    for gy in range(3):
        for gx in range(3):
            sl = (slice(gy * h // 3, (gy + 1) * h // 3), slice(gx * w // 3, (gx + 1) * w // 3))
            m = ok[sl]
            if m.sum() < GRID_MIN_PX:
                grid.append(None)
                continue
            grid.append((float(grey(L[sl])[m].mean()), float(grey(R[sl])[m].mean())))
    res["grid"] = grid
    return res


def regions(only_l, only_r, lit, depth, ws):
    """The differences as separate places: kind, which eye, box (work pixels), and whether it is just round an edge."""
    found = []
    min_area = MIN_AREA_SHARE * ws[0] * ws[1]
    kernel = np.ones((3, 3), np.uint8)
    for kind, mask, eye in (("thing", only_l, 0), ("thing", only_r, 1), ("light", lit, None)):
        m = cv2.morphologyEx(mask.astype(np.uint8), cv2.MORPH_CLOSE, kernel)
        n, lab, stats, _ = cv2.connectedComponentsWithStats(m, 8)
        for i in range(1, n):
            x, y, w, h, area = (int(v) for v in stats[i])
            if area < min_area:
                continue
            reg = {"kind": kind, "eye": eye, "box": (x, y, w, h), "area": area / float(ws[0] * ws[1]), "edge": False}
            if kind == "thing":
                band = max(EDGE_BAND_PX, w)
                left = depth[y:y + h, max(0, x - band):x]
                right = depth[y:y + h, x + w:x + w + band]
                dl = np.nanmedian(left) if np.isfinite(left).any() else np.nan
                dr = np.nanmedian(right) if np.isfinite(right).any() else np.nan
                step = abs(dl - dr) if np.isfinite(dl) and np.isfinite(dr) else 0.0
                reg["step"] = float(step)
                reg["edge"] = step >= EDGE_MIN_STEP and w <= step + EDGE_MARGIN_PX
            found.append(reg)
    return found


def stands_out(img, box_, ok):
    """How much the box's colours differ from the ring around it in the SAME eye (0 alike, 2 nothing shared): the eye
    with the extra thing differs from its own surroundings there, the other eye just shows more of the background."""
    x, y, w, h = box_
    pad = max(w, h) // 2 + BLOCK
    H, W = ok.shape
    inner = np.zeros(ok.shape, bool)
    inner[y:y + h, x:x + w] = True
    ring = np.zeros(ok.shape, bool)
    ring[max(0, y - pad):min(H, y + h + pad), max(0, x - pad):min(W, x + w + pad)] = True
    ring &= ~inner

    def hist(mask):
        q = (np.clip(img[mask & ok], 0, 255) // (256 // COLOUR_BINS)).astype(np.int32)
        idx = (q[:, 0] * COLOUR_BINS + q[:, 1]) * COLOUR_BINS + q[:, 2]
        hh = np.bincount(idx, minlength=COLOUR_BINS ** 3).astype(np.float64)
        return hh / max(hh.sum(), 1.0)
    return float(np.abs(hist(inner) - hist(ring)).sum())


def merge_same_place(found, L, R, ok):
    """A thing in one eye covers something the other eye shows, so both eyes look unmatched in the same place. Such
    pairs become one find, and its eye is the one that looks unlike its own surroundings there (a plant's green on bare
    ground; the other eye shows only more ground); when neither stands out, the eye is left open."""
    things = [g for g in found if g["kind"] == "thing" and not g["edge"]]
    rest = [g for g in found if g not in things]

    def touch(p, q):
        (x, y, w, h), (ox, oy, ow, oh) = p["box"], q["box"]
        return not (ox > x + w or ox + ow < x or oy > y + h or oy + oh < y)
    groups = []
    for g in things:                                      # every piece joins each group it touches
        hit = [grp for grp in groups if any(touch(g, o) for o in grp)]
        for grp in hit:
            groups.remove(grp)
        groups.append(sum(hit, []) + [g])
    out = []
    for grp in groups:
        x0, y0 = min(g["box"][0] for g in grp), min(g["box"][1] for g in grp)
        x1, y1 = max(g["box"][0] + g["box"][2] for g in grp), max(g["box"][1] + g["box"][3] for g in grp)
        eyes = {g["eye"] for g in grp}
        m = dict(grp[0], box=(x0, y0, x1 - x0, y1 - y0), area=sum(g["area"] for g in grp), eye=eyes.pop() if len(eyes) == 1 else None)
        if m["eye"] is None:
            nl, nr = stands_out(L, m["box"], ok), stands_out(R, m["box"], ok)
            m["stands_out"] = (nl, nr)
            if max(nl, nr) >= OWN_MIN and max(nl, nr) >= OWN_RATIO * min(nl, nr) and abs(nl - nr) >= OWN_GAP:
                m["eye"] = 0 if nl > nr else 1
        out.append(m)
    return out + rest


def vertical(gl, gr, ok):
    """How far the eyes are out of line up/down (work pixels), from the part both see."""
    if ok.sum() < LINEUP_MIN_PX:
        return 0.0
    a, b = gl.copy(), gr.copy()
    a[~ok], b[~ok] = a[ok].mean(), b[ok].mean()
    win = cv2.createHanningWindow(a.shape[::-1], cv2.CV_32F)
    (dx, dy), _ = cv2.phaseCorrelate(a, b, win)
    return float(dy)


def quality(lrgb, r_in_l, cover):
    """Fine edge energy (pixel-level: jagged / aliased) and coarser edge energy (sharpness) of each eye."""
    h, w = lrgb.shape[:2]
    s = QUALITY_W / float(w) if w > QUALITY_W else 1.0
    size = (int(w * s), int(h * s))
    m = cv2.erode(cv2.resize(cover.astype(np.uint8), size, interpolation=cv2.INTER_NEAREST), np.ones((QUALITY_TRIM, QUALITY_TRIM), np.uint8)) > 0
    res = {}
    for name, img in (("l", lrgb), ("r", r_in_l)):
        g = grey(cv2.resize(img, size, interpolation=cv2.INTER_AREA))
        fine = np.abs(cv2.Laplacian(g, cv2.CV_32F, ksize=1))
        coarse = np.abs(cv2.GaussianBlur(g, (0, 0), COARSE_SIGMAS[0]) - cv2.GaussianBlur(g, (0, 0), COARSE_SIGMAS[1]))
        res[name] = (float(fine[m].mean()) if m.any() else 0.0, float(coarse[m].mean()) if m.any() else 0.0)
    return res


# ---- words -------------------------------------------------------------------------------------------------------
def where(box_, ws):
    x, y, w, h = box_
    cx, cy = (x + w / 2.0) / ws[0], (y + h / 2.0) / ws[1]
    col = "left side" if cx < 1 / 3.0 else ("right side" if cx > 2 / 3.0 else "middle")
    row = "top" if cy < 1 / 3.0 else ("bottom" if cy > 2 / 3.0 else "centre")
    return "%s, %s" % (row, col)


def pct(a, b):
    return 100.0 * (a - b) / b if b > 1e-6 else 0.0


def summarise(results, folder):
    n = len(results)
    lines = ["Eye Difference BeG0nE: %s" % folder, "frames compared: %d (left eye's view; the right eye turned into it)" % n, ""]
    verdict = []
    # 1. light
    wl = np.array([r["light"]["whole_l"] for r in results])
    wr = np.array([r["light"]["whole_r"] for r in results])
    diff = [pct(b, a) for a, b in zip(wl, wr)]
    mean_d = float(np.mean(diff)) if diff else 0.0
    lines.append("1. LIGHT")
    if abs(mean_d) > WHOLE_LIGHT * 100:
        darker = "RIGHT" if mean_d < 0 else "LEFT"
        steady = "every frame" if all(abs(d) > WHOLE_LIGHT * 100 * STEADY_SHARE and (d < 0) == (mean_d < 0) for d in diff) else "not every frame"
        lines.append("   the %s eye is %.0f %% darker overall (%s; per frame %s)" % (
            darker, abs(mean_d), steady, ", ".join("%+.0f" % d for d in diff[:DEFAULT_FRAMES])))
        verdict.append("%s eye %.0f%% darker" % (darker, abs(mean_d)))
    else:
        lines.append("   whole picture: the same (%+.1f %%)" % mean_d)
    for c, name in enumerate(("red", "green", "blue")):
        cl = np.mean([r["light"]["chan_l"][c] for r in results])
        cr = np.mean([r["light"]["chan_r"][c] for r in results])
        rel = pct(cr, cl) - mean_d
        if abs(rel) > TINT * 100:
            lines.append("   tint: the right eye has %.0f %% %s %s than the left, against the other colours" % (abs(rel), "more" if rel > 0 else "less", name))
            verdict.append("tint in %s" % name)
    names = ["top left", "top", "top right", "left", "centre", "right", "bottom left", "bottom", "bottom right"]
    for i, nm in enumerate(names):
        vals = [pct(g[1], g[0]) for g in (r["light"]["grid"][i] for r in results) if g]
        if vals and abs(np.mean(vals) - mean_d) > PART_LIGHT * 100:
            lines.append("   part: %s of the picture, the right eye %+.0f %% against the left (beyond the whole picture's %+.0f %%)" % (nm, np.mean(vals), mean_d))
            verdict.append("light differs at %s" % nm)
    # 2. things in one eye only
    lines += ["", "2. THINGS IN ONE EYE ONLY"]
    ws = results[0]["L"].shape[1::-1] if results else (1, 1)
    real = [(k, reg) for k, r in enumerate(results) for reg in r["regions"] if reg["kind"] == "thing" and not reg["edge"]]
    edge = sum(1 for r in results for reg in r["regions"] if reg["kind"] == "thing" and reg["edge"])
    lit = [(k, reg) for k, r in enumerate(results) for reg in r["regions"] if reg["kind"] == "light"]
    if real:
        by, span = {}, {}
        for k, reg in real:
            key = (reg["eye"], where(reg["box"], ws))
            by.setdefault(key, set()).add(k)
            x, y, w, h = reg["box"]
            a = span.get(key, (x, y, x + w, y + h))
            span[key] = (min(a[0], x), min(a[1], y), max(a[2], x + w), max(a[3], y + h))
        for (eye, place), frames in sorted(by.items(), key=lambda kv: (-len(kv[1]), str(kv[0]))):
            who = "only in the %s eye" % EYE[eye] if eye is not None else "differs between the eyes (which eye has it: unclear, see the sheet)"
            x0, y0, x1, y1 = span[(eye, place)]
            lines.append("   %s: %s, in %d of %d frames (across the left eye's picture %d-%d %%, down %d-%d %%)" % (
                who, place, len(frames), n, 100 * x0 // ws[0], 100 * x1 // ws[0], 100 * y0 // ws[1], 100 * y1 // ws[1]))
        eyes = sorted({EYE.get(e, "unclear") for (e, _) in by})
        verdict.append("%d place(s) with something in one eye only (%s)" % (len(by), " and ".join(eyes)))
    else:
        lines.append("   none found")
    if lit:
        places = sorted({where(reg["box"], ws) for _, reg in lit})
        lines.append("   patches lit differently in the two eyes (same shapes): %s" % "; ".join(places))
        verdict.append("%d patch(es) lit differently" % len(places))
    lines.append("   (normal 3D: %d thin strip(s) beside nearer things, seen round their edge by one eye, not counted)" % edge)
    lines.append("   (not compared: the outer band each eye sees alone)")
    # 3. picture quality
    lines += ["", "3. PICTURE QUALITY"]
    usable = [r["quality"] for r in results if min(r["quality"]["l"] + r["quality"]["r"]) > 1e-3]
    fine = [q["l"][0] / q["r"][0] for q in usable]
    coarse = [q["l"][1] / q["r"][1] for q in usable]
    fm, cm = float(np.median(fine)) if fine else 1.0, float(np.median(coarse)) if coarse else 1.0
    if not usable:
        lines.append("   not measured: no part both eyes see alike was left to measure it on")
    elif fm > QUALITY_RATIO or fm < 1 / QUALITY_RATIO:
        more, less = ("LEFT", "RIGHT") if fm > 1 else ("RIGHT", "LEFT")
        ratio = fm if fm > 1 else 1 / fm
        kind = "more jagged / pixelated (fine edge noise)" if (cm if fm > 1 else 1 / cm) < ratio * JAGGED_COARSE else "sharper overall"
        lines.append("   the %s eye has %.0f %% more pixel-level edge detail than the %s: %s" % (more, (ratio - 1) * 100, less, kind))
        verdict.append("%s eye %s" % (more, "more jagged" if kind.startswith("more") else "sharper"))
    else:
        lines.append("   the same (fine edges %.2f, coarse edges %.2f, left against right)" % (fm, cm))
    # 4. line-up
    lines += ["", "4. LINE-UP"]
    vert = float(np.median([r["vertical"] for r in results])) if results else 0.0
    gaps = [float(np.linalg.norm(r["gap"])) * 1000 for r in results]
    turns = [r["turn"] for r in results]
    lines.append("   eye gap %.1f mm, eyes turned apart %.2f degrees, up/down offset %.1f px" % (
        float(np.median(gaps)) if gaps else 0, float(np.median(turns)) if turns else 0, vert))
    if abs(vert) > VERTICAL_PX:
        verdict.append("eyes %.1f px out of line vertically" % vert)
    if turns and np.median(turns) > TURN_DEG:
        verdict.append("eyes point %.1f degrees apart" % float(np.median(turns)))
    if gaps and (np.median(gaps) < EYE_GAP_MM[0] or np.median(gaps) > EYE_GAP_MM[1]):
        verdict.append("eye gap %.0f mm looks odd" % float(np.median(gaps)))
    head = "VERDICT: " + ("EYES DIFFER: " + "; ".join(verdict) if verdict else "EYES MATCH")
    return [head, ""] + lines


# ---- pictures ----------------------------------------------------------------------------------------------------
def fit(img, w=CELL_W):
    h = int(round(img.shape[0] * w / float(img.shape[1])))
    return cv2.resize(img, (w, h), interpolation=cv2.INTER_AREA)


def marked(r):
    img = r["L"].copy()
    for reg in r["regions"]:
        x, y, w, h = reg["box"]
        colour = MARK["edge"] if reg["edge"] else MARK.get(reg["eye"], MARK[None])
        cv2.rectangle(img, (x, y), (x + w, y + h), colour, 1 if reg["edge"] else 2)
    return img


def label(img, text):
    out = img.copy()
    cv2.putText(out, text, (6, 18), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 0), 3, cv2.LINE_AA)
    cv2.putText(out, text, (6, 18), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1, cv2.LINE_AA)
    return out


def sheet(results, frames, path):
    def score(r):
        return sum(reg["area"] for reg in r["regions"] if not reg["edge"])
    order = sorted(range(len(results)), key=lambda k: -score(results[k]))[:SHEET_ROWS]
    rows = []
    for k in sorted(order):
        r = results[k]
        rows.append(np.hstack([label(fit(r["L"]), "LEFT  frame %d" % frames[k]), label(fit(r["R"]), "RIGHT (turned to left)"),
                               label(fit(marked(r)), "orange L only, blue R only, yellow ?/light")]))
    crops = []
    for k in order:
        r = results[k]
        for reg in r["regions"]:
            if reg["edge"] or len(crops) >= CROPS:
                continue
            x, y, w, h = reg["box"]
            pad = max(w, h) // 2 + CROP_PAD
            y0, y1, x0, x1 = max(0, y - pad), y + h + pad, max(0, x - pad), x + w + pad
            pair = [cv2.resize(img[y0:y1, x0:x1], (CROP_PX, CROP_PX), interpolation=cv2.INTER_NEAREST) for img in (r["L"], r["R"])]
            has = {0: " HAS IT", 1: ""} if reg["eye"] == 0 else {0: "", 1: " HAS IT"} if reg["eye"] == 1 else {0: " ?", 1: " ?"}
            if reg["kind"] == "light":
                has = {0: " light", 1: " light"}
            crops.append(np.hstack([label(pair[0], "LEFT" + has[0]), label(pair[1], "RIGHT" + has[1])]))
    if crops:
        strip = np.hstack(crops)
        width = rows[0].shape[1] if rows else strip.shape[1]
        if strip.shape[1] > width:
            strip = fit(strip, width)
        else:
            strip = np.hstack([strip, np.zeros((strip.shape[0], width - strip.shape[1], 3), np.uint8)])
        rows.append(strip)
    if rows:
        cv2.imwrite(path, cv2.cvtColor(np.vstack(rows), cv2.COLOR_RGB2BGR), [cv2.IMWRITE_JPEG_QUALITY, JPEG_QUALITY])


def heat(results, path):
    acc = np.zeros(results[0]["mask"].shape, np.float32)
    for r in results:
        acc += r["mask"]
    acc = (255 * acc / max(len(results), 1)).astype(np.uint8)
    base = grey(results[0]["L"]).astype(np.uint8) // 3
    out = cv2.applyColorMap(acc, cv2.COLORMAP_INFERNO)
    out = cv2.addWeighted(out, HEAT_BLEND[0], cv2.cvtColor(base, cv2.COLOR_GRAY2BGR), HEAT_BLEND[1], 0)
    cv2.imwrite(path, out)


# ---- running -----------------------------------------------------------------------------------------------------
def read_dump(folder, max_frames):
    pairs = load_pairs(folder)
    if not pairs:
        raise SystemExit("no frame both eyes have in " + folder)
    if len(pairs) > max_frames:
        step = len(pairs) / float(max_frames)
        pairs = [pairs[int(i * step)] for i in range(max_frames)]
    results = [compare_frame(read_rgb(l["path"]), read_rgb(r["path"]), l, r) for l, r in pairs]
    report = summarise(results, os.path.basename(os.path.normpath(folder)))
    with open(os.path.join(folder, "eyediff-report.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report) + "\n")
    sheet(results, [l["frame"] for l, _ in pairs], os.path.join(folder, "eyediff-sheet.jpg"))
    heat(results, os.path.join(folder, "eyediff-heat.png"))
    return report


def dumps_in(folder):
    if os.path.exists(os.path.join(folder, "frames.txt")):
        return [folder]
    found = []
    for name in os.listdir(folder):
        m = re.match(r"flickers-dump-(\d+)$", name)
        if m and os.path.exists(os.path.join(folder, name, "frames.txt")):
            found.append((int(m.group(1)), os.path.join(folder, name)))
    return [p for _, p in sorted(found)]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("folder")
    ap.add_argument("--all", action="store_true", help="every dump in the folder, not only the newest")
    ap.add_argument("--frames", type=int, default=DEFAULT_FRAMES, help="frames compared per dump (spread over it)")
    a = ap.parse_args(argv)
    dumps = dumps_in(a.folder)
    if not dumps:
        raise SystemExit("no Flickers dump (frames.txt) in " + a.folder)
    for d in (dumps if a.all else dumps[-1:]):
        print("\n".join(read_dump(d, a.frames)))
        print()


if __name__ == "__main__":
    main()
