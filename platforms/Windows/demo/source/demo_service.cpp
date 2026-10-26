#include "demo_service.h"
namespace hiface_demo {
namespace {
std::string actionName(HiFaceFlowActionV1 action) {
    switch (action) {
        case HIFACE_ACTION_BLINK: return u8"眨眼"; case HIFACE_ACTION_MOUTH_OPEN: return u8"张嘴";
        case HIFACE_ACTION_NOD_HEAD: return u8"点头"; default: return u8"摇头";
    }
}
std::string progressText(const HiFaceFlowEventV1& event) {
    switch (event.stage) {
        case HIFACE_STAGE_MULTIPLE_FACES: return u8"请保持画面中只有一人";
        case HIFACE_STAGE_FACE_TOO_SMALL: return u8"请靠近摄像头";
        case HIFACE_STAGE_HOLD_STILL: return u8"请保持正脸不动";
        case HIFACE_STAGE_FACE_UNSTABLE: return u8"请保持人脸稳定";
        case HIFACE_STAGE_WAITING_FOR_FACE: return u8"请看向摄像头";
        case HIFACE_STAGE_PERFORM_ACTION: return u8"请" + actionName(event.action) + " (" + std::to_string(event.step_number) + "/" + std::to_string(event.step_count) + ")";
        default: return u8"请正对摄像头";
    }
}
std::string outcomeText(HiFaceFlowOutcomeV1 outcome) {
    switch (outcome) {
        case HIFACE_FLOW_OUTCOME_PASSED: return u8"通过";
        case HIFACE_FLOW_OUTCOME_NOT_MATCHED: return u8"身份不匹配";
        case HIFACE_FLOW_OUTCOME_SPOOF_DETECTED: return u8"活体未通过";
        case HIFACE_FLOW_OUTCOME_NO_FACE_TIMEOUT: return u8"等待人脸超时";
        case HIFACE_FLOW_OUTCOME_FACE_LOST: return u8"人脸丢失";
        case HIFACE_FLOW_OUTCOME_MULTIPLE_FACES: return u8"检测到多人";
        case HIFACE_FLOW_OUTCOME_ACTION_TIMEOUT: return u8"动作超时";
        default: return u8"最终采样超时";
    }
}

}
DemoService::DemoService(const std::filesystem::path& database)
    : client_(database), database_(database) {}
void DemoService::beginFlow(const FlowOptions& options) {
    client_.beginFlow(options); enrollment_id_.clear();
}
void DemoService::beginEnrollment(const std::string& id, const std::string& group, const std::string& tag) {
    if (id.empty() || id.size() > HIFACE_SEARCH_MAX_TEXT_BYTES || id.find('\0') != std::string::npos
        || group.size() > HIFACE_SEARCH_MAX_TEXT_BYTES || tag.size() > HIFACE_SEARCH_MAX_TEXT_BYTES
        || group.find('\0') != std::string::npos || tag.find('\0') != std::string::npos)
        throw std::runtime_error("Enter a person ID and metadata of at most 1024 UTF-8 bytes");
    if (database_.contains(id)) throw SdkError(HIFACE_STATUS_ALREADY_EXISTS, "Person ID already exists");
    client_.beginFlow(FlowOptions{});
    enrollment_id_ = id; enrollment_group_ = group; enrollment_tag_ = tag;
}
void DemoService::cancelEnrollment() { client_.cancelFlow(); enrollment_id_.clear(); }
Update DemoService::process(const CameraFrame& frame, const std::function<bool()>& current) {
    Update update;
    static_cast<hiface::FrameUpdate&>(update) = client_.process(frame, current);
    update.enrolling = update.flow_active;
    if (update.cancelled) { enrollment_id_.clear(); update.message = u8"已取消"; return update; }
    if (update.skipped) return update;
    if (update.result) {
        const auto& result = *update.result;
        update.message = outcomeText(result.metadata.outcome);
        if (result.metadata.flags & HIFACE_FLOW_RESULT_HAS_LIVENESS) update.message += u8" | 活体=" + std::to_string(result.metadata.liveness_score);
        if (result.metadata.flags & HIFACE_FLOW_RESULT_HAS_SIMILARITY) update.message += u8" | 相似度=" + std::to_string(result.metadata.similarity);
        const auto id = enrollment_id_; enrollment_id_.clear();
        if (!id.empty()) {
            if (current && !current()) { update.result.reset(); update.message = u8"已取消"; return update; }
            if (!update.matched || result.feature.empty()) throw std::runtime_error("Guided capture rejected enrollment");
            database_.save({{id, enrollment_group_, enrollment_tag_, result.feature, 0}}, false);
            update.saved = true; update.message = u8"录入成功: " + id;
        }
    } else if (update.flow_active) update.message = progressText(update.event);
    else if (update.paused) update.message = u8"搜索已暂停";
    else if (!update.searching) update.message = u8"检测到 " + std::to_string(update.faces.size()) + u8" 张人脸";
    else {
        update.message = update.faces.empty() ? u8"未检测到人脸" : u8"未匹配";
        if (update.end_reason == HIFACE_SEARCH_END_SPOOF_DETECTED) update.message = u8"活体未通过";
        else if (update.matched) {
            update.message = u8"匹配成功";
            for (const auto& face : update.faces) if (face.threshold_passed) update.message += " | " + std::string(face.face_id) + "=" + std::to_string(face.similarity);
        } else if (update.search_progress == HIFACE_SEARCH_STAGE_EMPTY_GALLERY) update.message = u8"当前范围人脸库为空，请先录入或导入";
        else if (update.end_reason == HIFACE_SEARCH_END_TIMEOUT) update.message = u8"本轮搜索超时，继续搜索";
        if (update.has_liveness) update.message += u8" | 活体=" + std::to_string(update.liveness_score);
    }
    return update;
}
Update DemoService::detectDetailed(const Image& image, uint32_t maximum, bool detailed) {
    Update update; static_cast<hiface::FrameUpdate&>(update) = client_.detectDetailed(image, maximum, detailed);
    update.message = u8"图片检测: " + std::to_string(update.faces.size()) + u8" 张人脸"; return update;
}
}
