#include "lighting.h"
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace hiface_linux_demo {
namespace {
bool writeValue(const std::string& file, int value) {
    const int fd = open(file.c_str(), O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;
    const auto text = std::to_string(value);
    ssize_t written;
    do { written = write(fd, text.data(), text.size()); } while (written < 0 && errno == EINTR);
    const int saved = errno;
    close(fd); errno = saved;
    return written == static_cast<ssize_t>(text.size());
}
bool control(int fd, uint32_t id, int& value) {
    v4l2_control request{}; request.id = id;
    if (ioctl(fd, VIDIOC_G_CTRL, &request) < 0) return false;
    value = request.value; return true;
}
double meanLuma(const CameraFrame& frame) {
    if (frame.width < 2 || frame.height < 2 ||
            frame.i420.size() < static_cast<size_t>(frame.width) * frame.height) return -1;
    uint64_t total = 0, samples = 0;
    // Subsample the central 80%; no full-frame conversion or extra image copy.
    // 对中心 80% 区域降采样，不做整帧转换或额外图像复制。
    for (int y = frame.height / 10; y < frame.height * 9 / 10; y += 8)
        for (int x = frame.width / 10; x < frame.width * 9 / 10; x += 8) {
            total += frame.i420[static_cast<size_t>(y) * frame.width + x]; ++samples;
        }
    const double value = samples ? double(total) / samples : -1;
    return frame.range == HIFACE_YUV_RANGE_VIDEO ? std::clamp((value - 16) * 255 / 219, 0.0, 255.0) : value;
}
}
CameraLighting::CameraLighting() {
    const char* pwm = std::getenv("HIFACE_LIGHT_PWM_PATH");
    const char* sensor = std::getenv("HIFACE_LIGHT_SENSOR_DEVICE");
    if (pwm && *pwm && sensor && *sensor) {
        pwm_path_ = pwm; sensor_path_ = sensor;
        status_ = "White auto: waiting | IR: OFF";
        setDuty(0);
    }
}
CameraLighting::~CameraLighting() {
    stop();
    if (sensor_fd_ >= 0) close(sensor_fd_);
}
bool CameraLighting::setDuty(int percent) {
    if (pwm_path_.empty() || percent == duty_) return true;
    percent = std::clamp(percent, 0, 25);
    // Period is fixed to 1 ms by the privileged startup script. The Demo can
    // only write duty/enable on this one white-light channel.
    // 特权启动脚本将周期固定为 1 ms；Demo 仅可写入此白光通道的 duty/enable。
    if (!writeValue(pwm_path_ + "/duty_cycle", percent * 10000) ||
            !writeValue(pwm_path_ + "/enable", percent ? 1 : 0)) {
        writeValue(pwm_path_ + "/duty_cycle", 0);
        writeValue(pwm_path_ + "/enable", 0);
        duty_ = -1;
        status_ = "White auto: output unavailable | IR: OFF";
        return false;
    }
    if (duty_ != percent) std::fprintf(stderr, "[Lighting] white=%d%% IR=off\n", percent);
    duty_ = percent;
    return true;
}
void CameraLighting::stop() {
    policy_.reset(); last_frame_ = 0;
    if (!pwm_path_.empty() && setDuty(0)) status_ = "White auto: OFF | IR: OFF";
}
void CameraLighting::update(const CameraFrame* frame, bool active) {
    if (pwm_path_.empty()) return;
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if (!active) { stop(); return; }
    if (frame) last_frame_ = now;
    if (!last_frame_ || now - last_frame_ > 1500) { stop(); return; }
    if (!frame || now < next_sample_) return;
    next_sample_ = now + 250;
    if (sensor_fd_ < 0) sensor_fd_ = open(sensor_path_.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    int exposure = 0, gain = 0;
    v4l2_queryctrl maximum{}; maximum.id = V4L2_CID_EXPOSURE;
    LightSample sample;
    sample.luma = meanLuma(*frame);
    sample.valid = sensor_fd_ >= 0 && sample.luma >= 0 &&
        ioctl(sensor_fd_, VIDIOC_QUERYCTRL, &maximum) == 0 && maximum.maximum > 0 &&
        control(sensor_fd_, V4L2_CID_EXPOSURE, exposure) &&
        control(sensor_fd_, V4L2_CID_ANALOGUE_GAIN, gain);
    sample.exposure_fraction = maximum.maximum > 0 ? double(exposure) / maximum.maximum : 0;
    sample.gain_db = gain * .3;
    if (!sample.valid) {
        policy_.reset(); setDuty(0);
        status_ = "White auto: sensor unavailable | IR: OFF";
        if (sensor_fd_ >= 0) { close(sensor_fd_); sensor_fd_ = -1; }
        next_sample_ = now + 2000;
        return;
    }
    if (!setDuty(policy_.update(sample, now))) { policy_.reset(); return; }
    status_ = "White auto: " + (duty_ ? std::to_string(duty_) + "%" : "OFF") + " | IR: OFF";
}
}
