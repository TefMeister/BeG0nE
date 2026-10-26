"""Tests for flicker_read.py on synthetic dumps: a picture panning sideways, with patches spoiled on known frames.

Run: python -I tools/test_flicker_read.py   (prints OK, exits 1 on the first failure)
"""
import os
import sys
import tempfile

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import flicker_read as fr  # noqa: E402

W, H = 320, 240
FOV = {0: (-0.94, 0.70, 0.77, -0.95), 1: (-0.70, 0.94, 0.77, -0.95)}


def scene(frame, eye):
    """A textured scene panning 3 px a frame (both eyes, 4 px apart), like a slow head turn."""
    rng = np.random.default_rng(7)
    world = (rng.random((H, W + 400)) * 120 + 40).astype(np.uint8)
    world = np.repeat(np.repeat(world[::8, ::8], 8, 0), 8, 1)[:H, :W + 400]   # blocky texture, 8 px cells
    x0 = 3 * frame + 4 * eye
    g = world[:, x0:x0 + W]
    return np.dstack([g, g * 0.9, g * 0.7]).astype(np.uint8)


def make_dump(folder, n=40, spoil=(), swap_fov=()):
    lines = ["# reason test", "frame eye px py pz qx qy qz qw fovL fovR fovU fovD image rect_x rect_y rect_w rect_h same fake"]
    for eye in (0, 1):
        for k in range(n):
            img = scene(k, eye)
            fake = (k, eye) in spoil
            if fake:
                img = img.copy()
                img[100:148, 150:198] = (255, 0, 255)
            fr.write_ppm(os.path.join(folder, "frame-%d-%02d.ppm" % (eye, k)), img)
            fov = FOV[1 - eye] if (k, eye) in swap_fov else FOV[eye]
            lines.append("%d %d %.6f 1.7 0 0 0 0 1 %.4f %.4f %.4f %.4f %d 0 0 %d %d 0 %d"
                         % (1000 + k, eye, -0.032 if eye == 0 else 0.032, fov[0], fov[1], fov[2], fov[3], k % 3, W, H, 1 if fake else 0))
    with open(os.path.join(folder, "frames.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")


def check(cond, what):
    if not cond:
        print("FAIL:", what)
        sys.exit(1)


def test_spoiled_frames_are_found():
    with tempfile.TemporaryDirectory() as d:
        make_dump(d, spoil={(11, 0), (27, 0), (19, 1)})
        rep = fr.read_dump(d)
        odd = sorted((o["eye"], o["index"]) for o in rep["odd"])
        check(odd == [(0, 11), (0, 27), (1, 19)], "odd frames %s" % odd)
        check(rep["verdict"] == "picture-only odd frames", rep["verdict"])
        check(rep["fake_match"] == (3, 3, 0), "fake match %s" % (rep["fake_match"],))


def test_steady_pan_has_none():
    with tempfile.TemporaryDirectory() as d:
        make_dump(d)
        rep = fr.read_dump(d)
        check(rep["odd"] == [], "a steady pan gave odd frames %s" % rep["odd"])
        check(rep["verdict"] == "no odd frames", rep["verdict"])


def test_other_eye_fov_is_named():
    with tempfile.TemporaryDirectory() as d:
        make_dump(d, spoil={(19, 0)}, swap_fov={(19, 0)})
        rep = fr.read_dump(d)
        check([o["index"] for o in rep["odd"]] == [19], "odd %s" % rep["odd"])
        check(rep["odd"][0]["fov_swapped"], "fov swap not seen")
        check(rep["swapped_frames"] == [(0, 19)], "swapped %s" % rep["swapped_frames"])
        check(rep["verdict"] == "other-eye data on odd frames", rep["verdict"])


def test_report_files_are_written():
    with tempfile.TemporaryDirectory() as d:
        make_dump(d, spoil={(11, 0)})
        fr.main([d])
        for name in ("report.txt", "sheet.jpg", "heat-0.png", "heat-1.png"):
            check(os.path.exists(os.path.join(d, name)), name + " missing")
        text = open(os.path.join(d, "report.txt")).read()
        check("ODD frame 1011 eye 0" in text and "VERDICT: picture-only odd frames" in text, text)
        # the closest calls are always listed, so the line can be set from real frames
        check("closest: frame 1011 eye 0 share 1.00" in text, text)
        check(text.count("closest:") == fr.CLOSEST_SHOWN, text)


if __name__ == "__main__":
    for t in (test_spoiled_frames_are_found, test_steady_pan_has_none, test_other_eye_fov_is_named, test_report_files_are_written):
        t()
    print("OK")
