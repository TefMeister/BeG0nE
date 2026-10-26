// fb_plugin.cpp -- the REFramework plugin shell of Flickers BeG0nE: the RE4 fix, one file in reframework/plugins/.
// REFramework delay-loads openxr_loader.dll; at initialize the plugin rewrites the nine slots it calls the loader's
// swapchain and frame functions through (in every loaded module that imports them), so those calls reach the core,
// which passes them on to the loader. The game's D3D12 queue comes from REFramework's renderer data. Files live in
// reframework/data/flickers/ beside the game's exe; nothing happens unless flickers_begone.txt there says on >= 1.
#include "../core/fb_core.h"
#include "../core/fb_settings.h"
#include "fb_iat.h"

#include <reframework/API.h>
#include <psapi.h>
#include <windows.h>
#include <string>
#include <vector>

namespace {

const REFrameworkRendererData* g_rd = nullptr;
bool g_have_queue = false;

std::string exe_dir() {
    char p[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, p, MAX_PATH);
    std::string s(p);
    return s.substr(0, s.find_last_of("\\/"));
}

std::string read_text(const std::string& path) {
    std::string t;
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") == 0 && f) {
        char b[4096];
        size_t n;
        while ((n = fread(b, 1, sizeof b, f)) > 0) t.append(b, n);
        fclose(f);
    }
    return t;
}

void take_queue() {
    if (g_have_queue || !g_rd || g_rd->renderer_type != REFRAMEWORK_RENDERER_D3D12 || !g_rd->command_queue) return;
    g_have_queue = true;
    fb::core_set_queue((ID3D12CommandQueue*)g_rd->command_queue, "REFramework");
}

void on_present() { take_queue(); }    // REFramework's renderer data can be filled in after initialize

void* ours(const char* name) {
    struct Map { const char* name; void* fn; };
    static const Map m[] = {
        {"xrEndFrame", (void*)fb::xrEndFrame}, {"xrCreateSwapchain", (void*)fb::xrCreateSwapchain},
        {"xrDestroySwapchain", (void*)fb::xrDestroySwapchain}, {"xrEnumerateSwapchainImages", (void*)fb::xrEnumerateSwapchainImages},
        {"xrAcquireSwapchainImage", (void*)fb::xrAcquireSwapchainImage}, {"xrWaitSwapchainImage", (void*)fb::xrWaitSwapchainImage},
        {"xrReleaseSwapchainImage", (void*)fb::xrReleaseSwapchainImage}, {"xrCreateSession", (void*)fb::xrCreateSession},
        {"xrDestroySession", (void*)fb::xrDestroySession},
    };
    for (const auto& x : m) if (strcmp(x.name, name) == 0) return x.fn;
    return nullptr;
}

// Rewrites the hooked slots in every loaded module (not this one) that imports them from openxr_loader.dll.
int patch_all() {
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&patch_all, &self);
    std::vector<HMODULE> mods(1024);
    DWORD need = 0;
    if (!EnumProcessModules(GetCurrentProcess(), mods.data(), (DWORD)(mods.size() * sizeof(HMODULE)), &need)) return 0;
    mods.resize(need / sizeof(HMODULE));
    int total = 0;
    for (HMODULE m : mods) {
        if (m == self) continue;
        MODULEINFO mi{};
        if (!GetModuleInformation(GetCurrentProcess(), m, &mi, sizeof mi)) continue;
        fb::ImportSlot slots[fb::kHookedCount];
        const int n = fb::find_import_slots((const uint8_t*)mi.lpBaseOfDll, mi.SizeOfImage, true, "openxr_loader.dll",
                                            fb::kHookedNames, fb::kHookedCount, slots);
        if (!n) continue;
        int done = 0;
        for (int i = 0; i < n; i++)
            if (fb::patch_slot((uint8_t*)mi.lpBaseOfDll, slots[i].slot_rva, ours(slots[i].name), nullptr)) done++;
        char name[MAX_PATH] = {};
        GetModuleBaseNameA(GetCurrentProcess(), m, name, MAX_PATH);
        fb::log("patched %s imports: %d of %d (%s)", name, done, n, slots[0].delay ? "delay-loaded" : "imported");
        total += done;
    }
    return total;
}

}  // namespace

extern "C" __declspec(dllexport) void reframework_plugin_required_version(REFrameworkPluginVersion* v) {
    v->major = REFRAMEWORK_PLUGIN_VERSION_MAJOR;
    v->minor = REFRAMEWORK_PLUGIN_VERSION_MINOR;
    v->patch = REFRAMEWORK_PLUGIN_VERSION_PATCH;
}

extern "C" __declspec(dllexport) bool reframework_plugin_initialize(const REFrameworkPluginInitializeParam* param) {
    const std::string base = exe_dir();
    const std::string dir = base + "\\reframework\\data\\flickers";
    fb::Settings s;
    fb::parse_settings(read_text(dir + "\\flickers_begone.txt").c_str(), s);
    char exe[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    if (s.on < 1 || !fb::exe_matches(s, exe)) return true;      // switched off: touch nothing, write nothing

    CreateDirectoryA((base + "\\reframework").c_str(), nullptr);
    CreateDirectoryA((base + "\\reframework\\data").c_str(), nullptr);
    HMODULE loader = LoadLibraryA("openxr_loader.dll");
    fb::Next n;
    if (loader) {
        n.EndFrame = (PFN_xrEndFrame)GetProcAddress(loader, "xrEndFrame");
        n.CreateSwapchain = (PFN_xrCreateSwapchain)GetProcAddress(loader, "xrCreateSwapchain");
        n.DestroySwapchain = (PFN_xrDestroySwapchain)GetProcAddress(loader, "xrDestroySwapchain");
        n.EnumerateSwapchainImages = (PFN_xrEnumerateSwapchainImages)GetProcAddress(loader, "xrEnumerateSwapchainImages");
        n.EnumerateSwapchainFormats = (PFN_xrEnumerateSwapchainFormats)GetProcAddress(loader, "xrEnumerateSwapchainFormats");
        n.AcquireSwapchainImage = (PFN_xrAcquireSwapchainImage)GetProcAddress(loader, "xrAcquireSwapchainImage");
        n.WaitSwapchainImage = (PFN_xrWaitSwapchainImage)GetProcAddress(loader, "xrWaitSwapchainImage");
        n.ReleaseSwapchainImage = (PFN_xrReleaseSwapchainImage)GetProcAddress(loader, "xrReleaseSwapchainImage");
        n.CreateSession = (PFN_xrCreateSession)GetProcAddress(loader, "xrCreateSession");
        n.DestroySession = (PFN_xrDestroySession)GetProcAddress(loader, "xrDestroySession");
    }
    fb::core_init(n, dir.c_str(), "REFramework plugin");
    if (!loader || !n.EndFrame || !n.EnumerateSwapchainFormats) {
        fb::log("no usable openxr_loader.dll: REFramework is not running VR through OpenXR here; doing nothing");
        return true;
    }
    if (!patch_all()) {
        fb::log("no module imports the OpenXR swapchain functions: doing nothing (a new REFramework build?)");
        return true;
    }
    if (param) {
        g_rd = param->renderer_data;
        take_queue();
        if (!g_have_queue && param->functions && param->functions->on_present) param->functions->on_present(on_present);
        if (!g_have_queue) fb::log("REFramework has no D3D12 queue yet: waiting for it");
    }
    return true;
}
