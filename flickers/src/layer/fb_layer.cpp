// fb_layer.cpp -- the OpenXR API layer shell of Flickers BeG0nE: for modders and any OpenXR game, and the shell the dev
// PC tests with. Negotiates with the loader (the VR Bench pattern), fetches the next functions once the instance
// exists, and hands the game's calls to the core. Does nothing at all unless flickers_begone.txt beside the DLL names
// this game's exe (or names none) and says on >= 1.
#include "../core/fb_core.h"
#include "../core/fb_settings.h"

#include <openxr/openxr_loader_negotiation.h>
#include <windows.h>
#include <cstring>
#include <string>

namespace {

PFN_xrGetInstanceProcAddr g_next_gipa = nullptr;
bool g_enabled = false;

std::string dll_dir() {
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)&dll_dir, &self);
    char p[MAX_PATH] = {};
    GetModuleFileNameA(self, p, MAX_PATH);
    std::string s(p);
    const size_t cut = s.find_last_of("\\/");
    return cut == std::string::npos ? std::string(".") : s.substr(0, cut);
}

bool enabled_for_this_exe(const std::string& dir) {
    std::string text;
    FILE* f = nullptr;
    if (fopen_s(&f, (dir + "\\flickers_begone.txt").c_str(), "rb") == 0 && f) {
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
        fclose(f);
    }
    fb::Settings s;
    fb::parse_settings(text.c_str(), s);
    char exe[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    return s.on >= 1 && fb::exe_matches(s, exe);
}

template <typename T> void get(XrInstance inst, const char* name, T& slot) {
    PFN_xrVoidFunction f = nullptr;
    if (XR_SUCCEEDED(g_next_gipa(inst, name, &f)) && f) slot = reinterpret_cast<T>(f);
}

XRAPI_ATTR XrResult XRAPI_CALL fbl_xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function) {
    const XrResult r = g_next_gipa(instance, name, function);
    if (!g_enabled || XR_FAILED(r) || !function || !*function || !name) return r;
    struct Hook { const char* name; PFN_xrVoidFunction ours; };
    static const Hook hooks[] = {
        {"xrEndFrame", (PFN_xrVoidFunction)fb::xrEndFrame},
        {"xrCreateSwapchain", (PFN_xrVoidFunction)fb::xrCreateSwapchain},
        {"xrDestroySwapchain", (PFN_xrVoidFunction)fb::xrDestroySwapchain},
        {"xrEnumerateSwapchainImages", (PFN_xrVoidFunction)fb::xrEnumerateSwapchainImages},
        {"xrAcquireSwapchainImage", (PFN_xrVoidFunction)fb::xrAcquireSwapchainImage},
        {"xrWaitSwapchainImage", (PFN_xrVoidFunction)fb::xrWaitSwapchainImage},
        {"xrReleaseSwapchainImage", (PFN_xrVoidFunction)fb::xrReleaseSwapchainImage},
        {"xrCreateSession", (PFN_xrVoidFunction)fb::xrCreateSession},
        {"xrDestroySession", (PFN_xrVoidFunction)fb::xrDestroySession},
    };
    for (const auto& h : hooks)
        if (std::strcmp(name, h.name) == 0) { *function = h.ours; break; }
    return r;
}

XRAPI_ATTR XrResult XRAPI_CALL fbl_xrCreateApiLayerInstance(const XrInstanceCreateInfo* info, const XrApiLayerCreateInfo* layer_info, XrInstance* instance) {
    if (!layer_info || !layer_info->nextInfo) return XR_ERROR_INITIALIZATION_FAILED;
    g_next_gipa = layer_info->nextInfo->nextGetInstanceProcAddr;
    XrApiLayerCreateInfo next_info = *layer_info;   // the next layer down gets the chain one step further on
    next_info.nextInfo = layer_info->nextInfo->next;
    const XrResult r = layer_info->nextInfo->nextCreateApiLayerInstance(info, &next_info, instance);
    if (XR_FAILED(r) || !instance) return r;
    const std::string dir = dll_dir();
    g_enabled = enabled_for_this_exe(dir);
    if (!g_enabled) return r;
    fb::Next n;
    get(*instance, "xrEndFrame", n.EndFrame);
    get(*instance, "xrCreateSwapchain", n.CreateSwapchain);
    get(*instance, "xrDestroySwapchain", n.DestroySwapchain);
    get(*instance, "xrEnumerateSwapchainImages", n.EnumerateSwapchainImages);
    get(*instance, "xrEnumerateSwapchainFormats", n.EnumerateSwapchainFormats);
    get(*instance, "xrAcquireSwapchainImage", n.AcquireSwapchainImage);
    get(*instance, "xrWaitSwapchainImage", n.WaitSwapchainImage);
    get(*instance, "xrReleaseSwapchainImage", n.ReleaseSwapchainImage);
    get(*instance, "xrCreateSession", n.CreateSession);
    get(*instance, "xrDestroySession", n.DestroySession);
    fb::core_init(n, dir.c_str(), "OpenXR layer");
    return r;
}

}  // namespace

extern "C" __declspec(dllexport) XrResult XRAPI_CALL xrNegotiateLoaderApiLayerInterface(const XrNegotiateLoaderInfo* loader_info, const char* /*layer_name*/, XrNegotiateApiLayerRequest* request) {
    if (!loader_info || !request || loader_info->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO ||
        request->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_REQUEST ||
        loader_info->minInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loader_info->maxInterfaceVersion < XR_CURRENT_LOADER_API_LAYER_VERSION) {
        return XR_ERROR_INITIALIZATION_FAILED;
    }
    request->layerInterfaceVersion = XR_CURRENT_LOADER_API_LAYER_VERSION;
    request->layerApiVersion = loader_info->maxApiVersion < XR_CURRENT_API_VERSION ? loader_info->maxApiVersion : XR_CURRENT_API_VERSION;
    request->getInstanceProcAddr = fbl_xrGetInstanceProcAddr;
    request->createApiLayerInstance = fbl_xrCreateApiLayerInstance;
    return XR_SUCCESS;
}
