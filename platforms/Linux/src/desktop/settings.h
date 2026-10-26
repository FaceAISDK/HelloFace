#pragma once
#include "sdk_helpers.h"

namespace hiface_linux_demo {
struct FormField { std::string key, label, value; };
struct DemoSettings {
    uint32_t detection_faces = 1;
    bool detailed = false, allow_multiple = false, verification_liveness = true;
    float verification_threshold = HIFACE_DEFAULT_VERIFICATION_THRESHOLD;
    std::vector<HiFaceFlowActionV1> actions;
    bool search_multi = false, search_liveness = true;
    uint32_t search_faces = 3, search_interval = 0, search_timeout = 3000;
    float search_threshold = .8f;
    std::string group, tag;
    int camera_width = 640, camera_height = 360, camera_fps = 25;
    bool camera_mjpeg = false, mirror = false;
};
std::vector<FormField> settingsFields(const DemoSettings& settings);
DemoSettings parseSettings(const std::vector<FormField>& fields);
DemoSettings loadSettings(const std::string& file);
void saveSettings(const std::string& file, const DemoSettings& settings);
const char* actionName(HiFaceFlowActionV1 action);
}
