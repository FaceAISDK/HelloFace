import Foundation

@main struct DemoL10nTests {
    static func main() {
        for language in ["zh", "zh-Hans", "zh-Hant-TW", "zh-CN"] {
            precondition(DemoL10n.language(for: language) == "zh-Hans")
            precondition(DemoL10n.text("任务正在执行", preferredLanguage: language) == "任务正在执行")
        }
        for language in ["en", "en-CN", "ja-CN", ""] {
            precondition(DemoL10n.language(for: language) == "en")
            precondition(DemoL10n.text("任务正在执行", preferredLanguage: language) == "Import in progress")
        }
        let systemLanguage = Locale.preferredLanguages.compactMap { preference -> String? in
            let code = preference.lowercased().split(whereSeparator: { $0 == "-" || $0 == "_" }).first
            return code == "zh" ? "zh-Hans" : code == "en" ? "en" : nil
        }.first ?? "en"
        precondition(DemoL10n.language(for: nil) == systemLanguage)
        precondition(DemoL10n.text("{0}：已保存 {1} 条，失败 {2} 条", ["Stopped", "37", "2"], preferredLanguage: "en") == "Stopped: saved 37, failed 2")
        precondition(DemoL10n.text("{0}：已保存 {1} 条，失败 {2} 条", ["{1}", "37", "2"], preferredLanguage: "en") == "{1}: saved 37, failed 2")
        for version in ["0.8.0", "0.8.1"] {
            precondition(DemoL10n.text("关于 HelloFace SDK {0}", [version], preferredLanguage: "zh") == "关于 HelloFace SDK \(version)")
            precondition(DemoL10n.text("关于 HelloFace SDK {0}", [version], preferredLanguage: "en") == "About HelloFace SDK \(version)")
        }
        print("Demo localization: preferred language, native resources and safe interpolation passed")
    }
}
