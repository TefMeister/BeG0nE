// formats_test.cpp -- the core's own swapchain must take a format the runtime offers in the game's format family.
#include "check.h"
#include "../src/core/fb_views.h"
#include "../src/core/fb_views.cpp"
#include <dxgiformat.h>

int main() {
    const int64_t uns[] = {DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB};
    CHECK(fb::pick_format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, uns, 2) == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    CHECK(fb::pick_format(DXGI_FORMAT_R8G8B8A8_UNORM, uns, 2) == DXGI_FORMAT_R8G8B8A8_UNORM);
    CHECK(fb::pick_format(DXGI_FORMAT_R8G8B8A8_TYPELESS, uns, 2) == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    const int64_t bgr[] = {DXGI_FORMAT_B8G8R8A8_UNORM_SRGB};
    CHECK(fb::pick_format(DXGI_FORMAT_B8G8R8A8_UNORM, bgr, 1) == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);
    const int64_t f16[] = {DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R16G16B16A16_FLOAT};
    CHECK(fb::pick_format(DXGI_FORMAT_R16G16B16A16_FLOAT, f16, 2) == DXGI_FORMAT_R16G16B16A16_FLOAT);
    CHECK(fb::pick_format(DXGI_FORMAT_R16G16B16A16_TYPELESS, f16, 2) == DXGI_FORMAT_R16G16B16A16_FLOAT);
    CHECK(fb::pick_format(DXGI_FORMAT_R10G10B10A2_UNORM, uns, 2) == 0);   // nothing in its family: pass through
    CHECK(fb::pick_format(DXGI_FORMAT_R8G8B8A8_UNORM, nullptr, 0) == 0);
    return DONE("formats_test");
}
