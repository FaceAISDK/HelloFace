# HiFace Linux Integration Demo

English | [简体中文](README.zh-CN.md)

**SDK 0.8.0 Beta** · Linux x86_64 / ARM64 · CPU inference.
Ubuntu 22.04 / glibc 2.35 baseline; Ubuntu 24.04 compatibility target. X11 or Wayland with XWayland is needed for the desktop.
**The SDK is in iterative public Beta testing. Please keep the SDK version up to date. The current model does not support face recognition for people under 12 and must not be used in financial scenarios.**
Target-device desktop, fonts, USB camera and recognition acceptance remains pending; cross-builds are not device acceptance.

## Run from HelloFace

```bash
cd platforms/Linux
runtime=runtimes/x86_64  # ARM64: runtimes/arm64
./self-test.sh
./run-demo.sh
# Optional camera:
./run-demo.sh /dev/video2

# Headless camera path check (no X11 required):
"$runtime/bin/hiface_desktop" --camera-test /dev/video2

# Rotate captured pixels 90 degrees counter-clockwise before preview and analysis:
HIFACE_CAMERA_ROTATE=left ./run-demo.sh /dev/video2

# Keep Xorg landscape and rotate the full-screen portrait UI inside HiFace:
HIFACE_DISPLAY_ROTATE=left ./run-demo.sh /dev/video2

# Kiosk/service mode: open Face search and start continuous search after SDK initialization:
HIFACE_START_SEARCH=1 ./run-demo.sh /dev/video2
```

Launchers choose `runtimes/x86_64` or `runtimes/arm64` using `uname -m`. Keep the complete directory tree.
`src/` is the integration Demo; `runtimes/{arch}/sdk/` holds only the closed-source SDK, public declarations,
protected model resources. Demo integration examples are outside the SDK. Read its [English SDK guide](runtimes/x86_64/sdk/README.md) or
[Chinese SDK guide](runtimes/x86_64/sdk/README.zh-CN.md) (same APIs for ARM64).

A package may use `runtimes/{arch}/` or direct `bin/` and `sdk/` directories. Extract the complete archive, enter its root directory, then choose the runtime:

```bash
runtime=runtimes/x86_64  # ARM64: runtimes/arm64
[ -d "$runtime" ] || runtime=.
sha256sum -c FILES.sha256
./self-test.sh
./run-demo.sh
```

On Ubuntu, desktop runtime tools can be installed with `sudo apt install libx11-6 xwayland v4l-utils`.
No Qt, OpenCV or compiler is required to run prebuilt programs. Launch in a graphical desktop terminal;
headless systems can use self-tests and the CLI. Camera support is V4L2 single-plane or multiplanar MMAP with
YUYV/MJPEG/NV12/NV21 (packed in one buffer plane),
requesting 640×360 at 25 FPS by default; the driver negotiates actual parameters.
Set `HIFACE_CAMERA_ROTATE=left` (aliases: `ccw`, `90ccw`) to turn a landscape sensor frame into
360×640 before both rendering and face analysis. Use `right` (aliases: `cw`, `90cw`) for the opposite direction.
Set `HIFACE_DISPLAY_ROTATE=left` to rotate the entire portrait interface and input mapping inside the HiFace window.
Keep the Xorg output at `normal`; combining it with `xrandr --rotate` can trigger DRM `flip timeout` failures.

On Rockchip boards, a separate full-resolution ISP path can feed the display while recognition stays at 640x360:

```sh
HIFACE_PREVIEW_DEVICE=/dev/video32 \
HIFACE_PREVIEW_WIDTH=1280 HIFACE_PREVIEW_HEIGHT=720 \
HIFACE_PREVIEW_OUTPUT_WIDTH=576 HIFACE_PREVIEW_OUTPUT_HEIGHT=1024 \
./run-demo.sh /dev/video31
```

The optional preview path uses the board's `librga.so.2` for one-pass NV12/NV21 scaling, color conversion and camera rotation. RGA emits the X11-native BGRX order, and local X servers use MIT-SHM image upload when available, avoiding a full-frame CPU channel swap and socket copy. Both optimizations retain portable fallbacks. If the second ISP path or RGA is unavailable, the Demo falls back to the recognition stream without failing startup.

On T92P, J3 white illumination is separate from J19 infrared and J4 RGB status LEDs. For RGB capture, the board init script runs `boards/t92p-lighting.sh prepare` as root, then sets `HIFACE_LIGHT_PWM_PATH=/sys/class/pwm/pwmchip0/pwm0` and `HIFACE_LIGHT_SENSOR_DEVICE=/dev/v4l-subdev5` (verify the `cv2002_720` sensor name). Only that white-light duty/enable pair is granted to the application; infrared remains off. Run the board script with `off` when stopping the service. The 1 kHz PWM follows the vendor driver. Camera exposure, calibrated analog gain and subsampled luma estimate low-light demand, not measured lux. Two seconds of darkness triggers a gradual ramp to 25%; separate bright/dark thresholds, a 15-second minimum hold and a six-second bright filter suppress flicker. Camera or telemetry loss switches white illumination off. Other boards remain unaffected without these environment variables.
Check device permissions instead of running the Demo as root. After joining the `video` group, log out fully and back in.

## Desktop workflows

Startup opens Home, with the first eight cards in Android order and 11 independent feature pages in total. Short portrait windows support wheel and Page Up/Down scrolling. Detection, enrollment, verification, liveness, continuous search, photo comparison, image search, settings, face library and guided capture each have their own page. Opening a camera feature connects the selected device; press Start to run that page's workflow. Home, photo and library pages close the camera. No camera is opened on startup.

| Page | Features |
|---|---|
| F1 Face search | Continuous 1:N/M:N; use Home for the other independent camera features |
| F2 Photo comparison | Two-photo comparison with input previews; detection/reference/enrollment photos belong to their own feature pages |
| F3 Face library | Paging, ID/group/tag filters, metadata edit, delete, reference selection, import/export |
| F4 Settings | Face count, landmarks, ordered actions, verification/liveness, search and camera options |
| F5 Image search | Up to ten faces, independent status, Top-1 ID/score, aligned thumbnails and selectable boxes |

Camera keys choose pages: D detection, C capture, L liveness, V verify, E enroll, S search, P pause/resume,
X cancel, M mirror, N reconnect, Q quit. Text inputs support UTF-8, Tab/Shift+Tab, copy/paste and Enter submit.
Esc stops a running camera flow, then returns Home; the camera Menu also links to Home. F1..F5 switch pages; dialogs retain focus. Settings preserve drafts and focus invalid fields.
Choose a captured/photo/library reference before verification. Library references keep original HF1 text.
Enrollment rejects existing IDs; batch replacement and deletion require confirmation.
Up to four actions can repeat; empty actions mean silent liveness. Verification enables silent liveness by default.
1:N search can use silent liveness; M:N omits it. Spoof rejection takes priority over a matching ID/score.

The full-screen search page captures and previews the newest frame at 25 FPS. Recognition consumes only the newest
available frame on a separate worker and is capped at 8 FPS, so slow inference does not stall preview. The overlay
reports actual Preview FPS, Recognition FPS and last inference time separately.
Set `HIFACE_START_SEARCH=1` for an unattended kiosk/service deployment that should enter Face search and start the
continuous search flow immediately after SDK initialization. The default remains the Home page with the camera closed.

## Photo search and demo data

Browse or enter a photo path and press Enter. Each face retains its own matched, unmatched, small-face or error status.
Click the photo box or result card; arrows, Home/End, Page Up/Down and the result pane's wheel select faces.
Changing images invalidates older requests; Esc cancels. Closing waits for the synchronous SDK call safely.
Photo search is whole-library Top-1 at the shared default threshold 0.80 and does not perform liveness checks.
JPEG EXIF orientation is normalized. Oversized JPEG/PNG/BMP images are scaled to 4096 then cropped to even dimensions;
limits are 64 MiB, 64 megapixels and 16384 pixels per edge. PPM retains a 4096 edge limit.

**Import demo 10k uses synthetic random vectors, labeled `synthetic-*`, group `synthetic-demo`, tag `not-a-person`.**
They are not derived from real faces and do not measure recognition accuracy. Import your own authorized
adult photos/features to verify real matches. JSON follows the Android HF1 exchange schema.
Folder import uses filenames as IDs and skips bad/no-face images; completed batches survive cancellation.
Export preserves original HF1 data and refuses to overwrite an existing file. SDK capacity is 50,000 records.

## Build without private SDK source

Install CMake 3.22.1+ and a GNU C++17 compiler, then run from this directory:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
```

CMake selects the SDK for the host architecture. Override with `-DHIFACE_SDK_DIR=/absolute/path/to/sdk`.
Use `-DHIFACE_BUILD_LINUX_DESKTOP=OFF` for CLI only. No NCNN/Core build is needed.
Set paths for CLI commands (`runtime=.` for a package with direct `bin/` and `sdk/` directories). The SDK uses built-in protected models; pass `--builtin`, not a model directory:

```bash
runtime=runtimes/x86_64  # ARM64: runtimes/arm64
[ -d "$runtime" ] || runtime=.
"$runtime/bin/hiface_demo" --help
"$runtime/bin/hiface_demo" detect --builtin face.jpg
"$runtime/bin/hiface_demo" compare --builtin first.jpg second.png
"$runtime/bin/hiface_demo" enroll --builtin hiface-search.db person-001 face.jpg
"$runtime/bin/hiface_demo" search-faces --builtin hiface-search.db group-photo.jpg
"$runtime/bin/hiface_demo" gallery-import hiface-search.db assets/features_10000.json
"$runtime/bin/hiface_demo" gallery-export hiface-search.db exported.json
# Run a newly compiled program using the same built-in protected models:
./build/hiface_demo self-test --builtin
```

CLI enrollment/import are upserts; desktop single enrollment rejects duplicate IDs. Import commits in batches.
`generate-demo-features OUTPUT.json` creates 10k synthetic vectors without models or personal data.
`-DBUILD_TESTING=ON` enables integration tests; run `ctest --test-dir build --output-on-failure`.
All Demo code consumes `HiFace::HiFace` public C ABI; camera, renderer, image decoder and JSON adapters belong to the Demo.

## Data and diagnostics

Capture reuses a bounded frame pool only after all UI/SDK readers release a frame, waits up to 10ms
for V4L2 readiness, and retains only the latest frames.
The ISP preview prefers V4L2 DMA-BUF input to RGA, falling back to mapped memory if unsupported;
`HIFACE_PREVIEW_DMA=0` forces that compatibility path. Recognition's I420 input is unchanged.
SDK work is event-driven; the UI repaints only on a new frame, state revision or input event.
Static pages no longer continuously composite.
The X11/Xrender portrait backend remains in place; this is not a DRM/KMS backend.
Set `HIFACE_CPU_THREADS=2` to leave CPU headroom for capture/display, or `4` to favor isolated
inference latency. Unset preserves SDK automatic selection. This process-wide setting requires
a restart and does not change models, features, thresholds or liveness checks.
`HIFACE_PERF_LOG=1` enables five-second preview/recognition/paint timing logs for diagnostics.

Default data directory: `${XDG_DATA_HOME:-$HOME/.local/share}/hiface-demo`, overridden by `HIFACE_DATA_DIR`.
It holds `hiface-search.db`, `reference.hf1`, settings and explicitly captured PPM images. Previews are not saved.
SQLite/HF1 are unencrypted; applications own data protection and cleanup. The UI uses English labels;
UTF-8 file paths/IDs depend on desktop fonts and input methods.

```bash
./diagnose.sh > linux-diagnostic.txt 2>&1
./run-demo.sh /dev/video0 2>&1 | tee desktop-run.log
```

Review diagnostics before sharing them. Do not publish user photos, databases, reference features or logs.
Model files are protected runtime assets, not training/source models. Keep SDK/runtime and Demo dependency notices.
[SDK support](https://github.com/FaceSDKPro/HelloFace/issues).

Camera settings has its own Home page and saves only camera fields. Detection, guided capture and enrollment accept a photo through their Menu; selecting it closes the live camera.
Enrollment first captures a preview, then requires an ID and explicit confirmation. Duplicate IDs retain the editable draft; retake, cancellation or leaving discards it. Enrollment does not replace the verification reference.
Pending enrollment pauses analysis. Page changes discard old images/search results, and changing the reference clears unrelated previews.
