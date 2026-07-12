#include "CodecState.hpp"
#include "subprocess.hpp"
#include "utils.hpp"

#include <string>
#include <format>
#include <vector>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <fstream>

static auto appdataPath = []() -> std::filesystem::path {
    wchar_t* appdata = nullptr;
    size_t len = 0;
    _wdupenv_s(&appdata, &len, L"APPDATA");
    if (appdata) {
        std::filesystem::path path(appdata);
        free(appdata);
        auto ret = path / "vfw-ffmpeg-bridge";

        std::error_code ec;
        std::filesystem::create_directories(ret, ec);
        if (ec) {
            MessageBoxW(nullptr, L"Failed to create settings directory.", L"VfW FFmpeg Bridge", MB_OK | MB_ICONERROR);
            return std::filesystem::path();
        }
        return ret;
    }
    return std::filesystem::path();
}();

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
            case QualityMode::VBR:      return std::format(L"-rc vbr_peak -b:v {}k -maxrate:v {}k", this->qualityValue1, this->qualityValue2);
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
    std::wstring cmd = std::format(L"\"{}\" -y -f rawvideo -pix_fmt bgr24 -s {}x{} -r {}/{} -i - ", 
                                  this->ffmpegPath, this->width, this->height, this->fpsNum, this->fpsDen);

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
    cmd += L"-vf \"vflip\" -an ";
    
    if (!this->extra_args.empty()) {
        cmd += std::format(L"{} ", this->extra_args);
    }

    std::wstringstream pathEscaper;
    pathEscaper << std::quoted(this->path);
    cmd += pathEscaper.str();

    return cmd;
}

bool testCodec(std::wstring_view codec, int width, int height) {
    auto testCmd = std::format(L"\"ffmpeg\" -hide_banner -loglevel error -f lavfi -i nullsrc=s={}x{} -vframes 1 -c:v {} -f null -", width, height, codec);
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

QualityMode QualityUtils::GetDefaultQualityModeForCodec(std::wstring_view codec) {
    if (codec.find(L"x264") != std::wstring_view::npos || codec.find(L"x265") != std::wstring_view::npos) {
        return QualityMode::CRF;
    }
    if (codec.find(L"nvenc") != std::wstring_view::npos) {
        return QualityMode::CQP;
    }
    if (codec.find(L"amf") != std::wstring_view::npos) {
        return QualityMode::CQP;
    }
    if (codec.find(L"qsv") != std::wstring_view::npos) {
        return QualityMode::ICQ;
    }
    if (codec.find(L"vaapi") != std::wstring_view::npos) {
        return QualityMode::CQP;
    }
    return QualityMode::None;
}

std::wstring GetAutoGeneratedName() {
    // e.g. 2026-07-05 17-23-54.mp4
    auto now = std::chrono::zoned_time{std::chrono::current_zone(), 
                                        std::chrono::system_clock::now()};
    auto local = std::chrono::floor<std::chrono::seconds>(now.get_local_time());
    return std::format(L"{:%Y-%m-%d %H-%M-%S}.mp4", local);
}

bool CodecState::SetRenderPath() {
    if (this->locationSelection == LocationSelection::Ask) {
        if (!this->path.empty()) {
            auto lastSlash = this->path.find_last_of(L"\\/");
            if (lastSlash != std::wstring::npos) {
                this->path = this->path.substr(0, lastSlash + 1);
            }
        }

        wchar_t fileBuffer[MAX_PATH] = {};
        if (!this->path.empty()) {
            wcsncpy_s(fileBuffer, (this->path + GetAutoGeneratedName()).c_str(), _TRUNCATE);
        } else {
            wcscpy_s(fileBuffer, GetAutoGeneratedName().c_str());
        }

        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = GetActiveWindow();
        ofn.lpstrFilter = L"MP4 Video (*.mp4)\0*.mp4\0MKV Video (*.mkv)\0*.mkv\0MOV Video (*.mov)\0*.mov\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = fileBuffer;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrDefExt = L"mp4";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetSaveFileNameW(&ofn)) {
            this->path = fileBuffer;
            return true;
        } else {
            return false;
        }
    } else if (this->locationSelection == LocationSelection::NextToAvi) {
        HMODULE hTMAudio = LoadLibraryW(L"tmaudio.dll");
        if (hTMAudio) {
            typedef const wchar_t* (*GetAviFilePathFunc)();
            GetAviFilePathFunc pGetAviFilePath = (GetAviFilePathFunc)GetProcAddress(hTMAudio, "GetAviFilePath");
            if (pGetAviFilePath) {
                const wchar_t* aviPath = pGetAviFilePath();
                if (aviPath) {
                    this->path = std::wstring(aviPath) + L".mp4";
                }
                delete[] aviPath;
            }
        }
        return true;
    } else if (this->locationSelection == LocationSelection::Folder) {
        this->path = this->otherLocation + L"\\" + GetAutoGeneratedName();
        return true;
    }

    // invalid location selector value, should be unreachable code without corrupted settings file
    return false;
}

void CodecState::SetAutoDefaults() {
    this->codec = determineBestCodec(this->width ? this->width : 1920, this->height ? this->height : 1080);
    this->qualityMode = QualityUtils::GetDefaultQualityModeForCodec(this->codec);
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
        case QualityMode::CRF:      return { 21, 0 };
        case QualityMode::Lossless: return { 0, 0 };
        case QualityMode::CQP:      return { 24, 0 };
        case QualityMode::ABR:      return { 16000, 0 };
        case QualityMode::VBR_LAT:  return { 16000, 24000 };
        case QualityMode::QVBR:     return { 16000, 24000 };
        case QualityMode::HQVBR:    return { 16000, 24000 };
        case QualityMode::HQCBR:    return { 16000, 0 };
        case QualityMode::ICQ:      return { 24, 0 };
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

static constexpr const uint8_t SERIALIZER_VERSION = 1;

template<typename T>
void appendPrimitiveToBuffer(std::vector<uint8_t>& buffer, const T& data) {
    const uint8_t* byteData = reinterpret_cast<const uint8_t*>(&data);
    buffer.insert(buffer.end(), byteData, byteData + sizeof(T));
}

void appendWstringToBuffer(std::vector<uint8_t>& buffer, const std::wstring& str) {
    uint64_t len = str.size();
    appendPrimitiveToBuffer(buffer, len);
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(str.data()), reinterpret_cast<const uint8_t*>(str.data()) + len * sizeof(wchar_t));
}

//for ICM_GETSTATE
std::vector<uint8_t> CodecState::Serialize() {
    std::vector<uint8_t> buffer;
    appendPrimitiveToBuffer(buffer, SERIALIZER_VERSION);

    appendWstringToBuffer(buffer, this->codec);
    appendWstringToBuffer(buffer, this->preset);
    appendWstringToBuffer(buffer, this->tune);
    appendWstringToBuffer(buffer, this->pix_fmt);
    appendWstringToBuffer(buffer, this->extra_args);
    appendWstringToBuffer(buffer, this->path);
    appendWstringToBuffer(buffer, this->otherLocation);

    appendPrimitiveToBuffer(buffer, this->qualityMode);
    appendPrimitiveToBuffer(buffer, this->qualityValue1);
    appendPrimitiveToBuffer(buffer, this->qualityValue2);
    appendPrimitiveToBuffer(buffer, this->selectAuto);
    appendPrimitiveToBuffer(buffer, this->tmAudioHooks);
    appendPrimitiveToBuffer(buffer, this->locationSelection);

    return buffer;
}

template <typename T>
T readPrimitiveFromBuffer(const std::vector<uint8_t>& buffer, size_t& offset) {
    if (offset + sizeof(T) > buffer.size()) {
        MessageBoxW(nullptr, L"An error has occured while reading your settings.\n(Attempted buffer overflow in readPrimitiveFromBuffer)", L"Error", MB_OK | MB_ICONERROR);
        return T{};
    }
    T data;
    std::memcpy(&data, buffer.data() + offset, sizeof(data));
    offset += sizeof(data);
    return data;
}

std::wstring readWstringFromBuffer(const std::vector<uint8_t>& buffer, size_t& offset) {
    uint64_t len = readPrimitiveFromBuffer<uint64_t>(buffer, offset);
    if (offset + len * sizeof(wchar_t) > buffer.size()) {
        MessageBoxW(nullptr, L"An error has occured while reading your settings.\n(Attempted buffer overflow in readWstringFromBuffer)", L"Error", MB_OK | MB_ICONERROR);
        return L"";
    }

    std::wstring str;
    str.assign(reinterpret_cast<const wchar_t*>(buffer.data() + offset), len);
    offset += len * sizeof(wchar_t);
    return str;
}

bool CodecState::Deserialize(const std::vector<uint8_t>& data) {
    size_t offset = 0;
    uint8_t version = readPrimitiveFromBuffer<uint8_t>(data, offset);
    if (version != SERIALIZER_VERSION) {
        MessageBoxW(nullptr, L"Saved VfW FFmpeg Bridge settings are incompatible with your current version, they will be reset.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    this->codec = std::move(readWstringFromBuffer(data, offset));
    this->preset = std::move(readWstringFromBuffer(data, offset));
    this->tune = std::move(readWstringFromBuffer(data, offset));
    this->pix_fmt = std::move(readWstringFromBuffer(data, offset));
    this->extra_args = std::move(readWstringFromBuffer(data, offset));
    this->path = std::move(readWstringFromBuffer(data, offset));
    this->otherLocation = std::move(readWstringFromBuffer(data, offset));

    this->qualityMode = readPrimitiveFromBuffer<QualityMode>(data, offset);
    this->qualityValue1 = readPrimitiveFromBuffer<int>(data, offset);
    this->qualityValue2 = readPrimitiveFromBuffer<int>(data, offset);
    this->selectAuto = readPrimitiveFromBuffer<bool>(data, offset);
    this->tmAudioHooks = readPrimitiveFromBuffer<bool>(data, offset);
    this->locationSelection = readPrimitiveFromBuffer<LocationSelection>(data, offset);
    return true;
}

static std::filesystem::path settingsFile = appdataPath / "settings.bin";

void CodecState::Save() {
    auto blob = this->Serialize();
    std::ofstream ofs(settingsFile, std::ios::binary);
    if (!ofs) {
        MessageBoxW(nullptr, L"Failed to open settings file for writing.", L"VfW FFmpeg Bridge", MB_OK | MB_ICONERROR);
        return;
    }
    ofs.write(reinterpret_cast<const char*>(blob.data()), blob.size());
}

void CodecState::Load() {
    std::filesystem::path settingsFile = appdataPath / "settings.bin";
    std::error_code ec;
    if (!std::filesystem::exists(settingsFile, ec) || ec) {
        return;
    }

    std::ifstream ifs(settingsFile, std::ios::binary);
    if (!ifs) {
        MessageBoxW(nullptr, L"Failed to open settings file for reading.", L"VfW FFmpeg Bridge", MB_OK | MB_ICONERROR);
        return;
    }

    std::vector<uint8_t> blob((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (!this->Deserialize(blob)) {
        // error box already pops up in deserialize itself
        return;
    }
}

bool CodecState::FindBestFfmpeg() {
    auto ffmpegProcess = std::make_unique<subprocess::Popen>(L"ffmpeg -version");
    if (ffmpegProcess->wait() == 0) {
        this->ffmpegPath = L"ffmpeg";
        return true;
    }

    auto adjacentPath = Bridge::GetAdjacentPath(Bridge::g_hInstance, L"ffmpeg.exe");
    if(!adjacentPath.empty() && std::filesystem::exists(adjacentPath)) {
        this->ffmpegPath = adjacentPath;
        return true;
    }

    return false;
}