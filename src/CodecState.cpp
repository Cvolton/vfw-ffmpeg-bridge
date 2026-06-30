#include "CodecState.hpp"

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