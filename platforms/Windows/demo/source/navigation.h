#pragma once
#include <cstddef>
namespace hiface_demo {
enum class Page { Detection, Reference, Verification, Liveness, Search, ImageSearch, Comparison, Gallery, CameraSettings, Enrollment, Home };
struct PageInfo { const wchar_t* title; const wchar_t* description; bool camera; };
inline constexpr PageInfo page_info[] = {
    {L"实时人脸检测", L"人脸框、五点、468 点与头姿", true},
    {L"人脸采集 / 1:1 基准", L"引导采集、图片或已有人员设为基准", true},
    {L"动态 1:1 验证", L"基准比对、动作挑战和静默活体", true},
    {L"动作 / 静默活体", L"独立演示动作与纯静默活体流程", true},
    {L"1:N / M:N 搜索", L"连续识别、范围筛选和暂停恢复", true},
    {L"图片人脸搜索", L"本地图片逐脸搜索，无需摄像头", false},
    {L"双图片 1:1 比对", L"选择两张图片，比较最大人脸", false},
    {L"人脸库管理", L"人员维护、分页、JSON 和图片导入", false},
    {L"相机设置", L"设备、分辨率、旋转、镜像与预览", true},
    {L"人脸录入", L"独立采集、预览确认后保存到人脸库", true},
    {L"功能首页", L"选择一个独立功能开始演示", false}
};
inline constexpr int kPageCount = int(sizeof(page_info) / sizeof(page_info[0]));
inline constexpr int kFeatureCount = kPageCount - 1;
// Android's eight home features first, then its search enrollment/library subpages.
// 先列出 Android 的八个首页功能，再列出搜索录入与库管理子页。
inline constexpr Page feature_pages[] = {Page::Detection, Page::Reference, Page::Verification, Page::Liveness,
    Page::Search, Page::Comparison, Page::ImageSearch, Page::CameraSettings, Page::Enrollment, Page::Gallery};
inline constexpr Page navigation_pages[] = {Page::Home, Page::Detection, Page::Reference, Page::Verification, Page::Liveness,
    Page::Search, Page::Comparison, Page::ImageSearch, Page::CameraSettings, Page::Enrollment, Page::Gallery};
static_assert(sizeof(feature_pages) / sizeof(Page) == kFeatureCount);
static_assert(sizeof(navigation_pages) / sizeof(Page) == kPageCount);
inline bool usesCamera(Page page) { return page_info[static_cast<std::size_t>(page)].camera; }
}
