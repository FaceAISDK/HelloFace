#pragma once
#include "app.h"
#include <memory>
#include <cstdlib>
#include <utility>
namespace hiface_linux_demo {
// Xlib protocol errors arrive during a later sync, even if the issuing call
// returned success. Only the optional request's own errors may become a fallback.
// All Demo Xlib calls run on the UI thread; no other thread changes this handler.
// Xlib 协议错误会在后续同步时到达；只把本次可选请求的错误转为回退。
// Demo 的 Xlib 调用均在 UI 线程，不存在其他线程修改全局错误处理器。
template<class DisplayType, class ErrorEvent>
class OptionalX11Request {
public:
    using ErrorHandler = int (*)(DisplayType*, ErrorEvent*);
    using SetHandler = ErrorHandler (*)(ErrorHandler);
    using Sync = int (*)(DisplayType*, int);
    using NextRequest = unsigned long (*)(DisplayType*);

    OptionalX11Request(DisplayType* display, SetHandler set_handler, Sync sync, NextRequest next_request)
        : display_(display), set_handler_(set_handler), sync_(sync), next_request_(next_request) {
        // Deliver earlier errors to their original handler before opening the trap.
        // 先让旧错误交给原处理器，不把其他绘制错误误当作可选能力失败。
        sync_(display_, 0);
        first_serial_ = next_request_(display_);
        previous_trap_ = active_;
        previous_handler_ = set_handler_(handleError);
        active_ = this;
    }
    OptionalX11Request(const OptionalX11Request&) = delete;
    OptionalX11Request& operator=(const OptionalX11Request&) = delete;
    ~OptionalX11Request() {
        finish();
        set_handler_(previous_handler_);
        active_ = previous_trap_;
    }
    bool finish() {
        if (!finished_) {
            sync_(display_, 0);
            finished_ = true;
        }
        return !failed_;
    }

private:
    static int handleError(DisplayType* display, ErrorEvent* error) {
        for (auto* trap = active_; trap; trap = trap->previous_trap_) {
            if (display == trap->display_ &&
                error->serial - trap->first_serial_ < trap->next_request_(display) - trap->first_serial_) {
                trap->failed_ = true;
                return 0;
            }
            if (trap->previous_handler_ != handleError) {
                if (trap->previous_handler_) return trap->previous_handler_(display, error);
                break;
            }
        }
        // A null previous handler means Xlib's fatal default; do not swallow
        // unrelated protocol failures while an optional operation is pending.
        // 空的旧处理器代表 Xlib 的退出默认行为，不能吞掉无关协议错误。
        std::exit(EXIT_FAILURE);
    }
    inline static OptionalX11Request* active_ = nullptr;
    DisplayType* display_;
    SetHandler set_handler_;
    Sync sync_;
    NextRequest next_request_;
    ErrorHandler previous_handler_ = nullptr;
    OptionalX11Request* previous_trap_ = nullptr;
    unsigned long first_serial_ = 0;
    bool failed_ = false, finished_ = false;
};

template<class DisplayType, class ErrorEvent, class Request>
bool tryOptionalX11Request(DisplayType* display, typename OptionalX11Request<DisplayType, ErrorEvent>::SetHandler set_handler,
                          typename OptionalX11Request<DisplayType, ErrorEvent>::Sync sync, typename OptionalX11Request<DisplayType, ErrorEvent>::NextRequest next_request,
                          Request&& request) {
    OptionalX11Request<DisplayType, ErrorEvent> trap(display, set_handler, sync, next_request);
    const bool sent = std::forward<Request>(request)();
    const bool accepted = trap.finish();
    return sent && accepted;
}

class DesktopWindow {
public:
    DesktopWindow();
    ~DesktopWindow();
    std::vector<Command> events(const ViewState& state);
    bool open() const;
    void paint(const ViewState& state);
    void smokePage(unsigned page);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
