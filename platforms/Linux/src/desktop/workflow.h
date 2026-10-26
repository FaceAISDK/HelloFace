#pragma once
#include "camera.h"
#include "gallery.h"
#include "image_search.h"
#include "settings.h"
#include "enrollment_draft.h"
#include <filesystem>
#include <memory>
namespace hiface_linux_demo {
struct Box {
    float x, y, width, height;
    std::string label;
    bool matched = false;
    std::vector<HiFacePointV1> points;
    bool selected = false;
};
// Worker-thread coordinator owning one SDK session and one database handle.
// 工作流线程的协调器，独占一个 SDK 会话和一个数据库句柄。
class FaceWorkflow {
public:
    FaceWorkflow(const std::string& models, const std::string& database);
    ~FaceWorkflow();
    void action(char key);
    void process(const CameraFrame& frame);
    void stop();
    void applySettings(const DemoSettings& settings);
    const DemoSettings& settings() const { return settings_; }
    void setEnrollment(const std::string& id, const std::string& group, const std::string& tag);
    void confirmEnrollment(const std::string& id, const std::string& group, const std::string& tag);
    bool pendingEnrollment() const { return enrollment_.pending(); }
    uint64_t enrollmentGeneration() const { return enrollment_generation_; }
    void clearPageResults();
    void imageOperation(const std::string& operation, const std::string& first, const std::string& second,
        const std::string& id, const std::string& group, const std::string& tag);
    void searchPhoto(const std::string& path, const std::function<bool()>& cancelled = {});
    void clearPhotoSearch();
    std::string photoSearchSummary() const;
    std::shared_ptr<Image> search_photo;
    std::shared_ptr<const ImageSearchReport> photo_search;
    std::string search_photo_path;
    void loadGallery(uint32_t offset, const std::string& id, const std::string& group, const std::string& tag);
    void deleteEntry(const std::string& id);
    void updateEntry(const std::string& id, const std::string& group, const std::string& tag);
    void selectReference(const std::string& id);
    void clearReference();
    void exportGallery(const std::string& path, const std::string& group, const std::string& tag);
    void startImport(const std::string& path, bool images, const std::string& group);
    void stepImport();
    void cancelImport();
    bool importing() const { return importing_; }
    void pauseSearch();
    void toggleMirror();
    char mode() const { return mode_; }
    std::string status = "Ready. Look at the camera.";
    std::vector<Box> boxes;
    double inference_ms = 0;
    uint64_t gallery_count = 0;
    uint64_t filtered_count = 0;
    uint32_t gallery_offset = 0;
    std::vector<GalleryEntry> gallery_rows;
    std::vector<GalleryEntry> image_matches;
    std::string reference_id;
    std::shared_ptr<Image> image_first, image_second, aligned_preview;
    uint32_t import_saved = 0, import_failed = 0, import_processed = 0, import_total = 0;
private:
    void finishFlow();
    void beginFlow(char mode);
    void saveReference(const Feature& feature, const std::string& id);
    void recount();
    Session session_;
    Database database_;
    std::filesystem::path data_directory_;
    Feature reference_{};
    bool has_reference_ = false;
    char mode_ = 'D';
    DemoSettings settings_;
    std::string enrollment_id_, enrollment_group_, enrollment_tag_;
    EnrollmentDraft enrollment_;
    uint64_t enrollment_generation_ = 0;
    bool importing_ = false, importing_images_ = false;
    std::string import_group_;
    std::vector<GalleryEntry> import_entries_;
    std::vector<std::filesystem::path> import_images_;
};
int64_t monotonicMilliseconds();
}
