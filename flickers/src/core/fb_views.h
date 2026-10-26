// fb_views.h -- eye bookkeeping (pure CPU; tested by test/views_test.cpp and test/formats_test.cpp).
// Follows the game's swapchains (created, destroyed, which image it acquired last) and, at xrEndFrame, turns the first
// stereo projection layer into one EyeRecord per eye: which swapchain, image, part, array slice, pose and field of view.
// Keyed by view index, never by swapchain alone: one swapchain can carry both eyes (two rects or two slices).
#pragma once
#include <stdint.h>
#include <deque>
#include <map>
#define XR_NO_PROTOTYPES
#include <openxr/openxr.h>

namespace fb {

struct EyeRecord {
    XrSwapchain sc = XR_NULL_HANDLE;
    uint32_t image = 0;                 // index of the image the game released last on that swapchain
    XrRect2Di rect{};                   // the part of the image that is this eye
    uint32_t slice = 0;                 // array slice
    XrPosef pose{};                     // the pose the eye was drawn for
    XrFovf fov{};
    uint64_t frame = 0;                 // the core's frame counter
    bool same_as_last = false;          // same image, part, slice and pose as this eye's previous frame (a repeat)
    uint32_t w = 0, h = 0;              // the swapchain's size and format, 0 when its creation was not seen
    int64_t fmt = 0;
};

class Views {
public:
    void on_create_swapchain(XrSwapchain sc, int64_t fmt, uint32_t w, uint32_t h, uint32_t array_size);
    void on_destroy_swapchain(XrSwapchain sc);
    void on_acquire(XrSwapchain sc, uint32_t index);
    // The game finished drawing its oldest held image. The image submitted at xrEndFrame is the last one released, not
    // the last one acquired: a game can already hold the next frame's image when it ends this one.
    void on_release(XrSwapchain sc);
    // False = nothing for the core to do this frame (no stereo projection layer, or an eye on an unknown swapchain).
    bool on_end_frame(const XrFrameEndInfo& info, uint64_t frame, EyeRecord out[2]);
    // Changes whenever a swapchain the eyes use is destroyed or the eyes move to another swapchain or size; the GPU side
    // rebuilds its own swapchains and ring when it sees a new value (Review Focus 1).
    uint32_t generation() const { return gen_; }

private:
    struct Chain {
        int64_t fmt = 0;
        uint32_t w = 0, h = 0, layers = 1, last = 0;
        bool acquired = false;
        std::deque<uint32_t> held;      // acquired and not yet released, oldest first
        bool released = false;          // a release was seen: `last` is the last released image
    };
    std::map<XrSwapchain, Chain> chains_;
    EyeRecord prev_[2]{};
    bool have_prev_ = false;
    uint32_t gen_ = 1;
};

// The format the core's own swapchain uses for a game image of format `app_fmt` (DXGI values): the same format if the
// runtime offers it, else the offered sRGB / UNORM / float sibling in its family; 0 = none, pass through.
int64_t pick_format(int64_t app_fmt, const int64_t* offered, uint32_t n);
// The own swapchain's format: from the format the game created its swapchain with (sc_fmt) when that was seen, else
// from the image's own format. Runtimes back a UNORM swapchain with TYPELESS images, and reading those as sRGB would
// change the picture's brightness.
int64_t own_format(int64_t sc_fmt, int64_t image_fmt, const int64_t* offered, uint32_t n);

}  // namespace fb
