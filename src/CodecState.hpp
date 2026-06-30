#pragma once
#include <minwindef.h>
#include <string>
#include <memory>
#include "subprocess.hpp"

enum class QualityMode {
    CBR = 0,
    VBR = 1,
    CRF = 2,
    Lossless = 3
};

struct CodecState {
    int width = 0;
    int height = 0;
    int frameCount = 0;

    DWORD fpsNum = 24;
    DWORD fpsDen = 1;

    /*std::string codec = "libx264";
    std::string bitrate = "100M";
    std::string extra_args;// = "-preset ultrafast -tune zerolatency";*/

    /*std::string codec = "av1_nvenc";
    std::string bitrate = "0"; 
    std::string extra_args = "-preset p4 -tune hq -rc vbr -cq 28 -spatial-aq 1 -multipass 0 -pix_fmt p010le";*/

    std::wstring codec = L"av1_nvenc";
    QualityMode qualityMode = QualityMode::CRF;
    int qualityValue = 28;
    int qualityValue2 = 0;

    std::wstring preset = L"p1";
    std::wstring tune = L"ull";
    std::wstring pix_fmt = L"yuv420p";

    std::wstring bitrate = L"0";
    std::wstring extra_args = L"-multipass 0";
    std::wstring path = L"c:\\temp\\output.mp4";

    std::unique_ptr<subprocess::Popen> ffmpegProcess = nullptr;
};