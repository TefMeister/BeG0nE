// fb_gpu.h -- the D3D12 side of the core: the core's own swapchain per eye (the seam every picture change goes
// through), the fake flicker, the catcher ring of each eye's last pictures, and the dump.
#pragma once
#include <string>
#include "fb_core.h"
#include "fb_settings.h"
#include "fb_views.h"

namespace fb {

class Gpu {
public:
    // Called from xrEndFrame for a stereo frame. May rewrite `out` so the eyes come from the core's own swapchains;
    // anything it cannot do (no device yet, a format with no match) leaves `out` untouched, and says so once in the log.
    void frame(const Settings& s, const char* dir, XrSession session, ID3D12CommandQueue* queue, uint32_t generation,
               const EyeRecord eyes[2], XrFrameEndInfo& out);
    void release();
    std::string status_line() const;      // appended to the once-a-second log line ("" when idle)
    ~Gpu();
private:
    struct Impl;
    Impl* p_ = nullptr;
};

}  // namespace fb
