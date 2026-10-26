#pragma once
#include "types.h"
namespace hiface_demo {
// A successful capture is only a draft. Explicit confirmation owns persistence.
// The HF1 stays on the worker and is never published to the UI or diagnostic log.
// 采集成功仅形成草稿，必须明确确认后才持久化；HF1 留在工作线程，不交给 UI 或诊断日志。
class EnrollmentDraft {
public:
    bool pending() const { return !feature_.empty(); }
    void clear() { feature_.clear(); }
    void stage(const FlowResult& result) {
        clear();
        if (result.metadata.outcome != HIFACE_FLOW_OUTCOME_PASSED || result.feature.empty() || result.preview.rgba.empty())
            throw std::runtime_error("Enrollment requires a successful capture and aligned preview");
        decodeFeature(result.feature); feature_ = result.feature;
    }
    void stage(const CaptureResult& result) {
        clear();
        if (result.preview.rgba.empty()) throw std::runtime_error("Enrollment requires an aligned preview");
        decodeFeature(result.text); feature_ = result.text;
    }
    void save(Gallery& gallery, const std::string& id, const std::string& group, const std::string& tag) {
        if (!pending()) throw std::runtime_error("Capture a face before confirming enrollment");
        if (id.find_first_not_of(" \t\r\n") == std::string::npos || id.size() > HIFACE_SEARCH_MAX_TEXT_BYTES
            || group.size() > HIFACE_SEARCH_MAX_TEXT_BYTES || tag.size() > HIFACE_SEARCH_MAX_TEXT_BYTES
            || id.find('\0') != std::string::npos || group.find('\0') != std::string::npos || tag.find('\0') != std::string::npos)
            throw std::runtime_error("Enter a person ID and metadata of at most 1024 UTF-8 bytes");
        gallery.save({{id, group, tag, feature_, 0}}, false);
        clear();
    }
private:
    std::string feature_;
};
}
