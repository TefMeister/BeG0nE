// iat_test.cpp -- the import-slot finder, on real files: REFramework's dinput8.dll (path in the environment variable
// FB_REF_DLL; skipped with a note when it is not set) and our own test program (always; built before the tests).
// Both delay-load openxr_loader.dll, and the plugin hooks the same nine functions in both.
#include "check.h"
#include "../src/plugin/fb_iat.h"
#include "../src/plugin/fb_iat.cpp"
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

static std::vector<uint8_t> read_all(const char* path) {
    std::vector<uint8_t> v;
    FILE* f = nullptr;
    if (fopen_s(&f, path, "rb") != 0 || !f) return v;
    fseek(f, 0, SEEK_END);
    v.resize((size_t)ftell(f));
    fseek(f, 0, SEEK_SET);
    fread(v.data(), 1, v.size(), f);
    fclose(f);
    return v;
}

static void check_file(const char* path, bool expect_delay) {
    const std::vector<uint8_t> img = read_all(path);
    CHECK(!img.empty());
    if (img.empty()) return;
    fb::ImportSlot slots[16];
    const int n = fb::find_import_slots(img.data(), img.size(), false, "openxr_loader.dll", fb::kHookedNames, fb::kHookedCount, slots);
    printf("  %s: %d of %d hooked names found\n", path, n, fb::kHookedCount);
    CHECK(n == fb::kHookedCount);
    bool all_delay = true, rvas = true;
    for (int i = 0; i < n; i++) {
        if (slots[i].delay != expect_delay) all_delay = false;
        if (slots[i].slot_rva == 0) rvas = false;
    }
    CHECK(all_delay);
    CHECK(rvas);
    // a dll the file does not import: nothing
    CHECK(fb::find_import_slots(img.data(), img.size(), false, "nothere.dll", fb::kHookedNames, fb::kHookedCount, slots) == 0);
}

int main() {
    CHECK(fb::kHookedCount == 9);
    char own[MAX_PATH];
    GetModuleFileNameA(nullptr, own, MAX_PATH);
    std::string app(own);
    app = app.substr(0, app.find_last_of('\\')) + "\\..\\testapp\\xr_testapp.exe";
    check_file(app.c_str(), true);
    char ref[MAX_PATH] = {};
    if (GetEnvironmentVariableA("FB_REF_DLL", ref, MAX_PATH)) check_file(ref, true);
    else printf("  FB_REF_DLL not set: REFramework's dinput8.dll not checked\n");
    // garbage is not a PE file: nothing, no crash
    const uint8_t junk[64] = {'M', 'Z'};
    fb::ImportSlot s[4];
    CHECK(fb::find_import_slots(junk, sizeof junk, false, "openxr_loader.dll", fb::kHookedNames, fb::kHookedCount, s) == 0);
    return DONE("iat_test");
}
