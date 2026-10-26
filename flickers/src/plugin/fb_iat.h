// fb_iat.h -- finds and rewrites the slots a program calls another DLL's functions through (its import table and its
// delay-import table). Pure PE reading, usable on a file on disk (tested by test/iat_test.cpp) or on a loaded module.
// REFramework delay-loads openxr_loader.dll, so the plugin shell rewrites REFramework's delay-import slots.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <windows.h>

namespace fb {

// The OpenXR functions the plugin takes over (the same nine the layer routes to the core).
extern const char* const kHookedNames[];
constexpr int kHookedCount = 9;

struct ImportSlot {
    const char* name = nullptr;           // points into kHookedNames (or the caller's list)
    uint32_t slot_rva = 0;                // where the function pointer lives, relative to the module's base
    bool delay = false;                   // in the delay-import table
};

// Every slot through which `image` (a whole file when !mapped, a loaded module when mapped) calls one of `names` in
// `dll` (any case). Returns how many were found (at most n). Anything that is not a well-formed 64-bit PE gives 0.
int find_import_slots(const uint8_t* image, size_t size, bool mapped, const char* dll, const char* const* names, int n,
                      ImportSlot* out);

// Writes `fn` into a slot of the loaded module at `base`, keeping the old pointer in *old. False if the page could not
// be made writable.
bool patch_slot(uint8_t* base, uint32_t slot_rva, void* fn, void** old);

}  // namespace fb
