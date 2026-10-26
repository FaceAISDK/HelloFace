import Foundation

@main struct DemoL10nTests {
    static func main() {
        for language in ["zh", "zh-Hans", "zh-Hant-TW", "zh-CN"] {
            precondition(DemoL10n.language(for: language) == "zh-Hans")
            precondition(DemoL10n.text("任务正在执行", preferredLanguage: language) == "任务正在执行")
        }
        for language in ["en", "en-CN", "ja-CN", "", nil] {
            precondition(DemoL10n.language(for: language) == "en")
            precondition(DemoL10n.text("任务正在执行", preferredLanguage: language) == "Import in progress")
        }
        precondition(DemoL10n.text("{0}：已保存 {1} 条，失败 {2} 条", ["Stopped", "37", "2"], preferredLanguage: "en") == "Stopped: saved 37, failed 2")
        precondition(DemoL10n.text("{0}：已保存 {1} 条，失败 {2} 条", ["{1}", "37", "2"], preferredLanguage: "en") == "{1}: saved 37, failed 2")
        print("Demo localization: preferred language, native resources and safe interpolation passed")
    }
}
