# Flickers BeG0nE, step 1: core, both shells, fake flicker, catcher, reader. Implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (inline: Tefa's standing rule is no
> subagents). Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** a core that sits on a game's OpenXR calls, can spoil a known frame on purpose, keeps each eye's last second
of pictures and dumps them, plus a reader that finds the odd frames; delivered as an OpenXR layer and as a
REFramework plugin, proven end to end on the dev PC with `hello_xr` and the OpenXR Simulator.

**Architecture:** one core (`flickers/src/core/`) exposes `fb_*` hook functions with the OpenXR signatures; each shell
only routes the game's calls to them and hands the core the next-in-chain function pointers. At `xrEndFrame` the core
copies each eye's submitted picture into its own per-eye swapchain and rewrites the submitted view to point at it (the
OpenXR Toolkit pattern), which is the seam both the fake flicker and, later, the cure use.

**Tech Stack:** C++17, MSVC (VS 2022 Build Tools, `cl`), D3D12, `D3DCompile` at run time (d3dcompiler_47), OpenXR
headers 1.1.63 (`D:\claude video game stuff\tools\openxr-loader-khronos\openxr_loader_windows-1.1.63.zip`, copied into
`flickers/third_party/openxr/`), REFramework plugin API 1.15 (`API.h` from the Village plugin), Python 3.12 + numpy +
OpenCV + Pillow for the reader.

**Spec:** `docs/specs/2026-10-26-flickers-begone-design.md`

## Global Constraints

- Source files under 800 lines; every number named in `fb_settings.h` (code-shape rule).
- No added frame of delay; a frame is never held twice in a row (spec §4 anti-goals). Step 1 only spoils/records, but
  the per-eye swapchain path must cost no extra frame.
- Does nothing unless switched on: the layer by its settings naming the exe, the plugin by its settings file.
- D3D12 first; any other graphics binding is logged and the core passes everything through untouched.
- No game files; nothing written outside `reframework/data/flickers/` (plugin) or the layer's own folder (layer).
- Settings file `flickers_begone.txt`, `key = value` lines; dumps in `flickers-dump-<n>/frame-<eye>-<kk>.ppm` +
  `frames.txt`.
- Privacy guard: no home-folder paths or logins in committed files; paths are found at run time.

## Review Focus

1. **Swapchain recreated mid-run** (resolution change, REFramework resetting VR): the core must drop its per-eye
   swapchain and ring and rebuild, not copy into a stale size. Test: Task 2 `views_test` case "swapchain destroyed".
2. **One eye per frame (AFR) or the same image for both eyes** (one swapchain, two `imageRect`s or two array slices):
   per-eye bookkeeping keyed by view index and rect, never by swapchain alone. Test: Task 2 `views_test` cases
   "shared swapchain, two rects" and "array slices".
3. **Plugin loads after the session exists:** no `xrCreateSession` seen; the queue must come from REFramework's
   `renderer_data->command_queue`, and the core must say so in the log and keep working. Test: Task 6 log line
   `queue from REFramework` in the injected-late run.
4. **Format families:** the game's image is `R8G8B8A8_UNORM_SRGB`, typeless or `R16G16B16A16_FLOAT`; the core's own
   swapchain must take a format the runtime offers in the same family, else pass through. Test: Task 2 `formats_test`.
5. **Dump while the GPU is busy:** dumping must not stall a frame (readback after a fence, written on a worker
   thread). Test: Task 4, frame time logged around a dump stays under `kDumpStallMs`.

---

## File structure

```
flickers/
  README.md                      the fault and the cure in plain words (Tefa's layout)
  build.bat                      builds both shells + host tests, runs the tests
  third_party/openxr/            openxr.h, openxr_platform.h, openxr_platform_defines.h, openxr_loader_negotiation.h
  third_party/reframework/       API.h (from the Village plugin)
  src/core/fb_settings.h         every named number + the settings-file reader (pure CPU)
  src/core/fb_views.h/.cpp       eye bookkeeping: swapchains, images, acquire index, per-eye records (pure CPU)
  src/core/fb_gpu.h/.cpp         D3D12: own per-eye swapchains, copy, half-size downscale shader, ring, readback
  src/core/fb_fake.h             fake flicker: which frames, which eye, which patch (pure CPU, seeded)
  src/core/fb_dump.cpp           dump job: ppm + frames.txt on a worker thread
  src/core/fb_core.h/.cpp        fb_* hook functions, next-in-chain table, log
  src/layer/fb_layer.cpp         OpenXR API layer shell (negotiate, dispatch)
  src/layer/flickers_begone_layer.json
  src/plugin/fb_plugin.cpp       REFramework plugin shell (delay-IAT + IAT slot patch, REFramework queue)
  src/plugin/fb_iat.h/.cpp       import/delay-import slot finder + patcher (pure CPU on a PE image)
  test/*.cpp                     host tests (no GPU) + run_tests.bat
  test/inject.cpp                test-only launcher: starts hello_xr suspended, loads fb_plugin.dll
  tools/flicker_read.py          the reader
  tools/test_flicker_read.py     reader tests on synthetic frames
```

Code lives in the BeG0nE repo (not Lanes): Lanes carries small recorders and scripts, this is a compiled add-on with
its own build, like VR Bench. The reader moves into Lanes `begone/flickers/` with a `tool.json` once step 3 works.
(Builder's call, recorded in the spec §8.)

---

### Task 1: build skeleton, settings, fake-flicker schedule

**Files:** Create `flickers/build.bat`, `flickers/third_party/...`, `src/core/fb_settings.h`, `src/core/fb_fake.h`,
`test/settings_test.cpp`, `test/fake_test.cpp`, `test/run_tests.bat`.

**Interfaces — Produces:**
- `struct fb::Settings { std::string exe; int on; int ring_frames=72; int ring_half=1; int auto_dump_s=20; int max_dumps_hand=4; int max_dumps_auto=12; int fake=0; int fake_every_min=10; int fake_every_max=20; int fake_patch_px=96; uint32_t fake_seed=1; };`
- `bool fb::parse_settings(const char* text, Settings& out);` unknown keys ignored, bad numbers keep the default.
- `class fb::FakeSchedule { FakeSchedule(uint32_t seed,int min,int max); bool spoil(uint64_t frame,int eye, int& x,int& y) const; }` deterministic, at most one eye per spoiled frame, gaps between `min` and `max`.

- [ ] Write `settings_test`: `"on = 1\nring_frames = 36\nbogus = 3\nfake = x"` → `on==1, ring_frames==36, fake==0`.
- [ ] Write `fake_test`: seed 1, 10..20: over 10,000 frames every gap is in [10,20]; same seed → same list; never
      both eyes on one frame.
- [ ] `build.bat` (vswhere → vcvars64, the VR Bench pattern) compiles the tests; run: both FAIL (not defined).
- [ ] Implement; run `flickers\build.bat` → `ALL TESTS PASS`.
- [ ] Commit `flickers: skeleton, settings, fake-flicker schedule`.

### Task 2: eye bookkeeping

**Files:** `src/core/fb_views.h/.cpp`, `test/views_test.cpp`, `test/formats_test.cpp`.

**Interfaces — Produces:**
- `struct fb::EyeRecord { XrSwapchain sc; uint32_t image; XrRect2Di rect; uint32_t slice; XrPosef pose; XrFovf fov; uint64_t frame; bool same_as_last; };`
- `class fb::Views { void on_create_swapchain(XrSwapchain, int64_t fmt, uint32_t w, uint32_t h, uint32_t arraySize); void on_destroy_swapchain(XrSwapchain); void on_acquire(XrSwapchain, uint32_t index); bool on_end_frame(const XrFrameEndInfo&, uint64_t frame, EyeRecord out[2]); uint32_t generation() const; }`
  — `on_end_frame` reads the first `XrCompositionLayerProjection` with 2 views; returns false (pass-through) for anything else; `generation()` bumps when a swapchain the eyes use is destroyed or changes size.
- `int64_t fb::pick_format(int64_t app_fmt, const int64_t* offered, uint32_t n);` same family (sRGB/UNORM/typeless of
  R8G8B8A8 or B8G8R8A8; R16G16B16A16 float) or 0.

- [ ] `views_test` cases: two swapchains one per eye; **shared swapchain, two rects**; **array slices** (one
      swapchain, `imageArrayIndex` 0/1); **swapchain destroyed** → generation bumps; quad layer only → false;
      the same pose and image twice → `same_as_last` on the second.
- [ ] `formats_test`: SRGB app + offered {UNORM, SRGB} → SRGB; typeless app → its SRGB/UNORM sibling; nothing in
      family → 0.
- [ ] Run → FAIL; implement; run → PASS; commit `flickers: eye bookkeeping`.

### Task 3: the core and the layer shell, pass-through only

**Files:** `src/core/fb_core.h/.cpp`, `src/layer/fb_layer.cpp`, `src/layer/flickers_begone_layer.json`.

**Interfaces — Produces:**
- `struct fb::Next { PFN_xrEndFrame EndFrame; PFN_xrCreateSwapchain CreateSwapchain; PFN_xrDestroySwapchain DestroySwapchain; PFN_xrEnumerateSwapchainImages EnumerateSwapchainImages; PFN_xrEnumerateSwapchainFormats EnumerateSwapchainFormats; PFN_xrAcquireSwapchainImage AcquireSwapchainImage; PFN_xrWaitSwapchainImage WaitSwapchainImage; PFN_xrReleaseSwapchainImage ReleaseSwapchainImage; PFN_xrCreateSession CreateSession; };`
- `void fb::core_init(const Next&, const char* data_dir);` `void fb::core_set_queue(ID3D12CommandQueue*, const char* where);`
- `XRAPI_ATTR XrResult XRAPI_CALL fb::EndFrame(XrSession, const XrFrameEndInfo*);` and the same for the other eight,
  each calling `Next` and feeding `Views`.
- Log: `flickers_begone.log` beside the settings; one line per event class the first time, then counts once a second.

- [ ] Layer negotiate + `xrGetInstanceProcAddr` routing (VR Bench's `vrbench_layer.cpp` is the pattern); implicit
      manifest with `disable_environment: DISABLE_FLICKERS_BEGONE`; enabled only when settings `exe` matches.
- [ ] Run: `XR_RUNTIME_JSON=<sim>\openxr_simulator.json`, `XR_API_LAYER_PATH=<flickers>\dist`, `hello_xr -g D3D12 -ff Hmd -vc Stereo` for 10 s.
      Expected log: `session D3D12, queue from xrCreateSession`, `views: 2 eyes, swapchains=2` (or the real shape), no
      errors; hello_xr's picture unchanged in the simulator window.
- [ ] Commit `flickers: core + layer shell, pass-through`.

### Task 4: own per-eye swapchain, fake flicker, ring, dump

**Files:** `src/core/fb_gpu.h/.cpp`, `src/core/fb_dump.cpp`, `test/dump_test.cpp` (CPU half: ppm writer, frames.txt line).

**Interfaces — Produces:**
- `class fb::Gpu { bool ensure(ID3D12Device*, XrSession, const EyeRecord&, int eye, uint32_t generation); XrSwapchain submit_copy(const EyeRecord&, int eye, const FakeSpot* spoil); void ring_push(int eye, const EyeRecord&); bool dump(int no, const char* dir); }`
- `frames.txt` line: `frame eye px py pz qx qy qz qw fovL fovR fovU fovD image rect_x rect_y rect_w rect_h same fake`.

- [ ] `submit_copy`: acquire own image, copy the game's rect (subresource = slice), if `spoil` fill a
      `fake_patch_px` square at (x,y) with solid magenta through a tiny clear, release; the core rewrites the view's
      `subImage` to the own swapchain at rect (0,0,w,h), slice 0.
- [ ] Ring: half-size downscale compute shader (2x2 box, compiled with `D3DCompile` at start), RGBA8, `ring_frames`
      slots per eye; on dump, copy to readback, fence, worker thread writes ppm + frames.txt.
- [ ] Auto dump every `auto_dump_s` while `on=2`; `on=3` = dump now then back to 2 (control file re-read once a second).
- [ ] `dump_test`: ppm header and bytes for a 4x2 image; frames.txt line formatting for a known record.
- [ ] Run hello_xr as in Task 3 with `fake=1 on=3`: magenta squares visible in the simulator on the scheduled frames
      (screenshot via the simulator MCP `capture_screenshot`), a `flickers-dump-1` folder with 72 frames per eye, the
      `fake` column 1 exactly on the scheduled frames; frame time around the dump under `kDumpStallMs` (=4).
- [ ] Commit `flickers: own swapchains, fake flicker, catcher ring and dump`.

### Task 5: the reader

**Files:** `tools/flicker_read.py`, `tools/test_flicker_read.py`.

**Interfaces — Produces:** `python flicker_read.py <dump-folder>` → `report.txt`, `sheet.jpg`, `heat-<eye>.png`.
`report.txt` lists `ODD frame <n> eye <e> share <x> pose_jump <deg> fov_swapped <yes/no>`, then a verdict line:
`VERDICT: other-eye data on odd frames` / `VERDICT: picture-only odd frames` / `VERDICT: no odd frames`.

- [ ] Method (from this session's clip analysis): per eye, align each frame to the one before (phase correlation),
      residual > 12 grey levels, a frame is ODD when its residual share is > 4x the running median AND the next frame
      returns (residual vs the frame before the odd one below 1.5x median). Pose/fov compared with both neighbours;
      `fov_swapped` when its fov equals the other eye's.
- [ ] `test_flicker_read.py`: synthetic 40-frame pan with a patch spoiled on frames 11 and 27 → exactly those ODD;
      a steady pan with no spoil → no ODD; frame 19 carrying the other eye's fov → `fov_swapped yes`.
- [ ] Run on Task 4's dump: ODD frames == the `fake` column, verdict `picture-only odd frames`.
- [ ] Commit `flickers: reader`.

### Task 6: the REFramework plugin shell

**Files:** `src/plugin/fb_iat.h/.cpp`, `src/plugin/fb_plugin.cpp`, `test/iat_test.cpp`, `test/inject.cpp`.

**Interfaces — Produces:**
- `int fb::find_import_slots(const uint8_t* image, bool mapped, const char* dll, const char* const* names, int n, void*** out_slots);` both the import and the delay-import tables.
- `bool fb::patch_slot(void** slot, void* fn, void** old);` (VirtualProtect, keeps the old pointer; a delay slot that
  still points at its load thunk is resolved first with `GetProcAddress` on `openxr_loader.dll`).
- Plugin exports `reframework_plugin_required_version` (1.15.0) and `reframework_plugin_initialize`: patch every loaded
  module that imports `openxr_loader.dll`, queue from `param->renderer_data->command_queue` when the renderer is D3D12.

- [ ] `iat_test` on the file `builds/re-village-scope-vr/base-layer-2026-09-27_praydog-a24c3459_dlss-310.9.1_pdperf-1.2.0/dinput8.dll`
      (path read from an environment variable, skipped with a note if absent): finds all 9 hooked names in the delay
      table of `openxr_loader.dll`.
- [ ] `inject.cpp`: `CreateProcess(hello_xr, CREATE_SUSPENDED)`, LoadLibrary of `fb_plugin.dll` via a remote thread,
      resume. The plugin with no REFramework calls its own init from `DllMain`'s deferred thread when it sees no
      REFramework host.
- [ ] Run hello_xr through `inject.exe`, layer NOT on the path, `fake=1 on=3`: same results as Task 4 (magenta on
      schedule, dump, reader verdict). Log says `patched hello_xr.exe imports: 9`.
- [ ] Run once more injecting 3 s after start (session exists): log `no xrCreateSession seen`; with no REFramework
      host this run is expected to pass through untouched and say `no queue` (the REFramework queue only exists in
      the game) — that is the Review Focus 3 log path.
- [ ] Commit `flickers: REFramework plugin shell`.

### Task 7: write-up and the board

- [ ] `flickers/README.md` in Tefa's readme layout (`claude-memory/docs/readme-style/`); BeG0nE `README.md` table row.
- [ ] `claude-memory/status/BeG0nE.md` rows: step 2 `[VR CLAUDE]` (auto catch in RE4 on the home PC, plugin build
      copied into `reframework/plugins/`, the simulator or VR Bench turning the head in the grass), owed reminder for
      the home PC (`tools/owed.sh add HOME`).
- [ ] Profile row + ACTIVITY.md line only if a piece is proven working (real progress rule).
- [ ] Commit, push, close-out box.
