#pragma once
#include <minwindef.h>
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
    ICQ = 10     // QSV specific Intelligent Constant Quality
};

struct CodecState {
    int width = 0;
    int height = 0;
    int frameCount = 0;

    DWORD fpsNum = 24;
    DWORD fpsDen = 1;

    std::wstring codec = L"av1_nvenc";
    QualityMode qualityMode = QualityMode::CRF;
    int qualityValue1 = 28;
    int qualityValue2 = 0;

    std::wstring preset = L"p1";
    std::wstring tune = L"ull";
    std::wstring pix_fmt = L"yuv420p";

    std::wstring bitrate = L"0";
    std::wstring extra_args = L"-multipass 0";
    std::wstring path = L"c:\\temp\\output.mp4";

    std::unique_ptr<subprocess::Popen> ffmpegProcess = nullptr;
};