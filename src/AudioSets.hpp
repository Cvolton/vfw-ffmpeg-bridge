#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <functional>

enum class AudioQualityMode {
    None,
    Bitrate,
    VBRQuality,
    CompressionLevel,
};

namespace AudioSets {
    constexpr const wchar_t* CustomEncoderLabel = L"Other";

    struct AudioQualityModeInfo {
        int minValue = 0;
        int maxValue = 0;
        int defaultValue = 0;
        const wchar_t* label = L"";
    };

    struct AudioCodecInfo {
        std::vector<std::pair<AudioQualityMode, AudioQualityModeInfo>> qualityModes;
        AudioQualityMode defaultQualityMode = AudioQualityMode::None;
        std::function<std::wstring(AudioQualityMode mode, int value)> formatQualityFlags;
    };

    const std::vector<std::pair<std::wstring, AudioCodecInfo>>& GetEncoders();
    const AudioCodecInfo* GetCodecInfo(std::wstring_view codecName);
    const AudioQualityModeInfo* GetQualityModeInfo(const AudioCodecInfo& info, AudioQualityMode mode);
}
