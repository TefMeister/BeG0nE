"""flicker_read.py -- the Flickers BeG0nE reader: finds the odd frames in a catcher dump.

    python -I flicker_read.py <flickers-dump-N folder>

Writes report.txt, sheet.jpg (each odd frame between its neighbours) and heat-<eye>.png (where the odd changes sit)
into the dump folder, and prints the report.

An ODD frame is one whose picture differs from BOTH neighbours in a place where the two neighbours agree with each
other: the one-frame-out signature of a flicker. Head motion is taken out first (each neighbour is shifted onto the
frame by phase correlation), so a steady turn is not a flicker. Each odd frame's pose and field of view are compared
with its neighbours and with the other eye's: other-eye data on odd frames is the cause the Village scope had.
Method from the 2026-10-26 reading of Tefa's RE4 clip and Village's ring_view.py.
"""
import os
import sys

import cv2
import numpy as np

DIFF_LEVEL = 12          # a pixel "changed" when some colour channel moved more than this (0-255)
TILE_PX = 32             # odd changes are judged per square of this side
ODD_TILE_SHARE = 0.25    # a frame is odd when one square is at least this full of changes only that frame has
POSE_JUMP_DEG = 2.0      # an odd frame turned this far off its neighbours' mean counts as pose data gone wrong
MAX_SHEET_ROWS = 8
CLOSEST_SHOWN = 5        # the highest-scoring frames are always listed, odd or not, so the line can be set from real frames
HEADER = "frame eye px py pz qx qy qz qw fovL fovR fovU fovD image rect_x rect_y rect_w rect_h same fake".split()


def write_ppm(path, rgb):
    """Writes an HxWx3 uint8 RGB array as a binary PPM (the format the core dumps)."""
    h, w = rgb.shape[:2]
    with open(path, "wb") as f:
        f.write(b"P6\n%d %d\n255\n" % (w, h))
        f.write(np.ascontiguousarray(rgb, dtype=np.uint8).tobytes())


def read_rgb(path):
    img = cv2.imread(path, cv2.IMREAD_COLOR)
    if img is None:
        raise IOError("cannot read " + path)
    return cv2.cvtColor(img, cv2.COLOR_BGR2RGB)


def load(folder):
    """Returns {eye: [record dicts in order]}, each with 'img' (RGB array) and the frames.txt numbers."""
    eyes = {0: [], 1: []}
    with open(os.path.join(folder, "frames.txt")) as f:
        rows = [ln.split() for ln in f if ln.strip() and not ln.startswith("#") and not ln.startswith("frame ")]
    for r in rows:
        rec = {k: float(v) for k, v in zip(HEADER, r)}
        rec["frame"], rec["eye"], rec["fake"] = int(r[0]), int(r[1]), int(r[19])
        eyes[rec["eye"]].append(rec)
    for eye, recs in eyes.items():
        for k, rec in enumerate(recs):
            rec["index"] = k
            rec["img"] = read_rgb(os.path.join(folder, "frame-%d-%02d.ppm" % (eye, k)))
    return eyes


def shifted_onto(src, ref_grey, src_grey):
    """src moved so it lines up with ref (whole-picture shift found by phase correlation), and the shift's size."""
    (dx, dy), _ = cv2.phaseCorrelate(ref_grey, src_grey)
    m = np.float32([[1, 0, -dx], [0, 1, -dy]])
    h, w = src.shape[:2]
    return cv2.warpAffine(src, m, (w, h), flags=cv2.INTER_NEAREST, borderMode=cv2.BORDER_REPLICATE), max(abs(dx), abs(dy))


def changed(a, b):
    return np.abs(a.astype(np.int16) - b.astype(np.int16)).max(axis=2) > DIFF_LEVEL


def odd_mask(prev, cur, nxt):
    """Pixels where cur differs from both neighbours while the neighbours agree, and the margin to ignore."""
    grey = [cv2.cvtColor(x, cv2.COLOR_RGB2GRAY).astype(np.float32) for x in (prev, cur, nxt)]
    p, sp = shifted_onto(prev, grey[1], grey[0])
    n, sn = shifted_onto(nxt, grey[1], grey[2])
    mask = changed(cur, p) & changed(cur, n) & ~changed(p, n)
    margin = int(np.ceil(max(sp, sn))) + 2
    mask[:margin, :] = False
    mask[-margin:, :] = False
    mask[:, :margin] = False
    mask[:, -margin:] = False
    return mask


def best_tile_share(mask):
    share = cv2.boxFilter(mask.astype(np.float32), -1, (TILE_PX, TILE_PX), normalize=True, borderType=cv2.BORDER_CONSTANT)
    return float(share.max())


def quat(rec):
    q = np.array([rec["qx"], rec["qy"], rec["qz"], rec["qw"]])
    return q / (np.linalg.norm(q) or 1.0)


def angle_deg(q1, q2):
    return float(np.degrees(2 * np.arccos(min(1.0, abs(float(np.dot(q1, q2)))))))


def fov_of(rec):
    return np.array([rec["fovL"], rec["fovR"], rec["fovU"], rec["fovD"]])


def read_dump(folder):
    """The whole reading as a dict: odd frames, frames carrying the other eye's field of view, the fake check, verdict."""
    eyes = load(folder)
    med = {e: np.median([fov_of(r) for r in recs], axis=0) for e, recs in eyes.items() if recs}
    swapped = []
    for e, recs in eyes.items():
        o = 1 - e
        if o not in med:
            continue
        for r in recs:
            own, other = np.abs(fov_of(r) - med[e]).sum(), np.abs(fov_of(r) - med[o]).sum()
            r["fov_swapped"] = bool(own > 1e-3 and other < 0.5 * own)
            if r["fov_swapped"]:
                swapped.append((e, r["index"]))
    odd, heat, scores = [], {}, []
    fakes = matched = 0
    for e, recs in eyes.items():
        heat[e] = np.zeros(recs[0]["img"].shape[:2], np.float32) if recs else None
        for k in range(1, len(recs) - 1):
            m = odd_mask(recs[k - 1]["img"], recs[k]["img"], recs[k + 1]["img"])
            heat[e] += m
            share = best_tile_share(m)
            scores.append((share, recs[k]["frame"], e))
            fakes += recs[k]["fake"]
            if share < ODD_TILE_SHARE:
                continue
            mid = quat(recs[k - 1]) + quat(recs[k + 1])
            jump = angle_deg(quat(recs[k]), mid / (np.linalg.norm(mid) or 1.0))
            matched += recs[k]["fake"]
            odd.append({"eye": e, "index": k, "frame": recs[k]["frame"], "share": share, "pose_jump": jump,
                        "fov_swapped": recs[k].get("fov_swapped", False), "fake": recs[k]["fake"]})
    if not odd:
        verdict = "no odd frames"
    elif sum(1 for o in odd if o["fov_swapped"] or o["pose_jump"] > POSE_JUMP_DEG) * 2 >= len(odd):
        verdict = "other-eye data on odd frames"
    else:
        verdict = "picture-only odd frames"
    extra = sum(1 for o in odd if not o["fake"])
    closest = sorted(scores, key=lambda s: (-s[0], s[2], s[1]))[:CLOSEST_SHOWN]
    return {"eyes": eyes, "odd": odd, "swapped_frames": swapped, "heat": heat, "verdict": verdict,
            "fake_match": (fakes, matched, extra), "closest": closest}


def report_text(rep, folder):
    n = {e: len(r) for e, r in rep["eyes"].items()}
    lines = ["Flickers BeG0nE reader: %s" % folder, "pictures: eye 0 = %d, eye 1 = %d (first and last of each cannot be judged)" % (n[0], n[1])]
    for o in rep["odd"]:
        lines.append("ODD frame %d eye %d index %d share %.2f pose_jump %.2f fov_swapped %s fake %d"
                     % (o["frame"], o["eye"], o["index"], o["share"], o["pose_jump"], "yes" if o["fov_swapped"] else "no", o["fake"]))
    if rep["swapped_frames"]:
        lines.append("frames carrying the other eye's field of view: " + ", ".join("eye %d #%d" % s for s in rep["swapped_frames"]))
    for share, frame, eye in rep["closest"]:
        lines.append("closest: frame %d eye %d share %.2f (odd from %.2f)" % (frame, eye, share, ODD_TILE_SHARE))
    fakes, matched, extra = rep["fake_match"]
    if fakes:
        lines.append("fake flicker check: %d spoiled, %d found, %d odd frames that were not spoiled" % (fakes, matched, extra))
    lines.append("VERDICT: " + rep["verdict"])
    return "\n".join(lines) + "\n"


def write_pictures(rep, folder):
    rows = []
    for o in rep["odd"][:MAX_SHEET_ROWS]:
        recs = rep["eyes"][o["eye"]]
        k = o["index"]
        trio = [recs[k - 1]["img"], recs[k]["img"], recs[k + 1]["img"]]
        row = np.hstack([cv2.copyMakeBorder(x, 4, 4, 4, 4, cv2.BORDER_CONSTANT, value=(255, 255, 0) if i == 1 else (40, 40, 40))
                         for i, x in enumerate(trio)])
        cv2.putText(row, "eye %d frame %d" % (o["eye"], o["frame"]), (10, 24), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 0), 2)
        rows.append(row)
    if not rows:
        first = next((r for r in rep["eyes"].values() if r), [])
        rows = [np.hstack([x["img"] for x in first[:3]])] if first else [np.zeros((8, 8, 3), np.uint8)]
    sheet = np.vstack(rows)
    if sheet.shape[1] > 1800:
        f = 1800 / sheet.shape[1]
        sheet = cv2.resize(sheet, None, fx=f, fy=f, interpolation=cv2.INTER_AREA)
    cv2.imwrite(os.path.join(folder, "sheet.jpg"), cv2.cvtColor(sheet, cv2.COLOR_RGB2BGR), [cv2.IMWRITE_JPEG_QUALITY, 85])
    for e, h in rep["heat"].items():
        if h is None:
            continue
        scaled = (h / h.max() * 255).astype(np.uint8) if h.max() > 0 else h.astype(np.uint8)
        cv2.imwrite(os.path.join(folder, "heat-%d.png" % e), cv2.applyColorMap(scaled, cv2.COLORMAP_INFERNO))


def main(argv):
    if len(argv) != 1 or not os.path.isdir(argv[0]):
        print(__doc__)
        return 2
    folder = argv[0]
    rep = read_dump(folder)
    text = report_text(rep, folder)
    with open(os.path.join(folder, "report.txt"), "w") as f:
        f.write(text)
    write_pictures(rep, folder)
    print(text, end="")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
