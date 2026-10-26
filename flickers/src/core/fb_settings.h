// fb_settings.h -- every number Flickers BeG0nE uses, named, plus the reader for flickers_begone.txt.
// Pure CPU (tested by test/settings_test.cpp). The file is `key = value` lines; `#` starts a comment; unknown keys are
// ignored and a value that is not a whole number in range keeps its default, so a typo never switches anything on.
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>

namespace fb {

// --- fixed numbers (not in the settings file) ---
constexpr int kEyes = 2;
constexpr int kDumpStallMs = 4;           // a frame around a dump may take at most this much longer (Review Focus 5)
constexpr int kControlPollMs = 1000;      // the settings file is read again about once a second
constexpr float kFakeColour[4] = {1.0f, 0.0f, 1.0f, 1.0f};   // magenta: never in a game picture by chance

struct Settings {
    std::string exe;                      // only this exe (file name, any case); empty = every exe
    int on = 0;                           // 0 off, 1 pass through + log, 2 catcher (auto dump), 3 dump now then 2
    int ring_frames = 72;                 // pictures kept per eye (about a second at 72-90 Hz)
    int ring_half = 1;                    // 1 = keep them at half size (an eye at full size costs ~16 MB a frame)
    int auto_dump_s = 20;                 // with on = 2, a dump every this many seconds
    int max_dumps_hand = 4;               // dumps asked for by hand (on = 3) per launch
    int max_dumps_auto = 12;              // automatic dumps per launch
    int fake = 0;                         // 1 = spoil a patch of one eye now and then, on purpose (testing only; on = 2)
    int fake_every_min = 10;              // frames between two fake flickers, at least ...
    int fake_every_max = 20;              // ... and at most
    int fake_patch_px = 96;               // the spoiled square's side, in pixels of the eye picture
    uint32_t fake_seed = 1;               // same seed, same frames
};

namespace detail {
inline std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) b--;
    return s.substr(a, b - a);
}
inline bool read_int(const std::string& v, long lo, long hi, long& out) {
    if (v.empty()) return false;
    char* end = nullptr;
    const long n = strtol(v.c_str(), &end, 10);
    if (*end != '\0' || n < lo || n > hi) return false;
    out = n;
    return true;
}
}  // namespace detail

// Reads `text` into `out`. Returns false only when text is null.
inline bool parse_settings(const char* text, Settings& out) {
    if (!text) return false;
    struct Num { const char* key; int* dst; long lo, hi; };
    const Num nums[] = {
        {"on", &out.on, 0, 3},
        {"ring_frames", &out.ring_frames, 4, 360},
        {"ring_half", &out.ring_half, 0, 1},
        {"auto_dump_s", &out.auto_dump_s, 2, 3600},
        {"max_dumps_hand", &out.max_dumps_hand, 0, 100},
        {"max_dumps_auto", &out.max_dumps_auto, 0, 100},
        {"fake", &out.fake, 0, 1},
        {"fake_every_min", &out.fake_every_min, 2, 10000},
        {"fake_every_max", &out.fake_every_max, 2, 10000},
        {"fake_patch_px", &out.fake_patch_px, 4, 1024},
    };
    std::string all(text), line;
    size_t pos = 0;
    while (pos <= all.size()) {
        size_t nl = all.find('\n', pos);
        if (nl == std::string::npos) nl = all.size();
        line = all.substr(pos, nl - pos);
        pos = nl + 1;
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line.resize(hash);
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = detail::trim(line.substr(0, eq)), v = detail::trim(line.substr(eq + 1));
        long n = 0;
        if (k == "exe") { out.exe = v; continue; }
        if (k == "fake_seed") { if (detail::read_int(v, 0, 2147483647L, n)) out.fake_seed = (uint32_t)n; continue; }
        for (const Num& m : nums)
            if (k == m.key && detail::read_int(v, m.lo, m.hi, n)) *m.dst = (int)n;
    }
    if (out.fake_every_max < out.fake_every_min) out.fake_every_max = out.fake_every_min;
    return true;
}

// True when `exe_path` (a full path or a file name) is the exe the settings name, or when they name none.
inline bool exe_matches(const Settings& s, const char* exe_path) {
    if (s.exe.empty()) return true;
    const char* base = exe_path;
    for (const char* p = exe_path; *p; p++) if (*p == '\\' || *p == '/') base = p + 1;
    return _stricmp(base, s.exe.c_str()) == 0;
}

}  // namespace fb
