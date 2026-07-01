#include "CodecState.hpp"
#include "subprocess.hpp"

#include <string>
#include <format>
#include <sstream>
#include <iomanip>

std::wstring formatBitrate(int target, int max) {
    if (max > 0) return std::format(L"-b:v {}k -maxrate:v {}k", target, max);
    return std::format(L"-b:v {}k", target);
}

std::wstring CodecState::GetQualityFlags() {
    std::wstring_view codecView = this->codec;

    if (codecView.find(L"x264") != std::wstring_view::npos || codecView.find(L"x265") != std::wstring_view::npos) {
        switch (this->qualityMode) {
            case QualityMode::CRF: return std::format(L"-crf {}", this->qualityValue1);
            case QualityMode::CBR: return std::format(L"-b:v {}k -maxrate:v {}k -bufsize:v {}k", this->qualityValue1, this->qualityValue1, (int)(this->qualityValue1 * 1.5));
            case QualityMode::VBR: return formatBitrate(this->qualityValue1, this->qualityValue2);
            case QualityMode::ABR: return std::format(L"-b:v {}k", this->qualityValue1);
            default: return L"";
        }
    }

    if (codecView.find(L"nvenc") != std::wstring_view::npos) {
        switch (this->qualityMode) {
            case QualityMode::CQP:      return std::format(L"-rc vbr -cq {}", this->qualityValue1);
            case QualityMode::VBR:      return std::format(L"-rc vbr -b:v {}k -maxrate:v {}k", this->qualityValue1, this->qualityValue2);
            case QualityMode::CBR:      return std::format(L"-rc cbr -b:v {}k", this->qualityValue1);
            case QualityMode::Lossless: return L"-rc constqp -qp 0";
            default: return L"";
        }
    }

    if (codecView.find(L"amf") != std::wstring_view::npos) {
        switch (this->qualityMode) {
            case QualityMode::CBR:      return std::format(L"-rc cbr -b:v {}k", this->qualityValue1);
            case QualityMode::CQP:      return std::format(L"-rc cqp -qp_i {} -qp_p {} -qp_b {}", this->qualityValue1, this->qualityValue1, this->qualityValue1);
            case QualityMode::VBR:      return std::format(L"-rc vbr -b:v {}k -maxrate:v {}k", this->qualityValue1, this->qualityValue2);
            case QualityMode::VBR_LAT:  return std::format(L"-rc vbr_lat -b:v {}k -maxrate:v {}k", this->qualityValue1, this->qualityValue2);
            case QualityMode::QVBR:     return std::format(L"-rc qvbr -qvbr_quality {}", this->qualityValue1);
            case QualityMode::HQVBR:    return std::format(L"-rc hqvbr -qvbr_quality {}", this->qualityValue1);
            case QualityMode::HQCBR:    return std::format(L"-rc hqcbr -b:v {}k", this->qualityValue1);
            default: return L"";
        }
    }

    if (codecView.find(L"qsv") != std::wstring_view::npos) {
        switch (this->qualityMode) {
            case QualityMode::ICQ: return std::format(L"-rc icq -global_quality {}", this->qualityValue1);
            case QualityMode::CQP: return std::format(L"-rc cqp -qp {}", this->qualityValue1);
            case QualityMode::VBR: return std::format(L"-rc vbr -b:v {}k -maxrate:v {}k", this->qualityValue1, this->qualityValue2);
            case QualityMode::CBR: return std::format(L"-rc cbr -b:v {}k", this->qualityValue1);
            default: return L"";
        }
    }

    if (codecView.find(L"vaapi") != std::wstring_view::npos) {
        switch (this->qualityMode) {
            case QualityMode::CQP: return std::format(L"-qp {}", this->qualityValue1);
            case QualityMode::VBR: return std::format(L"-b:v {}k -maxrate:v {}k", this->qualityValue1, this->qualityValue2);
            case QualityMode::CBR: return std::format(L"-b:v {}k", this->qualityValue1);
            default: return L"";
        }
    }

    return L"";
}

std::wstring CodecState::GetFfmpegCommand() {
    std::wstring cmd = std::format(L"\"ffmpeg\" -y -f rawvideo -pix_fmt bgr24 -s {}x{} -r {}/{} -i - ", 
                                  this->width, this->height, this->fpsNum, this->fpsDen);

    if (!this->codec.empty()) {
        cmd += std::format(L"-c:v {} ", this->codec);
    }
    
    if (!this->preset.empty()) {
        cmd += std::format(L"-preset {} ", this->preset);
    }
    if (!this->tune.empty() && this->tune != L"(none)") {
        cmd += std::format(L"-tune {} ", this->tune);
    }

    cmd += this->GetQualityFlags() + L" ";

    if (!this->pix_fmt.empty()) {
        cmd += std::format(L"-pix_fmt {} ", this->pix_fmt);
    }
    
    if (!this->extra_args.empty()) {
        cmd += std::format(L"{} ", this->extra_args);
    }

    std::wstringstream pathEscaper;
    pathEscaper << std::quoted(this->path);
    cmd += std::format(L"-vf \"vflip\" -an {}", pathEscaper.str());

    return cmd;
}

bool testCodec(std::wstring_view codec, int width, int height) {
    auto testCmd = std::format(L"\"ffmpeg\" -hide_banner -loglevel error -f lavfi -i nullsrc=s={}x{}:d=1 -c:v {} -f null -", width, height, codec);
    return subprocess::Popen(testCmd).wait() == 0;
}

const wchar_t* determineBestCodec(int width, int height) {
    for (const auto& codec : CodecState::defaultEncoders) {
        if (testCodec(codec, width, height)) {
            return codec;
        }
    }
    // libx264 takes everything, so getting to this code path implies
    // an error with the testing methodology
    return L"libx264";
}

void CodecState::SetAutoDefaults() {
    this->codec = determineBestCodec(this->width ? this->width : 1920, this->height ? this->height : 1080);
    this->qualityMode = QualityMode::CRF;
    std::tie(this->qualityValue1, this->qualityValue2) = QualityUtils::GetDefaultQualityForMode(this->qualityMode);
    this->preset = L"";
    this->tune = L"";
    this->pix_fmt = L"yuv420p";
    this->extra_args = L"";
}

QualityDefaults QualityUtils::GetDefaultQualityForMode(QualityMode mode) {
    switch (mode) {
        case QualityMode::CBR:      return { 16000, 0 };
        case QualityMode::VBR:      return { 16000, 24000 };
        case QualityMode::CRF:      return { 18, 0 };
        case QualityMode::Lossless: return { 0, 0 };
        case QualityMode::CQP:      return { 18, 0 };
        case QualityMode::ABR:      return { 16000, 0 };
        case QualityMode::VBR_LAT:  return { 16000, 24000 };
        case QualityMode::QVBR:     return { 16000, 24000 };
        case QualityMode::HQVBR:    return { 16000, 24000 };
        case QualityMode::HQCBR:    return { 16000, 0 };
        case QualityMode::ICQ:      return { 18, 0 };
        default:                    return { 0, 0 };
    }
}

QualityMode QualityUtils::GetQualityModeFromString(const std::wstring& modeStr) {
    if (modeStr == L"CBR") return QualityMode::CBR;
    if (modeStr == L"VBR") return QualityMode::VBR;
    if (modeStr == L"CRF") return QualityMode::CRF;
    if (modeStr == L"Lossless") return QualityMode::Lossless;
    if (modeStr == L"CQP") return QualityMode::CQP;
    if (modeStr == L"ABR") return QualityMode::ABR;
    if (modeStr == L"VBR_LAT") return QualityMode::VBR_LAT;
    if (modeStr == L"QVBR") return QualityMode::QVBR;
    if (modeStr == L"HQVBR") return QualityMode::HQVBR;
    if (modeStr == L"HQCBR") return QualityMode::HQCBR;
    if (modeStr == L"ICQ") return QualityMode::ICQ;

    return QualityMode::None;
}

const wchar_t* QualityUtils::GetStringFromQualityMode(QualityMode mode) {
    switch (mode) {
        case QualityMode::CBR: return L"CBR";
        case QualityMode::VBR: return L"VBR";
        case QualityMode::CRF: return L"CRF";
        case QualityMode::Lossless: return L"Lossless";
        case QualityMode::CQP: return L"CQP";
        case QualityMode::ABR: return L"ABR";
        case QualityMode::VBR_LAT: return L"VBR_LAT";
        case QualityMode::QVBR: return L"QVBR";
        case QualityMode::HQVBR: return L"HQVBR";
        case QualityMode::HQCBR: return L"HQCBR";
        case QualityMode::ICQ: return L"ICQ";
        default: return L"";
    }
}