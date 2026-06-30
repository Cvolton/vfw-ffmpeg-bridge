#include <iomanip>
#include <string>
#include <windows.h>
#include <vfw.h>
#include <commctrl.h>

#include <format>
#include <sstream>
#include <string_view>

#include "subprocess.hpp"
#include "resource.h"
#include "CodecState.hpp"
#include "BridgeConfig.hpp"

static HINSTANCE g_hInstance = nullptr;

void ffmpegBegin(CodecState& state) {
    if (state.ffmpegProcess) return;

    // Build the base command
    std::wstring cmd = std::format(L"\"ffmpeg\" -y -f rawvideo -pix_fmt bgr24 -s {}x{} -r {}/{} -i - ", 
                                  state.width, state.height, state.fpsNum, state.fpsDen);

    if (!state.codec.empty()) {
        cmd += std::format(L"-c:v {} ", state.codec);
    }
    
    if (!state.bitrate.empty()) {
        cmd += std::format(L"-b:v {} ", state.bitrate);
    }

    switch(state.qualityMode) {
        case QualityMode::CBR:
            cmd += L"-cbr 1 ";
            break;
        case QualityMode::VBR:
            cmd += L"-cbr 0 ";
            break;
        case QualityMode::CRF:
            cmd += std::format(L"-qp {} ", state.qualityValue1);
            break;
        case QualityMode::Lossless:
            cmd += L"-qp 0 ";
            break;
    }

    if (!state.preset.empty()) {
        cmd += std::format(L"-preset {} ", state.preset);
    }

    if (!state.tune.empty()) {
        cmd += std::format(L"-tune {} ", state.tune);
    }

    if (!state.pix_fmt.empty()) {
        cmd += std::format(L"-pix_fmt {} ", state.pix_fmt);
    }
    
    if (!state.extra_args.empty()) {
        cmd += std::format(L"{} ", state.extra_args);
    } else {
        cmd += L"-pix_fmt yuv420p ";
    }

    std::wstringstream pathEscaper;
    pathEscaper << std::quoted(state.path);
    
    cmd += std::format(L"-vf \"vflip\" -an {}", pathEscaper.str());

    state.ffmpegProcess = std::make_unique<subprocess::Popen>(cmd);

    // Set the video file path in tmaudio.dll
    HMODULE hTMAudio = LoadLibraryW(L"tmaudio.dll");
    if (hTMAudio) {
        typedef void (*SetVideoFilePathFunc)(const wchar_t*); 
        SetVideoFilePathFunc pSetVideoFilePath = (SetVideoFilePathFunc)GetProcAddress(hTMAudio, "SetVideoFilePath");
        
        if (pSetVideoFilePath) {
            pSetVideoFilePath(state.path.c_str());
        }
    }
}

extern "C" LRESULT WINAPI DriverProc(
    DWORD_PTR dwDriverId, 
    HDRVR     hDriver, 
    UINT      uMsg, 
    LPARAM    lParam1, 
    LPARAM    lParam2
) {
    CodecState* state = reinterpret_cast<CodecState*>(dwDriverId);

    switch (uMsg) {
        case DRV_LOAD:
            return 1;

        case DRV_FREE:
            return 1;

        case DRV_OPEN: {
            INITCOMMONCONTROLSEX icex;
            icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
            icex.dwICC = ICC_UPDOWN_CLASS | ICC_STANDARD_CLASSES;
            InitCommonControlsEx(&icex);

            return reinterpret_cast<LRESULT>(new CodecState());
        }

        case DRV_CLOSE: {
            CodecState* state = reinterpret_cast<CodecState*>(dwDriverId);
            if (state) {
                delete state;
            }
            return 1;
        }

        case ICM_GETINFO:
        {
            ICINFO* info = reinterpret_cast<ICINFO*>(lParam1);
            if (!info) return ICERR_MEMORY;

            info->dwSize = sizeof(ICINFO);
            info->fccType = ICTYPE_VIDEO;
            info->fccHandler = mmioFOURCC('F', 'B', 'R', 'G');
            info->dwFlags = VIDCF_TEMPORAL;
            
            wcscpy_s(info->szName, L"FFmpeg Bridge");
            wcscpy_s(info->szDescription, L"VFW FFmpeg Bridge test");
            return sizeof(ICINFO);
        }

        case ICM_COMPRESS_QUERY:
            MessageBoxW(NULL, L"ICM_COMPRESS_QUERY", L"VFW FFmpeg Bridge test", MB_OK);
            return ICERR_OK;
        case ICM_COMPRESS_GET_FORMAT: {
            BITMAPINFOHEADER* inFormat = reinterpret_cast<BITMAPINFOHEADER*>(lParam1);
            BITMAPINFOHEADER* outFormat = reinterpret_cast<BITMAPINFOHEADER*>(lParam2);

            if (!inFormat) return ICERR_BADPARAM;

            if (outFormat == nullptr) {
                return sizeof(BITMAPINFOHEADER);
            }

            outFormat->biSize = sizeof(BITMAPINFOHEADER);
            outFormat->biWidth = inFormat->biWidth;
            outFormat->biHeight = inFormat->biHeight;
            outFormat->biPlanes = 1;
            
            outFormat->biBitCount = 24; 
            outFormat->biCompression = mmioFOURCC('F', 'B', 'R', 'G');
            
            DWORD rawSize = inFormat->biWidth * abs(inFormat->biHeight) * 3;
            outFormat->biSizeImage = rawSize + 1024;

            outFormat->biXPelsPerMeter = 0;
            outFormat->biYPelsPerMeter = 0;
            outFormat->biClrUsed = 0;
            outFormat->biClrImportant = 0;

            return sizeof(BITMAPINFOHEADER);
        }
        case ICM_COMPRESS_BEGIN: {
            BITMAPINFOHEADER* inFormat = reinterpret_cast<BITMAPINFOHEADER*>(lParam1);
            BITMAPINFOHEADER* outFormat = reinterpret_cast<BITMAPINFOHEADER*>(lParam2);
            if (!inFormat || !outFormat) return ICERR_BADPARAM;

            CodecState* state = reinterpret_cast<CodecState*>(dwDriverId);
            if (!state) return ICERR_MEMORY;

            state->width = inFormat->biWidth;
            state->height = abs(inFormat->biHeight);
            state->frameCount = 0;

            return ICERR_OK;
        }
        case ICM_COMPRESS: {
            ICCOMPRESS* icinfo = reinterpret_cast<ICCOMPRESS*>(lParam1);
            if (!icinfo || !icinfo->lpbiInput || !icinfo->lpbiOutput) return ICERR_BADPARAM;

            CodecState* state = reinterpret_cast<CodecState*>(dwDriverId);
            if (!state) return ICERR_MEMORY;

            const BYTE* rawPixels = static_cast<const BYTE*>(icinfo->lpInput);
            BYTE* compressedBuffer = static_cast<BYTE*>(icinfo->lpOutput);

            int bpp = icinfo->lpbiInput->biBitCount; // Usually 24
            int bytesPerPixel = bpp / 8;
            int stride = ((state->width * bytesPerPixel) + 3) & ~3;
            DWORD frameSize = stride * state->height;

            // Dumping raw pixel data to a file for debugging
            /*std::wstring filename = L"C:\\temp\\vfw_frame_" + std::to_wstring(state->frameCount) + L".raw";
            
            std::ofstream outFile(filename, std::ios::binary);
            if (outFile.is_open()) {
                outFile.write(reinterpret_cast<const char*>(rawPixels), frameSize);
                outFile.close();
            }

            state->frameCount++;*/

            // Piping to FFmpeg
            ffmpegBegin(*state);
            state->ffmpegProcess->m_stdin.write(rawPixels, frameSize);

            if(state->ffmpegProcess->m_proc_info.hProcess == nullptr) {
                return ICERR_ERROR;
            }

            // Faking AVI compression so VFW doesn't complain
            compressedBuffer[0] = 0xFF; 
            icinfo->lpbiOutput->biSizeImage = 1;

            if (icinfo->lpdwFlags) {
                *icinfo->lpdwFlags = AVIIF_KEYFRAME;
            }
            if (icinfo->lpckid) {
                *icinfo->lpckid = mmioFOURCC('0', '0', 'd', 'c');
            }

            return ICERR_OK;
        }
        case ICM_COMPRESS_END:
            if (state->ffmpegProcess->close()) {
                return ICERR_ERROR;
            }
            state->ffmpegProcess.reset();
            return ICERR_OK; 
        case ICM_COMPRESS_FRAMES_INFO:
        {
            ICCOMPRESSFRAMES* icf = reinterpret_cast<ICCOMPRESSFRAMES*>(lParam1);
            CodecState* state = reinterpret_cast<CodecState*>(dwDriverId);

            if (icf && state) {
                if (icf->dwScale > 0) {
                    state->fpsNum = icf->dwRate;
                    state->fpsDen = icf->dwScale;
                }
            }
            return ICERR_OK;
        }
        case ICM_CONFIGURE: {
            if (lParam1 == static_cast<LPARAM>(-1)) {
                return ICERR_OK;
            }

            HWND hParent = reinterpret_cast<HWND>(lParam1);
            CodecState* state = reinterpret_cast<CodecState*>(dwDriverId);

            if (!state) return ICERR_MEMORY;

            DialogBoxParamW(
                g_hInstance,
                MAKEINTRESOURCEW(IDD_CONFIG_DIALOG),
                hParent,
                BridgeConfig::ConfigDlgProc,
                reinterpret_cast<LPARAM>(state)
            );

            return ICERR_OK;
        }
    }

    return DefDriverProc(dwDriverId, hDriver, uMsg, lParam1, lParam2);
}

void LoadAdjacentDLL(HMODULE hModule, const wchar_t* targetDllName)
{
    wchar_t modulePath[MAX_PATH];
    
    if (GetModuleFileNameW(hModule, modulePath, MAX_PATH) != 0)
    {
        std::wstring path(modulePath);
        
        size_t lastSlash = path.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos)
        {
            std::wstring dirPath = path.substr(0, lastSlash + 1);
            std::wstring targetDllPath = dirPath + targetDllName;
            
            LoadLibraryW(targetDllPath.c_str());
        }
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);

        g_hInstance = hModule;
        
        LoadAdjacentDLL(hModule, L"tmaudio.dll");
    }
    return TRUE;
}