// fb_gpu.cpp -- see fb_gpu.h. Each frame, per eye: acquire the core's own swapchain image, copy the game's eye picture
// into it, (fake flicker) clear a magenta square on the scheduled frames, shrink a copy into the catcher ring, and point
// the submitted view at the own image. A dump copies the whole ring to readback memory and a worker thread writes it.
// All of it is recorded on one command list submitted to the game's own queue after the game's work, so nothing waits.
#include "fb_gpu.h"
#include "fb_dump.h"
#include "fb_fake.h"

#include <d3dcompiler.h>
#include <string.h>
#include <windows.h>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace fb {

namespace {
constexpr int kInFlight = 3;              // command allocators in rotation
constexpr DWORD kAcquireTimeoutMs = 100;  // own image wait before giving up on a frame
constexpr int kDumpPollFrames = 30;       // how often the dump_now file is looked for

const char* kShrinkHlsl = R"(
Texture2D<float4> src : register(t0);
RWTexture2D<unorm float4> dst : register(u0);
cbuffer C : register(b0) { uint scale; uint enc; uint pad0; uint pad1; };
float3 enc_srgb(float3 c) { c = saturate(c); return c <= 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1.0 / 2.4) - 0.055; }
[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
    uint w, h; dst.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) return;
    float4 s = 0;
    for (uint y = 0; y < scale; y++) for (uint x = 0; x < scale; x++) s += src.Load(int3(id.x * scale + x, id.y * scale + y, 0));
    s /= (float)(scale * scale);
    if (enc) s.rgb = enc_srgb(s.rgb);
    dst[id.xy] = float4(s.rgb, 1);
})";

bool is_srgb_or_float(DXGI_FORMAT f) {
    return f == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || f == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || f == DXGI_FORMAT_R16G16B16A16_FLOAT;
}

void barrier(ID3D12GraphicsCommandList* l, ID3D12Resource* r, UINT sub, D3D12_RESOURCE_STATES a, D3D12_RESOURCE_STATES b) {
    D3D12_RESOURCE_BARRIER x{};
    x.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    x.Transition.pResource = r;
    x.Transition.Subresource = sub;
    x.Transition.StateBefore = a;
    x.Transition.StateAfter = b;
    l->ResourceBarrier(1, &x);
}

struct Slot { ID3D12Resource* tex = nullptr; EyeRecord rec; bool fake = false; };

struct EyeGpu {
    XrSwapchain own = XR_NULL_HANDLE;
    std::vector<ID3D12Resource*> own_images;
    UINT rtv0 = 0;
    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
    uint32_t w = 0, h = 0, rw = 0, rh = 0;
    bool enc = false;
    std::vector<Slot> ring;
    int head = 0, filled = 0;
    std::unique_ptr<FakeSchedule> fake;
};

struct DumpJob {
    ID3D12Resource* buf = nullptr;
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> fps;
    std::vector<std::pair<int, Slot>> order;   // (eye, slot info), same order as fps
    std::string dir, reason;
    ID3D12Fence* fence = nullptr;
    UINT64 value = 0;
    std::atomic<bool>* busy = nullptr;    // the readback buffer is the core's, made once; this frees it for the next dump
};

void write_dump(DumpJob* j) {
    // below the game's threads, but not "background mode": that also throttles the disk, and a dump then took 8+ s
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    HANDLE ev = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    if (j->fence->GetCompletedValue() < j->value) { j->fence->SetEventOnCompletion(j->value, ev); WaitForSingleObject(ev, 10000); }
    CloseHandle(ev);
    uint8_t* p = nullptr;
    D3D12_RANGE rr{0, 0};
    if (SUCCEEDED(j->buf->Map(0, nullptr, (void**)&p)) && p) {
        CreateDirectoryA(j->dir.c_str(), nullptr);
        std::string txt = "# reason " + j->reason + "\n" + frame_header() + "\n";
        int per_eye[2] = {0, 0};
        for (size_t i = 0; i < j->order.size(); i++) {
            const int eye = j->order[i].first;
            char name[MAX_PATH];
            snprintf(name, sizeof name, "%s\\frame-%d-%02d.ppm", j->dir.c_str(), eye, per_eye[eye]++);
            const auto& fp = j->fps[i];
            write_ppm(name, fp.Footprint.Width, fp.Footprint.Height, p + fp.Offset, fp.Footprint.RowPitch);
            txt += frame_line(j->order[i].second.rec, eye, j->order[i].second.fake) + "\n";
        }
        j->buf->Unmap(0, &rr);
        FILE* f = nullptr;
        if (fopen_s(&f, (j->dir + "\\frames.txt").c_str(), "wb") == 0 && f) { fwrite(txt.data(), 1, txt.size(), f); fclose(f); }
        log("dump written: %s (%zu pictures)", j->dir.c_str(), j->order.size());
    } else {
        log("dump FAILED: could not map the readback memory");
    }
    j->busy->store(false);
    delete j;
}
}  // namespace

struct Gpu::Impl {
    ID3D12Device* dev = nullptr;
    ID3D12CommandQueue* queue = nullptr;
    XrSession session = XR_NULL_HANDLE;
    uint32_t gen = 0;
    bool broken = false;                  // something could not be made: pass through for the rest of the run
    ID3D12CommandAllocator* alloc[kInFlight] = {};
    UINT64 alloc_v[kInFlight] = {};
    ID3D12GraphicsCommandList* list = nullptr;
    ID3D12Fence* fence = nullptr;
    UINT64 fence_v = 0;
    HANDLE fence_ev = nullptr;
    ID3D12DescriptorHeap* rtv = nullptr;
    UINT rtv_step = 0, rtv_next = 0;
    ID3D12DescriptorHeap* srv = nullptr;
    UINT srv_step = 0;
    ID3D12RootSignature* rs = nullptr;
    ID3D12PipelineState* pso = nullptr;
    std::map<XrSwapchain, std::vector<ID3D12Resource*>> game_images;
    EyeGpu eye[2];
    XrCompositionLayerProjection proj{};
    XrCompositionLayerProjectionView views[2]{};
    std::vector<const XrCompositionLayerBaseHeader*> layers;
    uint64_t frames = 0;
    // counters for the 1 s line
    uint32_t spoiled = 0, dumps_auto = 0, dumps_hand = 0;
    double stall_max_ms = 0;
    uint64_t last_dump_ms = 0;
    // the readback buffer for dumps: made once, when both rings exist, so a dump costs no allocation (Review Focus 5)
    ID3D12Resource* rb = nullptr;
    UINT64 rb_each = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT rb_fp{};
    std::atomic<bool> rb_busy{false};
    bool rb_tried = false;

    bool make_readback() {
        const D3D12_RESOURCE_DESC rd = eye[0].ring[0].tex->GetDesc();
        UINT64 bytes = 0;
        dev->GetCopyableFootprints(&rd, 0, 1, 0, &rb_fp, nullptr, nullptr, &bytes);
        rb_each = (bytes + 511) & ~511ull;
        D3D12_HEAP_PROPERTIES hp{D3D12_HEAP_TYPE_READBACK};
        D3D12_RESOURCE_DESC bd{};
        bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bd.Width = rb_each * (eye[0].ring.size() + eye[1].ring.size()); bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1;
        bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (FAILED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&rb)))) {
            log("catcher: %llu MB of readback memory refused: no dumps this run", (unsigned long long)(bd.Width >> 20));
            rb = nullptr;
            return false;
        }
        log("catcher: %llu MB readback memory set aside for dumps", (unsigned long long)(bd.Width >> 20));
        return true;
    }

    void wait_idle() {
        if (!fence) return;
        queue->Signal(fence, ++fence_v);
        if (fence->GetCompletedValue() < fence_v) { fence->SetEventOnCompletion(fence_v, fence_ev); WaitForSingleObject(fence_ev, 2000); }
    }
    void drop_eyes() {
        wait_idle();
        for (int i = 0; i < 200 && rb_busy.load(); i++) Sleep(50);   // a dump still being written keeps its buffer
        if (rb && !rb_busy.load()) { rb->Release(); rb = nullptr; }
        rb_tried = false;
        for (auto& e : eye) {
            if (e.own) core_next().DestroySwapchain(e.own);
            for (auto& s : e.ring) if (s.tex) s.tex->Release();
            e = EyeGpu{};
        }
        rtv_next = 0;
        game_images.clear();
    }
    bool setup(ID3D12CommandQueue* q, ID3D12Device* d) {
        queue = q; dev = d;
        for (auto& a : alloc) if (FAILED(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&a)))) return false;
        if (FAILED(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc[0], nullptr, IID_PPV_ARGS(&list)))) return false;
        list->Close();
        if (FAILED(dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) return false;
        fence_ev = CreateEventA(nullptr, FALSE, FALSE, nullptr);
        D3D12_DESCRIPTOR_HEAP_DESC hd{D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 32, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0};
        if (FAILED(dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&rtv)))) return false;
        rtv_step = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_DESCRIPTOR_HEAP_DESC sd{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2 * 2 * kInFlight, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0};
        if (FAILED(dev->CreateDescriptorHeap(&sd, IID_PPV_ARGS(&srv)))) return false;
        srv_step = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        // root signature: 4 constants at b0, a table of [SRV t0, UAV u0]
        D3D12_DESCRIPTOR_RANGE ranges[2] = {{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0}, {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 1}};
        D3D12_ROOT_PARAMETER rp[2]{};
        rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rp[0].Constants = {0, 0, 4};
        rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rp[1].DescriptorTable = {2, ranges};
        D3D12_ROOT_SIGNATURE_DESC rsd{2, rp, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ID3DBlob *blob = nullptr, *err = nullptr;
        if (FAILED(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))) return false;
        HRESULT hr = dev->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rs));
        blob->Release();
        if (FAILED(hr)) return false;
        if (FAILED(D3DCompile(kShrinkHlsl, strlen(kShrinkHlsl), "shrink", nullptr, nullptr, "main", "cs_5_0", 0, 0, &blob, &err))) {
            log("shrink shader did not compile: %s", err ? (const char*)err->GetBufferPointer() : "?");
            return false;
        }
        D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};
        pd.pRootSignature = rs;
        pd.CS = {blob->GetBufferPointer(), blob->GetBufferSize()};
        hr = dev->CreateComputePipelineState(&pd, IID_PPV_ARGS(&pso));
        blob->Release();
        return SUCCEEDED(hr);
    }
    ID3D12Resource* game_image(XrSwapchain sc, uint32_t index) {
        auto it = game_images.find(sc);
        if (it == game_images.end()) {
            uint32_t n = 0;
            std::vector<ID3D12Resource*> v;
            if (XR_SUCCEEDED(core_next().EnumerateSwapchainImages(sc, 0, &n, nullptr)) && n) {
                std::vector<XrSwapchainImageD3D12KHR> imgs(n, {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
                if (XR_SUCCEEDED(core_next().EnumerateSwapchainImages(sc, n, &n, (XrSwapchainImageBaseHeader*)imgs.data())))
                    for (auto& i : imgs) v.push_back(i.texture);
            }
            it = game_images.emplace(sc, v).first;
            log("game swapchain %p: %zu D3D12 images", (void*)sc, v.size());
        }
        return index < it->second.size() ? it->second[index] : nullptr;
    }
    bool make_eye(int e, const EyeRecord& r, ID3D12Resource* gimg, const Settings& s) {
        EyeGpu& g = eye[e];
        const D3D12_RESOURCE_DESC gd = gimg->GetDesc();
        uint32_t nf = 0;
        core_next().EnumerateSwapchainFormats(session, 0, &nf, nullptr);
        std::vector<int64_t> offered(nf);
        if (nf) core_next().EnumerateSwapchainFormats(session, nf, &nf, offered.data());
        const int64_t f = pick_format(gd.Format, offered.data(), nf);
        if (!f) { log("eye %d: no own format for the game's format %d: passing through", e, (int)gd.Format); return false; }
        XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        ci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        ci.format = f;
        ci.sampleCount = 1;
        ci.width = (uint32_t)r.rect.extent.width;
        ci.height = (uint32_t)r.rect.extent.height;
        ci.faceCount = 1; ci.arraySize = 1; ci.mipCount = 1;
        if (XR_FAILED(core_next().CreateSwapchain(session, &ci, &g.own))) { log("eye %d: own swapchain refused", e); return false; }
        uint32_t n = 0;
        core_next().EnumerateSwapchainImages(g.own, 0, &n, nullptr);
        std::vector<XrSwapchainImageD3D12KHR> imgs(n, {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
        core_next().EnumerateSwapchainImages(g.own, n, &n, (XrSwapchainImageBaseHeader*)imgs.data());
        g.fmt = (DXGI_FORMAT)f;
        g.w = ci.width; g.h = ci.height;
        g.enc = is_srgb_or_float(g.fmt);
        g.rtv0 = rtv_next;
        for (auto& i : imgs) {
            g.own_images.push_back(i.texture);
            D3D12_RENDER_TARGET_VIEW_DESC rv{};
            rv.Format = g.fmt;
            rv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            D3D12_CPU_DESCRIPTOR_HANDLE h = rtv->GetCPUDescriptorHandleForHeapStart();
            h.ptr += (SIZE_T)(rtv_next++) * rtv_step;
            dev->CreateRenderTargetView(i.texture, &rv, h);
        }
        const uint32_t scale = s.ring_half ? 2 : 1;
        g.rw = g.w / scale; g.rh = g.h / scale;
        D3D12_HEAP_PROPERTIES hp{D3D12_HEAP_TYPE_DEFAULT};
        D3D12_RESOURCE_DESC td{};
        td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        td.Width = g.rw; td.Height = g.rh; td.DepthOrArraySize = 1; td.MipLevels = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        g.ring.resize((size_t)s.ring_frames);
        for (auto& sl : g.ring)
            if (FAILED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&sl.tex)))) {
                log("eye %d: ring picture %ux%u refused", e, g.rw, g.rh);
                return false;
            }
        g.fake = std::make_unique<FakeSchedule>(s.fake_seed, s.fake_every_min, s.fake_every_max, s.fake_patch_px, (int)g.w, (int)g.h);
        log("eye %d: own swapchain %ux%u fmt %d (game fmt %d), ring %d x %ux%u", e, g.w, g.h, (int)g.fmt, (int)gd.Format, s.ring_frames, g.rw, g.rh);
        return true;
    }
};

void Gpu::frame(const Settings& s, const char* dir, XrSession session, ID3D12CommandQueue* queue, uint32_t generation,
                const EyeRecord eyes[2], XrFrameEndInfo& out) {
    if (!p_) p_ = new Impl;
    Impl& m = *p_;
    if (m.broken) return;
    LARGE_INTEGER t0, t1, qf;
    QueryPerformanceCounter(&t0);
    if (!m.dev) {
        ID3D12Device* d = nullptr;
        if (FAILED(queue->GetDevice(IID_PPV_ARGS(&d))) || !m.setup(queue, d)) { log("GPU setup failed: passing through"); m.broken = true; return; }
        m.session = session;
    }
    if (generation != m.gen) {
        if (m.gen) log("swapchains changed: rebuilding");
        m.drop_eyes();
        m.gen = generation;
    }
    ID3D12Resource* gimg[2] = {m.game_image(eyes[0].sc, eyes[0].image), m.game_image(eyes[1].sc, eyes[1].image)};
    if (!gimg[0] || !gimg[1]) return;
    for (int e = 0; e < 2; e++)
        if (!m.eye[e].own && !m.make_eye(e, eyes[e], gimg[e], s)) { m.drop_eyes(); m.broken = true; return; }
    if (s.on >= 2 && !m.rb_tried) { m.rb_tried = true; m.make_readback(); }

    const int slot_a = (int)(m.frames % kInFlight);
    if (m.fence->GetCompletedValue() < m.alloc_v[slot_a]) {
        m.fence->SetEventOnCompletion(m.alloc_v[slot_a], m.fence_ev);
        WaitForSingleObject(m.fence_ev, 1000);
    }
    m.alloc[slot_a]->Reset();
    m.list->Reset(m.alloc[slot_a], nullptr);
    uint32_t own_idx[2];
    for (int e = 0; e < 2; e++) {
        EyeGpu& g = m.eye[e];
        XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wi.timeout = (XrDuration)kAcquireTimeoutMs * 1000000;
        if (XR_FAILED(core_next().AcquireSwapchainImage(g.own, nullptr, &own_idx[e])) || XR_FAILED(core_next().WaitSwapchainImage(g.own, &wi))) {
            log("eye %d: own image not available: passing through this run", e);
            m.list->Close();
            m.drop_eyes();
            m.broken = true;
            return;
        }
        ID3D12Resource* own = g.own_images[own_idx[e]];
        const D3D12_RESOURCE_DESC gd = gimg[e]->GetDesc();
        const UINT gsub = eyes[e].slice * gd.MipLevels;
        barrier(m.list, gimg[e], gsub, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
        barrier(m.list, own, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
        D3D12_TEXTURE_COPY_LOCATION dst{own, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}}, src{gimg[e], D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}};
        dst.SubresourceIndex = 0;
        src.SubresourceIndex = gsub;
        const D3D12_BOX box{(UINT)eyes[e].rect.offset.x, (UINT)eyes[e].rect.offset.y, 0,
                            (UINT)(eyes[e].rect.offset.x + eyes[e].rect.extent.width), (UINT)(eyes[e].rect.offset.y + eyes[e].rect.extent.height), 1};
        m.list->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);
        barrier(m.list, gimg[e], gsub, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        barrier(m.list, own, 0, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
        FakeSpot spot;
        const bool fake = s.fake && g.fake->spoil(eyes[e].frame, e, spot);
        if (fake) {
            D3D12_CPU_DESCRIPTOR_HANDLE h = m.rtv->GetCPUDescriptorHandleForHeapStart();
            h.ptr += (SIZE_T)(g.rtv0 + own_idx[e]) * m.rtv_step;
            const D3D12_RECT rc{spot.x, spot.y, spot.x + spot.size, spot.y + spot.size};
            m.list->ClearRenderTargetView(h, kFakeColour, 1, &rc);
            m.spoiled++;
        }
        if (s.on >= 2) {                  // the catcher: a shrunk copy into the ring
            Slot& sl = g.ring[(size_t)g.head];
            barrier(m.list, own, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            const UINT base = (UINT)(slot_a * 4 + e * 2);
            D3D12_CPU_DESCRIPTOR_HANDLE c = m.srv->GetCPUDescriptorHandleForHeapStart();
            c.ptr += (SIZE_T)base * m.srv_step;
            D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
            sv.Format = g.fmt;
            sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            sv.Texture2D.MipLevels = 1;
            m.dev->CreateShaderResourceView(own, &sv, c);
            c.ptr += m.srv_step;
            D3D12_UNORDERED_ACCESS_VIEW_DESC uv{};
            uv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            uv.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            m.dev->CreateUnorderedAccessView(sl.tex, nullptr, &uv, c);
            ID3D12DescriptorHeap* heaps[] = {m.srv};
            m.list->SetDescriptorHeaps(1, heaps);
            m.list->SetComputeRootSignature(m.rs);
            m.list->SetPipelineState(m.pso);
            const UINT consts[4] = {s.ring_half ? 2u : 1u, g.enc ? 1u : 0u, 0, 0};
            m.list->SetComputeRoot32BitConstants(0, 4, consts, 0);
            D3D12_GPU_DESCRIPTOR_HANDLE gh = m.srv->GetGPUDescriptorHandleForHeapStart();
            gh.ptr += (UINT64)base * m.srv_step;
            m.list->SetComputeRootDescriptorTable(1, gh);
            m.list->Dispatch((g.rw + 7) / 8, (g.rh + 7) / 8, 1);
            barrier(m.list, own, 0, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
            sl.rec = eyes[e];
            sl.fake = fake;
            g.head = (g.head + 1) % (int)g.ring.size();
            if (g.filled < (int)g.ring.size()) g.filled++;
        }
    }

    // the dump: automatic every auto_dump_s, or when a `dump_now` file appears in the data folder
    const uint64_t now = GetTickCount64();
    const bool full = m.eye[0].filled == (int)m.eye[0].ring.size() && m.eye[1].filled == (int)m.eye[1].ring.size();
    std::string reason;
    if (s.on >= 2 && full && m.rb) {
        const std::string now_file = std::string(dir) + "\\dump_now";
        if (m.frames % kDumpPollFrames == 0 && GetFileAttributesA(now_file.c_str()) != INVALID_FILE_ATTRIBUTES) {
            DeleteFileA(now_file.c_str());
            if ((int)m.dumps_hand < s.max_dumps_hand) reason = "hand";
        } else if ((int)m.dumps_auto < s.max_dumps_auto && (!m.last_dump_ms || now - m.last_dump_ms >= (uint64_t)s.auto_dump_s * 1000)) {
            if (m.last_dump_ms) reason = "auto";
            else m.last_dump_ms = now;    // the first interval starts when the ring is full
        }
    }
    DumpJob* job = nullptr;
    if (!reason.empty()) {
        job = new DumpJob;
        const int no = (int)(m.dumps_auto + m.dumps_hand) + 1;
        job->dir = std::string(dir) + "\\flickers-dump-" + std::to_string(no);
        job->reason = reason;
        std::vector<std::pair<int, Slot*>> order;
        for (int e = 0; e < 2; e++) {
            EyeGpu& g = m.eye[e];
            const int n = (int)g.ring.size();
            for (int k = 0; k < g.filled; k++) order.push_back({e, &g.ring[(size_t)((g.head - g.filled + k + 2 * n) % n)]});
        }
        if (m.rb_busy.load()) {
            log("dump skipped: the previous one is still being written");
            delete job;
            job = nullptr;
            m.last_dump_ms = now;
        } else {
            m.rb_busy.store(true);
            job->buf = m.rb;
            job->busy = &m.rb_busy;
            for (size_t i = 0; i < order.size(); i++) {
                ID3D12Resource* t = order[i].second->tex;
                barrier(m.list, t, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
                D3D12_PLACED_SUBRESOURCE_FOOTPRINT f = m.rb_fp;
                f.Offset = m.rb_each * i;
                D3D12_TEXTURE_COPY_LOCATION dst{job->buf, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, {}}, src{t, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}};
                dst.PlacedFootprint = f;
                src.SubresourceIndex = 0;
                m.list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
                barrier(m.list, t, 0, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                job->fps.push_back(f);
                job->order.push_back({order[i].first, *order[i].second});
            }
            if (reason == "hand") m.dumps_hand++; else m.dumps_auto++;
            m.last_dump_ms = now;
        }
    }

    m.list->Close();
    ID3D12CommandList* lists[] = {m.list};
    m.queue->ExecuteCommandLists(1, lists);
    m.queue->Signal(m.fence, ++m.fence_v);
    m.alloc_v[slot_a] = m.fence_v;
    if (job) {
        job->fence = m.fence;
        job->value = m.fence_v;
        std::thread(write_dump, job).detach();
    }
    for (int e = 0; e < 2; e++) core_next().ReleaseSwapchainImage(m.eye[e].own, nullptr);

    // point the submitted eyes at the own images (the first stereo projection layer, as fb::Views chose it)
    m.layers.assign(out.layers, out.layers + out.layerCount);
    for (auto& l : m.layers) {
        if (l->type != XR_TYPE_COMPOSITION_LAYER_PROJECTION || ((const XrCompositionLayerProjection*)l)->viewCount != 2) continue;
        m.proj = *(const XrCompositionLayerProjection*)l;
        for (int e = 0; e < 2; e++) {
            m.views[e] = m.proj.views[e];
            m.views[e].subImage.swapchain = m.eye[e].own;
            m.views[e].subImage.imageRect = {{0, 0}, {(int32_t)m.eye[e].w, (int32_t)m.eye[e].h}};
            m.views[e].subImage.imageArrayIndex = 0;
        }
        m.proj.views = m.views;
        l = (const XrCompositionLayerBaseHeader*)&m.proj;
        break;
    }
    out.layers = m.layers.data();
    m.frames++;
    QueryPerformanceCounter(&t1);
    QueryPerformanceFrequency(&qf);
    const double ms = 1000.0 * (double)(t1.QuadPart - t0.QuadPart) / (double)qf.QuadPart;
    if (ms > m.stall_max_ms) m.stall_max_ms = ms;
    if (job) log("dump %s queued (%s), this frame took %.2f ms in the core (limit %d)", job->dir.c_str(), reason.c_str(), ms, kDumpStallMs);
}

void Gpu::release() {
    if (!p_) return;
    p_->drop_eyes();
    p_->gen = 0;
}

std::string Gpu::status_line() const {
    if (!p_ || !p_->dev) return "";
    char b[160];
    snprintf(b, sizeof b, " | own=%s spoiled=%u dumps=%u+%u core_max=%.2fms", p_->broken ? "off" : "on", p_->spoiled,
             p_->dumps_auto, p_->dumps_hand, p_->stall_max_ms);
    p_->spoiled = 0;
    p_->stall_max_ms = 0;
    return b;
}

Gpu::~Gpu() { delete p_; }

}  // namespace fb
