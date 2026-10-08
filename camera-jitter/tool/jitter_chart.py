"""jitter_chart.py -- one picture per run: how the view moved, second by second, and what each second was judged.

No game pictures go into it, only lines and coloured bands, so it is safe to keep on a public repo.
Drawn with OpenCV so the tool needs nothing beyond what it already uses.
"""
import cv2
import numpy as np

CHART_W, ROW_H, MARGIN = 1400, 220, 60
KIND_COLOUR = {            # BGR
    "smooth": (120, 200, 120), "JITTER": (60, 60, 230), "SHAKE": (40, 160, 250),
    "still": (200, 200, 200), "jump": (230, 200, 120), "too few pictures": (90, 90, 90),
}
FREEZE_COLOUR, HITCH_COLOUR, LINE_COLOUR, TEXT_COLOUR = (30, 30, 30), (200, 0, 200), (255, 255, 255), (20, 20, 20)


def _row(canvas, top, title, info, rows, summary):
    secs = max(summary["seconds"], 1e-6)
    x_of = lambda t: int(MARGIN + (CHART_W - 2 * MARGIN) * t / secs)
    band_top, band_bot = top + 28, top + ROW_H - 20
    for w in summary["windows"]:
        cv2.rectangle(canvas, (x_of(w["at_s"]), band_top), (x_of(w["at_s"] + 1.0), band_bot),
                      KIND_COLOUR.get(w.get("kind"), (90, 90, 90)), -1)
    for f in summary["freezes"]:
        cv2.rectangle(canvas, (x_of(f["at_s"]), band_top), (x_of(f["at_s"] + f["for_s"]), band_bot), FREEZE_COLOUR, -1)
    for t in summary["hitch_times_s"]:
        cv2.line(canvas, (x_of(t), band_bot), (x_of(t), band_bot + 8), HITCH_COLOUR, 2)
    # the sideways step of every new picture, % of the width, scaled to the band
    fps, width = info["fps"], info["width"]
    pts = [(r["t"], r["dx"] * 100.0 / width / r["gap"]) for r in rows if r["new"] and not np.isnan(r["dx"])]
    if pts:
        steps = np.array([p[1] for p in pts])
        lim = max(0.2, float(np.percentile(np.abs(steps), 98)))
        mid = (band_top + band_bot) // 2
        y_of = lambda v: int(mid - (band_bot - band_top) / 2 * max(-1.0, min(1.0, v / lim)))
        cv2.line(canvas, (MARGIN, mid), (CHART_W - MARGIN, mid), (255, 255, 255), 1)
        poly = np.array([[x_of(t), y_of(v)] for t, v in pts], np.int32)
        cv2.polylines(canvas, [poly], False, LINE_COLOUR, 1, cv2.LINE_AA)
    cv2.putText(canvas, f"{title}: {summary['verdict'][:150]}", (MARGIN, top + 20), cv2.FONT_HERSHEY_SIMPLEX, 0.45,
                TEXT_COLOUR, 1, cv2.LINE_AA)
    for s in range(0, int(secs) + 1, 5):
        cv2.putText(canvas, f"{s}s", (x_of(s) - 8, top + ROW_H - 4), cv2.FONT_HERSHEY_SIMPLEX, 0.35, TEXT_COLOUR, 1)


def draw(path, parts):
    """parts: list of (title, info, rows, summary). Writes a PNG at path."""
    legend_h = 30
    canvas = np.full((ROW_H * len(parts) + legend_h, CHART_W, 3), 245, np.uint8)
    for k, (title, info, rows, summary) in enumerate(parts):
        _row(canvas, k * ROW_H, title, info, rows, summary)
    x, y = MARGIN, ROW_H * len(parts) + 20
    for name, col in list(KIND_COLOUR.items())[:5] + [("freeze", FREEZE_COLOUR), ("hitch", HITCH_COLOUR)]:
        cv2.rectangle(canvas, (x, y - 10), (x + 14, y), col, -1)
        cv2.putText(canvas, name, (x + 18, y), cv2.FONT_HERSHEY_SIMPLEX, 0.45, TEXT_COLOUR, 1)
        x += 105
    cv2.putText(canvas, "white line = sideways step of each new picture", (x, y), cv2.FONT_HERSHEY_SIMPLEX, 0.45,
                TEXT_COLOUR, 1)
    cv2.imwrite(path, canvas)
