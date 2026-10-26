// fb_calm.h -- the cure for shimmer: each eye's picture is blended with that eye's previous picture, turned to where
// the head now points, and kept within the spread of the new picture around each pixel, so a pixel that only
// shimmers settles while anything that really changes (a turn, an enemy, a muzzle flash) comes through at once.
// Head turning is taken out by turning the previous picture by the change of the eye's orientation; head moving
// sideways is not (standing still is the case it is for), the spread limit keeps that from smearing.
#pragma once
#include <d3d12.h>
#include <stdint.h>
#define XR_NO_PROTOTYPES
#include <openxr/openxr.h>
#include "fb_settings.h"

namespace fb {

// A unit quaternion as a 3x3 rotation matrix (pure CPU).
inline void calm_rot(const XrQuaternionf& q, float r[3][3]) {
    const float x = q.x, y = q.y, z = q.z, w = q.w;
    r[0][0] = 1 - 2 * (y * y + z * z); r[0][1] = 2 * (x * y - z * w);     r[0][2] = 2 * (x * z + y * w);
    r[1][0] = 2 * (x * y + z * w);     r[1][1] = 1 - 2 * (x * x + z * z); r[1][2] = 2 * (y * z - x * w);
    r[2][0] = 2 * (x * z - y * w);     r[2][1] = 2 * (y * z + x * w);     r[2][2] = 1 - 2 * (x * x + y * y);
}

// The turn from the current eye to the previous one, as 3 rows: a direction seen by the current eye, times these
// rows, is the same direction seen by the previous eye (pure CPU, tested by test/calm_test.cpp).
inline void calm_turn(const XrQuaternionf& cur, const XrQuaternionf& prev, float rows[3][4]) {
    float c[3][3], p[3][3];
    calm_rot(cur, c);
    calm_rot(prev, p);
    for (int i = 0; i < 3; i++) {         // rows = transpose(prev) * cur: current eye -> world -> previous eye
        for (int j = 0; j < 3; j++) rows[i][j] = p[0][i] * c[0][j] + p[1][i] * c[1][j] + p[2][i] * c[2][j];
        rows[i][3] = 0;
    }
}

class Calm {
public:
    bool setup(ID3D12Device* dev);        // shaders and root signature, once per device
    bool make_eye(int e, DXGI_FORMAT fmt, uint32_t w, uint32_t h);
    void drop_eyes();
    // The game's eye picture is copied into this (it is in COPY_DEST state) instead of into the own image.
    ID3D12Resource* input(int e) const { return eye_[e].cur; }
    // Blends input(e) with the history into `own` (RENDER_TARGET state before and after; `own_rtv` is its view).
    void draw(ID3D12GraphicsCommandList* l, int e, D3D12_CPU_DESCRIPTOR_HANDLE own_rtv, const XrPosef& pose,
              const XrFovf& fov, const Settings& s);
    ~Calm();

private:
    struct Eye {
        ID3D12Resource* cur = nullptr;
        ID3D12Resource* hist[2] = {};
        ID3D12PipelineState* pso = nullptr;
        uint32_t w = 0, h = 0;
        int read = 0;                     // the history picture read this frame; the other one is written
        bool have_prev = false;
        XrQuaternionf prev_q{};
        XrFovf prev_fov{};
    };
    ID3D12Device* dev_ = nullptr;
    ID3D12RootSignature* rs_ = nullptr;
    ID3DBlob* vs_ = nullptr;
    ID3DBlob* ps_ = nullptr;
    ID3D12DescriptorHeap* srv_ = nullptr;   // per eye, two tables of [cur, hist[k]]
    ID3D12DescriptorHeap* rtv_ = nullptr;   // per eye, hist[0] and hist[1]
    UINT srv_step_ = 0, rtv_step_ = 0;
    Eye eye_[2];
};

}  // namespace fb
