#pragma once

enum Language : uint16_t {
    None,
    // ja
    Japanese,
    // en
    English,
    // de
    German,
    // fr
    French,
    // chs
    ChineseSimplified,
    // cht
    ChineseTraditional,
    // ko
    Korean
};

inline std::string_view getLanguageCode(const Language lang) {
    switch(lang) {
        case Language::Japanese:
            return "ja";
        case Language::English:
            return "en";
        case Language::German:
            return "de";
        case Language::French:
            return "fr";
        case Language::ChineseSimplified:
            return "chs";
        case Language::ChineseTraditional:
            return "cht";
        case Language::Korean:
            return "ko";
    }

    return "";
}