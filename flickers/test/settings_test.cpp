// settings_test.cpp -- the settings file: known keys read, unknown keys ignored, bad numbers keep the default.
#include "check.h"
#include "../src/core/fb_settings.h"

int main() {
    fb::Settings s;
    CHECK(s.on == 0);
    CHECK(s.ring_frames == 72);
    CHECK(s.auto_dump_s == 20);

    CHECK(fb::parse_settings("on = 1\nring_frames = 36\nbogus = 3\nfake = x\n", s));
    CHECK(s.on == 1);
    CHECK(s.ring_frames == 36);
    CHECK(s.fake == 0);                  // "x" is not a number: the default stays

    fb::Settings t;
    CHECK(fb::parse_settings("# a comment\r\nexe = re4.exe\r\nfake_seed=7\r\n  max_dumps_auto =  3 \r\n", t));
    CHECK(t.exe == "re4.exe");
    CHECK(t.fake_seed == 7u);
    CHECK(t.max_dumps_auto == 3);

    fb::Settings u;
    CHECK(fb::parse_settings("ring_frames = -5\nring_frames_extra = 9\n", u));
    CHECK(u.ring_frames == 72);          // out of range keeps the default
    CHECK(fb::exe_matches(t, "C:\\Games\\RE4\\RE4.EXE"));
    CHECK(!fb::exe_matches(t, "C:\\Games\\hello_xr.exe"));
    CHECK(fb::exe_matches(u, "anything.exe"));   // no exe named: every exe
    return DONE("settings_test");
}
