// fake_test.cpp -- the fake flicker's schedule: gaps in range, one eye per spoiled frame, repeatable from the seed.
#include "check.h"
#include "../src/core/fb_fake.h"
#include <vector>

int main() {
    fb::FakeSchedule a(1, 10, 20, 96, 640, 640), b(1, 10, 20, 96, 640, 640), c(2, 10, 20, 96, 640, 640);
    std::vector<uint64_t> fa, fc;
    bool gaps_ok = true, one_eye = true, inside = true, same = true;
    uint64_t last = 0;
    for (uint64_t f = 1; f <= 10000; f++) {
        fb::FakeSpot s0{}, s1{}, t0{}, t1{};
        const bool e0 = a.spoil(f, 0, s0), e1 = a.spoil(f, 1, s1);
        if (e0 != b.spoil(f, 0, t0) || e1 != b.spoil(f, 1, t1)) same = false;
        if (e0 && e1) one_eye = false;
        if (e0 || e1) {
            if (last && (f - last < 10 || f - last > 20)) gaps_ok = false;
            last = f;
            fa.push_back(f);
            const fb::FakeSpot& s = e0 ? s0 : s1;
            if (s.x < 0 || s.y < 0 || s.x + s.size > 640 || s.y + s.size > 640 || s.size != 96) inside = false;
        }
        fb::FakeSpot u{};
        if (c.spoil(f, 0, u) || c.spoil(f, 1, u)) fc.push_back(f);
    }
    CHECK(fa.size() > 400);
    CHECK(gaps_ok);
    CHECK(one_eye);
    CHECK(inside);
    CHECK(same);
    CHECK(fa != fc);                    // another seed gives another list
    return DONE("fake_test");
}
