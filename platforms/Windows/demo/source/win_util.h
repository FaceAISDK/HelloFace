#pragma once
#include "types.h"
#include <windows.h>
#include <filesystem>
#include <string>

namespace hiface_demo {
void checkHr(HRESULT status, const char* operation);
std::wstring widen(const std::string& utf8);
std::string utf8(const std::wstring& text);
std::filesystem::path executableDirectory();
std::filesystem::path demoDataDirectory();
void initializeLog(const char* application);
void writeLog(const std::string& message) noexcept;
std::filesystem::path logDirectory();
Image readImage(const std::filesystem::path& path, bool fit_to_sdk_limit = false);
void writeImage(const std::filesystem::path& path, const Image& image);
void testWindowsImageDecoder();
class ComApartment {
public:
    explicit ComApartment(bool sta = false);
    ~ComApartment();
    ComApartment(const ComApartment&) = delete;
    ComApartment& operator=(const ComApartment&) = delete;
};
}
