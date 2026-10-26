// fb_dump.cpp -- see fb_dump.h.
#include "fb_dump.h"
#include <stdio.h>
#include <vector>

namespace fb {

bool write_ppm(const char* path, uint32_t w, uint32_t h, const uint8_t* rgba, uint32_t pitch) {
    FILE* f = nullptr;
    if (fopen_s(&f, path, "wb") != 0 || !f) return false;
    fprintf(f, "P6\n%u %u\n255\n", w, h);
    std::vector<uint8_t> row(w * 3);
    for (uint32_t y = 0; y < h; y++) {
        const uint8_t* s = rgba + (size_t)y * pitch;
        for (uint32_t x = 0; x < w; x++) {
            row[x * 3 + 0] = s[x * 4 + 0];
            row[x * 3 + 1] = s[x * 4 + 1];
            row[x * 3 + 2] = s[x * 4 + 2];
        }
        fwrite(row.data(), 1, row.size(), f);
    }
    fclose(f);
    return true;
}

std::string frame_header() {
    return "frame eye px py pz qx qy qz qw fovL fovR fovU fovD image rect_x rect_y rect_w rect_h same fake";
}

std::string frame_line(const EyeRecord& r, int eye, bool fake) {
    char b[512];
    snprintf(b, sizeof b, "%llu %d %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.4f %.4f %.4f %.4f %u %d %d %d %d %d %d",
             (unsigned long long)r.frame, eye, r.pose.position.x, r.pose.position.y, r.pose.position.z,
             r.pose.orientation.x, r.pose.orientation.y, r.pose.orientation.z, r.pose.orientation.w,
             r.fov.angleLeft, r.fov.angleRight, r.fov.angleUp, r.fov.angleDown, r.image, r.rect.offset.x,
             r.rect.offset.y, r.rect.extent.width, r.rect.extent.height, r.same_as_last ? 1 : 0, fake ? 1 : 0);
    return b;
}

}  // namespace fb
