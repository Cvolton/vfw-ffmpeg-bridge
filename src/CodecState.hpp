#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <string_view>
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

enum class LocationSelection {
    Ask,
    NextToAvi,
    Folder,
};

struct CodecState {
    int width = 0;
    int height = 0;
    int frameCount = 0;

    DWORD fpsNum = 24;
    DWORD fpsDen = 1;

    bool selectAuto = true;
    bool tmAudioHooks = true;

    std::wstring codec = L"libx264";
    QualityMode qualityMode = QualityMode::CRF;
    int qualityValue1 = 21;
    int qualityValue2 = 0;

    std::wstring preset = L"";
    std::wstring tune = L"";
    std::wstring pix_fmt = L"";

    std::wstring extra_args = L"";

    LocationSelection locationSelection = LocationSelection::Ask;
    std::wstring otherLocation = L"";

    std::wstring path = L"c:\\temp\\output.mp4";

    std::wstring ffmpegPath = L"ffmpeg";
    std::unique_ptr<subprocess::Popen> ffmpegProcess = nullptr;

    std::wstring GetQualityFlags();
    std::wstring GetFfmpegCommand();
    void SetAutoDefaults();
    bool SetRenderPath();

    std::vector<uint8_t> Serialize();
    bool Deserialize(const std::vector<uint8_t>& data);

    void Save();
    void Load();

    bool FindBestFfmpeg();

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
    QualityMode GetDefaultQualityModeForCodec(std::wstring_view codec);
}