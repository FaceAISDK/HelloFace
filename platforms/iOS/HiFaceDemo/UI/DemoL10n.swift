import Foundation

/// The Demo uses the language selected by the operating system without changing SDK errors, logs,
/// or feature formats.
/// Demo 使用系统选择的应用语言，不改变 SDK 的错误、日志或特征格式。
enum DemoL10n {
    private static let supportedLanguages = ["en", "zh-Hans"]

    static func language(for preferredLanguage: String?) -> String {
        let preferences = preferredLanguage.map { [$0] } ?? Locale.preferredLanguages
        for preference in preferences {
            let languageCode = preference.lowercased().split(whereSeparator: { $0 == "-" || $0 == "_" }).first
            if languageCode == "zh" { return "zh-Hans" }
            if languageCode == "en" { return "en" }
        }
        return "en"
    }

    private static let bundledLanguages: [String: Bundle] = {
        var bundles: [String: Bundle] = [:]
        for language in supportedLanguages {
            if let path = Bundle.main.path(forResource: language, ofType: "lproj"),
               let bundle = Bundle(path: path) { bundles[language] = bundle }
        }
        return bundles
    }()

    static func text(_ key: String, _ arguments: [String] = [],
                     preferredLanguage: String? = nil, bundle: Bundle = .main) -> String {
        let selectedLanguage = language(for: preferredLanguage)
        let localizedBundle: Bundle
        if bundle === Bundle.main {
            localizedBundle = bundledLanguages[selectedLanguage] ?? bundledLanguages["en"] ?? bundle
        } else {
            let path = bundle.path(forResource: selectedLanguage, ofType: "lproj")
                ?? bundle.path(forResource: "en", ofType: "lproj")
            localizedBundle = path.flatMap(Bundle.init(path:)) ?? bundle
        }
        let format = localizedBundle.localizedString(forKey: key, value: nil, table: "Localizable")
        // Replace once so user-entered text such as {0} is not interpreted as another format placeholder.
        // 一次替换，防止用户输入的 {0} 等文本再次被当成格式占位符。
        var result = ""
        var remainder = format[...]
        while let open = remainder.firstIndex(of: "{"), let close = remainder[open...].firstIndex(of: "}") {
            result += remainder[..<open]
            let placeholder = remainder[remainder.index(after: open)..<close]
            if let index = Int(placeholder), arguments.indices.contains(index) { result += arguments[index] }
            else { result += remainder[open...close] }
            remainder = remainder[remainder.index(after: close)...]
        }
        return result + remainder
    }
}
