# Ghost Hand: judge a magazine miss by "how close it got" against "how close it had to get"

From: a session on 2026-10-31 (RE2 report `ghost-hand/data/visceral-re2-vr/2026-10-31_18-56-58_openxr`)

`[raw]` · `[looks doable]`, checked against `ghost_hand_check.py` (lanes 0.95.0) only.

The RE2 magazine gesture (`mag`) left 3 of 6 misses "not judged". The checker's `<g>_need` means **how far the hands
must move APART** (right for a rack or pump), and `why()` compares it with the hands' spread. A magazine is the
other way round: it must come **within** a distance of the magwell. RE2 and Village already log `mag_closest_m`;
the checker reads no `_closest_m` value. Feeding `mag_need` as it stands would print "the hands moved X apart,
enough for the Y needed", a wrong verdict, so RE2 does not send it yet.

**What it'd take:** in `why()`, when a gesture carries `<g>_closest_m` and `<g>_need`, judge "came within X of the Y
needed" (miss = closest > need; a near miss under 1.5 x need says the zone may be too tight). Then RE2 and Village
log `mag_need` beside `mag_closest_m`.
