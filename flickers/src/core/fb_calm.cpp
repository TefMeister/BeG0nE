// fb_calm.cpp -- see fb_calm.h.
#include "fb_calm.h"
#include "fb_core.h"

#include <d3dcompiler.h>
#include <math.h>
#include <string.h>

namespace fb {

namespace {
constexpr DXGI_FORMAT kHistFmt = DXGI_FORMAT_R16G16B16A16_FLOAT;
constexpr int kConsts = 28;               // 3 turn rows, cur tangents, prev tangents, (keep, clip, have_prev, calm), (hold, ratio, min, 0)

const char* kCalmHlsl = R"(
cbuffer C : register(b0) { float4 m0; float4 m1; float4 m2; float4 tc; float4 tp; float4 p; float4 hd; };
Texture2D<float4> cur : register(t0);
Texture2D<float4> hist : register(t1);
SamplerState lin : register(s0);
struct V { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
V vs(uint id : SV_VertexID) {
    V o; o.uv = float2((id << 1) & 2, id & 2);
    o.pos = float4(o.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}
float3 toy(float3 c) { return float3(dot(c, float3(0.25, 0.5, 0.25)), dot(c, float3(0.5, 0, -0.5)), dot(c, float3(-0.25, 0.5, -0.25))); }
float3 fromy(float3 y) { return float3(y.x + y.y - y.z, y.x + y.z, y.x - y.y - y.z); }
struct O { float4 a : SV_Target0; float4 b : SV_Target1; };
O ps(V i) {
    uint w, h; cur.GetDimensions(w, h);
    int2 ip = int2(i.pos.xy);
    float3 c = cur.Load(int3(ip, 0)).rgb;
    float3 res = c;
    float held = 0;
    if (p.z > 0.5) {
        float3 s1 = 0, s2 = 0;
        [unroll] for (int y = -1; y <= 1; y++) [unroll] for (int x = -1; x <= 1; x++) {
            int2 q = clamp(ip + int2(x, y), int2(0, 0), int2(w - 1, h - 1));
            float3 t = toy(cur.Load(int3(q, 0)).rgb); s1 += t; s2 += t * t;
        }
        float3 mu = s1 / 9, sd = sqrt(max(s2 / 9 - mu * mu, 0));
        float2 uv = (float2(ip) + 0.5) / float2(w, h);
        float3 d = float3(lerp(tc.x, tc.y, uv.x), lerp(tc.z, tc.w, uv.y), -1);
        float3 q = float3(dot(m0.xyz, d), dot(m1.xyz, d), dot(m2.xyz, d));
        if (q.z < -1e-4) {
            float2 t = q.xy / -q.z;
            float2 puv = float2((t.x - tp.x) / (tp.y - tp.x), (tp.z - t.y) / (tp.z - tp.w));
            if (all(puv >= 0) && all(puv <= 1)) {
                float4 hs = hist.SampleLevel(lin, puv, 0);
                // hold: the patch around this pixel jumped in brightness since the frame before, and this pixel was not
                // held last frame (a real change is never held twice, so it is at most one frame late)
                float a = mu.x + 0.002, b = toy(hs.rgb).x + 0.002;
                bool flash = hd.x > 0.5 && hs.a < 0.5 && max(a, b) / min(a, b) > hd.y && abs(a - b) > hd.z;
                if (flash) { res = hs.rgb; held = 1; }
                else if (p.w > 0.5) {
                    float3 hy = clamp(toy(hs.rgb), mu - p.y * sd, mu + p.y * sd);
                    res = lerp(c, fromy(hy), p.x);
                }
            }
        }
    }
    O o; o.a = float4(res, 1); o.b = float4(res, held);
    return o;
})";

void barrier(ID3D12GraphicsCommandList* l, ID3D12Resource* r, D3D12_RESOURCE_STATES a, D3D12_RESOURCE_STATES b) {
    D3D12_RESOURCE_BARRIER x{};
    x.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    x.Transition.pResource = r;
    x.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    x.Transition.StateBefore = a;
    x.Transition.StateAfter = b;
    l->ResourceBarrier(1, &x);
}

void tans(const XrFovf& f, float* o) { o[0] = tanf(f.angleLeft); o[1] = tanf(f.angleRight); o[2] = tanf(f.angleUp); o[3] = tanf(f.angleDown); }

template <class T> void rel(T*& p) { if (p) { p->Release(); p = nullptr; } }
}  // namespace

bool Calm::setup(ID3D12Device* dev) {
    if (dev_) return true;
    ID3DBlob* err = nullptr;
    if (FAILED(D3DCompile(kCalmHlsl, strlen(kCalmHlsl), "calm", nullptr, nullptr, "vs", "vs_5_0", 0, 0, &vs_, &err)) ||
        FAILED(D3DCompile(kCalmHlsl, strlen(kCalmHlsl), "calm", nullptr, nullptr, "ps", "ps_5_0", 0, 0, &ps_, &err))) {
        log("calm shader did not compile: %s", err ? (const char*)err->GetBufferPointer() : "?");
        return false;
    }
    D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, 0};
    D3D12_ROOT_PARAMETER rp[2]{};
    rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rp[0].Constants = {0, 0, kConsts};
    rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rp[1].DescriptorTable = {1, &range};
    rp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC smp{};
    smp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    smp.AddressU = smp.AddressV = smp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    smp.MaxLOD = D3D12_FLOAT32_MAX;
    smp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC rsd{2, rp, 1, &smp, D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ID3DBlob* blob = nullptr;
    if (FAILED(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))) return false;
    const HRESULT hr = dev->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rs_));
    blob->Release();
    if (FAILED(hr)) return false;
    D3D12_DESCRIPTOR_HEAP_DESC sd{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0};
    D3D12_DESCRIPTOR_HEAP_DESC rd{D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 4, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0};
    if (FAILED(dev->CreateDescriptorHeap(&sd, IID_PPV_ARGS(&srv_))) || FAILED(dev->CreateDescriptorHeap(&rd, IID_PPV_ARGS(&rtv_)))) return false;
    srv_step_ = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    rtv_step_ = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    dev_ = dev;
    return true;
}

bool Calm::make_eye(int e, DXGI_FORMAT fmt, uint32_t w, uint32_t h) {
    Eye& g = eye_[e];
    g = Eye{};
    g.w = w; g.h = h;
    D3D12_HEAP_PROPERTIES hp{D3D12_HEAP_TYPE_DEFAULT};
    D3D12_RESOURCE_DESC td{};
    td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    td.Width = w; td.Height = h; td.DepthOrArraySize = 1; td.MipLevels = 1; td.SampleDesc.Count = 1;
    td.Format = fmt;
    if (FAILED(dev_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&g.cur)))) return false;
    td.Format = kHistFmt;
    td.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    for (auto& t : g.hist)
        if (FAILED(dev_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&t)))) return false;
    D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
    sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sv.Texture2D.MipLevels = 1;
    for (int k = 0; k < 2; k++) {
        D3D12_CPU_DESCRIPTOR_HANDLE c = srv_->GetCPUDescriptorHandleForHeapStart();
        c.ptr += (SIZE_T)(e * 4 + k * 2) * srv_step_;
        sv.Format = fmt;
        dev_->CreateShaderResourceView(g.cur, &sv, c);
        c.ptr += srv_step_;
        sv.Format = kHistFmt;
        dev_->CreateShaderResourceView(g.hist[k], &sv, c);
        D3D12_CPU_DESCRIPTOR_HANDLE r = rtv_->GetCPUDescriptorHandleForHeapStart();
        r.ptr += (SIZE_T)(e * 2 + k) * rtv_step_;
        dev_->CreateRenderTargetView(g.hist[k], nullptr, r);
    }
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};
    pd.pRootSignature = rs_;
    pd.VS = {vs_->GetBufferPointer(), vs_->GetBufferSize()};
    pd.PS = {ps_->GetBufferPointer(), ps_->GetBufferSize()};
    pd.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pd.BlendState.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pd.SampleMask = 0xffffffff;
    pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pd.RasterizerState.DepthClipEnable = TRUE;
    pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pd.NumRenderTargets = 2;
    pd.RTVFormats[0] = fmt;
    pd.RTVFormats[1] = kHistFmt;
    pd.SampleDesc.Count = 1;
    if (FAILED(dev_->CreateGraphicsPipelineState(&pd, IID_PPV_ARGS(&g.pso)))) { log("eye %d: calm pipeline refused (fmt %d)", e, (int)fmt); return false; }
    log("eye %d: calm on, %ux%u, %llu MB of video memory", e, w, h, (unsigned long long)(((uint64_t)w * h * 20) >> 20));
    return true;
}

void Calm::draw(ID3D12GraphicsCommandList* l, int e, D3D12_CPU_DESCRIPTOR_HANDLE own_rtv, const XrPosef& pose, const XrFovf& fov,
                const Settings& s) {
    Eye& g = eye_[e];
    const int write = g.read ^ 1;
    float c[kConsts] = {};
    float rows[3][4];
    calm_turn(pose.orientation, g.have_prev ? g.prev_q : pose.orientation, rows);
    memcpy(c, rows, sizeof rows);
    tans(fov, c + 12);
    tans(g.have_prev ? g.prev_fov : fov, c + 16);
    c[20] = (float)s.calm_keep / 100.0f;
    c[21] = (float)s.calm_clip / 10.0f;
    c[22] = g.have_prev ? 1.0f : 0.0f;
    c[23] = s.calm ? 1.0f : 0.0f;
    c[24] = s.hold ? 1.0f : 0.0f;
    c[25] = (float)s.hold_ratio / 10.0f;
    c[26] = (float)s.hold_min / 1000.0f;
    barrier(l, g.cur, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    barrier(l, g.hist[write], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE rt[2] = {own_rtv, rtv_->GetCPUDescriptorHandleForHeapStart()};
    rt[1].ptr += (SIZE_T)(e * 2 + write) * rtv_step_;
    l->OMSetRenderTargets(2, rt, FALSE, nullptr);
    ID3D12DescriptorHeap* heaps[] = {srv_};
    l->SetDescriptorHeaps(1, heaps);
    l->SetGraphicsRootSignature(rs_);
    l->SetPipelineState(g.pso);
    l->SetGraphicsRoot32BitConstants(0, kConsts, c, 0);
    D3D12_GPU_DESCRIPTOR_HANDLE tbl = srv_->GetGPUDescriptorHandleForHeapStart();
    tbl.ptr += (UINT64)(e * 4 + g.read * 2) * srv_step_;
    l->SetGraphicsRootDescriptorTable(1, tbl);
    const D3D12_VIEWPORT vp{0, 0, (float)g.w, (float)g.h, 0, 1};
    const D3D12_RECT sc{0, 0, (LONG)g.w, (LONG)g.h};
    l->RSSetViewports(1, &vp);
    l->RSSetScissorRects(1, &sc);
    l->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    l->DrawInstanced(3, 1, 0, 0);
    barrier(l, g.hist[write], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    barrier(l, g.cur, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    g.read = write;
    g.have_prev = true;
    g.prev_q = pose.orientation;
    g.prev_fov = fov;
}

void Calm::drop_eyes() {
    for (auto& g : eye_) {
        rel(g.cur); rel(g.hist[0]); rel(g.hist[1]); rel(g.pso);
        g = Eye{};
    }
}

Calm::~Calm() {
    drop_eyes();
    rel(srv_); rel(rtv_); rel(rs_); rel(vs_); rel(ps_);
}

}  // namespace fb
