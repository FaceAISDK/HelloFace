#pragma once
#include "gallery.h"
namespace hiface_linux_demo {
struct ImageFaceMatch {
    float x = 0, y = 0, width = 0, height = 0;
    HiFaceStatus capture_status = HIFACE_STATUS_OK, search_status = HIFACE_STATUS_OK;
    Image preview;
    std::vector<GalleryEntry> matches;
    std::string label;
    bool matched() const { return capture_status == HIFACE_STATUS_OK && search_status == HIFACE_STATUS_OK && !matches.empty(); }
};
struct ImageSearchReport {
    std::vector<ImageFaceMatch> faces;
    uint64_t library_count = 0;
    uint32_t matched = 0;
    bool cancelled = false;
};
// Android image search policy: at most ten faces, largest first; Top-1 over the
// whole library at the shared default identity threshold. No camera/liveness gates.
// Results own their pixels/strings, and never retain borrowed SDK pointers.
// 沿用 Android 图片搜索策略：最多十张脸，最大脸优先，全库按共享默认身份阈值取 Top-1，不执行相机或活体门禁。结果独占像素和字符串，不保留借用的 SDK 指针。
ImageSearchReport searchImage(Session& session, Database& database, const Image& image,
    const std::function<bool()>& cancelled = {});
}
