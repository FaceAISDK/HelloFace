#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace hiface_linux_demo {
struct LightSample {
    bool valid = false;
    double exposure_fraction = 0, gain_db = 0, luma = 0;
};

// Exposure is normalized to the sensor's current maximum. CV2002 gain comes
// from its vendor IQ calibration (0.3 dB per register unit). This is an AE
// demand estimate, not a lux reading from an unverified ADC input.
// 曝光按传感器当前上限归一化，CV2002 增益采用厂商 IQ 标定的每寄存器单位 0.3 dB；这是自动曝光需求估计，不是未经验证 ADC 的照度读数。
class AutoLightPolicy {
public:
    int update(const LightSample& sample, int64_t now) {
        if (!sample.valid || !std::isfinite(sample.exposure_fraction) ||
                !std::isfinite(sample.gain_db) || !std::isfinite(sample.luma)) {
            reset(); return 0;
        }
        const double demand = sample.exposure_fraction * std::pow(10.0, sample.gain_db / 20.0)
            * 96.0 / std::clamp(sample.luma, 8.0, 255.0);
        // Require substantial gain as well as near-limit exposure, so a dark
        // object passing in front of an otherwise bright camera does not flash.
        // 同时要求较高增益与接近上限的曝光，避免明亮环境中短暂经过的暗物体触发闪灯。
        const bool dark = sample.exposure_fraction >= .85 && sample.gain_db >= 12 && demand >= 6;
        const bool bright = demand <= 1.5;
        if (!on_) {
            bright_since_ = -1;
            if (!dark) dark_since_ = -1;
            else if (dark_since_ < 0) dark_since_ = now;
            else if (now - dark_since_ >= 2000) {
                on_ = true; on_since_ = now; dark_since_ = -1; last_step_ = now - 500;
            }
        } else {
            if (!bright) bright_since_ = -1;
            else if (bright_since_ < 0) bright_since_ = now;
            else if (now - bright_since_ >= 6000 && now - on_since_ >= 15000) {
                on_ = false; bright_since_ = -1;
            }
        }
        // A fixed moderate 25% level avoids chasing AE's reaction to our own
        // illumination. The wide on/off hysteresis and hold time suppress flicker.
        // 固定适中的 25% 亮度，避免追逐自身补光引起的自动曝光变化；较宽的开关迟滞与保持时间用于抑制闪烁。
        const int target = on_ ? 25 : 0;
        if (now - last_step_ >= 500) {
            duty_ += std::clamp(target - duty_, -5, 5);
            last_step_ = now;
        }
        return duty_;
    }
    void reset() {
        on_ = false; duty_ = 0;
        dark_since_ = bright_since_ = -1;
        on_since_ = last_step_ = 0;
    }
private:
    bool on_ = false;
    int duty_ = 0;
    int64_t dark_since_ = -1, bright_since_ = -1, on_since_ = 0, last_step_ = 0;
};
}
