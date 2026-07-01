#pragma once
#include <windows.h>
#include <string>
#include <memory>
#include "subprocess.hpp"

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

struct CodecState {
    int width = 0;
    int height = 0;
    int frameCount = 0;

    DWORD fpsNum = 24;
    DWORD fpsDen = 1;

    bool selectAuto = true;

    std::wstring codec = L"av1_nvenc";
    QualityMode qualityMode = QualityMode::CRF;
    int qualityValue1 = 28;
    int qualityValue2 = 0;

    std::wstring preset = L"p1";
    std::wstring tune = L"ull";
    std::wstring pix_fmt = L"yuv420p";

    std::wstring extra_args = L"-multipass 0";
    std::wstring path = L"c:\\temp\\output.mp4";

    std::unique_ptr<subprocess::Popen> ffmpegProcess = nullptr;

    std::wstring GetQualityFlags();
    std::wstring GetFfmpegCommand();
    void SetAutoDefaults();

    static constexpr const wchar_t* defaultEncoders[] = {
        L"h264_nvenc", L"h264_amf", L"h264_qsv", L"h264_vaapi", 
        L"hevc_nvenc", L"hevc_amf", L"hevc_qsv", L"hevc_vaapi", 
        L"libx264", L"libx265", L"Other"
    };

    static constexpr const wchar_t* x264Quals[] = { L"CRF", L"VBR", L"CBR", L"ABR" };
    static constexpr const wchar_t* nvencQuals[] = { L"CQP", L"VBR", L"CBR", L"Lossless" };
    static constexpr const wchar_t* amfQuals[] = { L"CBR", L"CQP", L"VBR", L"VBR_LAT", L"QVBR", L"HQVBR", L"HQCBR" };
    static constexpr const wchar_t* qsvQuals[] = { L"ICQ", L"CQP", L"VBR", L"CBR" };
    static constexpr const wchar_t* vaapiQuals[] = { L"CQP", L"VBR", L"CBR" };
};

using QualityDefaults = std::pair<int, int>;
namespace QualityUtils {
    QualityDefaults GetDefaultQualityForMode(QualityMode mode);
    QualityMode GetQualityModeFromString(const std::wstring& modeStr);
    const wchar_t* GetStringFromQualityMode(QualityMode mode);
}