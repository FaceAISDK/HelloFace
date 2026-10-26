#pragma once
#include "app.h"
#include <memory>
namespace hiface_linux_demo {
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
