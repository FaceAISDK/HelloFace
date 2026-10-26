#include "workflow.h"
#include "image_decode.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
namespace hiface_linux_demo {
int64_t monotonicMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
namespace {
std::string score(float value) { std::ostringstream out; out << std::fixed << std::setprecision(3) << value; return out.str(); }
std::string uniqueId(const char* prefix) {
    return std::string(prefix) + std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}
std::string stage(HiFaceFlowStageV1 value) {
    switch (value) {
        case HIFACE_STAGE_MULTIPLE_FACES: return "One person at a time";
        case HIFACE_STAGE_FACE_TOO_SMALL: return "Move closer to the camera";
        case HIFACE_STAGE_FACE_NOT_FRONTAL: return "Face the camera";
        case HIFACE_STAGE_HOLD_STILL: return "Hold still";
        case HIFACE_STAGE_FACE_FORWARD: return "Look straight ahead";
        case HIFACE_STAGE_FACE_UNSTABLE: return "Keep your face steady";
        default: return "Waiting for a face";
    }
}
std::string outcome(HiFaceFlowOutcomeV1 value) {
    switch (value) {
        case HIFACE_FLOW_OUTCOME_PASSED: return "Passed";
        case HIFACE_FLOW_OUTCOME_NOT_MATCHED: return "Not matched";
        case HIFACE_FLOW_OUTCOME_SPOOF_DETECTED: return "Rejected: spoof detected";
        case HIFACE_FLOW_OUTCOME_NO_FACE_TIMEOUT: return "No face: acquisition timeout";
        case HIFACE_FLOW_OUTCOME_FACE_LOST: return "Face lost";
        case HIFACE_FLOW_OUTCOME_MULTIPLE_FACES: return "Rejected: multiple faces";
        case HIFACE_FLOW_OUTCOME_ACTION_TIMEOUT: return "Action timeout";
        case HIFACE_FLOW_OUTCOME_FINAL_CAPTURE_TIMEOUT: return "Final capture timeout";
        default: return "Unknown flow outcome";
    }
}
std::vector<Box> detectionBoxes(const HiFaceFaceV1* faces, uint32_t count) {
    std::vector<Box> boxes;
    for (uint32_t i = 0; i < count; ++i) {
        const auto& face = faces[i];
        Box box{face.x, face.y, face.width, face.height, "Face " + score(face.confidence), false, {}};
        for (const auto& point : face.landmarks) box.points.push_back(point);
        if (face.mesh_valid && face.mesh) for (uint32_t j = 0; j < face.mesh_count; ++j) box.points.push_back({face.mesh[j].x, face.mesh[j].y});
        if (face.pose_valid) box.label += " yaw=" + score(face.yaw) + " pitch=" + score(face.pitch);
        boxes.push_back(std::move(box));
    }
    return boxes;
}
bool imageExtension(std::filesystem::path file) {
    auto extension = file.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return std::tolower(c); });
    return extension == ".jpg" || extension == ".jpeg" || extension == ".png" || extension == ".bmp" || extension == ".ppm";
}
}
FaceWorkflow::FaceWorkflow(const std::string& models, const std::string& database)
    : database_(database.c_str()), data_directory_(std::filesystem::path(database).has_parent_path()
        ? std::filesystem::path(database).parent_path() : std::filesystem::path(".")) {
    loadModels(session_, models);
    recount();
    try { settings_ = loadSettings((data_directory_ / "settings.conf").string()); }
    catch (const DemoError& error) { status = "Settings ignored: " + error.message; }
    const auto file = data_directory_ / "reference.hf1";
    if (std::filesystem::exists(file)) {
        try {
            std::ifstream input(file); std::string space, id, text;
            std::getline(input, space); std::getline(input, id); std::getline(input, text);
            require(space == HIFACE_FEATURE_SPACE_ID && !id.empty() && id.size() <= HIFACE_SEARCH_MAX_TEXT_BYTES, "Reference feature space or ID mismatch");
            check(hiface_feature_decode_text_v1(text.c_str(), text.size(), reference_.data(), reference_.size()), "read saved reference");
            reference_id = id; has_reference_ = true;
        } catch (const DemoError& error) { status = "Reference ignored: " + error.message; }
    }
}
FaceWorkflow::~FaceWorkflow() {
    hiface_session_search_stop_v1(session_.handle);
    hiface_session_face_flow_stop_v1(session_.handle);
}
void FaceWorkflow::recount() { check(hiface_search_count_v1(database_.handle, nullptr, nullptr, &gallery_count), "count gallery"); }
void FaceWorkflow::stop() {
    check(hiface_session_search_stop_v1(session_.handle), "stop search");
    check(hiface_session_face_flow_stop_v1(session_.handle), "stop flow");
    mode_ = 'D'; boxes.clear();
    if (enrollment_.pending()) aligned_preview.reset();
    enrollment_.clear();
}
void FaceWorkflow::clearPageResults() {
    stop(); image_first.reset(); image_second.reset(); aligned_preview.reset(); image_matches.clear(); clearPhotoSearch(); inference_ms = 0;
}
void FaceWorkflow::confirmEnrollment(const std::string& id, const std::string& group, const std::string& tag) {
    enrollment_.save(database_, id, group, tag); recount(); status = "Enrollment confirmed: " + id;
}
void FaceWorkflow::pauseSearch() {
    require(mode_ == 'S' || mode_ == 'P', "Start search before pausing");
    if (mode_ == 'P') { action('S'); return; }
    stop(); check(hiface_search_release_memory_v1(database_.handle), "release search cache");
    mode_ = 'P'; status = "Search paused. Click Pause/resume to continue.";
}
void FaceWorkflow::applySettings(const DemoSettings& settings) {
    const auto validated = parseSettings(settingsFields(settings));
    saveSettings((data_directory_ / "settings.conf").string(), validated);
    stop(); settings_ = validated; status = "Settings saved. Start the required mode.";
}
void FaceWorkflow::toggleMirror() {
    auto next = settings_; next.mirror = !next.mirror;
    saveSettings((data_directory_ / "settings.conf").string(), next); settings_ = next;
}
void FaceWorkflow::setEnrollment(const std::string& id, const std::string& group, const std::string& tag) {
    require(id.size() <= HIFACE_SEARCH_MAX_TEXT_BYTES && id.find_first_of("\r\n") == std::string::npos, "Invalid enrollment ID");
    enrollment_id_ = id; enrollment_group_ = group; enrollment_tag_ = tag;
}
void FaceWorkflow::saveReference(const Feature& feature, const std::string& id) {
    require(!id.empty() && id.find_first_of("\r\n") == std::string::npos, "Invalid reference ID");
    const auto text = encodeFeature(feature);
    // Use the same persisted HF1 for the current and future process, avoiding a
    // full-precision current reference that differs after a restart.
    // 当前进程与后续进程使用同一份持久化 HF1，避免当前全精度基准与重启后基准不一致。
    Feature persisted{};
    check(hiface_feature_decode_text_v1(text.data(), HIFACE_FEATURE_TEXT_LENGTH, persisted.data(), persisted.size()), "decode saved reference");
    atomicWrite((data_directory_ / "reference.hf1").string(), std::string(HIFACE_FEATURE_SPACE_ID) + '\n' + id + '\n' + text.data() + '\n');
    reference_ = persisted; reference_id = id; has_reference_ = true;
}
void FaceWorkflow::selectReference(const std::string& id) {
    require(id.find_first_of("\r\n") == std::string::npos, "Reference ID must be a single line");
    const auto row = getEntry(database_, id);
    Feature feature{};
    check(hiface_feature_decode_text_v1(row.feature.c_str(), row.feature.size(), feature.data(), feature.size()), "decode reference");
    // Preserve the original Q7 text verbatim; do not quantize an existing HF1 again.
    // 原样保留 Q7 文本，不对已有 HF1 再次量化。
    atomicWrite((data_directory_ / "reference.hf1").string(), std::string(HIFACE_FEATURE_SPACE_ID) + '\n' + row.id + '\n' + row.feature + '\n');
    reference_ = feature; reference_id = row.id; has_reference_ = true; aligned_preview.reset(); status = "Reference selected: " + id;
}
void FaceWorkflow::clearReference() {
    stop(); std::filesystem::remove(data_directory_ / "reference.hf1");
    std::fill(reference_.begin(), reference_.end(), 0); reference_id.clear(); has_reference_ = false;
    aligned_preview.reset(); status = "Verification reference cleared";
}
void FaceWorkflow::beginFlow(char mode) {
    if (mode == 'V') require(has_reference_, "Capture, load a photo, or select a gallery reference first");
    if (mode == 'E' && !enrollment_id_.empty()) {
        uint32_t exists = 0; check(hiface_search_contains_v1(database_.handle, enrollment_id_.c_str(), &exists), "check enrollment ID");
        require(!exists, "ALREADY_EXISTS: choose a different enrollment ID");
    }
    stop();
    HiFaceFlowConfigV1 config{};
    config.struct_size = sizeof(config); config.api_version = HIFACE_C_API_VERSION_V1;
    config.mode = mode == 'L' ? HIFACE_FLOW_ACTION_LIVENESS : mode == 'V'
        ? (settings_.verification_liveness ? HIFACE_FLOW_FACE_AUTHENTICATION : HIFACE_FLOW_FACE_VERIFICATION) : HIFACE_FLOW_GUIDED_CAPTURE;
    config.frontal_hold_ms = mode == 'C' || mode == 'E' ? HIFACE_CAPTURE_FRONTAL_HOLD_MS : HIFACE_LIVENESS_FRONTAL_HOLD_MS;
    config.min_face_size_pixels = HIFACE_FLOW_DEFAULT_MIN_FACE_SIZE_PIXELS;
    config.verification_threshold = settings_.verification_threshold;
    config.multiple_faces_allowed = (mode == 'L' || mode == 'V') && settings_.allow_multiple;
    if (mode == 'L' || mode == 'V') {
        config.action_count = settings_.actions.size();
        std::copy(settings_.actions.begin(), settings_.actions.end(), config.actions);
        config.action_timeout_ms = HIFACE_FLOW_DEFAULT_ACTION_TIMEOUT_MS;
        config.face_acquisition_timeout_ms = HIFACE_FLOW_DEFAULT_FACE_ACQUISITION_TIMEOUT_MS;
        config.face_lost_timeout_ms = HIFACE_FLOW_DEFAULT_FACE_LOST_TIMEOUT_MS;
        config.multiple_faces_timeout_ms = HIFACE_FLOW_DEFAULT_MULTIPLE_FACES_TIMEOUT_MS;
        config.final_capture_timeout_ms = HIFACE_FLOW_DEFAULT_FINAL_CAPTURE_TIMEOUT_MS;
    }
    if (mode == 'V') { config.verification_reference = reference_.data(); config.verification_reference_count = reference_.size(); }
    check(hiface_session_face_flow_start_v1(session_.handle, &config, monotonicMilliseconds()), "start flow");
    mode_ = mode; status = "Look at the camera and hold still";
}
void FaceWorkflow::action(char key) {
    if (key == 'X' || key == 'D') { cancelImport(); stop(); status = "Detection running"; return; }
    require(!importing_, "Stop the import before starting another operation");
    if (key == 'C' || key == 'E' || key == 'L' || key == 'V') { beginFlow(key); return; }
    if (key == 'S') {
        stop();
        check(hiface_search_warm_up_v1(database_.handle, settings_.group.c_str(), settings_.tag.c_str()), "warm search scope");
        HiFaceSearchCameraConfigV1 config{};
        config.struct_size = sizeof(config); config.max_faces = settings_.search_faces;
        config.multi_face = settings_.search_multi; config.need_liveness = settings_.search_liveness && !settings_.search_multi;
        config.threshold = settings_.search_threshold; config.timeout_ms = settings_.search_timeout; config.interval_ms = settings_.search_interval;
        config.group = settings_.group.c_str(); config.tag = settings_.tag.c_str();
        check(hiface_session_search_start_v1(session_.handle, &config), "start camera search");
        mode_ = 'S'; status = "Search running";
    }
}
void FaceWorkflow::finishFlow() {
    HiFaceFlowResultV1 result{}; result.struct_size = sizeof(result);
    Feature feature{}; std::vector<uint8_t> preview(HIFACE_ALIGNED_PREVIEW_RGBA_BYTES);
    check(hiface_session_face_flow_copy_result_v1(session_.handle, &result, feature.data(), feature.size(), preview.data(), preview.size()), "copy flow result");
    const char finished_mode = mode_;
    // Always release the completed Core flow, including when a later disk write fails.
    // 即使后续磁盘写入失败，也必须释放已经结束的 Core 流程。
    stop();
    std::string message = outcome(result.outcome);
    if (result.flags & HIFACE_FLOW_RESULT_HAS_LIVENESS) message += " | liveness " + score(result.liveness_score);
    if (result.flags & HIFACE_FLOW_RESULT_HAS_SIMILARITY) message += " | similarity " + score(result.similarity);
    if (result.flags & HIFACE_FLOW_RESULT_HAS_PREVIEW)
        aligned_preview = std::make_shared<Image>(Image{HIFACE_ALIGNED_PREVIEW_SIDE, HIFACE_ALIGNED_PREVIEW_SIDE, preview});
    if (result.outcome == HIFACE_FLOW_OUTCOME_PASSED && (finished_mode == 'C' || finished_mode == 'E')) {
        require(result.flags & HIFACE_FLOW_RESULT_HAS_FEATURE, "Capture returned no feature");
        const auto id = finished_mode == 'E' && !enrollment_id_.empty() ? enrollment_id_ : uniqueId(finished_mode == 'E' ? "person-" : "capture-");
        if (finished_mode == 'E') {
            require(result.flags & HIFACE_FLOW_RESULT_HAS_PREVIEW, "Enrollment returned no aligned preview");
            enrollment_.stage(feature); ++enrollment_generation_;
            status = "Capture ready. Review the preview and confirm saving; nothing has been added yet.";
            return;
        } else {
            if (result.flags & HIFACE_FLOW_RESULT_HAS_PREVIEW) {
                const auto directory = data_directory_ / "captures"; std::filesystem::create_directories(directory);
                savePreview((directory / (id + ".ppm")).string(), preview);
            }
            message = "Captured preview saved.";
        }
        try { saveReference(feature, id); message += " Reference saved."; }
        catch (const DemoError& error) { message += " Reference was not saved: " + error.message; }
    }
    status = message;
}
void FaceWorkflow::process(const CameraFrame& input) {
    if (mode_ == 'P' || importing_ || enrollment_.pending()) return;
    const auto begin = monotonicMilliseconds(); const auto frame = input.view();
    if (mode_ == 'S') {
        HiFaceSearchBatchHandle raw = nullptr;
        const auto prepared = hiface_session_search_prepare_yuv420_v1(session_.handle, database_.handle, &frame, begin, &raw);
        if (prepared == HIFACE_STATUS_FRAME_SKIPPED) return;
        check(prepared, "prepare camera search");
        std::unique_ptr<HiFaceSearchBatchOpaque, decltype(&hiface_search_batch_destroy_v1)> batch(raw, hiface_search_batch_destroy_v1);
        check(hiface_search_batch_resolve_v1(raw, database_.handle), "resolve camera search");
        check(hiface_session_search_complete_v1(session_.handle, raw), "complete camera search");
        HiFaceSearchCameraResultV1 result{}; result.struct_size = sizeof(result);
        check(hiface_search_batch_read_v1(raw, &result), "read camera search");
        boxes.clear();
        for (uint32_t i = 0; i < result.face_count; ++i) {
            const auto& face = result.faces[i];
            std::string label = face.face_id[0] ? face.face_id : "Unknown";
            if (face.has_similarity) label += " " + score(face.similarity);
            Box box{face.x, face.y, face.width, face.height, label,
                result.end_reason == HIFACE_SEARCH_END_MATCH_FOUND && face.threshold_passed != 0, {}};
            for (size_t j = 0; j < HIFACE_FACE_LANDMARK_FLOAT_COUNT; j += 2) box.points.push_back({face.landmarks[j], face.landmarks[j + 1]});
            boxes.push_back(std::move(box));
        }
        switch (result.end_reason) {
            case HIFACE_SEARCH_END_MATCH_FOUND: status = "Match found"; break;
            case HIFACE_SEARCH_END_TIMEOUT: status = "No match: round timed out"; break;
            case HIFACE_SEARCH_END_NO_FACE: status = "No face"; break;
            case HIFACE_SEARCH_END_SPOOF_DETECTED: status = "Rejected: spoof detected (identity match does not grant access)"; break;
            default:
                if (result.progress == HIFACE_SEARCH_STAGE_EMPTY_GALLERY) status = "Selected gallery is empty. Enroll or change group/tag.";
                else if (result.progress == HIFACE_SEARCH_STAGE_FACE_TOO_SMALL) status = "Move closer: search requires a 100px face";
                else if (result.progress == HIFACE_SEARCH_STAGE_SEARCHING) status = "Searching...";
                else if (result.progress == HIFACE_SEARCH_STAGE_SUCCESS_INTERVAL) status = "Waiting for the next search round";
                else status = "Waiting for a face";
        }
        if (result.has_liveness_score) status += " | liveness " + score(result.liveness_score);
    } else {
        std::array<HiFaceFaceV1, HIFACE_MAX_FACES> faces{};
        HiFaceFrameResultV1 result{}; result.struct_size = sizeof(result); result.faces = faces.data(); result.capacity = faces.size();
        HiFaceDetectionRequestV1 request{sizeof(request), HIFACE_C_API_VERSION_V1, settings_.detection_faces, settings_.detailed ? 1u : 0u};
        HiFaceFlowEventV1 event{}; event.struct_size = sizeof(event);
        const auto processed = hiface_session_process_camera_yuv420_v1(session_.handle, &frame, &request, begin, &result, &event);
        if (processed == HIFACE_STATUS_FRAME_SKIPPED) return;
        check(processed, "process camera frame"); boxes = detectionBoxes(faces.data(), result.count);
        if (mode_ != 'D') {
            status = stage(event.stage);
            if (event.has_action) status = "Action " + std::to_string(event.step_number) + "/" + std::to_string(event.step_count) + ": " + actionName(event.action);
            if (event.kind == HIFACE_FLOW_EVENT_ERROR) { stop(); check(event.flow_status, "face flow"); }
            else if (event.kind == HIFACE_FLOW_EVENT_RESULT_AVAILABLE) finishFlow();
        }
    }
    inference_ms = monotonicMilliseconds() - begin;
}
void FaceWorkflow::imageOperation(const std::string& operation, const std::string& first, const std::string& second,
        const std::string& id, const std::string& group, const std::string& tag) {
    require(!importing_, "Stop the import before processing an image"); stop();
    image_first.reset(); image_second.reset(); aligned_preview.reset(); image_matches.clear(); boxes.clear();
    image_first = std::make_shared<Image>(readImage(first.c_str()));
    if (operation == "detect") {
        std::array<HiFaceFaceV1, HIFACE_MAX_FACES> faces{};
        HiFaceDetectionRequestV1 request{sizeof(request), HIFACE_C_API_VERSION_V1, settings_.detection_faces, settings_.detailed ? 1u : 0u};
        HiFaceFrameResultV1 result{}; result.struct_size = sizeof(result); result.faces = faces.data(); result.capacity = faces.size();
        check(hiface_session_process_rgba_v1(session_.handle, image_first->rgba.data(), image_first->rgba.size(), image_first->width, image_first->height, &request, &result), "detect image");
        boxes = detectionBoxes(faces.data(), result.count); status = "Photo faces: " + std::to_string(result.count); return;
    }
    std::vector<uint8_t> preview;
    const auto feature = captureFeature(session_, *image_first, &preview);
    aligned_preview = std::make_shared<Image>(Image{HIFACE_ALIGNED_PREVIEW_SIDE, HIFACE_ALIGNED_PREVIEW_SIDE, std::move(preview)});
    if (operation == "compare") {
        image_second = std::make_shared<Image>(readImage(second.c_str()));
        const auto other = captureFeature(session_, *image_second);
        float similarity = 0;
        check(hiface_feature_compare_v1(feature.data(), other.data(), feature.size(), &similarity), "compare photos");
        status = std::string(similarity >= settings_.verification_threshold ? "Matched" : "Not matched") + " | similarity " + score(similarity) + " (photo comparison; no liveness)";
    } else if (operation == "reference") {
        saveReference(feature, id.empty() ? uniqueId("photo-") : id); status = "Photo reference saved: " + reference_id;
    } else if (operation == "enroll") {
        enrollment_.stage(feature); ++enrollment_generation_;
        status = "Photo capture ready. Review the preview and confirm saving; nothing has been added yet.";
    } else if (operation == "search") {
        HiFaceSearchResultsHandle raw = nullptr;
        check(hiface_search_query_v1(database_.handle, feature.data(), feature.size(), settings_.group.c_str(), settings_.tag.c_str(), 5, settings_.search_threshold, &raw), "search photo");
        SearchResults owned(raw, hiface_search_results_destroy_v1); image_matches = readResults(raw);
        status = "Photo Top-5 matches: " + std::to_string(image_matches.size());
    } else throw DemoError{"Unknown image operation"};
}
void FaceWorkflow::clearPhotoSearch() { search_photo.reset(); photo_search.reset(); search_photo_path.clear(); }
std::string FaceWorkflow::photoSearchSummary() const {
    if (!photo_search) return "Choose a photo to search. A camera is not required.";
    auto summary = photo_search->faces.empty() ? std::string("No faces found in this photo.") : "Photo search: " + std::to_string(photo_search->faces.size()) + " faces, " + std::to_string(photo_search->matched) + " matched.";
    if (photo_search->library_count == 0) summary += " Face library is empty. Enroll faces or import features first.";
    return summary;
}
void FaceWorkflow::searchPhoto(const std::string& path, const std::function<bool()>& cancelled) {
    require(!importing_, "Stop the import before searching a photo"); stop(); clearPhotoSearch();
    const auto begin = monotonicMilliseconds();
    auto image = std::make_shared<Image>(readSearchImage(path.c_str()));
    if (cancelled && cancelled()) return;
    auto report = std::make_shared<ImageSearchReport>(searchImage(session_, database_, *image, cancelled));
    if (report->cancelled || (cancelled && cancelled())) return;
    search_photo = std::move(image); photo_search = std::move(report); search_photo_path = path;
    inference_ms = monotonicMilliseconds() - begin;
    status = photoSearchSummary();
}
void FaceWorkflow::loadGallery(uint32_t offset, const std::string& id, const std::string& group, const std::string& tag) {
    gallery_offset = offset;
    check(hiface_search_count_v1(database_.handle, group.c_str(), tag.c_str(), &filtered_count), "count filtered gallery");
    recount(); gallery_rows.clear();
    if (id.empty()) gallery_rows = listGallery(database_, group, tag, offset, 10);
    else {
        uint32_t exists = 0; check(hiface_search_contains_v1(database_.handle, id.c_str(), &exists), "find face ID");
        if (exists) gallery_rows.push_back(getEntry(database_, id));
    }
    if (!importing_) status = id.empty() ? "Gallery loaded" : gallery_rows.empty() ? "Face ID not found" : "Face ID found";
}
void FaceWorkflow::deleteEntry(const std::string& id) {
    require(!importing_, "Stop import before deleting a face");
    uint64_t deleted = 0; check(hiface_search_delete_v1(database_.handle, id.c_str(), &deleted), "delete face ID"); recount();
    if (id == reference_id) clearReference();
    status = deleted ? "Deleted " + id : "Face ID not found";
}
void FaceWorkflow::updateEntry(const std::string& id, const std::string& group, const std::string& tag) {
    require(!importing_, "Stop import before editing a face");
    check(hiface_search_update_metadata_v1(database_.handle, id.c_str(), group.c_str(), tag.c_str(), 0), "update face metadata"); status = "Metadata updated: " + id;
}
void FaceWorkflow::exportGallery(const std::string& path, const std::string& group, const std::string& tag) {
    require(!importing_, "Stop import before exporting");
    require(!std::filesystem::exists(path), "Export file already exists; choose a new name");
    exportFeatureFile(database_, path, group, tag); status = "Exported HF1 JSON: " + path;
}
void FaceWorkflow::startImport(const std::string& path, bool images, const std::string& group) {
    require(!importing_, "An import is already running"); stop();
    import_entries_.clear(); import_images_.clear(); import_saved = import_failed = import_processed = 0;
    importing_images_ = images; import_group_ = group;
    if (images) {
        require(std::filesystem::is_directory(path), "Select a local image directory");
        for (const auto& file : std::filesystem::directory_iterator(path)) if (file.is_regular_file() && imageExtension(file.path())) {
            require(import_images_.size() < HIFACE_SEARCH_MAX_ENTRIES, "Image directory exceeds 50000 entries"); import_images_.push_back(file.path());
        }
        std::sort(import_images_.begin(), import_images_.end()); import_total = import_images_.size();
    } else { import_entries_ = readFeatureFile(path); import_total = import_entries_.size(); }
    importing_ = import_total != 0; status = importing_ ? "Import started. Stop import keeps completed batches." : "Import source is empty";
}
void FaceWorkflow::stepImport() {
    if (!importing_) return;
    try {
        if (importing_images_) {
            GalleryEntry row; bool captured = false;
            try {
                const auto& file = import_images_[import_processed];
                const auto text = encodeFeature(captureFeature(session_, readImage(file.c_str())));
                row = {file.stem().string(), text.data(), import_group_, ""}; captured = true;
            } catch (const DemoError&) { ++import_failed; }
            if (captured) { saveEntry(database_, row, true); ++import_saved; }
            ++import_processed;
        } else {
            const auto count = std::min<size_t>(100, import_entries_.size() - import_processed);
            saveEntries(database_, import_entries_, import_processed, count, true); import_processed += count; import_saved += count;
        }
        recount();
        status = "Import " + std::to_string(import_processed) + "/" + std::to_string(import_total) + " | saved " + std::to_string(import_saved) + " | failed " + std::to_string(import_failed);
        if (import_processed == import_total) { importing_ = false; import_entries_.clear(); import_images_.clear(); status = "Completed: " + status; }
    } catch (...) {
        importing_ = false; import_entries_.clear(); import_images_.clear(); recount();
        status = "Import failed; previous batches saved: " + std::to_string(import_saved); throw;
    }
}
void FaceWorkflow::cancelImport() {
    if (!importing_) return;
    importing_ = false; import_entries_.clear(); import_images_.clear();
    status = "Import stopped | saved " + std::to_string(import_saved) + " | failed " + std::to_string(import_failed); recount();
}
}
