// fb_views.cpp -- see fb_views.h.
#include "fb_views.h"
#include <string.h>

namespace fb {

void Views::on_create_swapchain(XrSwapchain sc, int64_t fmt, uint32_t w, uint32_t h, uint32_t array_size) {
    Chain& c = chains_[sc];
    c.fmt = fmt; c.w = w; c.h = h; c.layers = array_size ? array_size : 1;
}

void Views::on_destroy_swapchain(XrSwapchain sc) {
    chains_.erase(sc);
    gen_++;
}

void Views::on_acquire(XrSwapchain sc, uint32_t index) {
    Chain& c = chains_[sc];                  // a swapchain made before the core was in place is learned here
    c.last = index;
    c.acquired = true;
}

bool Views::on_end_frame(const XrFrameEndInfo& info, uint64_t frame, EyeRecord out[2]) {
    const XrCompositionLayerProjection* proj = nullptr;
    for (uint32_t i = 0; i < info.layerCount && info.layers; i++) {
        const XrCompositionLayerBaseHeader* l = info.layers[i];
        if (l && l->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
            const auto* p = (const XrCompositionLayerProjection*)l;
            if (p->viewCount == 2 && p->views) { proj = p; break; }
        }
    }
    if (!proj) return false;
    for (int e = 0; e < 2; e++) {
        const XrCompositionLayerProjectionView& v = proj->views[e];
        auto it = chains_.find(v.subImage.swapchain);
        if (it == chains_.end() || !it->second.acquired) return false;
        EyeRecord& r = out[e];
        r.sc = v.subImage.swapchain;
        r.image = it->second.last;
        r.rect = v.subImage.imageRect;
        r.slice = v.subImage.imageArrayIndex;
        r.pose = v.pose;
        r.fov = v.fov;
        r.frame = frame;
        r.w = it->second.w; r.h = it->second.h; r.fmt = it->second.fmt;
        const EyeRecord& p = prev_[e];
        r.same_as_last = have_prev_ && p.sc == r.sc && p.image == r.image && p.slice == r.slice &&
                         memcmp(&p.rect, &r.rect, sizeof r.rect) == 0 && memcmp(&p.pose, &r.pose, sizeof r.pose) == 0;
        if (have_prev_ && (p.sc != r.sc || p.rect.extent.width != r.rect.extent.width ||
                           p.rect.extent.height != r.rect.extent.height)) gen_++;
    }
    prev_[0] = out[0]; prev_[1] = out[1];
    have_prev_ = true;
    return true;
}

namespace {
// DXGI format families the core can copy between: {typeless, unorm, srgb-or-float}. 0 where a family has no member.
struct Family { int64_t typeless, unorm, srgb; };
constexpr Family kFamilies[] = {
    {27, 28, 29},    // R8G8B8A8 TYPELESS / UNORM / UNORM_SRGB
    {90, 87, 91},    // B8G8R8A8 TYPELESS / UNORM / UNORM_SRGB
    {9, 0, 10},      // R16G16B16A16 TYPELESS / - / FLOAT
};
bool offered_has(const int64_t* offered, uint32_t n, int64_t f) {
    if (!f) return false;
    for (uint32_t i = 0; i < n; i++) if (offered[i] == f) return true;
    return false;
}
}  // namespace

int64_t pick_format(int64_t app_fmt, const int64_t* offered, uint32_t n) {
    if (!offered || !n) return 0;
    for (const Family& f : kFamilies) {
        if (app_fmt != f.typeless && app_fmt != f.unorm && app_fmt != f.srgb) continue;
        if (app_fmt != f.typeless && offered_has(offered, n, app_fmt)) return app_fmt;
        if (offered_has(offered, n, f.srgb)) return f.srgb;     // a typeless game image is read as sRGB first
        if (offered_has(offered, n, f.unorm)) return f.unorm;
        return 0;
    }
    return 0;
}

}  // namespace fb
