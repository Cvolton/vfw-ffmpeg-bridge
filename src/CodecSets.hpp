#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <functional>

enum class QualityMode {
    None = -1,
    CBR = 0,
    VBR = 1,
    CRF = 2,
    Lossless = 3,
    CQP = 4,
    ABR = 5,
    VBR_LAT = 6, // AMF specific
    QVBR = 7,    // AMF specific
    HQVBR = 8,   // AMF specific
    HQCBR = 9,   // AMF specific
    ICQ = 10     // QSV specific
};

namespace CodecSets {
    constexpr const wchar_t* CustomEncoderLabel = L"Other";

    struct CodecInfo {
        std::vector<const wchar_t*> presets;
        const wchar_t* defaultPreset = nullptr;

        std::vector<const wchar_t*> tunes;
        const wchar_t* defaultTune = L"(none)";

        std::vector<QualityMode> qualityModes;
        QualityMode defaultQualityMode = QualityMode::None;

        std::function<std::wstring(QualityMode mode, int value1, int value2)> formatQualityFlags;

        std::wstring testExtraArgs;
    };

    const std::vector<std::pair<std::wstring, CodecInfo>>& GetEncoders();
    const CodecInfo* GetCodecInfo(std::wstring_view codecName);
    const std::vector<const wchar_t*>& GetPixelFormats();
}
