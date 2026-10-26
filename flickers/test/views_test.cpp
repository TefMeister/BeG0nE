// views_test.cpp -- eye bookkeeping: which swapchain, image, part and pose each eye was submitted with.
#include "check.h"
#include "../src/core/fb_views.h"
#include "../src/core/fb_views.cpp"

static XrSwapchain sc(uint64_t n) { return (XrSwapchain)n; }

struct Frame {
    XrCompositionLayerProjectionView v[2];
    XrCompositionLayerProjection proj;
    const XrCompositionLayerBaseHeader* layers[2];
    XrFrameEndInfo info;
    void set(XrSwapchain a, XrSwapchain b, XrRect2Di ra, XrRect2Di rb, uint32_t sa, uint32_t sb, float px) {
        for (int i = 0; i < 2; i++) {
            v[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
            v[i].pose.orientation.w = 1.0f;
            v[i].pose.position.x = i ? px + 0.064f : px;
            v[i].fov = {-0.8f, 0.8f, 0.8f, -0.8f};
        }
        v[0].subImage = {a, ra, sa};
        v[1].subImage = {b, rb, sb};
        proj = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        proj.viewCount = 2;
        proj.views = v;
        layers[0] = (const XrCompositionLayerBaseHeader*)&proj;
        info = {XR_TYPE_FRAME_END_INFO};
        info.layerCount = 1;
        info.layers = layers;
    }
};

int main() {
    const XrRect2Di full{{0, 0}, {1280, 1400}}, left{{0, 0}, {1280, 1400}}, right{{1280, 0}, {1280, 1400}};
    fb::EyeRecord e[2];

    // two swapchains, one per eye
    {
        fb::Views vw;
        vw.on_create_swapchain(sc(1), 29, 1280, 1400, 1);
        vw.on_create_swapchain(sc(2), 29, 1280, 1400, 1);
        vw.on_acquire(sc(1), 2);
        vw.on_acquire(sc(2), 1);
        Frame f; f.set(sc(1), sc(2), full, full, 0, 0, 0.0f);
        CHECK(vw.on_end_frame(f.info, 10, e));
        CHECK(e[0].sc == sc(1) && e[0].image == 2 && e[1].sc == sc(2) && e[1].image == 1);
        CHECK(e[0].frame == 10 && e[1].pose.position.x > 0.06f);
        CHECK(e[0].w == 1280 && e[0].h == 1400 && e[0].fmt == 29);
        CHECK(!e[0].same_as_last);
        // the same image and pose again: same_as_last
        Frame g; g.set(sc(1), sc(2), full, full, 0, 0, 0.0f);
        CHECK(vw.on_end_frame(g.info, 11, e));
        CHECK(e[0].same_as_last && e[1].same_as_last);
        // a new image: not the same
        vw.on_acquire(sc(1), 0);
        CHECK(vw.on_end_frame(g.info, 12, e));
        CHECK(!e[0].same_as_last && e[1].same_as_last);
    }
    // the game already holds the next image when it ends this frame: the submitted one is the last RELEASED
    // (images are released in the order they were acquired)
    {
        fb::Views vw;
        vw.on_create_swapchain(sc(1), 29, 1280, 1400, 1);
        vw.on_create_swapchain(sc(2), 29, 1280, 1400, 1);
        vw.on_acquire(sc(1), 0); vw.on_acquire(sc(2), 0);
        vw.on_release(sc(1)); vw.on_release(sc(2));
        vw.on_acquire(sc(1), 1); vw.on_acquire(sc(2), 1);       // drawing frame N+1 already
        Frame f; f.set(sc(1), sc(2), full, full, 0, 0, 0.0f);
        CHECK(vw.on_end_frame(f.info, 1, e));
        CHECK(e[0].image == 0 && e[1].image == 0);
        vw.on_release(sc(1)); vw.on_release(sc(2));
        CHECK(vw.on_end_frame(f.info, 2, e));
        CHECK(e[0].image == 1 && e[1].image == 1);
        // two held at once, released in order
        vw.on_acquire(sc(1), 2); vw.on_acquire(sc(1), 0); vw.on_release(sc(1));
        CHECK(vw.on_end_frame(f.info, 3, e));
        CHECK(e[0].image == 2);
    }
    // shared swapchain, two rects
    {
        fb::Views vw;
        vw.on_create_swapchain(sc(5), 29, 2560, 1400, 1);
        vw.on_acquire(sc(5), 0);
        Frame f; f.set(sc(5), sc(5), left, right, 0, 0, 0.0f);
        CHECK(vw.on_end_frame(f.info, 1, e));
        CHECK(e[0].sc == sc(5) && e[1].sc == sc(5) && e[0].image == 0 && e[1].image == 0);
        CHECK(e[0].rect.offset.x == 0 && e[1].rect.offset.x == 1280 && e[1].rect.extent.width == 1280);
    }
    // array slices
    {
        fb::Views vw;
        vw.on_create_swapchain(sc(7), 29, 1280, 1400, 2);
        vw.on_acquire(sc(7), 1);
        Frame f; f.set(sc(7), sc(7), full, full, 0, 1, 0.0f);
        CHECK(vw.on_end_frame(f.info, 1, e));
        CHECK(e[0].slice == 0 && e[1].slice == 1 && e[1].image == 1);
    }
    // swapchain destroyed: generation bumps; an eye on an unknown swapchain = pass through
    {
        fb::Views vw;
        vw.on_create_swapchain(sc(1), 29, 1280, 1400, 1);
        vw.on_create_swapchain(sc(2), 29, 1280, 1400, 1);
        vw.on_acquire(sc(1), 0); vw.on_acquire(sc(2), 0);
        Frame f; f.set(sc(1), sc(2), full, full, 0, 0, 0.0f);
        CHECK(vw.on_end_frame(f.info, 1, e));
        const uint32_t g0 = vw.generation();
        vw.on_destroy_swapchain(sc(2));
        CHECK(vw.generation() != g0);
        CHECK(!vw.on_end_frame(f.info, 2, e));
        vw.on_create_swapchain(sc(3), 29, 1920, 1920, 1);
        vw.on_acquire(sc(3), 0);
        Frame h; h.set(sc(1), sc(3), full, {{0, 0}, {1920, 1920}}, 0, 0, 0.0f);
        CHECK(vw.on_end_frame(h.info, 3, e));
        CHECK(e[1].w == 1920);
    }
    // only a quad layer: pass through
    {
        fb::Views vw;
        XrCompositionLayerQuad q{XR_TYPE_COMPOSITION_LAYER_QUAD};
        const XrCompositionLayerBaseHeader* ls[1] = {(const XrCompositionLayerBaseHeader*)&q};
        XrFrameEndInfo info{XR_TYPE_FRAME_END_INFO};
        info.layerCount = 1; info.layers = ls;
        CHECK(!vw.on_end_frame(info, 1, e));
    }
    // a swapchain the core never saw being created (plugin loaded late): learned from the first acquire, size unknown
    {
        fb::Views vw;
        vw.on_acquire(sc(9), 0); vw.on_acquire(sc(10), 0);
        Frame f; f.set(sc(9), sc(10), full, full, 0, 0, 0.0f);
        CHECK(vw.on_end_frame(f.info, 1, e));
        CHECK(e[0].w == 0 && e[0].fmt == 0);   // the GPU side reads the size and format off the image itself
    }
    return DONE("views_test");
}
