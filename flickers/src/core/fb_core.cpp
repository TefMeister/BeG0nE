// fb_core.cpp -- see fb_core.h. Step 1: pass everything through, follow the eyes, log; the GPU work (own swapchains,
// fake flicker, catcher ring, dump) hangs off xrEndFrame through fb_gpu.h.
#include "fb_core.h"
#include "fb_gpu.h"
#include "fb_settings.h"
#include "fb_views.h"

#include <stdarg.h>
#include <stdio.h>
#include <windows.h>
#include <mutex>
#include <string>

namespace fb {

namespace {
Next g_next;
std::string g_dir, g_shell;
std::mutex g_log_mx;
FILE* g_log = nullptr;
Settings g_set;
uint64_t g_set_read_ms = 0;
std::mutex g_mx;                          // guards the bookkeeping: OpenXR calls may come from more than one thread
Views g_views;
uint64_t g_frame = 0;
ID3D12CommandQueue* g_queue = nullptr;
XrSession g_session = XR_NULL_HANDLE;
Gpu g_gpu;

// once-a-second counters, printed as one line
struct Counts { uint32_t frames = 0, stereo = 0, passthrough = 0, repeats[2] = {0, 0}; } g_cnt;
uint64_t g_cnt_ms = 0;
bool g_said_first_stereo = false, g_said_no_queue = false;

std::string read_file(const std::string& path) {
    std::string s;
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) return s;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n);
    fclose(f);
    return s;
}

void reload_settings(uint64_t now) {
    if (g_set_read_ms && now - g_set_read_ms < (uint64_t)kControlPollMs) return;
    g_set_read_ms = now;
    const std::string text = read_file(g_dir + "\\flickers_begone.txt");
    Settings s;
    parse_settings(text.c_str(), s);
    if (s.on != g_set.on || s.fake != g_set.fake) log("settings: on=%d fake=%d ring_frames=%d half=%d", s.on, s.fake, s.ring_frames, s.ring_half);
    g_set = s;
}

void per_second(uint64_t now) {
    if (!g_cnt_ms) g_cnt_ms = now;
    if (now - g_cnt_ms < 1000) return;
    log("1s: frames=%u stereo=%u passthrough=%u repeats L=%u R=%u%s", g_cnt.frames, g_cnt.stereo, g_cnt.passthrough,
        g_cnt.repeats[0], g_cnt.repeats[1], g_gpu.status_line().c_str());
    g_cnt = Counts{};
    g_cnt_ms = now;
}
}  // namespace

void log(const char* fmt, ...) {
    std::lock_guard<std::mutex> lk(g_log_mx);
    if (!g_log) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(g_log, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list a;
    va_start(a, fmt);
    vfprintf(g_log, fmt, a);
    va_end(a);
    fputc('\n', g_log);
    fflush(g_log);
}

void core_init(const Next& next, const char* data_dir, const char* shell) {
    g_next = next;
    g_dir = data_dir;
    g_shell = shell;
    CreateDirectoryA(g_dir.c_str(), nullptr);
    {
        std::lock_guard<std::mutex> lk(g_log_mx);
        if (!g_log) fopen_s(&g_log, (g_dir + "\\flickers_begone.log").c_str(), "w");
    }
    char exe[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    const char* base = exe;
    for (const char* p = exe; *p; p++) if (*p == '\\' || *p == '/') base = p + 1;
    log("Flickers BeG0nE core, %s shell, in %s", shell, base);
    log("next: EndFrame=%d CreateSwapchain=%d EnumerateSwapchainFormats=%d Acquire=%d Release=%d CreateSession=%d",
        next.EndFrame != nullptr, next.CreateSwapchain != nullptr, next.EnumerateSwapchainFormats != nullptr,
        next.AcquireSwapchainImage != nullptr, next.ReleaseSwapchainImage != nullptr, next.CreateSession != nullptr);
    reload_settings(GetTickCount64());
}

void core_set_queue(ID3D12CommandQueue* queue, const char* where) {
    std::lock_guard<std::mutex> lk(g_mx);
    if (!queue || queue == g_queue) return;
    g_queue = queue;
    log("queue from %s", where);
}

const Next& core_next() { return g_next; }

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSession(XrInstance instance, const XrSessionCreateInfo* info, XrSession* session) {
    const XrResult r = g_next.CreateSession(instance, info, session);
    if (XR_FAILED(r) || !info) return r;
    const char* api = "not D3D12";
    for (auto* b = (const XrBaseInStructure*)info->next; b; b = b->next) {
        if (b->type == XR_TYPE_GRAPHICS_BINDING_D3D12_KHR) {
            api = "D3D12";
            core_set_queue(((const XrGraphicsBindingD3D12KHR*)b)->queue, "xrCreateSession");
        }
    }
    {
        std::lock_guard<std::mutex> lk(g_mx);
        g_session = *session;
    }
    log("session %s", api);
    return r;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySession(XrSession session) {
    {
        std::lock_guard<std::mutex> lk(g_mx);
        g_gpu.release();
        if (session == g_session) g_session = XR_NULL_HANDLE;
    }
    log("session destroyed");
    return g_next.DestroySession(session);
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSwapchain(XrSession s, const XrSwapchainCreateInfo* info, XrSwapchain* sc) {
    const XrResult r = g_next.CreateSwapchain(s, info, sc);
    if (XR_SUCCEEDED(r) && info) {
        std::lock_guard<std::mutex> lk(g_mx);
        g_views.on_create_swapchain(*sc, info->format, info->width, info->height, info->arraySize);
        log("game swapchain %ux%u fmt %lld array %u", info->width, info->height, (long long)info->format, info->arraySize);
    }
    return r;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySwapchain(XrSwapchain sc) {
    {
        std::lock_guard<std::mutex> lk(g_mx);
        g_views.on_destroy_swapchain(sc);
    }
    return g_next.DestroySwapchain(sc);
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateSwapchainImages(XrSwapchain sc, uint32_t cap, uint32_t* n, XrSwapchainImageBaseHeader* imgs) {
    return g_next.EnumerateSwapchainImages(sc, cap, n, imgs);
}

XRAPI_ATTR XrResult XRAPI_CALL xrAcquireSwapchainImage(XrSwapchain sc, const XrSwapchainImageAcquireInfo* info, uint32_t* index) {
    const XrResult r = g_next.AcquireSwapchainImage(sc, info, index);
    if (XR_SUCCEEDED(r) && index) {
        std::lock_guard<std::mutex> lk(g_mx);
        g_views.on_acquire(sc, *index);
    }
    return r;
}

XRAPI_ATTR XrResult XRAPI_CALL xrWaitSwapchainImage(XrSwapchain sc, const XrSwapchainImageWaitInfo* info) {
    return g_next.WaitSwapchainImage(sc, info);
}

XRAPI_ATTR XrResult XRAPI_CALL xrReleaseSwapchainImage(XrSwapchain sc, const XrSwapchainImageReleaseInfo* info) {
    const XrResult r = g_next.ReleaseSwapchainImage(sc, info);
    if (XR_SUCCEEDED(r)) {
        std::lock_guard<std::mutex> lk(g_mx);
        g_views.on_release(sc);
    }
    return r;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEndFrame(XrSession session, const XrFrameEndInfo* info) {
    if (!info) return g_next.EndFrame(session, info);
    std::lock_guard<std::mutex> lk(g_mx);
    const uint64_t now = GetTickCount64();
    reload_settings(now);
    g_frame++;
    g_cnt.frames++;
    if (g_session == XR_NULL_HANDLE) g_session = session;    // the plugin may have missed xrCreateSession
    EyeRecord eyes[2];
    XrFrameEndInfo out = *info;
    const bool stereo = g_set.on > 0 && g_views.on_end_frame(*info, g_frame, eyes);
    if (stereo) {
        g_cnt.stereo++;
        for (int e = 0; e < 2; e++) if (eyes[e].same_as_last) g_cnt.repeats[e]++;
        if (!g_said_first_stereo) {
            g_said_first_stereo = true;
            log("views: eye0 sc=%p img=%u rect=%d,%d %dx%d slice=%u | eye1 sc=%p img=%u rect=%d,%d %dx%d slice=%u",
                (void*)eyes[0].sc, eyes[0].image, eyes[0].rect.offset.x, eyes[0].rect.offset.y, eyes[0].rect.extent.width,
                eyes[0].rect.extent.height, eyes[0].slice, (void*)eyes[1].sc, eyes[1].image, eyes[1].rect.offset.x,
                eyes[1].rect.offset.y, eyes[1].rect.extent.width, eyes[1].rect.extent.height, eyes[1].slice);
        }
        if (g_set.on < 2) {
            // on = 1: watch and log only; the game's images go to the headset untouched
        } else if (g_queue) {
            g_gpu.frame(g_set, g_dir.c_str(), session, g_queue, g_views.generation(), eyes, out);
        } else if (!g_said_no_queue) {
            g_said_no_queue = true;
            log("no queue yet: passing through (the plugin needs REFramework's queue, the layer xrCreateSession's)");
        }
    } else {
        g_cnt.passthrough++;
    }
    per_second(now);
    return g_next.EndFrame(session, &out);
}

}  // namespace fb
