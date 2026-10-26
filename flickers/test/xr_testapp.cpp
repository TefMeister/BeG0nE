// xr_testapp.cpp -- a minimal D3D12 OpenXR program for testing Flickers BeG0nE with no game and no headset (run it on
// the OpenXR Simulator). Each eye gets a moving picture: a dark background and bright bars sliding sideways, so frames
// differ the way a slow head turn does. Like REFramework, it DELAY-LOADS openxr_loader.dll (linked with
// /DELAYLOAD:openxr_loader.dll), so the plugin shell's slot patching is exercised the same way.
//
//   xr_testapp.exe [--seconds N] [--plugin <flickers_begone.dll>] [--late]
//     --plugin   act as REFramework: LoadLibrary the plugin and call its reframework_plugin_initialize with a
//                renderer_data that carries the D3D12 queue (only renderer_data is filled in)
//     --late     do that after the session and swapchains exist (the plugin then misses their creation)
//     --sessions N  run N sessions one after another, N seconds each (a headset reconnecting, VR restarted)
#define XR_USE_GRAPHICS_API_D3D12
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <reframework/API.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

namespace {

#define CHK(x) do { XrResult r_ = (x); if (XR_FAILED(r_)) { printf("FAILED %s = %d (line %d)\n", #x, (int)r_, __LINE__); exit(2); } } while (0)
#define HCHK(x) do { HRESULT h_ = (x); if (FAILED(h_)) { printf("FAILED %s = 0x%08lx (line %d)\n", #x, (unsigned long)h_, __LINE__); exit(3); } } while (0)

constexpr int kBars = 4;                 // bright bars per eye
constexpr int kBarW = 60;                // bar width, pixels
constexpr int kBarSpeedPx = 3;           // pixels a bar moves per frame
const float kBack[4] = {0.08f, 0.06f, 0.05f, 1.0f};
const float kBar[4] = {0.75f, 0.62f, 0.40f, 1.0f};

struct Eye {
    XrSwapchain sc = XR_NULL_HANDLE;
    uint32_t w = 0, h = 0;
    std::vector<ID3D12Resource*> images;
    UINT rtv0 = 0;
};

ID3D12Device* g_dev = nullptr;
ID3D12CommandQueue* g_queue = nullptr;
ID3D12DescriptorHeap* g_rtv = nullptr;
UINT g_rtv_step = 0;
ID3D12CommandAllocator* g_alloc = nullptr;
ID3D12GraphicsCommandList* g_list = nullptr;
ID3D12Fence* g_fence = nullptr;
UINT64 g_fence_v = 0;
HANDLE g_fence_ev = nullptr;

void gpu_wait() {
    g_queue->Signal(g_fence, ++g_fence_v);
    if (g_fence->GetCompletedValue() < g_fence_v) {
        g_fence->SetEventOnCompletion(g_fence_v, g_fence_ev);
        WaitForSingleObject(g_fence_ev, 2000);
    }
}

void make_device(XrInstance inst, XrSystemId sys) {
    PFN_xrGetD3D12GraphicsRequirementsKHR req_fn = nullptr;
    CHK(xrGetInstanceProcAddr(inst, "xrGetD3D12GraphicsRequirementsKHR", (PFN_xrVoidFunction*)&req_fn));
    XrGraphicsRequirementsD3D12KHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR};
    CHK(req_fn(inst, sys, &req));
    IDXGIFactory4* fac = nullptr;
    HCHK(CreateDXGIFactory1(IID_PPV_ARGS(&fac)));
    IDXGIAdapter1* ad = nullptr;
    for (UINT i = 0; fac->EnumAdapters1(i, &ad) != DXGI_ERROR_NOT_FOUND; i++) {
        DXGI_ADAPTER_DESC1 d;
        ad->GetDesc1(&d);
        if (memcmp(&d.AdapterLuid, &req.adapterLuid, sizeof d.AdapterLuid) == 0) break;
        ad->Release();
        ad = nullptr;
    }
    HCHK(D3D12CreateDevice(ad, req.minFeatureLevel, IID_PPV_ARGS(&g_dev)));
    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    HCHK(g_dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&g_queue)));
    HCHK(g_dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_alloc)));
    HCHK(g_dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_alloc, nullptr, IID_PPV_ARGS(&g_list)));
    g_list->Close();
    HCHK(g_dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)));
    g_fence_ev = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    if (ad) ad->Release();
    fac->Release();
}

void load_plugin(const char* path) {
    HMODULE m = LoadLibraryA(path);
    if (!m) { printf("plugin: could not load %s (%lu)\n", path, GetLastError()); exit(4); }
    using InitFn = bool (*)(const REFrameworkPluginInitializeParam*);
    auto init = (InitFn)GetProcAddress(m, "reframework_plugin_initialize");
    if (!init) { printf("plugin: no reframework_plugin_initialize\n"); exit(4); }
    static REFrameworkRendererData rd{};
    rd.renderer_type = REFRAMEWORK_RENDERER_D3D12;
    rd.device = g_dev;
    rd.command_queue = g_queue;
    static REFrameworkPluginVersion ver{REFRAMEWORK_PLUGIN_VERSION_MAJOR, REFRAMEWORK_PLUGIN_VERSION_MINOR, REFRAMEWORK_PLUGIN_VERSION_PATCH, 0};
    static REFrameworkPluginInitializeParam p{};
    p.version = &ver;
    p.renderer_data = &rd;
    printf("plugin: initialize -> %d\n", (int)init(&p));
}

// OpenXR hands D3D12 colour images over in RENDER_TARGET state and wants them back in it, so no barriers here.
void draw_eye(const Eye& e, uint32_t index, uint64_t frame, int eye) {
    D3D12_CPU_DESCRIPTOR_HANDLE h = g_rtv->GetCPUDescriptorHandleForHeapStart();
    h.ptr += (SIZE_T)(e.rtv0 + index) * g_rtv_step;
    g_list->ClearRenderTargetView(h, kBack, 0, nullptr);
    D3D12_RECT bars[kBars];
    for (int i = 0; i < kBars; i++) {
        const int span = (int)e.w + kBarW;
        const int x = (int)((frame * kBarSpeedPx + (uint64_t)i * span / kBars + (uint64_t)eye * 8) % (uint64_t)span) - kBarW;
        bars[i] = {x < 0 ? 0 : x, (LONG)(e.h / 6 * (i + 1) / 2), x + kBarW, (LONG)(e.h - e.h / 8)};
    }
    g_list->ClearRenderTargetView(h, kBar, kBars, bars);
}

uint64_t run_session(XrInstance inst, XrSystemId sys, int seconds, const char* late_plugin);

}  // namespace

int main(int argc, char** argv) {
    int seconds = 10, sessions = 1;
    const char* plugin = nullptr;
    bool late = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--seconds") && i + 1 < argc) seconds = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--plugin") && i + 1 < argc) plugin = argv[++i];
        else if (!strcmp(argv[i], "--late")) late = true;
        else if (!strcmp(argv[i], "--sessions") && i + 1 < argc) sessions = atoi(argv[++i]);
    }
    setvbuf(stdout, nullptr, _IONBF, 0);

    const char* exts[] = {XR_KHR_D3D12_ENABLE_EXTENSION_NAME};
    XrInstanceCreateInfo ici{XR_TYPE_INSTANCE_CREATE_INFO};
    strcpy_s(ici.applicationInfo.applicationName, "flickers_testapp");
    ici.applicationInfo.apiVersion = XR_API_VERSION_1_0;
    ici.enabledExtensionCount = 1;
    ici.enabledExtensionNames = exts;
    XrInstance inst;
    CHK(xrCreateInstance(&ici, &inst));
    XrSystemGetInfo sgi{XR_TYPE_SYSTEM_GET_INFO};
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId sys;
    CHK(xrGetSystem(inst, &sgi, &sys));
    make_device(inst, sys);
    if (plugin && !late) load_plugin(plugin);
    uint64_t frames = 0;
    for (int run = 0; run < sessions; run++) {
        if (sessions > 1) printf("session %d\n", run + 1);
        frames += run_session(inst, sys, seconds, plugin && late && run == 0 ? plugin : nullptr);
    }
    printf("frames: %llu\n", (unsigned long long)frames);
    xrDestroyInstance(inst);
    return 0;
}

namespace {

// One session: swapchains, `seconds` of moving bars, then a clean exit. Returns the frames drawn.
uint64_t run_session(XrInstance inst, XrSystemId sys, int seconds, const char* late_plugin) {
    XrGraphicsBindingD3D12KHR bind{XR_TYPE_GRAPHICS_BINDING_D3D12_KHR};
    bind.device = g_dev;
    bind.queue = g_queue;
    XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};
    sci.next = &bind;
    sci.systemId = sys;
    XrSession ses;
    CHK(xrCreateSession(inst, &sci, &ses));
    XrReferenceSpaceCreateInfo rsi{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    rsi.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    rsi.poseInReferenceSpace.orientation.w = 1.0f;
    XrSpace space;
    CHK(xrCreateReferenceSpace(ses, &rsi, &space));

    uint32_t nv = 0;
    XrViewConfigurationView vcv[2]{{XR_TYPE_VIEW_CONFIGURATION_VIEW}, {XR_TYPE_VIEW_CONFIGURATION_VIEW}};
    CHK(xrEnumerateViewConfigurationViews(inst, sys, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &nv, vcv));
    if (!g_rtv) {
        D3D12_DESCRIPTOR_HEAP_DESC hd{};
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        hd.NumDescriptors = 16;
        HCHK(g_dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&g_rtv)));
        g_rtv_step = g_dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }
    Eye eyes[2];
    UINT next_rtv = 0;
    for (int e = 0; e < 2; e++) {
        XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        ci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        ci.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        ci.sampleCount = 1;
        ci.width = vcv[e].recommendedImageRectWidth;
        ci.height = vcv[e].recommendedImageRectHeight;
        ci.faceCount = 1; ci.arraySize = 1; ci.mipCount = 1;
        CHK(xrCreateSwapchain(ses, &ci, &eyes[e].sc));
        eyes[e].w = ci.width; eyes[e].h = ci.height;
        uint32_t n = 0;
        CHK(xrEnumerateSwapchainImages(eyes[e].sc, 0, &n, nullptr));
        std::vector<XrSwapchainImageD3D12KHR> imgs(n, {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
        CHK(xrEnumerateSwapchainImages(eyes[e].sc, n, &n, (XrSwapchainImageBaseHeader*)imgs.data()));
        eyes[e].rtv0 = next_rtv;
        for (uint32_t i = 0; i < n; i++) {
            eyes[e].images.push_back(imgs[i].texture);
            D3D12_RENDER_TARGET_VIEW_DESC rv{};
            rv.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            rv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            D3D12_CPU_DESCRIPTOR_HANDLE h = g_rtv->GetCPUDescriptorHandleForHeapStart();
            h.ptr += (SIZE_T)(next_rtv++) * g_rtv_step;
            g_dev->CreateRenderTargetView(imgs[i].texture, &rv, h);
        }
    }
    printf("eyes: %ux%u, %zu images each\n", eyes[0].w, eyes[0].h, eyes[0].images.size());
    if (late_plugin) load_plugin(late_plugin);

    bool running = false, quit = false, exit_asked = false;
    const ULONGLONG start = GetTickCount64();
    uint64_t frame = 0;
    while (!quit) {
        XrEventDataBuffer ev{XR_TYPE_EVENT_DATA_BUFFER};
        while (xrPollEvent(inst, &ev) == XR_SUCCESS) {
            if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                const auto* sc = (const XrEventDataSessionStateChanged*)&ev;
                if (sc->state == XR_SESSION_STATE_READY) {
                    XrSessionBeginInfo bi{XR_TYPE_SESSION_BEGIN_INFO};
                    bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    CHK(xrBeginSession(ses, &bi));
                    running = true;
                } else if (sc->state == XR_SESSION_STATE_STOPPING) {
                    xrEndSession(ses);
                    running = false;
                } else if (sc->state == XR_SESSION_STATE_EXITING || sc->state == XR_SESSION_STATE_LOSS_PENDING) {
                    quit = true;
                }
            }
            ev = {XR_TYPE_EVENT_DATA_BUFFER};
        }
        if (!exit_asked && GetTickCount64() - start > (ULONGLONG)seconds * 1000) {
            exit_asked = true;
            if (running) xrRequestExitSession(ses); else quit = true;
        }
        if (GetTickCount64() - start > (ULONGLONG)(seconds + 10) * 1000) quit = true;    // a runtime that never says EXITING
        if (!running) { Sleep(10); continue; }

        XrFrameState fs{XR_TYPE_FRAME_STATE};
        CHK(xrWaitFrame(ses, nullptr, &fs));
        CHK(xrBeginFrame(ses, nullptr));
        XrViewLocateInfo li{XR_TYPE_VIEW_LOCATE_INFO};
        li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        li.displayTime = fs.predictedDisplayTime;
        li.space = space;
        XrViewState vs{XR_TYPE_VIEW_STATE};
        XrView views[2]{{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
        uint32_t vn = 0;
        CHK(xrLocateViews(ses, &li, &vs, 2, &vn, views));
        XrCompositionLayerProjectionView pv[2]{};
        g_alloc->Reset();
        g_list->Reset(g_alloc, nullptr);
        uint32_t idx[2];
        for (int e = 0; e < 2; e++) {
            CHK(xrAcquireSwapchainImage(eyes[e].sc, nullptr, &idx[e]));
            XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            wi.timeout = XR_INFINITE_DURATION;
            CHK(xrWaitSwapchainImage(eyes[e].sc, &wi));
            draw_eye(eyes[e], idx[e], frame, e);
        }
        g_list->Close();
        ID3D12CommandList* lists[] = {g_list};
        g_queue->ExecuteCommandLists(1, lists);
        gpu_wait();
        for (int e = 0; e < 2; e++) {
            CHK(xrReleaseSwapchainImage(eyes[e].sc, nullptr));
            pv[e] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
            pv[e].pose = views[e].pose;
            pv[e].fov = views[e].fov;
            pv[e].subImage.swapchain = eyes[e].sc;
            pv[e].subImage.imageRect = {{0, 0}, {(int32_t)eyes[e].w, (int32_t)eyes[e].h}};
        }
        XrCompositionLayerProjection proj{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        proj.space = space;
        proj.viewCount = 2;
        proj.views = pv;
        const XrCompositionLayerBaseHeader* layers[] = {(const XrCompositionLayerBaseHeader*)&proj};
        XrFrameEndInfo fe{XR_TYPE_FRAME_END_INFO};
        fe.displayTime = fs.predictedDisplayTime;
        fe.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        fe.layerCount = fs.shouldRender ? 1 : 0;
        fe.layers = layers;
        CHK(xrEndFrame(ses, &fe));
        frame++;
    }
    gpu_wait();
    for (auto& e : eyes) xrDestroySwapchain(e.sc);
    xrDestroySpace(space);
    xrDestroySession(ses);
    return frame;
}

}  // namespace
