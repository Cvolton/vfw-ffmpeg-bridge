#include "AudioSets.hpp"
#include <format>

const std::vector<std::pair<std::wstring, AudioSets::AudioCodecInfo>>& AudioSets::GetEncoders() {
    static const std::vector<std::pair<std::wstring, AudioSets::AudioCodecInfo>> encoders = {
        { L"aac", {
            .qualityModes = {
                { AudioQualityMode::Bitrate, { 64, 320, 256, L"Bitrate (kbps)" } },
            },
            .defaultQualityMode = AudioQualityMode::Bitrate,
            .formatQualityFlags = [](AudioQualityMode, int v) -> std::wstring {
                return std::format(L"-b:a {}k", v);
            },
        } },
        { L"libmp3lame", {
            .qualityModes = {
                { AudioQualityMode::Bitrate,    { 64, 320, 320, L"Bitrate (kbps)" } },
                { AudioQualityMode::VBRQuality, { 0, 9, 0, L"VBR Quality (0=best)" } },
            },
            .defaultQualityMode = AudioQualityMode::Bitrate,
            .formatQualityFlags = [](AudioQualityMode mode, int v) -> std::wstring {
                if (mode == AudioQualityMode::VBRQuality) {
                    return std::format(L"-q:a {}", v);
                }
                return std::format(L"-b:a {}k", v);
            },
        } },
        { L"flac", {
            .qualityModes = {
                { AudioQualityMode::CompressionLevel, { 0, 8, 5, L"Compression Level" } },
            },
            .defaultQualityMode = AudioQualityMode::CompressionLevel,
            .formatQualityFlags = [](AudioQualityMode, int v) -> std::wstring {
                return std::format(L"-compression_level {}", v);
            },
        } },
        { L"alac", {
            .qualityModes = {},
            .defaultQualityMode = AudioQualityMode::None,
            .formatQualityFlags = [](AudioQualityMode, int) -> std::wstring { return L""; },
        } },
        { L"pcm_s16le", {
            .qualityModes = {},
            .defaultQualityMode = AudioQualityMode::None,
            .formatQualityFlags = [](AudioQualityMode, int) -> std::wstring { return L""; },
        } },
    };
    return encoders;
}

const AudioSets::AudioCodecInfo* AudioSets::GetCodecInfo(std::wstring_view codecName) {
    for (const auto& [name, info] : GetEncoders()) {
        if (name == codecName) {
            return &info;
        }
    }
    return nullptr;
}

const AudioSets::AudioQualityModeInfo* AudioSets::GetQualityModeInfo(const AudioCodecInfo& info, AudioQualityMode mode) {
    for (const auto& [m, modeInfo] : info.qualityModes) {
        if (m == mode) {
            return &modeInfo;
        }
    }
    return nullptr;
}
