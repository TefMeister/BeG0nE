"""test_eye_diff_read.py -- made-up eye pairs with known answers, written as real Flickers dumps and read back.

    python -I test_eye_diff_read.py

A far textured background (the same in both eyes), a near textured box drawn with the normal stereo shift (must NOT
count), and, per case, one change: a plant in one eye only, one eye darker, a lit patch, one eye blurred or jagged,
the eyes out of line, or a canted right eye (turned outwards, its view turned back by the recorded direction).
"""
import math
import os
import shutil
import sys
import tempfile

import cv2
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import eye_diff_read as edr  # noqa: E402

W, H = 640, 576
FOV = math.radians(45)
NEAR_SHIFT = 22           # the near box's stereo shift between the eyes, pixels
fails = []


def texture(h, w, seed, cell):
    rng = np.random.RandomState(seed)
    small = rng.randint(0, 255, (h // cell + 2, w // cell + 2, 3)).astype(np.uint8)
    return cv2.GaussianBlur(cv2.resize(small, (w + 2 * cell, h + 2 * cell), interpolation=cv2.INTER_CUBIC), (0, 0), 1.0)[:h, :w]


def leaves(h, w, seed):
    img = np.zeros((h, w, 3), np.uint8)
    rng = np.random.RandomState(seed)
    for _ in range(60):
        c = (int(rng.randint(20, 80)), int(rng.randint(120, 230)), int(rng.randint(20, 90)))
        p = (int(rng.randint(0, w)), int(rng.randint(0, h)))
        cv2.ellipse(img, p, (int(rng.randint(4, 12)), int(rng.randint(2, 5))), int(rng.randint(0, 180)), 0, 360, c, -1)
    return img


def scene(case, rng_seed=1):
    back = texture(H, W + 200, 7, 9)
    left = back[:, 100:100 + W].copy()
    right = left.copy()
    box = texture(160, 140, 11, 5)
    lx, y = 300, 260
    left[y:y + 160, lx:lx + 140] = box
    right[y:y + 160, lx - NEAR_SHIFT:lx - NEAR_SHIFT + 140] = box
    if case in ("plant-left", "plant-right"):
        plant = leaves(110, 120, 3)
        tgt = left if case == "plant-left" else right
        region = tgt[60:170, 70:190]
        m = plant.any(axis=2)
        region[m] = plant[m]
    if case == "right-darker":
        right = (right.astype(np.float32) * 0.85).astype(np.uint8)
    if case == "patch-lit":
        right[400:520, 60:200] = np.clip(right[400:520, 60:200].astype(np.int16) + 45, 0, 255).astype(np.uint8)
    if case == "right-blurred":
        right = cv2.GaussianBlur(right, (0, 0), 1.6)
    if case == "left-jagged":
        small = cv2.resize(left, (W // 3, H // 3), interpolation=cv2.INTER_AREA)
        left = cv2.resize(small, (W, H), interpolation=cv2.INTER_NEAREST)
    if case == "out-of-line":
        right = np.roll(right, 6, axis=0)
    return left, right


def record(eye, cant_deg):
    yaw = -math.radians(cant_deg) if eye == 1 else 0.0       # turning right = negative yaw about +Y (OpenXR)
    fovv = math.atan(math.tan(FOV) * H / W)
    return {"px": -0.032 if eye == 0 else 0.032, "py": 1.6, "pz": 0.0, "qx": 0.0, "qy": math.sin(yaw / 2), "qz": 0.0,
            "qw": math.cos(yaw / 2), "fovL": -FOV, "fovR": FOV, "fovU": fovv, "fovD": -fovv}


def write_dump(folder, case, frames=3, cant_deg=0.0):
    os.makedirs(folder, exist_ok=True)
    lines = ["# reason test", " ".join(edr.HEADER)]
    for k in range(frames):
        left, right = scene(case)
        recs = [record(0, cant_deg), record(1, cant_deg)]
        if cant_deg:
            # the right eye turned outwards: its picture is the left-view picture seen through the turned eye
            hom, _, _ = edr.far_view(recs[0], recs[1], (W, H), (W, H))
            right = cv2.warpPerspective(right, np.linalg.inv(hom), (W, H), flags=cv2.INTER_LINEAR, borderMode=cv2.BORDER_REFLECT)
        for eye, img in ((0, left), (1, right)):
            cv2.imwrite(os.path.join(folder, "frame-%d-%02d.ppm" % (eye, k)), cv2.cvtColor(img, cv2.COLOR_RGB2BGR))
            r = recs[eye]
            lines.append("%d %d %.4f %.4f %.4f %.6f %.6f %.6f %.6f %.4f %.4f %.4f %.4f 0 0 0 %d %d 0 0" % (
                100 + k, eye, r["px"], r["py"], r["pz"], r["qx"], r["qy"], r["qz"], r["qw"], r["fovL"], r["fovR"], r["fovU"], r["fovD"], W, H))
    with open(os.path.join(folder, "frames.txt"), "w") as fh:
        fh.write("\n".join(lines) + "\n")


def direction_check():
    """Independent of the reader's own maths: the right eye turned 8 degrees to its right looks at something that the
    left eye sees right of its centre, by f * tan(8 degrees)."""
    hom, gap, turn = edr.far_view(record(0, 8.0), record(1, 8.0), (W, H), (W, H))
    p = hom @ np.array([W / 2.0, H / 2.0, 1.0])
    u, v = p[0] / p[2], p[1] / p[2]
    want = W / 2.0 + (W / (2 * math.tan(FOV))) * math.tan(math.radians(8.0))
    return abs(u - want) < 0.5 and abs(v - H / 2.0) < 0.5 and abs(turn - 8.0) < 0.01 and abs(gap[0] - 0.064) < 1e-6, (u, v, want, turn, gap)


QUEST_FOV = {0: (-0.9425, 0.6981, 0.7676, -0.9472), 1: (-0.6981, 0.9425, 0.7676, -0.9472)}   # the simulator's eyes


def traced(eye, plant_eye, w=512, h=560):
    """One eye's picture traced ray by ray, with lopsided Quest-style views: a far textured sky (infinity), a textured
    board 1.5 m ahead and, in `plant_eye` only, a bush-sized leafy patch 3 m ahead (1.2 x 1 m). Returns the picture and its frames.txt record."""
    fl, fr, fu, fd = QUEST_FOV[eye]
    xs = np.tan(fl) + (np.arange(w) + 0.5) / w * (np.tan(fr) - np.tan(fl))
    ys = np.tan(fu) - (np.arange(h) + 0.5) / h * (np.tan(fu) - np.tan(fd))
    tx, ty = np.meshgrid(xs, ys)
    ex = -0.032 if eye == 0 else 0.032
    sky = texture(400, 800, 7, 9)
    u = np.clip(((np.arctan(tx) + 1.2) / 2.4 * 799).astype(int), 0, 799)
    v = np.clip(((np.arctan(ty) + 1.0) / 2.0 * 399).astype(int), 0, 399)
    img = sky[399 - v, u].copy()

    def plane(depth, x0, x1, y0, y1, tex, mask_fn=None):
        px, py = ex + tx * depth, ty * depth                       # where each ray meets the plane, metres
        inside = (px >= x0) & (px < x1) & (py >= y0) & (py < y1)
        th, tw = tex.shape[:2]
        iu = np.clip(((px - x0) / (x1 - x0) * tw).astype(int), 0, tw - 1)
        iv = np.clip(((y1 - py) / (y1 - y0) * th).astype(int), 0, th - 1)
        pix = tex[iv, iu]
        if mask_fn is not None:
            inside &= mask_fn(iv, iu)
        img[inside] = pix[inside]
    plane(1.5, 0.05, 0.45, -0.35, 0.15, texture(200, 160, 11, 5))
    if eye == plant_eye:
        lv = leaves(110, 120, 3)
        plane(3.0, -1.6, -0.4, 0.2, 1.2, lv, lambda iv, iu: lv[iv, iu].any(axis=-1))
    rec = {"px": ex, "py": 1.7, "pz": 0.0, "qx": 0.0, "qy": 0.0, "qz": 0.0, "qw": 1.0, "fovL": fl, "fovR": fr, "fovU": fu, "fovD": fd}
    return img, rec


def write_traced(folder, plant_eye):
    os.makedirs(folder, exist_ok=True)
    lines = ["# reason test", " ".join(edr.HEADER)]
    for eye in (0, 1):
        img, r = traced(eye, plant_eye)
        cv2.imwrite(os.path.join(folder, "frame-%d-00.ppm" % eye), cv2.cvtColor(img, cv2.COLOR_RGB2BGR))
        lines.append("500 %d %.4f %.4f %.4f %.6f %.6f %.6f %.6f %.4f %.4f %.4f %.4f 0 0 0 %d %d 0 0" % (
            eye, r["px"], r["py"], r["pz"], r["qx"], r["qy"], r["qz"], r["qw"], r["fovL"], r["fovR"], r["fovU"], r["fovD"], img.shape[1], img.shape[0]))
    with open(os.path.join(folder, "frames.txt"), "w") as fh:
        fh.write("\n".join(lines) + "\n")
    rep = edr.read_dump(folder, 12)
    return rep, rep[0], "\n".join(rep)


def check(name, cond, report):
    if cond:
        print("  ok   " + name)
    else:
        print("  FAIL " + name)
        fails.append(name)
        print("\n".join("        " + l for l in report))


def run(case, tmp, **kw):
    folder = os.path.join(tmp, case)
    write_dump(folder, case, **kw)
    rep = edr.read_dump(folder, 12)
    return rep, rep[0], "\n".join(rep)


def main():
    tmp = tempfile.mkdtemp(prefix="eyediff-test-")
    try:
        ok, nums = direction_check()
        check("the right eye's recorded turn moves its view the right way and the right distance", ok, [str(nums)])
        rep, head, text = run("same", tmp)
        check("same eyes (with a near box in normal 3D): EYES MATCH", head == "VERDICT: EYES MATCH", rep)
        for side, word in (("plant-left", "LEFT"), ("plant-right", "RIGHT")):
            rep, head, text = run(side, tmp)
            check("a plant in the %s eye only is found, in that eye, top left" % word,
                  ("only in the %s eye: top, left side" % word) in text and "one eye only" in head, rep)
            check("  ... and nothing is said about the other eye or the light", ("only in the %s eye" % ("RIGHT" if word == "LEFT" else "LEFT")) not in text
                  and "darker" not in head, rep)
        rep, head, text = run("right-darker", tmp)
        check("right eye 15 %% darker: said, and nothing in one eye only", "RIGHT eye 15% darker" in head and "one eye only" not in head, rep)
        rep, head, text = run("patch-lit", tmp)
        check("a patch lit only in the right eye: a lit patch, bottom left", "lit differently" in head and "bottom, left side" in text, rep)
        rep, head, text = run("right-blurred", tmp)
        check("right eye blurred: the left one is sharper", "LEFT eye sharper" in head, rep)
        rep, head, text = run("left-jagged", tmp)
        check("left eye blocky: the left one is more jagged", "LEFT eye more jagged" in head, rep)
        rep, head, text = run("out-of-line", tmp)
        check("eyes 6 px out of line vertically: said", "out of line vertically" in head, rep)
        rep, head, text = run("same", os.path.join(tmp, "canted"), cant_deg=8.0)
        check("right eye canted 8 degrees outwards, turned back by its recorded direction: EYES MATCH",
              head == "VERDICT: EYES MATCH" or head.startswith("VERDICT: EYES DIFFER: eyes point"), rep)
        rep, head, text = run("plant-left", os.path.join(tmp, "canted"), cant_deg=8.0)
        check("  ... and the plant in the left eye is still found", "only in the LEFT eye" in text, rep)
        rep, head, text = write_traced(os.path.join(tmp, "traced-none"), None)
        check("traced 3D, Quest-style lopsided views, a board 1.5 m away: EYES MATCH", head == "VERDICT: EYES MATCH", rep)
        for pe, word in ((0, "LEFT"), (1, "RIGHT")):
            rep, head, text = write_traced(os.path.join(tmp, "traced-%d" % pe), pe)
            check("traced 3D: a bush 3 m away in the %s eye only is found in that eye, at the top, and nothing else" % word,
                  ("only in the %s eye: top," % word) in text and head == "VERDICT: EYES DIFFER: 1 place(s) with something in one eye only (%s)" % word, rep)
        print("sheet and heat map written:", os.path.exists(os.path.join(tmp, "plant-left", "eyediff-sheet.jpg")),
              os.path.exists(os.path.join(tmp, "plant-left", "eyediff-heat.png")))
    finally:
        if "--keep" in sys.argv:
            print("kept:", tmp)
        else:
            shutil.rmtree(tmp, ignore_errors=True)
    print("RESULT:", "PASS" if not fails else "FAIL (%d)" % len(fails))
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
