#pragma once
#include "camera.h"
#include "light_policy.h"

namespace hiface_linux_demo {
// The capture thread alone owns the light controller. Sysfs preparation and
// permission grants are performed by the board's root startup script.
// 仅采集线程持有补光控制器；sysfs 初始化与访问权限由板级 root 启动脚本配置。
class CameraLighting {
public:
    CameraLighting();
    ~CameraLighting();
    void update(const CameraFrame* frame, bool active);
    void stop();
    const std::string& status() const { return status_; }
private:
    bool setDuty(int percent);
    std::string pwm_path_, sensor_path_, status_;
    AutoLightPolicy policy_;
    int sensor_fd_ = -1, duty_ = -1;
    int64_t next_sample_ = 0, last_frame_ = 0;
};
}
