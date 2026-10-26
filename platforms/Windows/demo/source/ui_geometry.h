#pragma once
#include <algorithm>
#include <cstdint>

namespace hiface_demo {
// Device-pixel geometry shared by drawing, pointer hit testing and scrolling.
// Keeping this independent of HWND also allows layout regressions to run on the host.
// 绘制、指针命中测试和滚动共用设备像素几何；独立于 HWND，便于在宿主执行布局回归。
struct UiRect {
    int left = 0, top = 0, right = 0, bottom = 0;
    int width() const { return std::max(0, right - left); }
    int height() const { return std::max(0, bottom - top); }
    bool contains(int x, int y) const { return x >= left && x < right && y >= top && y < bottom; }
};
struct ImageFit {
    int x = 0, y = 0, width = 0, height = 0;
    double scale = 0;
};
inline ImageFit fitImage(int width, int height, const UiRect& area) {
    ImageFit fit;
    if (width < 1 || height < 1 || !area.width() || !area.height()) return fit;
    fit.scale = std::min(double(area.width()) / width, double(area.height()) / height);
    fit.width = static_cast<int>(width * fit.scale); fit.height = static_cast<int>(height * fit.scale);
    fit.x = area.left + (area.width() - fit.width) / 2; fit.y = area.top + (area.height() - fit.height) / 2;
    return fit;
}
struct PhotoLayout {
    UiRect client, photo, details, strip;
    int dpi = 96;
    PhotoLayout(int width, int height, int count, bool notice, int scale = 96) : client{0, 0, width, height}, dpi(scale) {
        const int strip_height = count ? px(height >= px(460) ? 140 : height >= px(340) ? 116 : 96) : 0;
        const int detail_height = count ? px(height >= px(400) ? 82 : 64) : notice ? px(64) : 0;
        strip = {0, height - strip_height, width, height};
        details = {px(12), strip.top - detail_height, std::max(px(12), width - px(12)), strip.top - px(8)};
        const int top = px(height >= px(280) ? 64 : 48);
        photo = {px(12), top, std::max(px(12), width - px(12)), std::max(top, strip.top - detail_height - px(12))};
    }
    int px(int value) const { return static_cast<int>((int64_t(value) * dpi + 48) / 96); }
    int viewport() const { return std::max(1, client.width() - px(24)); }
    int clampScroll(int position, int count) const {
        const int content = count ? (count - 1) * px(146) + px(138) : 0;
        return std::clamp(position, 0, std::max(0, content - viewport()));
    }
    UiRect tile(int index, int scroll) const {
        const int left = px(12) + index * px(146) - scroll;
        return {left, strip.top + px(6), left + px(138), strip.bottom - px(6)};
    }
    int tileAt(int x, int y, int count, int scroll) const {
        if (!client.contains(x, y) || !strip.contains(x, y) || x < px(12) || x >= client.right - px(12)) return -1;
        const int index = (x - px(12) + scroll) / px(146);
        return index >= 0 && index < count && tile(index, scroll).contains(x, y) ? index : -1;
    }
    int reveal(int index, int scroll, int count) const {
        const auto area = tile(index, 0);
        const int left = area.left - px(12), right = area.right - px(12);
        if (left < scroll) scroll = left;
        else if (right > scroll + viewport()) scroll = right - viewport();
        return clampScroll(scroll, count);
    }
};
inline UiRect homeCard(int width, int height, int main_left, int count, int index) {
    const int columns = width < 900 ? 2 : 3, gap = 12, rows = (count + columns - 1) / columns;
    const int card_width = (width - main_left - 32 - gap * (columns - 1)) / columns;
    const int card_height = std::min(142, (height - 190 - gap * (rows - 1)) / rows);
    const int x = main_left + 8 + (index % columns) * (card_width + gap), y = 102 + (index / columns) * (card_height + gap);
    return {x, y, x + card_width, y + card_height};
}
inline UiRect navigationItem(int height, int width, int count, int index) {
    const int step = std::min(46, (height - 106) / count);
    const int top = 94 + index * step;
    return {12, top, width - 20, top + std::max(1, step - 6)};
}
// Toolbar controls wrap as a unit; native combobox popup height is independent.
// 工具栏控件整体换行；原生组合框弹出列表的高度独立处理。
struct ToolbarLayout {
    int left, right, x, y;
    ToolbarLayout(int start, int end, int top) : left(start), right(end), x(start), y(top) {}
    UiRect place(int width) {
        if (x > left && x + width > right) { x = left; y += 42; }
        UiRect area{x, y, std::min(right, x + width), y + 34}; x += width + 8; return area;
    }
};
}
