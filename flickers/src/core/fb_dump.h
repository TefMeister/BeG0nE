// fb_dump.h -- the CPU half of a catcher dump (tested by test/dump_test.cpp): pictures as binary .ppm (RGB, 8 bit) and
// one frames.txt line per picture. The GPU half (copying the ring to readback memory) is in fb_gpu.cpp.
#pragma once
#include <stdint.h>
#include <string>
#include "fb_views.h"

namespace fb {

// Writes an RGBA8 picture (rows `pitch` bytes apart) as a binary PPM, dropping alpha. False if the file cannot be made.
bool write_ppm(const char* path, uint32_t w, uint32_t h, const uint8_t* rgba, uint32_t pitch);

// frames.txt: the header and one line per picture (see the spec §5.2).
std::string frame_header();
std::string frame_line(const EyeRecord& r, int eye, bool fake);

}  // namespace fb
