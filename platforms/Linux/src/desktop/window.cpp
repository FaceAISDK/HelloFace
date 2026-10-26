#include "window.h"
#include "ui.h"
#include <X11/Xlib.h>
#include <algorithm>
#include <array>
#include <clocale>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <utility>
namespace hiface_linux_demo {
namespace {
// Keep Xrender optional at build time just like Xlib. These declarations are
// the small ABI subset needed to rotate the completed HiFace scene, not Xorg.
// 与 Xlib 一样，构建时可不依赖 Xrender；这里只声明旋转完整 HiFace 场景所需的最小 ABI，不包含 Xorg 实现。
using RenderPicture = unsigned long;
using RenderFixed = int32_t;
struct RenderTransform { RenderFixed matrix[3][3]; };
struct RenderPictFormat;
struct RenderPictureAttributes;
using SharedMemorySegment = unsigned long;
struct SharedMemorySegmentInfo {
    SharedMemorySegment segment;
    int id;
    char* address;
    Bool read_only;
};
constexpr int kPictOpSrc = 1;
RenderFixed fixed(int value) { return static_cast<RenderFixed>(static_cast<int64_t>(value) << 16); }
}
struct DesktopWindow::Impl {
    enum class Rotation { none, left, right };
    void* library = nullptr; void* render_library = nullptr; void* xext_library = nullptr; Display* display = nullptr; Window window = 0; Pixmap back_buffer = 0; GC gc = nullptr; Visual* visual = nullptr; XImage* image = nullptr;
    SharedMemorySegmentInfo shared_image{};
    bool shared_image_attached = false;
    RenderPicture source_picture = 0, destination_picture = 0;
    std::array<XFontSet, 4> fonts{}; XIM input_method = nullptr; XIC input_context = nullptr;
    Atom delete_window = 0, clipboard = 0, utf8 = 0, paste_property = 0, targets = 0, atom_type = 0, string_type = 0;
    int depth = 0, width = 1060, height = 740, physical_width = 1060, physical_height = 740;
    int min_width = 600, min_height = 540, buffer_width = 0, buffer_height = 0;
    Rotation rotation = Rotation::none;
    bool redraw = true;
    uint64_t painted_revision = 0, painted_preview = 0;
    const CameraFrame* painted_frame = nullptr;
    std::string paste_target, clipboard_text;
#define FN(name) decltype(&name) name##_fn = nullptr
    FN(XOpenDisplay); FN(XCloseDisplay); FN(XDefaultScreen); FN(XDisplayWidth); FN(XDisplayHeight); FN(XDefaultVisual); FN(XDefaultDepth); FN(XRootWindow);
    FN(XCreateSimpleWindow); FN(XDestroyWindow); FN(XMapWindow); FN(XResizeWindow); FN(XStoreName); FN(XSelectInput);
    FN(XCreateGC); FN(XFreeGC); FN(XPending); FN(XNextEvent); FN(XSetForeground); FN(XFillRectangle); FN(XFillArc); FN(XDrawString);
    FN(XCreateImage); FN(XPutImage); FN(XFlush); FN(XSync); FN(XInternAtom); FN(XSetWMProtocols); FN(XCreatePixmap); FN(XFreePixmap); FN(XCopyArea);
    FN(XCreateFontSet); FN(XFreeFontSet); FN(XFreeStringList); FN(Xutf8DrawString); FN(Xutf8TextExtents);
    FN(XOpenIM); FN(XCloseIM); FN(XCreateIC); FN(XDestroyIC); FN(XSetICFocus); FN(XUnsetICFocus); FN(Xutf8LookupString); FN(XFilterEvent);
    FN(XConvertSelection); FN(XGetWindowProperty); FN(XDeleteProperty); FN(XFree); FN(XSetSelectionOwner); FN(XChangeProperty); FN(XSendEvent);
    FN(XSetClipRectangles); FN(XSetClipMask);
#undef FN
    RenderPictFormat* (*XRenderFindVisualFormat_fn)(Display*, Visual*) = nullptr;
    RenderPicture (*XRenderCreatePicture_fn)(Display*, Drawable, const RenderPictFormat*, unsigned long, const RenderPictureAttributes*) = nullptr;
    void (*XRenderFreePicture_fn)(Display*, RenderPicture) = nullptr;
    void (*XRenderSetPictureTransform_fn)(Display*, RenderPicture, RenderTransform*) = nullptr;
    void (*XRenderComposite_fn)(Display*, int, RenderPicture, RenderPicture, RenderPicture,
        int, int, int, int, int, int, unsigned, unsigned) = nullptr;
    Bool (*XShmQueryExtension_fn)(Display*) = nullptr;
    XImage* (*XShmCreateImage_fn)(Display*, Visual*, unsigned, int, char*, SharedMemorySegmentInfo*, unsigned, unsigned) = nullptr;
    Bool (*XShmAttach_fn)(Display*, SharedMemorySegmentInfo*) = nullptr;
    Bool (*XShmDetach_fn)(Display*, SharedMemorySegmentInfo*) = nullptr;
    Bool (*XShmPutImage_fn)(Display*, Drawable, GC, XImage*, int, int, int, int, unsigned, unsigned, Bool) = nullptr;
    int (*lookup)(XKeyEvent*, char*, int, KeySym*, void*) = nullptr;
    DemoUi ui{[this](const std::string& text, Font font) { return measure(text, font); }};
    void initialize() {
        std::setlocale(LC_CTYPE, ""); library = dlopen("libX11.so.6", RTLD_NOW | RTLD_LOCAL); require(library, "X11 runtime missing: install libx11-6");
#define LOAD(name) name##_fn = reinterpret_cast<decltype(name##_fn)>(dlsym(library, #name)); require(name##_fn, "Missing X11 function: " #name)
        LOAD(XOpenDisplay); LOAD(XCloseDisplay); LOAD(XDefaultScreen); LOAD(XDisplayWidth); LOAD(XDisplayHeight); LOAD(XDefaultVisual); LOAD(XDefaultDepth); LOAD(XRootWindow);
        LOAD(XCreateSimpleWindow); LOAD(XDestroyWindow); LOAD(XMapWindow); LOAD(XResizeWindow); LOAD(XStoreName); LOAD(XSelectInput);
        LOAD(XCreateGC); LOAD(XFreeGC); LOAD(XPending); LOAD(XNextEvent); LOAD(XSetForeground); LOAD(XFillRectangle); LOAD(XFillArc); LOAD(XDrawString);
        LOAD(XCreateImage); LOAD(XPutImage); LOAD(XFlush); LOAD(XSync); LOAD(XInternAtom); LOAD(XSetWMProtocols); LOAD(XCreatePixmap); LOAD(XFreePixmap); LOAD(XCopyArea);
        LOAD(XCreateFontSet); LOAD(XFreeFontSet); LOAD(XFreeStringList); LOAD(Xutf8DrawString); LOAD(Xutf8TextExtents);
        LOAD(XOpenIM); LOAD(XCloseIM); LOAD(XCreateIC); LOAD(XDestroyIC); LOAD(XSetICFocus); LOAD(XUnsetICFocus); LOAD(Xutf8LookupString); LOAD(XFilterEvent);
        LOAD(XConvertSelection); LOAD(XGetWindowProperty); LOAD(XDeleteProperty); LOAD(XFree); LOAD(XSetSelectionOwner); LOAD(XChangeProperty); LOAD(XSendEvent);
        LOAD(XSetClipRectangles); LOAD(XSetClipMask);
#undef LOAD
        lookup = reinterpret_cast<decltype(lookup)>(dlsym(library, "XLookupString")); require(lookup, "Missing X11 keyboard support");
        display = XOpenDisplay_fn(nullptr); require(display, "Cannot open DISPLAY. Start from an Ubuntu desktop terminal (X11 or XWayland).");
        xext_library = dlopen("libXext.so.6", RTLD_NOW | RTLD_LOCAL);
        if (xext_library) {
#define XLOAD(name) name##_fn = reinterpret_cast<decltype(name##_fn)>(dlsym(xext_library, #name))
            XLOAD(XShmQueryExtension); XLOAD(XShmCreateImage); XLOAD(XShmAttach); XLOAD(XShmDetach); XLOAD(XShmPutImage);
#undef XLOAD
            if (!XShmQueryExtension_fn || !XShmCreateImage_fn || !XShmAttach_fn || !XShmDetach_fn ||
                    !XShmPutImage_fn || !XShmQueryExtension_fn(display)) {
                dlclose(xext_library); xext_library = nullptr;
            }
        }
        const int screen = XDefaultScreen_fn(display); visual = XDefaultVisual_fn(display, screen); depth = XDefaultDepth_fn(display, screen); require(depth == 24 || depth == 32, "This Demo requires a 24/32-bit desktop visual");
        const int screen_width = XDisplayWidth_fn(display, screen), screen_height = XDisplayHeight_fn(display, screen);
        const char* requested_rotation = std::getenv("HIFACE_DISPLAY_ROTATE");
        if (requested_rotation && (!std::strcmp(requested_rotation, "left") || !std::strcmp(requested_rotation, "ccw") || !std::strcmp(requested_rotation, "90ccw"))) rotation = Rotation::left;
        else if (requested_rotation && (!std::strcmp(requested_rotation, "right") || !std::strcmp(requested_rotation, "cw") || !std::strcmp(requested_rotation, "90cw"))) rotation = Rotation::right;
        else require(!requested_rotation || !*requested_rotation || !std::strcmp(requested_rotation, "none"),
            "HIFACE_DISPLAY_ROTATE must be left/ccw/90ccw, right/cw/90cw or none");
        const bool embedded_display = screen_width <= 1280 || screen_height <= 720;
        physical_width = embedded_display ? screen_width : std::max(960, std::min(physical_width, screen_width - 48));
        physical_height = embedded_display ? screen_height : std::max(700, std::min(physical_height, screen_height - 80));
        width = rotation == Rotation::none ? physical_width : physical_height;
        height = rotation == Rotation::none ? physical_height : physical_width;
        min_width = std::min(min_width, width); min_height = std::min(min_height, height);
        const int window_x = embedded_display ? 0 : 20, window_y = embedded_display ? 0 : 20;
        window = XCreateSimpleWindow_fn(display, XRootWindow_fn(display, screen), window_x, window_y, physical_width, physical_height, 0, 0, pixel(0xf5f5f7)); require(window, "Cannot create window");
        XStoreName_fn(display, window, "HiFace - Linux Demo"); XSelectInput_fn(display, window, ExposureMask | KeyPressMask | ButtonPressMask | PointerMotionMask | StructureNotifyMask | FocusChangeMask | PropertyChangeMask);
        delete_window = XInternAtom_fn(display, "WM_DELETE_WINDOW", False); clipboard = XInternAtom_fn(display, "CLIPBOARD", False); utf8 = XInternAtom_fn(display, "UTF8_STRING", False);
        paste_property = XInternAtom_fn(display, "HIFACE_PASTE", False); targets = XInternAtom_fn(display, "TARGETS", False); atom_type = XInternAtom_fn(display, "ATOM", False); string_type = XInternAtom_fn(display, "STRING", False);
        XSetWMProtocols_fn(display, window, &delete_window, 1); gc = XCreateGC_fn(display, window, 0, nullptr); require(gc, "Cannot create graphics context");
        if (embedded_display || rotation != Rotation::none) {
            const Atom wm_state = XInternAtom_fn(display, "_NET_WM_STATE", False);
            const Atom fullscreen = XInternAtom_fn(display, "_NET_WM_STATE_FULLSCREEN", False);
            XChangeProperty_fn(display, window, wm_state, atom_type, 32, PropModeReplace,
                reinterpret_cast<const unsigned char*>(&fullscreen), 1);
        }
        if (rotation != Rotation::none) {
            render_library = dlopen("libXrender.so.1", RTLD_NOW | RTLD_LOCAL);
            require(render_library, "Internal display rotation needs libXrender.so.1");
#define RLOAD(name) name##_fn = reinterpret_cast<decltype(name##_fn)>(dlsym(render_library, #name)); require(name##_fn, "Missing Xrender function: " #name)
            RLOAD(XRenderFindVisualFormat); RLOAD(XRenderCreatePicture); RLOAD(XRenderFreePicture);
            RLOAD(XRenderSetPictureTransform); RLOAD(XRenderComposite);
#undef RLOAD
            auto* format = XRenderFindVisualFormat_fn(display, visual); require(format, "Xrender does not support the desktop visual");
            destination_picture = XRenderCreatePicture_fn(display, window, format, 0, nullptr);
            require(destination_picture, "Cannot create rotated display target");
        }
        // The target Buildroot image ships the X11 misc bitmap font family,
        // not Helvetica. Name its installed aliases first so kiosk typography
        // retains the requested pixel sizes instead of silently using `fixed`.
        // 目标 Buildroot 镜像自带 X11 misc 位图字体而非 Helvetica；优先指定已安装别名，以保留要求的像素字号，避免静默回退 fixed。
        const char* patterns[] = {"7x14,fixed", "6x12,fixed", "10x20,9x18B,fixed", "9x18B,10x20,fixed"};
        for (size_t i = 0; i < fonts.size(); ++i) { char** missing = nullptr; int count = 0; char* fallback = nullptr; fonts[i] = XCreateFontSet_fn(display, patterns[i], &missing, &count, &fallback); if (missing) XFreeStringList_fn(missing); }
        input_method = XOpenIM_fn(display, nullptr, nullptr, nullptr);
        if (input_method) input_context = XCreateIC_fn(input_method, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, window, XNFocusWindow, window, static_cast<char*>(nullptr));
        XMapWindow_fn(display, window); XFlush_fn(display);
    }
    ~Impl() {
        if (source_picture) XRenderFreePicture_fn(display, source_picture);
        if (destination_picture) XRenderFreePicture_fn(display, destination_picture);
        destroyPreviewImage(); if (back_buffer) XFreePixmap_fn(display, back_buffer);
        if (input_context) XDestroyIC_fn(input_context); if (input_method) XCloseIM_fn(input_method);
        for (auto font : fonts) if (font) XFreeFontSet_fn(display, font);
        if (gc) XFreeGC_fn(display, gc); if (window) XDestroyWindow_fn(display, window); if (display) XCloseDisplay_fn(display);
        if (render_library) dlclose(render_library); if (xext_library) dlclose(xext_library); if (library) dlclose(library);
    }
    unsigned long pixel(uint32_t rgb) const {
        if (visual->red_mask == 0xff0000 && visual->green_mask == 0xff00 && visual->blue_mask == 0xff) return rgb;
        auto channel = [](uint32_t v, unsigned long mask) { if (!mask) return 0UL; unsigned shift = 0; while (!((mask >> shift) & 1)) ++shift; return ((v * (mask >> shift) + 127) / 255 << shift) & mask; };
        return channel((rgb >> 16) & 255, visual->red_mask) | channel((rgb >> 8) & 255, visual->green_mask) | channel(rgb & 255, visual->blue_mask);
    }
    int measure(const std::string& text, Font font) const {
        if (fonts[size_t(font)]) { XRectangle ink{}, logical{}; Xutf8TextExtents_fn(fonts[size_t(font)], text.c_str(), text.size(), &ink, &logical); return logical.width; }
        return int(text.size()) * 7;
    }
    void box(const Draw& d) {
        const auto r = d.bounds; if (r.w <= 0 || r.h <= 0) return; const int radius = std::min({d.radius, r.w / 2, r.h / 2});
        XSetForeground_fn(display, gc, pixel(d.color));
        if (!radius) { XFillRectangle_fn(display, back_buffer, gc, r.x, r.y, r.w, r.h); return; }
        XFillRectangle_fn(display, back_buffer, gc, r.x + radius, r.y, r.w - radius * 2, r.h); XFillRectangle_fn(display, back_buffer, gc, r.x, r.y + radius, r.w, r.h - radius * 2);
        XFillArc_fn(display, back_buffer, gc, r.x, r.y, radius * 2, radius * 2, 90 * 64, 90 * 64);
        XFillArc_fn(display, back_buffer, gc, r.x + r.w - radius * 2, r.y, radius * 2, radius * 2, 0, 90 * 64);
        XFillArc_fn(display, back_buffer, gc, r.x, r.y + r.h - radius * 2, radius * 2, radius * 2, 180 * 64, 90 * 64);
        XFillArc_fn(display, back_buffer, gc, r.x + r.w - radius * 2, r.y + r.h - radius * 2, radius * 2, radius * 2, 270 * 64, 90 * 64);
    }
    void destroyPreviewImage() {
        if (!image) return;
        if (shared_image_attached) {
            XShmDetach_fn(display, &shared_image);
            XSync_fn(display, False);
            const auto address = shared_image.address;
            image->data = nullptr;
            image->f.destroy_image(image);
            shmdt(address);
            shmctl(shared_image.id, IPC_RMID, nullptr);
            shared_image = {};
            shared_image_attached = false;
        } else image->f.destroy_image(image);
        image = nullptr;
    }
    void createPreviewImage(int w, int h) {
        destroyPreviewImage();
        if (xext_library) {
            shared_image = {};
            auto* shared = XShmCreateImage_fn(display, visual, depth, ZPixmap, nullptr, &shared_image, w, h);
            if (shared && shared->bits_per_pixel == 32) {
                const size_t bytes = static_cast<size_t>(shared->bytes_per_line) * shared->height;
                shared_image.id = shmget(IPC_PRIVATE, bytes, IPC_CREAT | 0600);
                if (shared_image.id >= 0) {
                    shared_image.address = static_cast<char*>(shmat(shared_image.id, nullptr, 0));
                    shared_image.read_only = False;
                    if (shared_image.address != reinterpret_cast<char*>(-1)) {
                        shared->data = shared_image.address;
                        if (XShmAttach_fn(display, &shared_image)) {
                            XSync_fn(display, False);
                            // Remove the kernel name immediately. Both Xorg and
                            // the client keep their attachments, and a crash or
                            // service restart can no longer leak the segment.
                            // 立即删除内核名称；Xorg 与客户端仍保留各自连接，崩溃或服务重启后不再泄漏共享段。
                            shmctl(shared_image.id, IPC_RMID, nullptr);
                            image = shared;
                            shared_image_attached = true;
                            return;
                        }
                        shared->data = nullptr;
                        shmdt(shared_image.address);
                    }
                    shmctl(shared_image.id, IPC_RMID, nullptr);
                }
                shared->data = nullptr;
            }
            if (shared) shared->f.destroy_image(shared);
            shared_image = {};
        }
        image = XCreateImage_fn(display, visual, depth, ZPixmap, 0, nullptr, w, h, 32, 0);
        require(image && image->bits_per_pixel == 32, "Unsupported X11 image format");
        image->data = static_cast<char*>(std::calloc(image->bytes_per_line, image->height));
        require(image->data, "Cannot allocate preview image");
    }
    void picture(const Draw& d) {
        const int w = d.bounds.w, h = d.bounds.h; if (!d.pixels || w < 1 || h < 1) return;
        if (!image || image->width != w || image->height != h) {
            createPreviewImage(w, h);
        }
        // XShm uploads remain asynchronous. Finish the previous frame before
        // reusing its shared storage; the local round trip is still much less
        // work than copying the complete image through the X11 socket.
        // XShm 上传是异步的；复用共享缓冲前先完成上一帧，本地往返仍比经 X11 套接字复制完整图像更省。
        if (shared_image_attached) XSync_fn(display, False);
        const bool native = d.x11_bgrx && d.source_width == w && d.source_height == h &&
            visual->red_mask == 0xff0000 && visual->green_mask == 0xff00 && visual->blue_mask == 0xff;
        if (native && !d.mirror) {
            for (int y = 0; y < h; ++y)
                std::memcpy(image->data + size_t(y) * image->bytes_per_line,
                    d.pixels + size_t(y) * w * 4, size_t(w) * 4);
        } else for (int y = 0; y < h; ++y) {
            auto* row = reinterpret_cast<uint32_t*>(image->data + size_t(y) * image->bytes_per_line);
            for (int x = 0; x < w; ++x) {
                int sx = x * d.source_width / w; if (d.mirror) sx = d.source_width - 1 - sx;
                const auto* p = d.pixels + (size_t(y * d.source_height / h) * d.source_width + sx) * 4;
                row[x] = native ? *reinterpret_cast<const uint32_t*>(p)
                    : d.x11_bgrx ? pixel(uint32_t(p[2]) << 16 | uint32_t(p[1]) << 8 | p[0])
                    : pixel(uint32_t(p[0]) << 16 | uint32_t(p[1]) << 8 | p[2]);
            }
        }
        if (shared_image_attached)
            XShmPutImage_fn(display, back_buffer, gc, image, 0, 0, d.bounds.x, d.bounds.y, w, h, False);
        else XPutImage_fn(display, back_buffer, gc, image, 0, 0, d.bounds.x, d.bounds.y, w, h);
    }
    void paint(const ViewState& state) {
        if (!window || !ui.open()) return;
        if (!redraw && painted_revision == state.visual_revision && painted_preview == state.preview_sequence &&
            painted_frame == state.frame.get() && width == buffer_width && height == buffer_height) return;
        if (!back_buffer || width != buffer_width || height != buffer_height) {
            if (source_picture) { XRenderFreePicture_fn(display, source_picture); source_picture = 0; }
            if (back_buffer) XFreePixmap_fn(display, back_buffer);
            back_buffer = XCreatePixmap_fn(display, window, width, height, depth); require(back_buffer, "Cannot create back buffer");
            buffer_width = width; buffer_height = height;
            if (rotation != Rotation::none) {
                auto* format = XRenderFindVisualFormat_fn(display, visual);
                source_picture = XRenderCreatePicture_fn(display, back_buffer, format, 0, nullptr);
                require(source_picture, "Cannot create rotated scene source");
                RenderTransform transform{};
                transform.matrix[2][2] = fixed(1);
                if (rotation == Rotation::left) {
                    transform.matrix[0][1] = fixed(1);
                    transform.matrix[1][0] = fixed(-1);
                    transform.matrix[1][2] = fixed(height);
                } else {
                    transform.matrix[0][1] = fixed(-1);
                    transform.matrix[0][2] = fixed(width);
                    transform.matrix[1][0] = fixed(1);
                }
                XRenderSetPictureTransform_fn(display, source_picture, &transform);
            }
        }
        const auto& scene = ui.render(state, width, height);
        for (const auto& d : scene.draws) {
            const auto clip = d.clip.intersect({0, 0, width, height}); if (!clip.w || !clip.h) continue;
            XRectangle rectangle{static_cast<short>(clip.x), static_cast<short>(clip.y), static_cast<unsigned short>(clip.w), static_cast<unsigned short>(clip.h)};
            XSetClipRectangles_fn(display, gc, 0, 0, &rectangle, 1, Unsorted);
            if (d.kind == Draw::Box) box(d);
            else if (d.kind == Draw::Image) picture(d);
            else { XSetForeground_fn(display, gc, pixel(d.color)); if (fonts[size_t(d.font)]) Xutf8DrawString_fn(display, back_buffer, fonts[size_t(d.font)], gc, d.bounds.x, d.bounds.y, d.text.c_str(), d.text.size()); else XDrawString_fn(display, back_buffer, gc, d.bounds.x, d.bounds.y, d.text.c_str(), d.text.size()); }
        }
        XSetClipMask_fn(display, gc, None);
        if (rotation == Rotation::none) XCopyArea_fn(display, back_buffer, window, gc, 0, 0, width, height, 0, 0);
        else XRenderComposite_fn(display, kPictOpSrc, source_picture, 0, destination_picture,
            0, 0, 0, 0, 0, 0, physical_width, physical_height);
        XFlush_fn(display);
        painted_revision = state.visual_revision; painted_preview = state.preview_sequence;
        painted_frame = state.frame.get(); redraw = false;
    }
    void key(XKeyEvent& event, const ViewState& state) {
        char buffer[256]{}; KeySym symbol = 0; Status status = 0;
        int count = input_context ? Xutf8LookupString_fn(input_context, &event, buffer, sizeof(buffer), &symbol, &status) : lookup(&event, buffer, sizeof(buffer), &symbol, nullptr);
        if (count < 0 || count > int(sizeof(buffer))) count = 0;
        const bool ctrl = event.state & ControlMask, shift = event.state & ShiftMask;
        if (ctrl && (symbol == 'v' || symbol == 'V') && !ui.focusedField().empty()) { paste_target = ui.focusedField(); XConvertSelection_fn(display, clipboard, utf8, paste_property, window, event.time); return; }
        if (ctrl && (symbol == 'c' || symbol == 'C')) { const auto text = ui.selectedText(); if (!text.empty()) { clipboard_text = text; XSetSelectionOwner_fn(display, clipboard, window, event.time); } return; }
        Key key = Key::Text;
        if (ctrl && (symbol == 'a' || symbol == 'A')) key = Key::SelectAll;
        else switch (symbol) {
            case 0xff1b: key = Key::Escape; break; case 0xff0d: key = Key::Enter; break; case 0xff08: key = Key::Backspace; break; case 0xffff: key = Key::Delete; break;
            case 0xff51: key = Key::Left; break; case 0xff53: key = Key::Right; break; case 0xff52: key = Key::Up; break; case 0xff54: key = Key::Down; break; case 0xff55: key = Key::PreviousPage; break; case 0xff56: key = Key::NextPage; break; case 0xff50: key = Key::Home; break; case 0xff57: key = Key::End; break;
            case 0xff09: case 0xfe20: key = Key::Tab; break;
            case 0xffbe: key = Key::Page1; break; case 0xffbf: key = Key::Page2; break; case 0xffc0: key = Key::Page3; break; case 0xffc1: key = Key::Page4; break; case 0xffc2: key = Key::Page5; break;
            default: if (ctrl) return;
        }
        if (key != Key::Text || count) ui.key(key, std::string(buffer, count), shift, state);
    }
    void selection(const XSelectionRequestEvent& request) {
        const auto property = request.property ? request.property : request.target; XEvent reply{}; auto& e = reply.xselection;
        e.type = SelectionNotify; e.display = display; e.requestor = request.requestor; e.selection = request.selection; e.target = request.target; e.time = request.time; e.property = None;
        if (request.target == targets) { const Atom types[] = {targets, utf8}; XChangeProperty_fn(display, request.requestor, property, atom_type, 32, PropModeReplace, reinterpret_cast<const unsigned char*>(types), 2); e.property = property; }
        else if (request.target == utf8 || request.target == string_type && std::all_of(clipboard_text.begin(), clipboard_text.end(), [](unsigned char c) { return c < 128; })) {
            XChangeProperty_fn(display, request.requestor, property, request.target, 8, PropModeReplace, reinterpret_cast<const unsigned char*>(clipboard_text.data()), clipboard_text.size()); e.property = property;
        }
        XSendEvent_fn(display, request.requestor, False, 0, &reply); XFlush_fn(display);
    }
    std::pair<int, int> logicalPoint(int x, int y) const {
        if (rotation == Rotation::left) return {y, height - 1 - x};
        if (rotation == Rotation::right) return {width - 1 - y, x};
        return {x, y};
    }
};
DesktopWindow::DesktopWindow() : impl_(new Impl) { impl_->initialize(); }
DesktopWindow::~DesktopWindow() = default;
bool DesktopWindow::open() const { return impl_->window && impl_->ui.open(); }
void DesktopWindow::paint(const ViewState& state) { impl_->paint(state); }
void DesktopWindow::smokePage(unsigned index) { impl_->ui.pageForSmoke(index); impl_->redraw = true; }
std::vector<Command> DesktopWindow::events(const ViewState& state) {
    auto& ui = *impl_;
    while (ui.XPending_fn(ui.display)) {
        XEvent event{}; ui.XNextEvent_fn(ui.display, &event); if (ui.XFilterEvent_fn(&event, ui.window)) continue;
        ui.redraw = true;
        if (event.type == ClientMessage && static_cast<Atom>(event.xclient.data.l[0]) == ui.delete_window) ui.ui.requestClose(state);
        if (event.type == DestroyNotify) { ui.window = 0; }
        if (event.type == FocusIn && ui.input_context) ui.XSetICFocus_fn(ui.input_context); if (event.type == FocusOut && ui.input_context) ui.XUnsetICFocus_fn(ui.input_context);
        if (event.type == ConfigureNotify) {
            const int min_physical_width = ui.rotation == Impl::Rotation::none ? ui.min_width : ui.min_height;
            const int min_physical_height = ui.rotation == Impl::Rotation::none ? ui.min_height : ui.min_width;
            ui.physical_width = std::max(min_physical_width, event.xconfigure.width);
            ui.physical_height = std::max(min_physical_height, event.xconfigure.height);
            ui.width = ui.rotation == Impl::Rotation::none ? ui.physical_width : ui.physical_height;
            ui.height = ui.rotation == Impl::Rotation::none ? ui.physical_height : ui.physical_width;
            if (ui.physical_width != event.xconfigure.width || ui.physical_height != event.xconfigure.height)
                ui.XResizeWindow_fn(ui.display, ui.window, ui.physical_width, ui.physical_height);
        }
        try {
            if (event.type == MotionNotify) { const auto point = ui.logicalPoint(event.xmotion.x, event.xmotion.y); ui.ui.pointer(point.first, point.second); }
            if (event.type == KeyPress) ui.key(event.xkey, state);
            if (event.type == ButtonPress && event.xbutton.button == Button1) { const auto point = ui.logicalPoint(event.xbutton.x, event.xbutton.y); ui.ui.click(point.first, point.second, state); }
            if (event.type == ButtonPress && (event.xbutton.button == Button4 || event.xbutton.button == Button5)) { const auto point = ui.logicalPoint(event.xbutton.x, event.xbutton.y); ui.ui.scroll(event.xbutton.button == Button4 ? -1 : 1, point.first, point.second, state); }
            if (event.type == SelectionRequest) ui.selection(event.xselectionrequest);
            if (event.type == SelectionClear) ui.clipboard_text.clear();
            if (event.type == SelectionNotify && event.xselection.property == ui.paste_property) {
                Atom type = 0; int format = 0; unsigned long count = 0, remaining = 0; unsigned char* value = nullptr;
                if (ui.XGetWindowProperty_fn(ui.display, ui.window, ui.paste_property, 0, 1024, True, ui.utf8, &type, &format, &count, &remaining, &value) == Success && type == ui.utf8 && format == 8 && !remaining && value) ui.ui.paste(std::string(reinterpret_cast<char*>(value), count), ui.paste_target);
                if (value) ui.XFree_fn(value); ui.XDeleteProperty_fn(ui.display, ui.window, ui.paste_property); ui.paste_target.clear();
            }
        } catch (const DemoError& error) { ui.ui.reportError(error.message); }
        catch (const std::exception& error) { ui.ui.reportError(error.what()); }
    }
    return ui.ui.takeCommands();
}
}
