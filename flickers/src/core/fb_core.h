// fb_core.h -- the Flickers BeG0nE core: OpenXR hook functions shared by both shells (the REFramework plugin and the
// OpenXR layer). A shell routes the game's calls to the fb::xr* functions below and fills fb::Next with the functions
// one step further down (the runtime, or the next layer). The core never calls the game.
#pragma once
#include <stdint.h>
#define XR_NO_PROTOTYPES
#define XR_USE_GRAPHICS_API_D3D12
#include <d3d12.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

namespace fb {

struct Next {
    PFN_xrEndFrame EndFrame = nullptr;
    PFN_xrCreateSwapchain CreateSwapchain = nullptr;
    PFN_xrDestroySwapchain DestroySwapchain = nullptr;
    PFN_xrEnumerateSwapchainImages EnumerateSwapchainImages = nullptr;
    PFN_xrEnumerateSwapchainFormats EnumerateSwapchainFormats = nullptr;
    PFN_xrAcquireSwapchainImage AcquireSwapchainImage = nullptr;
    PFN_xrWaitSwapchainImage WaitSwapchainImage = nullptr;
    PFN_xrReleaseSwapchainImage ReleaseSwapchainImage = nullptr;
    PFN_xrCreateSession CreateSession = nullptr;
    PFN_xrDestroySession DestroySession = nullptr;
};

// `data_dir` holds flickers_begone.txt (settings), flickers_begone.log and the dumps; `shell` names the shell in the log.
void core_init(const Next& next, const char* data_dir, const char* shell);
// The D3D12 queue the game submits on, and where it came from ("xrCreateSession", "REFramework").
void core_set_queue(ID3D12CommandQueue* queue, const char* where);
const Next& core_next();
void log(const char* fmt, ...);

XRAPI_ATTR XrResult XRAPI_CALL xrEndFrame(XrSession, const XrFrameEndInfo*);
XRAPI_ATTR XrResult XRAPI_CALL xrCreateSwapchain(XrSession, const XrSwapchainCreateInfo*, XrSwapchain*);
XRAPI_ATTR XrResult XRAPI_CALL xrDestroySwapchain(XrSwapchain);
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateSwapchainImages(XrSwapchain, uint32_t, uint32_t*, XrSwapchainImageBaseHeader*);
XRAPI_ATTR XrResult XRAPI_CALL xrAcquireSwapchainImage(XrSwapchain, const XrSwapchainImageAcquireInfo*, uint32_t*);
XRAPI_ATTR XrResult XRAPI_CALL xrWaitSwapchainImage(XrSwapchain, const XrSwapchainImageWaitInfo*);
XRAPI_ATTR XrResult XRAPI_CALL xrReleaseSwapchainImage(XrSwapchain, const XrSwapchainImageReleaseInfo*);
XRAPI_ATTR XrResult XRAPI_CALL xrCreateSession(XrInstance, const XrSessionCreateInfo*, XrSession*);
XRAPI_ATTR XrResult XRAPI_CALL xrDestroySession(XrSession);

}  // namespace fb
