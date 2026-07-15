#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <string_view>
#include <memory>
#include "subprocess.hpp"
#include "CodecSets.hpp"

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

    bool ffmpegCrashed = false;

    bool selectAuto = true;
    bool tmAudioHooks = true;

    std::wstring codec = L"libx264";
    QualityMode qualityMode = QualityMode::CRF;
    int qualityValue1 = 21;
    int qualityValue2 = 0;

    std::wstring preset = L"";
    std::wstring tune = L"";
    std::wstring pix_fmt = L"";
    std::wstring input_pix_fmt = L"bgr24";
    bool shouldVFlip = true;

    std::wstring extra_args = L"";

    LocationSelection locationSelection = LocationSelection::Ask;
    std::wstring otherLocation = L"";

    std::wstring path = L"c:\\temp\\output.mp4";

    std::wstring ffmpegPath = L"ffmpeg";
    std::unique_ptr<subprocess::Popen> ffmpegProcess = nullptr;

    std::wstring lastBestCodec = L"";

    std::wstring GetQualityFlags();
    std::wstring GetFfmpegCommand();
    void SetAutoDefaults();
    bool SetRenderPath();

    std::vector<uint8_t> Serialize();
    bool Deserialize(const std::vector<uint8_t>& data);

    void Save();
    void Load();

    bool FindBestFfmpeg();
};

using QualityDefaults = std::pair<int, int>;
namespace QualityUtils {
    QualityDefaults GetDefaultQualityForMode(QualityMode mode);
    QualityMode GetQualityModeFromString(const std::wstring& modeStr);
    const wchar_t* GetStringFromQualityMode(QualityMode mode);
    QualityMode GetDefaultQualityModeForCodec(std::wstring_view codec);
}
