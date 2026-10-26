// dump_test.cpp -- the CPU half of the dump: the .ppm writer and one frames.txt line.
#include "check.h"
#include "../src/core/fb_dump.h"
#include "../src/core/fb_dump.cpp"
#include <stdio.h>
#include <string.h>
#include <string>
#include <windows.h>

int main() {
    // a 4x2 RGBA picture with a 20-byte row pitch (4 bytes of padding per row): the writer must drop A and the padding
    uint8_t px[2 * 20] = {};
    for (int y = 0; y < 2; y++)
        for (int x = 0; x < 4; x++) {
            uint8_t* p = px + y * 20 + x * 4;
            p[0] = (uint8_t)(10 * x); p[1] = (uint8_t)(100 + y); p[2] = 7; p[3] = 255;
        }
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    const std::string path = std::string(tmp) + "fb_dump_test.ppm";
    CHECK(fb::write_ppm(path.c_str(), 4, 2, px, 20));
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "rb");
    CHECK(f != nullptr);
    uint8_t buf[64] = {};
    const size_t n = f ? fread(buf, 1, sizeof buf, f) : 0;
    if (f) fclose(f);
    const char* head = "P6\n4 2\n255\n";
    CHECK(n == strlen(head) + 4 * 2 * 3);
    CHECK(memcmp(buf, head, strlen(head)) == 0);
    const uint8_t* body = buf + strlen(head);
    CHECK(body[0] == 0 && body[1] == 100 && body[2] == 7);          // (0,0)
    CHECK(body[9] == 30 && body[10] == 100);                         // (3,0)
    CHECK(body[12] == 0 && body[13] == 101);                         // (0,1): padding skipped
    DeleteFileA(path.c_str());

    fb::EyeRecord r;
    r.frame = 1234; r.image = 2; r.slice = 0;
    r.pose.position = {0.032f, 1.6f, -0.1f};
    r.pose.orientation = {0.0f, 0.1f, 0.0f, 0.995f};
    r.fov = {-0.9f, 0.8f, 0.85f, -0.95f};
    r.rect = {{0, 0}, {1280, 1400}};
    r.same_as_last = true;
    const std::string line = fb::frame_line(r, 1, true);
    CHECK(line == "1234 1 0.032000 1.600000 -0.100000 0.000000 0.100000 0.000000 0.995000 -0.9000 0.8000 0.8500 -0.9500 2 0 0 1280 1400 1 1");
    CHECK(fb::frame_header() == "frame eye px py pz qx qy qz qw fovL fovR fovU fovD image rect_x rect_y rect_w rect_h same fake");
    return DONE("dump_test");
}
