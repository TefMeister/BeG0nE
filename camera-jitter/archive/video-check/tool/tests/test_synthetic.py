"""Self-test: make recordings whose answer is known, run them through the tool, check every verdict.

    python tests/test_synthetic.py            # exit code 0 = every case judged right

Each case pans a camera across a large made-up picture at a slow, even speed (like a slow head turn) and draws a
small counter in a corner that changes every frame the game "draws", the way a HUD or animated scene does. The
videos go through the same H.264 encoder OBS uses (via ffmpeg) when ffmpeg is present, so encoder noise is in them.
"""
import os
import shutil
import subprocess
import sys
import tempfile

import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import jitter_judge  # noqa: E402
import jitter_measure  # noqa: E402

W, H, FPS, SECONDS = 640, 360, 60, 8
PAN_PX = 3.0                 # per 60 Hz frame: ~28% of the width a second, a slow head turn


def world(seed=7):
    rng = np.random.default_rng(seed)
    big = (rng.random((H * 2, W * 6)) * 255).astype(np.uint8)
    big = cv2.GaussianBlur(big, (0, 0), 3)
    for _ in range(400):
        x, y = rng.integers(0, W * 6), rng.integers(0, H * 2)
        cv2.circle(big, (int(x), int(y)), int(rng.integers(4, 30)), int(rng.integers(0, 255)), -1)
    return cv2.cvtColor(big, cv2.COLOR_GRAY2BGR)


def view(big, x, y, counter):
    M = np.float32([[1, 0, -x], [0, 1, -y]])
    img = cv2.warpAffine(big, M, (W, H), flags=cv2.INTER_LINEAR)
    if counter is not None:
        cv2.rectangle(img, (4, 4), (90, 30), (0, 0, 0), -1)
        cv2.putText(img, str(counter), (8, 26), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255, 255, 255), 2)
    return img


def frames(case, big):
    """Yields the picture the capture sees at each 60 Hz frame."""
    shown = None
    for i in range(FPS * SECONDS):
        t = i
        if case == "smooth":
            shown = view(big, 50 + PAN_PX * t, 100, i)
        elif case == "stepped":        # picture every frame, camera renewed only every 3rd frame
            shown = view(big, 50 + PAN_PX * (t - t % 3), 100, i)
        elif case == "shake":          # slow pan plus a fast wobble
            shown = view(big, 50 + PAN_PX * t + 9 * np.sin(t * 2.4), 100 + 3 * np.cos(t * 1.9), i)
        elif case == "lowfps":         # the game draws 30 a second, camera smooth: frame rate only
            if i % 2 == 0:
                shown = view(big, 50 + PAN_PX * t, 100, i)
        elif case == "freeze":         # smooth, then nothing new for 2 seconds, then smooth again
            if not (3 * FPS <= i < 5 * FPS):
                shown = view(big, 50 + PAN_PX * t, 100, i)
        elif case == "lowfps_stepped":  # the game draws 30 a second AND renews the camera only every 2nd drawing
            if i % 2 == 0:
                shown = view(big, 50 + PAN_PX * (t - t % 4), 100, i)
        elif case == "cadence40":      # the game draws 40 a second, the capture takes 60: uneven gaps, smooth camera
            if (i * 40) // 60 != ((i - 1) * 40) // 60 or i == 0:
                shown = view(big, 50 + PAN_PX * t, 100, i)
        elif case == "late1in5":       # every 5th picture the camera is one picture late, then catches up
            shown = view(big, 50 + PAN_PX * (t - 1 if t % 5 == 0 else t), 100, i)
        elif case == "still":          # head still, HUD alive
            shown = view(big, 300, 100, i)
        yield shown


EXPECT = {
    "smooth": lambda s: s["seconds_jitter"] == 0 and s["seconds_shake"] == 0 and s["quality"] == "good",
    "stepped": lambda s: s["seconds_jitter"] >= s["seconds_travelling"] - 1 and s["seconds_travelling"] >= 5,
    "shake": lambda s: s["seconds_shake"] >= SECONDS - 2,
    "lowfps": lambda s: s["seconds_jitter"] == 0 and s["seconds_shake"] == 0 and 25 <= s["new_pictures_per_s"] <= 35,
    "freeze": lambda s: len(s["freezes"]) == 1 and 1.8 <= s["freezes"][0]["for_s"] <= 2.2 and s["seconds_jitter"] == 0,
    "lowfps_stepped": lambda s: s["seconds_jitter"] >= s["seconds_travelling"] - 1 and s["seconds_travelling"] >= 5,
    "cadence40": lambda s: s["seconds_jitter"] == 0 and s["seconds_shake"] == 0,
    "late1in5": lambda s: s["seconds_jitter"] >= s["seconds_travelling"] - 1 and s["seconds_travelling"] >= 5,
    "still": lambda s: s["quality"] == "no-movement" and s["seconds_shake"] == 0 and not s["freezes"],
}


def write(case, big, folder):
    raw = os.path.join(folder, case + "_raw.avi")
    vw = cv2.VideoWriter(raw, cv2.VideoWriter_fourcc(*"MJPG"), FPS, (W, H))
    for f in frames(case, big):
        vw.write(f)
    vw.release()
    if shutil.which("ffmpeg"):
        out = os.path.join(folder, case + ".mp4")
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", raw, "-c:v", "libx264", "-preset", "fast", "-crf", "20",
                        "-pix_fmt", "yuv420p", out], check=True)
        return out
    return raw


def main():
    big = world()
    folder = tempfile.mkdtemp(prefix="begone-jitter-test-")
    failed = 0
    try:
        for case, ok in EXPECT.items():
            path = write(case, big, folder)
            info, rows = jitter_measure.measure(path)
            s = jitter_judge.judge(info, rows)
            good = ok(s)
            failed += not good
            print(f"{'PASS' if good else 'FAIL'}  {case:7s}  {s['verdict']}")
            if not good:
                print("      ", {k: s[k] for k in ("new_pictures_per_s", "seconds_travelling", "seconds_smooth",
                                                     "seconds_jitter", "seconds_shake", "seconds_still", "quality")})
                print("      ", s["windows"][1:4])
    finally:
        shutil.rmtree(folder, ignore_errors=True)
    print(f"{len(EXPECT) - failed}/{len(EXPECT)} cases judged right")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
