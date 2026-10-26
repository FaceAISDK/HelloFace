#pragma once
#include <filesystem>
namespace hiface_demo {
struct CameraPreferences {
    unsigned width = 640, height = 480;
    int rotation = 0;
    bool mirror = false;
};
CameraPreferences loadCameraPreferences(const std::filesystem::path& file);
void saveCameraPreferences(const std::filesystem::path& file, const CameraPreferences& value);
}
