#include "scene.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace hiface_linux_demo {
Rect Rect::intersect(Rect b) const {
    const int left = std::max(x, b.x), top = std::max(y, b.y);
    return {left, top, std::max(0, std::min(x + w, b.x + b.w) - left), std::max(0, std::min(y + h, b.y + b.h) - top)};
}
void Scene::box(Rect bounds, uint32_t color, int radius) { Draw d; d.kind = Draw::Box; d.bounds = bounds; d.clip = clip; d.color = color; d.radius = radius; draws.push_back(std::move(d)); }
void Scene::text(int x, int y, std::string value, uint32_t color, Font font, int width) {
    Draw d; d.kind = Draw::Text; d.bounds = {x, y, width, 0}; d.clip = clip; d.text = std::move(value); d.color = color; d.font = font; d.text_width = width; draws.push_back(std::move(d));
}
void Scene::image(Rect bounds, const uint8_t* pixels, int width, int height, bool mirror, bool x11_bgrx) {
    Draw d; d.kind = Draw::Image; d.bounds = bounds; d.clip = clip; d.pixels = pixels; d.source_width = width; d.source_height = height; d.mirror = mirror; d.x11_bgrx = x11_bgrx; draws.push_back(std::move(d));
}
namespace {
std::string escape(const std::string& text) {
    std::string out;
    for (char c : text) { if (c == '&') out += "&amp;"; else if (c == '<') out += "&lt;"; else if (c == '>') out += "&gt;"; else if (c == '"') out += "&quot;"; else out += c; }
    return out;
}
std::string color(uint32_t rgb) { std::ostringstream out; out << '#' << std::hex << std::setw(6) << std::setfill('0') << rgb; return out.str(); }
void be32(std::vector<uint8_t>& out, uint32_t value) { for (int i = 24; i >= 0; i -= 8) out.push_back(uint8_t(value >> i)); }
uint32_t crc(const uint8_t* bytes, size_t count) {
    uint32_t sum = ~0u;
    for (size_t i = 0; i < count; ++i) { sum ^= bytes[i]; for (int bit = 0; bit < 8; ++bit) sum = (sum >> 1) ^ (0xedb88320u & (0u - (sum & 1u))); }
    return ~sum;
}
void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
    be32(out, data.size()); const auto begin = out.size(); out.insert(out.end(), type, type + 4); out.insert(out.end(), data.begin(), data.end()); be32(out, crc(out.data() + begin, 4 + data.size()));
}
std::string png(const Draw& d) {
    std::vector<uint8_t> raw;
    for (int y = 0; y < d.source_height; ++y) {
        raw.push_back(0);
        const auto* row = d.pixels + size_t(y) * d.source_width * 4;
        if (!d.x11_bgrx) raw.insert(raw.end(), row, row + size_t(d.source_width) * 4);
        else for (int x = 0; x < d.source_width; ++x) {
            const auto* pixel = row + size_t(x) * 4;
            raw.insert(raw.end(), {pixel[2], pixel[1], pixel[0], uint8_t(255)});
        }
    }
    std::vector<uint8_t> compressed{0x78, 0x01};
    for (size_t offset = 0; offset < raw.size();) {
        const size_t length = std::min<size_t>(65535, raw.size() - offset);
        compressed.push_back(offset + length == raw.size() ? 1 : 0);
        compressed.push_back(uint8_t(length)); compressed.push_back(uint8_t(length >> 8));
        compressed.push_back(uint8_t(~length)); compressed.push_back(uint8_t((~length) >> 8));
        compressed.insert(compressed.end(), raw.begin() + offset, raw.begin() + offset + length); offset += length;
    }
    uint32_t a = 1, b = 0; for (uint8_t c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; } be32(compressed, (b << 16) | a);
    std::vector<uint8_t> bytes{137,80,78,71,13,10,26,10}, header;
    be32(header, d.source_width); be32(header, d.source_height); header.insert(header.end(), {8, 6, 0, 0, 0});
    chunk(bytes, "IHDR", header); chunk(bytes, "IDAT", compressed); chunk(bytes, "IEND", {});
    const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; std::string encoded;
    for (size_t i = 0; i < bytes.size(); i += 3) {
        const auto value = uint32_t(bytes[i]) << 16 | (i + 1 < bytes.size() ? uint32_t(bytes[i + 1]) << 8 : 0) | (i + 2 < bytes.size() ? bytes[i + 2] : 0);
        encoded += alphabet[(value >> 18) & 63]; encoded += alphabet[(value >> 12) & 63]; encoded += i + 1 < bytes.size() ? alphabet[(value >> 6) & 63] : '='; encoded += i + 2 < bytes.size() ? alphabet[value & 63] : '=';
    }
    return encoded;
}
}
void writeSceneSvg(const Scene& scene, const std::string& file) {
    std::ofstream out(file);
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" width=\"" << scene.width << "\" height=\"" << scene.height << "\" viewBox=\"0 0 " << scene.width << ' ' << scene.height << "\">\n";
    size_t index = 0;
    for (const auto& d : scene.draws) {
        const auto r = d.bounds, c = d.clip;
        out << "<defs><clipPath id=\"c" << index << "\"><rect x=\"" << c.x << "\" y=\"" << c.y << "\" width=\"" << c.w << "\" height=\"" << c.h << "\"/></clipPath></defs><g clip-path=\"url(#c" << index++ << ")\">";
        if (d.kind == Draw::Box) out << "<rect x=\"" << r.x << "\" y=\"" << r.y << "\" width=\"" << std::max(0, r.w) << "\" height=\"" << std::max(0, r.h) << "\" rx=\"" << d.radius << "\" fill=\"" << color(d.color) << "\"/>";
        else if (d.kind == Draw::Text && !d.text.empty()) out << "<text x=\"" << r.x << "\" y=\"" << r.y << "\" font-family=\"Arial, sans-serif\" font-size=\"" << (d.font == Font::Title ? 22 : d.font == Font::Large ? 18 : d.font == Font::Small ? 12 : 14) << "\" fill=\"" << color(d.color) << "\" textLength=\"" << d.text_width << "\" lengthAdjust=\"spacingAndGlyphs\">" << escape(d.text) << "</text>";
        else if (d.kind == Draw::Image && d.pixels) {
            if (d.mirror) out << "<g transform=\"translate(" << 2 * r.x + r.w << " 0) scale(-1 1)\">";
            out << "<image x=\"" << r.x << "\" y=\"" << r.y << "\" width=\"" << r.w << "\" height=\"" << r.h << "\" preserveAspectRatio=\"none\" xlink:href=\"data:image/png;base64," << png(d) << "\"/>";
            if (d.mirror) out << "</g>";
        }
        out << "</g>\n";
    }
    out << "</svg>\n"; out.close(); if (!out) throw std::runtime_error("Cannot write UI preview: " + file);
}
}
