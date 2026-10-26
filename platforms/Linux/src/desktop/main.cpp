#include "window.h"
#include "navigation.h"
#include "camera.h"
#include <iostream>
#include <cstdlib>
using namespace hiface_linux_demo;
namespace {
void logMessage(HiFaceLogLevel level, const char* tag, const char* message, void*) {
    if (level >= HIFACE_LOG_WARNING) std::fprintf(stderr, "[%s] %s\n", tag, message);
}
}
int main(int argc, char** argv) {
    const bool help = argc == 2 && std::string(argv[1]) == "--help";
    const bool camera_test = argc == 3 && std::string(argv[1]) == "--camera-test";
    const bool preview_test = argc == 5 && std::string(argv[1]) == "--preview-test";
    if (help || (!camera_test && !preview_test && (argc < 3 || argc > 4))) {
        std::cout << "Usage: hiface_desktop MODEL_DIR DATABASE [CAMERA_DEVICE|--smoke-test]\n"
                     "       hiface_desktop --camera-test CAMERA_DEVICE\n"
                     "       hiface_desktop --preview-test CAMERA_DEVICE OUTPUT_WIDTH OUTPUT_HEIGHT\n"
                     "Home opens feature pages; Esc stops the current flow or returns Home. F1 Search, F2 Photos / comparison, F3 Face library, F4 Settings, F5 Image search.\n"
                     "Camera shortcuts: D detection page, C capture, L liveness, V verify, E enroll, S search, P pause, X stop, M mirror, N next camera, Q quit.\n";
        return help ? 0 : 2;
    }
    try {
        if (camera_test) {
            Camera camera; CameraFrame frame;
            camera.open(argv[2]);
            const auto deadline = monotonicMilliseconds() + 5000;
            unsigned frames = 0;
            while (frames < 3 && monotonicMilliseconds() < deadline) {
                if (camera.read(frame)) ++frames;
                else std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            require(frames == 3, "Camera test timed out waiting for three frames");
            const auto view = frame.view();
            require(view.y.data && view.u.data && view.v.data, "Camera test produced invalid YUV planes");
            std::cout << "Camera test passed: " << camera.description() << ", " << frames
                      << " NV12/I420 adapter frames\n";
            return 0;
        }
        if (preview_test) {
            const int output_width = std::stoi(argv[3]), output_height = std::stoi(argv[4]);
            Camera camera; CameraFrame frame;
            camera.open(argv[2], 1280, 720, 25, false);
            const auto deadline = monotonicMilliseconds() + 5000;
            unsigned frames = 0;
            while (frames < 3 && monotonicMilliseconds() < deadline) {
                if (camera.readPreview(frame, output_width, output_height)) ++frames;
                else std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            require(frames == 3 && frame.rgba.size() == static_cast<size_t>(output_width) * output_height * 4
                    && frame.i420.empty() && frame.x11_bgrx, "RGA preview test failed");
            std::cout << "RGA preview test passed: " << camera.description() << " -> "
                      << output_width << 'x' << output_height << " X11 BGRX, " << frames << " frames\n";
            return 0;
        }
        DesktopWindow window;
        hiface_set_log_callback_v1(logMessage, nullptr);
        const bool smoke = argc == 4 && std::string(argv[3]) == "--smoke-test";
        const char* assets_env = std::getenv("HIFACE_DEMO_ASSETS_DIR");
        std::error_code error;
        auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
        if (error) executable = std::filesystem::absolute(argv[0]);
        const auto assets = assets_env ? std::string(assets_env) : (executable.parent_path().parent_path() / "assets").string();
        DesktopApp app(argv[1], argv[2], argc == 4 && !smoke ? argv[3] : "/dev/video0", smoke, assets);
        unsigned smoke_frames = 0;
        const bool performance_log = std::getenv("HIFACE_PERF_LOG") != nullptr;
        const auto start = monotonicMilliseconds();
        int64_t next_preview = start, last_performance_log = start, paint_ms = 0;
        unsigned paints = 0;
        while (window.open()) {
            const auto state = app.snapshot();
            for (auto command : window.events(*state)) app.post(std::move(command));
            const auto paint_start = performance_log ? monotonicMilliseconds() : 0;
            window.paint(*state);
            if (performance_log) {
                paint_ms += monotonicMilliseconds() - paint_start; ++paints;
                if (monotonicMilliseconds() - last_performance_log >= 5000) {
                    std::fprintf(stderr, "[Performance] preview=%.1f recognition=%.1f analysis_ms=%.1f paint_ms=%.1f faces=%zu\n",
                        state->fps, state->analysis_fps, state->milliseconds, double(paint_ms) / paints, state->boxes.size());
                    last_performance_log = monotonicMilliseconds(); paint_ms = 0; paints = 0;
                }
            }
            if (smoke) {
                require(monotonicMilliseconds() - start < 60000, "Desktop smoke test timed out during initialization");
                if (state->ready) {
                    if (++smoke_frames % 5 == 0) window.smokePage(smoke_frames / 5);
                    if (smoke_frames == (smoke_page_count + 1) * 5) { std::cout << "Desktop smoke test passed: home and all " << (smoke_page_count - 1) << " feature pages rendered\n"; return 0; }
                }
            }
            next_preview += 40; // 25 FPS UI/preview clock, independent from recognition. / UI 与预览时钟为 25 FPS，独立于识别。
            const auto now = monotonicMilliseconds();
            if (next_preview <= now) next_preview = now;
            else std::this_thread::sleep_for(std::chrono::milliseconds(next_preview - now));
        }
        return 0;
    } catch (const DemoError& error) { std::cerr << error.message << '\n'; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; }
    catch (...) { std::cerr << "Unexpected desktop failure\n"; }
    return 1;
}
