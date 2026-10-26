// fb_gpu.cpp -- see fb_gpu.h. (Step 1, task 3: pass-through only; task 4 fills it in.)
#include "fb_gpu.h"

namespace fb {

struct Gpu::Impl {};

void Gpu::frame(const Settings&, const char*, XrSession, ID3D12CommandQueue*, uint32_t, const EyeRecord*, XrFrameEndInfo&) {}
void Gpu::release() {}
std::string Gpu::status_line() const { return ""; }
Gpu::~Gpu() { delete p_; }

}  // namespace fb
