"""jitter_measure.py -- read a recording frame by frame and measure how the picture moves.

For every frame of the video it records two things:
  * whether the game drew a NEW picture at all (the frame differs from the one before), and
  * how far the whole picture moved since the last new picture (phase correlation on a grey, shrunk copy).

Those two together separate the three faults Tefa tells apart (claude-memory PREFERENCES.md, "what jittery means"):
  * no new picture for a while          -> frame rate / hitch (the game or capture did not draw)
  * a new picture, but the view held     -> JITTER: the camera was renewed less often than the picture
  * the view moving back against itself  -> SHAKE: smooth but wrong, wobbling
Nothing here judges; jitter_judge.py does that from the numbers this file returns.
"""
import cv2
import numpy as np

# --- settings: every number with a name (game-mod-rules "code shape") ---
WORK_WIDTH = 320            # frames are shrunk to this width before measuring; enough for whole-picture motion
WORK_MIN_HEIGHT = 120       # ... but never below this height (a flat strip gives the motion reading too little)
TWO_EYES_MATCH = 0.2        # the left half found inside the right half with this confidence (and no vertical offset)
                            # means two eyes side by side: measure the left eye only, because two matching halves make
                            # the motion reading lock onto half-width jumps. Shape alone cannot tell: a 1280x720 game
                            # window held two eyes in a Hard Reset recording (2026-10-07; match 0.26-0.55 there,
                            # 0.01-0.14 on five one-picture recordings, measured 2026-10-08)
TWO_EYES_MAX_DY = 2         # pixels (at 144 high) the two eyes may sit apart vertically
EYE_CHECK_FRAMES = 9        # how many frames, spread over the recording, vote on "two eyes or one picture"
CENTRE_FRACTION = 0.70      # measure only the middle 70% of the picture: HUD, borders and black bars stay out
PIXEL_CHANGED = 6           # a pixel counts as changed when its grey value moved by this much (0-255)
NEW_PICTURE_FRACTION = 0.0005  # a frame is a NEW picture when at least this share of pixels changed. The video
                            # encoder repaints whole repeated frames with tiny noise (mean ~0.1, max ~2-4, measured
                            # 2026-10-08 on OBS/NVENC files), so the mean alone cannot tell a repeat from a new frame
BLUR_KERNEL = 3             # light blur so encoder noise does not count as motion
MIN_RESPONSE = 0.04         # phase-correlation confidence below which the motion reading is not trusted


def _prep(frame, size, one_eye=False):
    if one_eye:
        frame = frame[:, :frame.shape[1] // 2]
    g = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    g = cv2.resize(g, size, interpolation=cv2.INTER_AREA)
    if BLUR_KERNEL > 1:
        g = cv2.GaussianBlur(g, (BLUR_KERNEL, BLUR_KERNEL), 0)
    return g.astype(np.float32)


def _centre(img):
    h, w = img.shape
    mh, mw = int(h * (1 - CENTRE_FRACTION) / 2), int(w * (1 - CENTRE_FRACTION) / 2)
    return img[mh:h - mh, mw:w - mw]


def _halves_alike(frame):
    g = cv2.cvtColor(cv2.resize(frame, (512, 144), interpolation=cv2.INTER_AREA), cv2.COLOR_BGR2GRAY).astype(np.float32)
    if g.std() < 2:
        return False          # a black or flat picture says nothing
    (_dx, dy), resp = cv2.phaseCorrelate(g[:, :256], g[:, 256:])
    return resp >= TWO_EYES_MATCH and abs(dy) <= TWO_EYES_MAX_DY


def two_eyes(cap, total):
    votes = []
    for k in range(EYE_CHECK_FRAMES):
        cap.set(cv2.CAP_PROP_POS_FRAMES, int(total * (k + 1) / (EYE_CHECK_FRAMES + 1)))
        ok, f = cap.read()
        if ok:
            votes.append(_halves_alike(f))
    cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
    return bool(votes) and sum(votes) > len(votes) / 2


def measure(path, progress=None, max_seconds=None):
    """Returns (info, rows). rows: one dict per frame after the first:
    t (s), diff (mean grey change), new (bool), dx, dy (pixels at full video width, since the last NEW picture),
    resp (motion confidence 0-1), gap (frames since the last new picture)."""
    cap = cv2.VideoCapture(path)
    if not cap.isOpened():
        raise RuntimeError("cannot open the video")
    fps = cap.get(cv2.CAP_PROP_FPS) or 60.0
    w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH)); h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    total = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    one_eye = two_eyes(cap, total)
    mw = w // 2 if one_eye else w
    scale = max(1.0, min(mw / WORK_WIDTH, h / WORK_MIN_HEIGHT))
    size = (int(round(mw / scale)), int(round(h / scale)))
    ok, frame = cap.read()
    if not ok:
        raise RuntimeError("the video has no frames")
    last_new = _prep(frame, size, one_eye)
    last_seen = last_new
    win = cv2.createHanningWindow(_centre(last_new).shape[::-1], cv2.CV_32F)
    # this OpenCV build reports a fixed offset (half a pixel sideways, measured 2026-10-08) for two IDENTICAL
    # pictures; read it once from a textured test pattern against itself and take it off every reading. (Not from
    # the first frame: recordings often start black, and a black frame gave a nonsense offset of 116 pixels.)
    pattern = cv2.GaussianBlur(np.random.default_rng(1).random(_centre(last_new).shape).astype(np.float32), (5, 5), 0)
    (bias_x, bias_y), _ = cv2.phaseCorrelate(pattern, pattern, win)
    rows, i, gap = [], 0, 0
    limit = int(max_seconds * fps) if max_seconds else None
    while True:
        ok, frame = cap.read()
        if not ok or (limit and i >= limit):
            break
        i += 1; gap += 1
        cur = _prep(frame, size, one_eye)
        delta = np.abs(cur - last_seen)
        diff = float(np.mean(delta))
        new = float(np.mean(delta >= PIXEL_CHANGED)) >= NEW_PICTURE_FRACTION
        dx = dy = resp = 0.0
        if new:
            (dx, dy), resp = cv2.phaseCorrelate(_centre(last_new), _centre(cur), win)
            dx -= bias_x; dy -= bias_y
            if resp < MIN_RESPONSE:
                dx = dy = float("nan")
            last_new = cur
        rows.append({"t": i / fps, "diff": diff, "new": new, "dx": dx * scale, "dy": dy * scale,
                     "resp": float(resp), "gap": gap})
        if new:
            gap = 0
        last_seen = cur
        if progress and i % 600 == 0:
            progress(i, total)
    cap.release()
    info = {"fps": fps, "width": mw, "one_eye": one_eye, "height": h, "frames": i + 1, "seconds": round((i + 1) / fps, 2)}
    return info, rows
