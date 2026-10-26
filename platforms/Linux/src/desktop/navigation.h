#pragma once
#include <string_view>
namespace hiface_linux_demo {
struct DemoPage {
    const char* route;
    const char* title;
    const char* description;
    char camera_mode;
};
// Each feature owns a route; camera rendering and SDK lifetime stay shared.
// 每个功能拥有独立路由，相机绘制与 SDK 生命周期仍共用。
inline constexpr DemoPage demo_pages[] = {
    {"detection", "Face detection", "Live boxes, five landmarks and optional detailed contours", 'D'},
    {"capture", "Guided capture", "Capture an aligned face and save a 1:1 reference", 'C'},
    {"verification", "1:1 verification", "Verify against a saved or selected face reference", 'V'},
    {"liveness", "Face liveness", "Passive liveness and configurable action challenges", 'L'},
    {"camera", "Face search", "Continuous 1:N or M:N identification with pause and resume", 'S'},
    {"images", "Photo comparison", "Compare two photos without liveness checks", 0},
    {"image-search", "Image search", "Search every face in a local photo without a camera", 0},
    {"camera-settings", "Camera settings", "Resolution, frame rate, format and mirror preferences", 0},
    {"enrollment", "Face enrollment", "Capture, review the preview, then confirm saving to the library", 'E'},
    {"gallery", "Face library", "Manage identities and import or export HF1 JSON", 0},
    {"settings", "Advanced settings", "Detection, verification, liveness and search preferences", 0},
};
inline const DemoPage* demoPage(std::string_view route) {
    for (const auto& page : demo_pages) if (page.route == route) return &page;
    return nullptr;
}
inline bool cameraPage(std::string_view route) {
    const auto* page = demoPage(route);
    return page && page->camera_mode;
}
inline constexpr const char* smoke_pages[] = {
    "camera", "images", "gallery", "settings", "image-search", "home",
    "detection", "capture", "verification", "liveness", "enrollment", "camera-settings"
};
inline constexpr unsigned smoke_page_count = sizeof(smoke_pages) / sizeof(smoke_pages[0]);
}
