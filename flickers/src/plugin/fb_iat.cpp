// fb_iat.cpp -- see fb_iat.h.
#include "fb_iat.h"
#include <string.h>

namespace fb {

const char* const kHookedNames[] = {
    "xrEndFrame", "xrCreateSwapchain", "xrDestroySwapchain", "xrEnumerateSwapchainImages", "xrAcquireSwapchainImage",
    "xrWaitSwapchainImage", "xrReleaseSwapchainImage", "xrCreateSession", "xrDestroySession",
};

namespace {
struct Pe {
    const uint8_t* img;
    size_t size;
    bool mapped;
    const IMAGE_NT_HEADERS64* nt = nullptr;
    const IMAGE_SECTION_HEADER* sec = nullptr;

    bool open() {
        if (size < sizeof(IMAGE_DOS_HEADER)) return false;
        const auto* dos = (const IMAGE_DOS_HEADER*)img;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || (size_t)dos->e_lfanew + sizeof(IMAGE_NT_HEADERS64) > size) return false;
        nt = (const IMAGE_NT_HEADERS64*)(img + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return false;
        sec = IMAGE_FIRST_SECTION(nt);
        return (const uint8_t*)(sec + nt->FileHeader.NumberOfSections) <= img + size;
    }
    // A pointer to `len` bytes at `rva`, or null when they are not inside the image.
    const uint8_t* at(uint32_t rva, size_t len) const {
        size_t off = rva;
        if (!mapped) {
            off = (size_t)-1;
            for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
                const IMAGE_SECTION_HEADER& s = sec[i];
                const uint32_t span = s.Misc.VirtualSize > s.SizeOfRawData ? s.Misc.VirtualSize : s.SizeOfRawData;
                if (rva >= s.VirtualAddress && rva < s.VirtualAddress + span) { off = rva - s.VirtualAddress + s.PointerToRawData; break; }
            }
        }
        if (off == (size_t)-1 || off + len > size || off + len < off) return nullptr;
        return img + off;
    }
    const char* str(uint32_t rva) const {
        const uint8_t* p = at(rva, 1);
        if (!p) return nullptr;
        for (const uint8_t* q = p; q < img + size; q++) if (!*q) return (const char*)p;
        return nullptr;
    }
    const IMAGE_DATA_DIRECTORY& dir(int i) const { return nt->OptionalHeader.DataDirectory[i]; }
};

// Walks one name table (INT) beside its address table (IAT) and records the slots of the wanted names.
int match(const Pe& pe, uint32_t int_rva, uint32_t iat_rva, bool delay, const char* const* names, int n, ImportSlot* out, int found) {
    for (uint32_t i = 0; found < n; i++) {
        const uint8_t* t = pe.at(int_rva + i * 8, 8);
        if (!t) break;
        const uint64_t v = *(const uint64_t*)t;
        if (!v) break;
        if (v & IMAGE_ORDINAL_FLAG64) continue;
        const char* nm = pe.str((uint32_t)v + 2);            // IMAGE_IMPORT_BY_NAME: hint (2 bytes), then the name
        if (!nm) continue;
        for (int k = 0; k < n; k++)
            if (strcmp(nm, names[k]) == 0) { out[found++] = {names[k], iat_rva + i * 8, delay}; break; }
    }
    return found;
}
}  // namespace

int find_import_slots(const uint8_t* image, size_t size, bool mapped, const char* dll, const char* const* names, int n,
                      ImportSlot* out) {
    Pe pe{image, size, mapped};
    if (!image || !pe.open()) return 0;
    int found = 0;
    const IMAGE_DATA_DIRECTORY& imp = pe.dir(IMAGE_DIRECTORY_ENTRY_IMPORT);
    for (uint32_t i = 0; imp.VirtualAddress && found < n; i++) {
        const auto* d = (const IMAGE_IMPORT_DESCRIPTOR*)pe.at(imp.VirtualAddress + i * (uint32_t)sizeof(IMAGE_IMPORT_DESCRIPTOR), sizeof(IMAGE_IMPORT_DESCRIPTOR));
        if (!d || !d->Name) break;
        const char* nm = pe.str(d->Name);
        if (nm && _stricmp(nm, dll) == 0)
            found = match(pe, d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk, d->FirstThunk, false, names, n, out, found);
    }
    const IMAGE_DATA_DIRECTORY& del = pe.dir(IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT);
    for (uint32_t i = 0; del.VirtualAddress && found < n; i++) {
        const auto* d = (const IMAGE_DELAYLOAD_DESCRIPTOR*)pe.at(del.VirtualAddress + i * (uint32_t)sizeof(IMAGE_DELAYLOAD_DESCRIPTOR), sizeof(IMAGE_DELAYLOAD_DESCRIPTOR));
        if (!d || !d->DllNameRVA) break;
        if (!d->Attributes.RvaBased) continue;               // the old pointer-based form: not written by any compiler we meet
        const char* nm = pe.str(d->DllNameRVA);
        if (nm && _stricmp(nm, dll) == 0)
            found = match(pe, d->ImportNameTableRVA, d->ImportAddressTableRVA, true, names, n, out, found);
    }
    return found;
}

bool patch_slot(uint8_t* base, uint32_t slot_rva, void* fn, void** old) {
    void** slot = (void**)(base + slot_rva);
    DWORD prot = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &prot)) return false;
    if (old) *old = *slot;
    *slot = fn;
    VirtualProtect(slot, sizeof(void*), prot, &prot);
    return true;
}

}  // namespace fb
