#pragma once
#include "types.h"
#include <functional>
#include <optional>

namespace hiface_demo {
struct ImageSearchFace {
    HiFaceFaceV1 face{};
    HiFaceStatus status = HIFACE_STATUS_OK;
    Image preview; // owned aligned RGBA; empty for unsuccessful captures / 独占的对齐 RGBA；采集未成功时为空。
    std::optional<hiface::SearchMatch> match;
    std::string label;
};
struct ImageSearchResult {
    std::vector<ImageSearchFace> faces;
    size_t matched = 0;
    bool empty_library = false;
    std::string summary;
};
// Demo policy matches Android: whole library, Top-1 / 0.80, no liveness.
// A stale request returns no result and cannot publish partial old identities.
// Demo 策略与 Android 一致：全库、Top-1 / 0.80，不执行活体。过期请求不返回结果，也不发布部分旧身份。
std::optional<ImageSearchResult> resolveImageFaces(const Gallery& database,
    std::vector<hiface::ImageFaceCapture> captures, const std::function<bool()>& is_current = {});
std::optional<ImageSearchResult> searchPhoto(hiface::Client& client, const Gallery& database,
    const Image& image, const std::function<bool()>& is_current = {});
}
