#pragma once
#include "types.h"
#include <functional>
namespace hiface_demo {
struct ImportProgress {
    size_t processed = 0, total = 0, saved = 0, failed = 0;
    bool stopped = false;
};
using ImportCallback = std::function<bool(const ImportProgress&)>;
ImportProgress importDataset(Gallery& database, const std::filesystem::path& file, const ImportCallback& progress = {});
size_t exportDataset(const Gallery& database, const std::filesystem::path& file, const std::string& group = {}, const std::string& tag = {});
}
