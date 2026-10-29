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
    {"detection", "Live Detection", "View five landmarks and key facial contours", 'D'},
    {"capture", "Face Capture", "Capture a face and save its feature", 'C'},
    {"verification", "1:1 Face Verification", "Select a Face ID, then verify with liveness", 'V'},
    {"liveness", "Liveness", "Action challenges and passive liveness", 'L'},
    {"camera", "Face Search", "Find identities in the face library", 'S'},
    {"images", "Photo 1:1 Comparison", "Compare faces in two photos", 0},
    {"image-search", "Image search", "Search every face in a local photo without a camera", 0},
    {"camera-settings", "SDK Settings", "Video, mirror and UI rotation preferences", 0},
    {"enrollment", "Face enrollment", "Capture, review the preview, then confirm saving to the library", 'E'},
    {"gallery", "Face library", "Manage identities and import or export HF1 JSON", 0},
    {"settings", "Feature settings", "Adjust parameters, then apply to save", 0},
    {"references", "Select a Face ID", "Choose a reference saved by Face Capture", 0},
};
// The same eight primary entries as Android. Enrollment/library/settings are
// children of their feature pages, rather than additional Home destinations.
// 首页与 Android 一致，仅八项；录入、人脸库和功能参数由所属功能页进入。
inline constexpr int home_page_count = 8;
inline bool homePage(std::string_view route) {
    for (int i = 0; i < home_page_count; ++i) if (demo_pages[i].route == route) return true;
    return false;
}
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
    "detection", "capture", "verification", "liveness", "enrollment", "camera-settings", "references"
};
inline constexpr unsigned smoke_page_count = sizeof(smoke_pages) / sizeof(smoke_pages[0]);
}
