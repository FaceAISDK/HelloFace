#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace hiface_linux_demo {
struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
    bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    Rect intersect(Rect other) const;
};
enum class Font { Body, Small, Title, Large };
struct Draw {
    enum Kind { Box, Text, Image } kind = Box;
    Rect bounds, clip;
    uint32_t color = 0;
    int radius = 0;
    Font font = Font::Body;
    std::string text;
    int text_width = 0;
    const uint8_t* pixels = nullptr;
    int source_width = 0, source_height = 0;
    bool mirror = false, x11_bgrx = false;
};
struct Scene {
    int width = 0, height = 0;
    Rect clip;
    std::vector<Draw> draws;
    void box(Rect bounds, uint32_t color, int radius = 0);
    void text(int x, int baseline, std::string value, uint32_t color, Font font, int width);
    void image(Rect bounds, const uint8_t* pixels, int width, int height, bool mirror,
               bool x11_bgrx = false);
};
// Exports the same drawing commands used by X11, for portable layout QA.
// Image buffers remain borrowed from the ViewState until serialization completes.
// 导出与 X11 相同的绘图命令，供跨平台布局检查；序列化结束前继续借用 ViewState 的图像缓冲。
void writeSceneSvg(const Scene& scene, const std::string& file);
}
