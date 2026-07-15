#include "CodecSets.hpp"
#include <cassert>

const std::vector<std::pair<std::wstring, CodecSets::CodecInfo>>& CodecSets::GetEncoders() {
    static const auto encoders = [] {
        std::vector<std::pair<std::wstring, CodecSets::CodecInfo>> vec = {
            { L"h264_nvenc", {
                .presets = { L"p1", L"p2", L"p3", L"p4", L"p5", L"p6", L"p7" },
                .defaultPreset = L"p4",
                .tunes = { L"(none)", L"ull", L"ll", L"hq", L"llhq", L"lossless" },
                .defaultTune = L"(none)",
                .qualityModes = { QualityMode::CQP, QualityMode::VBR, QualityMode::CBR, QualityMode::Lossless },
                .defaultQualityMode = QualityMode::CQP,
            } },
            { L"h264_amf", {
                .presets = { L"quality", L"balanced", L"speed" },
                .defaultPreset = L"balanced",
                .tunes = {},
                .qualityModes = { QualityMode::CBR, QualityMode::CQP, QualityMode::VBR, QualityMode::VBR_LAT, QualityMode::QVBR, QualityMode::HQVBR, QualityMode::HQCBR },
                .defaultQualityMode = QualityMode::CQP,
            } },
            { L"h264_qsv", {
                .presets = { L"veryfast", L"faster", L"fast", L"medium", L"slow", L"slower", L"veryslow" },
                .defaultPreset = L"medium",
                .tunes = {},
                .qualityModes = { QualityMode::ICQ, QualityMode::CQP, QualityMode::VBR, QualityMode::CBR },
                .defaultQualityMode = QualityMode::ICQ,
            } },
            { L"h264_vaapi", {
                .presets = {},
                .tunes = {},
                .qualityModes = { QualityMode::CQP, QualityMode::VBR, QualityMode::CBR },
                .defaultQualityMode = QualityMode::CQP,
            } },
        };

        size_t hwEncoderCount = vec.size();
        for (size_t i = 0; i < hwEncoderCount; ++i) {
            auto hevcCopy = vec[i];

            assert(hevcCopy.first.starts_with(L"h264") &&
                   "hevc-name derivation assumes the first hwEncoderCount entries start with 'h264'");
            hevcCopy.first.replace(0, 4, L"hevc");

            vec.push_back(std::move(hevcCopy));
        }

        CodecSets::CodecInfo x264Info = {
            .presets = { L"ultrafast", L"superfast", L"veryfast", L"faster", L"fast", L"medium", L"slow", L"slower", L"veryslow" },
            .defaultPreset = L"medium",
            .tunes = { L"(none)", L"film", L"animation", L"grain", L"stillimage", L"psnr", L"ssim", L"fastdecode", L"zerolatency" },
            .defaultTune = L"(none)",
            .qualityModes = { QualityMode::CRF, QualityMode::VBR, QualityMode::CBR, QualityMode::ABR },
            .defaultQualityMode = QualityMode::CRF,
        };

        vec.push_back({ L"libx264", x264Info });
        vec.push_back({ L"libx265", x264Info });

        return vec;
    }();
    return encoders;
}

const CodecSets::CodecInfo* CodecSets::GetCodecInfo(std::wstring_view codecName) {
    const auto& encoders = GetEncoders();
    for (const auto& [name, info] : encoders) {
        if (name == codecName) {
            return &info;
        }
    }
    return nullptr;
}

const std::vector<const wchar_t*>& CodecSets::GetPixelFormats() {
    static const std::vector<const wchar_t*> formats = {
        L"yuv420p", L"yuv422p", L"yuv444p", L"nv12", L"nv16", L"nv21", L"p010le"
    };
    return formats;
}
