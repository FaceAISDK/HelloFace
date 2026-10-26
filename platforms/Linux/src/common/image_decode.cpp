#include "image_decode.h"
#include <cstring>
#include <memory>
#include <algorithm>
#include <cctype>
#include <limits>
#include <cmath>
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_MAX_DIMENSIONS 16384
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace hiface_linux_demo {
namespace {
std::string ppmToken(std::istream& input) {
    std::string value;
    while (true) {
        input >> std::ws;
        if (input.peek() != '#') break;
        input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    while (input.good() && input.peek() != EOF && !std::isspace(static_cast<unsigned char>(input.peek())) && input.peek() != '#') {
        require(value.size() < 32, "PPM header token too long");
        value.push_back(static_cast<char>(input.get()));
    }
    require(!value.empty(), "Truncated PPM header");
    return value;
}
Image convert(stbi_uc* pixels, int width, int height) {
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> owned(pixels, stbi_image_free);
    require(pixels != nullptr, std::string("Cannot decode image: ") +
            (stbi_failure_reason() ? stbi_failure_reason() : "unknown format"));
    require(width >= 2 && height >= 2 && width <= 4096 && height <= 4096, "Image size must be 2..4096");
    // Match Android static-image adaptation: crop only the final odd row/column,
    // without resizing or introducing additional edge pixels.
    // 与 Android 静态图片适配一致：仅裁掉末尾奇数行或列，不缩放、不补充边缘像素。
    Image image{width & ~1, height & ~1, {}};
    image.rgba.resize(static_cast<size_t>(image.width) * image.height * 4);
    for (int y = 0; y < image.height; ++y) for (int x = 0; x < image.width; ++x) {
        const int sx = x < width ? x : width - 1, sy = y < height ? y : height - 1;
        std::memcpy(image.rgba.data() + (static_cast<size_t>(y) * image.width + x) * 4,
                pixels + (static_cast<size_t>(sy) * width + sx) * 4, 4);
    }
    return image;
}
}
Image readImage(const char* path) {
    std::ifstream input(path, std::ios::binary);
    require(input.is_open(), std::string("Cannot open image: ") + path);
    char magic[2]{};
    input.read(magic, 2); input.clear(); input.seekg(0);
    if (magic[0] != 'P' || magic[1] != '6') return decodeImage(path);
    require(ppmToken(input) == "P6", "Expected binary P6 PPM image");
    auto dimension = [&] {
        const auto value = ppmToken(input);
        require(value.find_first_not_of("0123456789") == std::string::npos, "Invalid PPM dimension");
        const auto number = std::stoul(value);
        require(number >= 2 && number <= HIFACE_MAX_IMAGE_DIMENSION, "Image dimensions must be between 2 and 4096");
        return static_cast<int>(number);
    };
    const int width = dimension(), height = dimension();
    require(ppmToken(input) == "255", "Expected 8-bit PPM channels (maxval 255)");
    const int separator = input.get();
    require(separator != EOF && std::isspace(static_cast<unsigned char>(separator)), "Missing PPM separator");
    if (separator == '\r' && input.peek() == '\n') input.get();
    std::vector<uint8_t> rgb(static_cast<size_t>(width) * height * 3);
    require(bool(input.read(reinterpret_cast<char*>(rgb.data()), rgb.size())), "Truncated PPM pixels");
    Image image{width & ~1, height & ~1, {}};
    image.rgba.resize(static_cast<size_t>(image.width) * image.height * 4);
    for (int y = 0; y < image.height; ++y) for (int x = 0; x < image.width; ++x) {
        const auto src = (static_cast<size_t>(std::min(y, height - 1)) * width + std::min(x, width - 1)) * 3;
        const auto dst = (static_cast<size_t>(y) * image.width + x) * 4;
        std::copy_n(rgb.data() + src, 3, image.rgba.data() + dst);
        image.rgba[dst + 3] = 255;
    }
    return image;
}

void savePreview(const std::string& path, const std::vector<uint8_t>& rgba) {
    require(rgba.size() == HIFACE_ALIGNED_PREVIEW_RGBA_BYTES, "Invalid aligned preview");
    std::ofstream output(path, std::ios::binary);
    output << "P6\n" << HIFACE_ALIGNED_PREVIEW_SIDE << ' ' << HIFACE_ALIGNED_PREVIEW_SIDE << "\n255\n";
    for (size_t i = 0; i < rgba.size(); i += 4) output.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
    output.close();
    require(bool(output), "Cannot save captured preview: " + path);
}

Image decodeImage(const char* path) {
    int width = 0, height = 0, channels = 0;
    require(stbi_info(path, &width, &height, &channels) != 0, "Cannot decode JPEG/PNG/BMP header");
    require(width >= 2 && height >= 2 && width <= 4096 && height <= 4096, "Image size must be 2..4096");
    auto* pixels = stbi_load(path, &width, &height, &channels, 4);
    return convert(pixels, width, height);
}
Image decodeJpeg(const uint8_t* data, size_t size) {
    require(size > 0 && size <= 32 * 1024 * 1024, "Invalid camera JPEG size");
    int width = 0, height = 0, channels = 0;
    require(stbi_info_from_memory(data, static_cast<int>(size), &width, &height, &channels) && width >= 2 && height >= 2 && width <= 4096 && height <= 4096, "Invalid camera JPEG dimensions");
    auto* pixels = stbi_load_from_memory(data, static_cast<int>(size), &width, &height, &channels, 4);
    return convert(pixels, width, height);
}
namespace {
// Only inspect bounded JPEG APP1 segments. Malformed/unsupported metadata means
// orientation 1; decoding and pixel-limit validation are handled independently.
// 只检查有界的 JPEG APP1 段；损坏或不支持的元数据按方向 1 处理，解码与像素上限另行校验。
int jpegOrientation(const char* path) {
    std::ifstream input(path, std::ios::binary); unsigned char signature[2]{}; input.read(reinterpret_cast<char*>(signature), 2);
    if (signature[0] != 0xff || signature[1] != 0xd8) return 1;
    for (int segment = 0; segment < 128 && input.good(); ++segment) {
        if (input.get() != 0xff) break;
        int marker; do { marker = input.get(); } while (marker == 0xff);
        if (marker < 0 || marker == 0xda || marker == 0xd9) break;
        if (marker == 1 || marker >= 0xd0 && marker <= 0xd7) continue;
        const int hi = input.get(), lo = input.get(); if (hi < 0 || lo < 0) break;
        const int length = (hi << 8 | lo) - 2; if (length < 0) break;
        if (marker != 0xe1) { input.seekg(length, std::ios::cur); continue; }
        std::vector<uint8_t> data(length); if (!input.read(reinterpret_cast<char*>(data.data()), data.size())) break;
        if (data.size() < 14 || std::memcmp(data.data(), "Exif\0\0", 6)) continue;
        const bool little = data[6] == 'I' && data[7] == 'I', big = data[6] == 'M' && data[7] == 'M'; if (!little && !big) continue;
        auto u16 = [&](size_t offset) -> uint32_t { if (offset > data.size() || data.size() - offset < 2) return 0; return little ? data[offset] | uint32_t(data[offset+1]) << 8 : uint32_t(data[offset]) << 8 | data[offset+1]; };
        auto u32 = [&](size_t offset) -> uint32_t { if (offset > data.size() || data.size() - offset < 4) return 0; return little ? u16(offset) | u16(offset+2) << 16 : u16(offset) << 16 | u16(offset+2); };
        if (u16(8) != 42) continue;
        const uint64_t start = uint64_t(u32(10)) + 6;
        if (start > data.size() || data.size() - start < 2) continue;
        const auto count = u16(start);
        for (uint32_t i = 0; i < count; ++i) {
            const uint64_t at = start + 2 + uint64_t(i) * 12; if (at > data.size() || data.size() - at < 12) break;
            if (u16(at) == 0x112 && u16(at+2) == 3 && u32(at+4) == 1) { const auto value = u16(at+8); return value >= 1 && value <= 8 ? int(value) : 1; }
        }
    }
    return 1;
}
}
Image readSearchImage(const char* path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    require(input.is_open(), std::string("Cannot open image: ") + path);
    require(input.tellg() > 0 && input.tellg() <= 64 * 1024 * 1024, "Photo file must be between 1 byte and 64 MiB");
    char magic[2]{}; input.seekg(0); input.read(magic, 2);
    if (magic[0] == 'P' && magic[1] == '6') return readImage(path);
    int sw = 0, sh = 0, channels = 0;
    require(stbi_info(path, &sw, &sh, &channels) && sw >= 2 && sh >= 2 && sw <= 16384 && sh <= 16384 && uint64_t(sw) * sh <= 64 * 1024 * 1024, "Invalid photo or decoded photo exceeds 64 megapixels");
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(stbi_load(path, &sw, &sh, &channels, 4), stbi_image_free);
    require(bool(pixels), "Cannot decode JPEG/PNG/BMP photo");
    const int orientation = jpegOrientation(path), ow = orientation >= 5 ? sh : sw, oh = orientation >= 5 ? sw : sh;
    const double scale = std::min(1.0, double(HIFACE_MAX_IMAGE_DIMENSION) / std::max(ow, oh));
    const int scaled_width = int(std::floor(ow * scale)), scaled_height = int(std::floor(oh * scale));
    const int width = scaled_width & ~1, height = scaled_height & ~1;
    require(width >= 2 && height >= 2, "Photo is too small after resizing");
    Image image{width, height, std::vector<uint8_t>(size_t(width) * height * 4)};
    auto sample = [&](int x, int y, int c) -> uint8_t {
        int sx = x, sy = y;
        switch (orientation) {
            case 2: sx = sw-1-x; break; case 3: sx = sw-1-x; sy = sh-1-y; break; case 4: sy = sh-1-y; break;
            case 5: sx = y; sy = x; break; case 6: sx = y; sy = sh-1-x; break;
            case 7: sx = sw-1-y; sy = sh-1-x; break; case 8: sx = sw-1-y; sy = x; break;
        }
        return pixels.get()[(size_t(sy) * sw + sx) * 4 + c];
    };
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        auto* dst = image.rgba.data() + (size_t(y) * width + x) * 4;
        if (scale == 1) { for (int c = 0; c < 4; ++c) dst[c] = sample(x, y, c); continue; }
        const double fx = std::clamp((x + .5) * ow / scaled_width - .5, 0.0, double(ow-1));
        const double fy = std::clamp((y + .5) * oh / scaled_height - .5, 0.0, double(oh-1));
        const int x0 = int(fx), y0 = int(fy), x1 = std::min(x0+1, ow-1), y1 = std::min(y0+1, oh-1);
        const double dx = fx-x0, dy = fy-y0;
        for (int c = 0; c < 4; ++c) dst[c] = uint8_t(std::lround((1-dy) * ((1-dx)*sample(x0,y0,c) + dx*sample(x1,y0,c)) + dy*((1-dx)*sample(x0,y1,c)+dx*sample(x1,y1,c))));
    }
    return image;
}

}
