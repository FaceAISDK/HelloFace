#pragma once
#include "types.h"
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace hiface_demo {
struct CameraDevice { std::wstring name, symbolic_link; };
class Camera {
public:
    static std::vector<CameraDevice> devices();
    explicit Camera(unsigned device_index = 0, const std::wstring& symbolic_link = {}, unsigned width = 640, unsigned height = 480);
    ~Camera();
    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;
    // At most one pending frame; false means a bounded wait elapsed. Device
    // errors throw, allowing the worker to close the source and release SDK.
    // 最多保留一帧待处理数据；false 表示有界等待超时。设备错误抛出异常，由工作线程关闭相机源并释放 SDK。
    bool next(CameraFrame& frame, std::chrono::milliseconds timeout);
    uint64_t capturedFrames() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
