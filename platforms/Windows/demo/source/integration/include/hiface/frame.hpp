#pragma once
#include "hiface/hiface_c_api.h"
#include <cstdint>
#include <array>
#include <vector>

namespace hiface {
using Feature = std::array<float, HIFACE_FACE_FEATURE_DIMENSION>;
struct Image {
    int width = 0, height = 0;
    std::vector<uint8_t> rgba;
};
struct CameraFrame {
    int width = 0, height = 0;
    HiFaceYuvRangeV1 range = HIFACE_YUV_RANGE_VIDEO;
    HiFaceYuvMatrixV1 matrix = HIFACE_YUV_MATRIX_BT601;
    std::vector<uint8_t> nv12;
    int rotation_degrees = 0;
    HiFaceYuv420FrameV1 view() const;
    // Display conversion only. Enrollment and inference use the Core YUV path.
    // 仅供显示转换；录入与推理使用 Core 的 YUV 路径。
    std::vector<uint8_t> previewBgra() const;
    Image displayBgra(bool mirror) const;
};
}
