// fb_fake.h -- the fake flicker: which frames get a spoiled patch, in which eye, where. Pure CPU, deterministic from
// the seed (tested by test/fake_test.cpp). It exists so the catcher and the reader can be proven on frames whose
// numbers are known, before any real game: every spoiled frame must come back from the reader as an odd frame.
#pragma once
#include <stdint.h>

namespace fb {

struct FakeSpot { int x = 0, y = 0, size = 0; };

class FakeSchedule {
public:
    FakeSchedule(uint32_t seed, int every_min, int every_max, int patch_px, int eye_w, int eye_h)
        : seed_(seed ? seed : 1), min_(every_min), max_(every_max < every_min ? every_min : every_max),
          patch_(patch_px), w_(eye_w), h_(eye_h) {}

    // True when frame `frame` spoils eye `eye`; `out` is the square, always inside the eye picture.
    // Spoiled frames: the first lands at 1 + gap(0), each next one a gap in [min, max] after the one before.
    // Answering a frame walks the schedule from the start, so it is the same answer whatever the call order.
    bool spoil(uint64_t frame, int eye, FakeSpot& out) const {
        uint64_t at = 0;
        uint32_t k = 0;
        while (true) {
            at += (uint64_t)gap(k);
            if (at > frame) return false;
            if (at == frame) break;
            k++;
        }
        const uint32_t r = mix(k * 4u + 1u);
        if ((int)(r & 1u) != eye) return false;
        const int span_x = w_ > patch_ ? w_ - patch_ : 0, span_y = h_ > patch_ ? h_ - patch_ : 0;
        out.size = patch_;
        out.x = span_x ? (int)(mix(k * 4u + 2u) % (uint32_t)(span_x + 1)) : 0;
        out.y = span_y ? (int)(mix(k * 4u + 3u) % (uint32_t)(span_y + 1)) : 0;
        return true;
    }

private:
    int gap(uint32_t k) const { return min_ + (int)(mix(k * 4u) % (uint32_t)(max_ - min_ + 1)); }
    uint32_t mix(uint32_t x) const {      // a small integer hash (lowbias32) keyed by the seed
        x ^= seed_ * 0x9E3779B9u;
        x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu; x ^= x >> 16;
        return x;
    }
    uint32_t seed_;
    int min_, max_, patch_, w_, h_;
};

}  // namespace fb
