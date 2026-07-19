#include "CodecSets.hpp"
#include <cassert>
#include <format>

std::wstring formatBitrate(int target, int max) {
    if (max > 0) return std::format(L"-b:v {}k -maxrate:v {}k", target, max);
    return std::format(L"-b:v {}k", target);
}

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
                .formatQualityFlags = [](QualityMode mode, int v1, int v2) -> std::wstring {
                    switch (mode) {
                        case QualityMode::CQP:      return std::format(L"-rc vbr -cq {}", v1);
                        case QualityMode::VBR:      return std::format(L"-rc vbr -b:v {}k -maxrate:v {}k", v1, v2);
                        case QualityMode::CBR:      return std::format(L"-rc cbr -b:v {}k", v1);
                        case QualityMode::Lossless: return L"-rc constqp -qp 0";
                        default: return L"";
                    }
                },
                .testExtraArgs = L"-preset p1 -tune ull -delay 0 -bf 0",
            } },
            { L"h264_amf", {
                .presets = { L"quality", L"balanced", L"speed" },
                .defaultPreset = L"balanced",
                .tunes = {},
                .qualityModes = { QualityMode::CBR, QualityMode::CQP, QualityMode::VBR, QualityMode::VBR_LAT, QualityMode::QVBR, QualityMode::HQVBR, QualityMode::HQCBR },
                .defaultQualityMode = QualityMode::CQP,
                .formatQualityFlags = [](QualityMode mode, int v1, int v2) -> std::wstring {
                    switch (mode) {
                        case QualityMode::CBR:     return std::format(L"-rc cbr -b:v {}k", v1);
                        case QualityMode::CQP:     return std::format(L"-rc cqp -qp_i {} -qp_p {} -qp_b {}", v1, v1, v1);
                        case QualityMode::VBR:     return std::format(L"-rc vbr_peak -b:v {}k -maxrate:v {}k", v1, v2);
                        case QualityMode::VBR_LAT: return std::format(L"-rc vbr_lat -b:v {}k -maxrate:v {}k", v1, v2);
                        case QualityMode::QVBR:    return std::format(L"-rc qvbr -qvbr_quality {}", v1);
                        case QualityMode::HQVBR:   return std::format(L"-rc hqvbr -qvbr_quality {}", v1);
                        case QualityMode::HQCBR:   return std::format(L"-rc hqcbr -b:v {}k", v1);
                        default: return L"";
                    }
                },
                .testExtraArgs = L"-quality speed -bf 0",
            } },
            { L"h264_qsv", {
                .presets = { L"veryfast", L"faster", L"fast", L"medium", L"slow", L"slower", L"veryslow" },
                .defaultPreset = L"medium",
                .tunes = {},
                .qualityModes = { QualityMode::ICQ, QualityMode::CQP, QualityMode::VBR, QualityMode::CBR },
                .defaultQualityMode = QualityMode::ICQ,
                .formatQualityFlags = [](QualityMode mode, int v1, int v2) -> std::wstring {
                    switch (mode) {
                        case QualityMode::ICQ: return std::format(L"-rc icq -global_quality {}", v1);
                        case QualityMode::CQP: return std::format(L"-rc cqp -qp {}", v1);
                        case QualityMode::VBR: return std::format(L"-rc vbr -b:v {}k -maxrate:v {}k", v1, v2);
                        case QualityMode::CBR: return std::format(L"-rc cbr -b:v {}k", v1);
                        default: return L"";
                    }
                },
                .testExtraArgs = L"-preset veryfast -bf 0 -async_depth 1",
            } },
            { L"h264_vaapi", {
                .presets = {},
                .tunes = {},
                .qualityModes = { QualityMode::CQP, QualityMode::VBR, QualityMode::CBR },
                .defaultQualityMode = QualityMode::CQP,
                .formatQualityFlags = [](QualityMode mode, int v1, int v2) -> std::wstring {
                    switch (mode) {
                        case QualityMode::CQP: return std::format(L"-qp {}", v1);
                        case QualityMode::VBR: return std::format(L"-b:v {}k -maxrate:v {}k", v1, v2);
                        case QualityMode::CBR: return std::format(L"-b:v {}k", v1);
                        default: return L"";
                    }
                },
                .testExtraArgs = L"-bf 0",
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
            .formatQualityFlags = [](QualityMode mode, int v1, int v2) -> std::wstring {
                switch (mode) {
                    case QualityMode::CRF: return std::format(L"-crf {}", v1);
                    case QualityMode::CBR: return std::format(L"-b:v {}k -maxrate:v {}k -bufsize:v {}k", v1, v1, (int)(v1 * 1.5));
                    case QualityMode::VBR: return formatBitrate(v1, v2);
                    case QualityMode::ABR: return std::format(L"-b:v {}k", v1);
                    default: return L"";
                }
            },
            .testExtraArgs = L"-preset ultrafast -tune zerolatency",
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
