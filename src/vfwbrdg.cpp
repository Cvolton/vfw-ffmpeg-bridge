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
#include "utils.hpp"

std::wstring GetFfmpegPixFmt(const BITMAPINFOHEADER* bmi) {
    if (!bmi) return L"";

    if (bmi->biCompression == BI_RGB) {
        if (bmi->biBitCount == 24) return L"bgr24";
        if (bmi->biBitCount == 32) return L"bgra"; 
        if (bmi->biBitCount == 16) return L"rgb565le";
    } 
    else {
        switch (bmi->biCompression) {
            // 4:2:0 Formats (12 bpp)
            case mmioFOURCC('Y', 'V', '1', '2'): return L"yuv420p";
            case mmioFOURCC('I', '4', '2', '0'): return L"yuv420p";
            case mmioFOURCC('N', 'V', '1', '2'): return L"nv12";
            case mmioFOURCC('N', 'V', '2', '1'): return L"nv21";

            // 4:2:2 Formats (16 bpp)
            case mmioFOURCC('Y', 'V', '1', '6'): return L"yuv422p";
            case mmioFOURCC('Y', 'U', 'Y', '2'): return L"yuyv422";
            case mmioFOURCC('U', 'Y', 'V', 'Y'): return L"uyvy422";
            case mmioFOURCC('N', 'V', '1', '6'): return L"nv16";

            // 4:4:4 Formats (24 bpp)
            case mmioFOURCC('Y', 'V', '2', '4'): return L"yuv444p";
            case mmioFOURCC('I', '4', '4', '4'): return L"yuv444p";

            // 10-bit Formats (24 bpp)
            case mmioFOURCC('P', '0', '1', '0'): return L"p010le";
            case mmioFOURCC('v', '2', '1', '0'): return L"v210"; 

            // 16-bit RGB formats
            case mmioFOURCC('b', '6', '4', 'a'): return L"bgra64le";
            case mmioFOURCC('b', '4', '8', 'r'): return L"bgr48le";
        }
    }
    return L"";
}

void ffmpegBegin(CodecState& state) {
    if (state.ffmpegProcess) return;

    state.ffmpegProcess = std::make_unique<subprocess::Popen>(state.GetFfmpegCommand(), false, true, false);

    HMODULE hTMAudio = LoadLibraryW(L"vfwbrdg-tmaudio.dll");
    if (hTMAudio) {
        typedef void (*SetVideoFilePathFunc)(const wchar_t*); 
        SetVideoFilePathFunc pSetVideoFilePath = (SetVideoFilePathFunc)GetProcAddress(hTMAudio, "SetVideoFilePath");
        if (pSetVideoFilePath) {
            pSetVideoFilePath(state.path.c_str());
        }

        if(state.tmAudioHooks) {
            typedef void (*EnableTMAudioHooksFunc)(); 
            EnableTMAudioHooksFunc pEnableTMAudioHooks = (EnableTMAudioHooksFunc)GetProcAddress(hTMAudio, "EnableTMAudioHooks");
            if (pEnableTMAudioHooks) {
                pEnableTMAudioHooks();
            }
        } else {
            typedef void (*DisableTMAudioHooksFunc)(); 
            DisableTMAudioHooksFunc pDisableTMAudioHooks = (DisableTMAudioHooksFunc)GetProcAddress(hTMAudio, "DisableTMAudioHooks");
            if (pDisableTMAudioHooks) {
                pDisableTMAudioHooks();
            }
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

            auto state = new CodecState();
            state->Load();
            if(!state->FindBestFfmpeg()) {
                MessageBoxW(nullptr, L"Failed to find ffmpeg.exe.", L"VfW FFmpeg Bridge", MB_OK | MB_ICONERROR);
            }

            HMODULE hTMAudio = LoadLibraryW(L"vfwbrdg-tmaudio.dll");
            if (hTMAudio) {
                typedef void (*SetFfmpegPathFunc)(const wchar_t*); 
                SetFfmpegPathFunc pSetFfmpegPath = (SetFfmpegPathFunc)GetProcAddress(hTMAudio, "SetFfmpegPath");
                if (pSetFfmpegPath) {
                    pSetFfmpegPath(state->ffmpegPath.c_str());
                }
            }

            return reinterpret_cast<LRESULT>(state);
        }

        case DRV_CLOSE: {
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
            #ifdef _DEBUG
            wcscpy_s(info->szDescription, L"VFW FFmpeg Bridge (Debug)");
            #else
            wcscpy_s(info->szDescription, L"VFW FFmpeg Bridge");
            #endif
            return sizeof(ICINFO);
        }

        case ICM_COMPRESS_QUERY: {
            BITMAPINFOHEADER* inFormat = reinterpret_cast<BITMAPINFOHEADER*>(lParam1);
            if (!inFormat) return ICERR_BADFORMAT;

            std::wstring ffmpegFmt = GetFfmpegPixFmt(inFormat);
            if (ffmpegFmt.empty()) {
                return ICERR_BADFORMAT;
            }

            return ICERR_OK;
        }

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
            #ifdef _DEBUG
            outFormat->biCompression = mmioFOURCC('F', 'D', 'B', 'G');
            #else
            outFormat->biCompression = mmioFOURCC('F', 'B', 'R', 'G');
            #endif
            
            //DWORD rawSize = inFormat->biWidth * abs(inFormat->biHeight) * 3;
            //outFormat->biSizeImage = rawSize + 1024;
            outFormat->biSizeImage = 1024;

            outFormat->biXPelsPerMeter = 0;
            outFormat->biYPelsPerMeter = 0;
            outFormat->biClrUsed = 0;
            outFormat->biClrImportant = 0;

            return ICERR_OK;
        }
        case ICM_COMPRESS_BEGIN: {
            BITMAPINFOHEADER* inFormat = reinterpret_cast<BITMAPINFOHEADER*>(lParam1);
            BITMAPINFOHEADER* outFormat = reinterpret_cast<BITMAPINFOHEADER*>(lParam2);
            if (!inFormat || !outFormat) return ICERR_BADPARAM;

            CodecState* state = reinterpret_cast<CodecState*>(dwDriverId);
            if (!state) return ICERR_MEMORY;

            state->shouldVFlip = (inFormat->biCompression == BI_RGB || inFormat->biCompression == BI_BITFIELDS) && inFormat->biHeight > 0;
            state->input_pix_fmt = GetFfmpegPixFmt(inFormat);
            if (state->input_pix_fmt.empty()) {
                return ICERR_BADFORMAT;
            }

            state->width = inFormat->biWidth;
            state->height = abs(inFormat->biHeight);
            state->frameCount = 0;

            if(state->selectAuto) {
                state->SetAutoDefaults();
            }

            if(!state->SetRenderPath()) {
                return ICERR_BADPARAM;
            } else {
                state->Save();
            }

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
            
            DWORD frameSize = 0;
            DWORD fcc = icinfo->lpbiInput->biCompression;

            // RGB
            if (fcc == BI_RGB || fcc == BI_BITFIELDS) {
                int bpp = icinfo->lpbiInput->biBitCount;
                int bytesPerPixel = bpp / 8;
                int stride = ((state->width * bytesPerPixel) + 3) & ~3; 
                frameSize = stride * state->height;
            }
            // 4:2:0
            else if (fcc == mmioFOURCC('Y', 'V', '1', '2') || 
                    fcc == mmioFOURCC('I', '4', '2', '0') ||
                    fcc == mmioFOURCC('N', 'V', '1', '2') ||
                    fcc == mmioFOURCC('N', 'V', '2', '1')) {
                frameSize = (state->width * state->height * 3) / 2;
            }
            // 4:2:2
            else if (fcc == mmioFOURCC('Y', 'V', '1', '6') || 
                    fcc == mmioFOURCC('Y', 'U', 'Y', '2') || 
                    fcc == mmioFOURCC('U', 'Y', 'V', 'Y') ||
                    fcc == mmioFOURCC('N', 'V', '1', '6')) {
                frameSize = state->width * state->height * 2;
            }
            // 4:4:4 && 4:2:0 10-bit
            else if (fcc == mmioFOURCC('Y', 'V', '2', '4') || 
                    fcc == mmioFOURCC('I', '4', '4', '4') ||
                    fcc == mmioFOURCC('P', '0', '1', '0')) {
                frameSize = state->width * state->height * 3;
            }
            // 16-bit RGB
            else if (fcc == mmioFOURCC('b', '4', '8', 'r')) {
                int stride = ((state->width * 6) + 3) & ~3;
                frameSize = stride * state->height;
            }
            else if (fcc == mmioFOURCC('b', '6', '4', 'a')) {
                int stride = ((state->width * 8) + 3) & ~3;
                frameSize = stride * state->height;
            }
            // 4:2:2 10-bit
            else if (fcc == mmioFOURCC('v', '2', '1', '0')) {
                int stride = ((state->width + 47) / 48) * 128;
                frameSize = stride * state->height;
            }
            else {
                return ICERR_BADFORMAT;
            }

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
            if (!state->ffmpegProcess || state->ffmpegProcess->close()) {
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
                Bridge::g_hInstance,
                MAKEINTRESOURCEW(IDD_CONFIG_DIALOG),
                hParent,
                BridgeConfig::ConfigDlgProc,
                reinterpret_cast<LPARAM>(state)
            );

            return ICERR_OK;
        }
        case ICM_SETSTATE: {
            CodecState* state = reinterpret_cast<CodecState*>(dwDriverId);
            if (!state) return ICERR_MEMORY;
 
            if (lParam1 == 0 || lParam2 == 0) {
                return 0;
            }

            std::vector<uint8_t> data(reinterpret_cast<const uint8_t*>(lParam1), reinterpret_cast<const uint8_t*>(lParam1) + lParam2);
 
            if (!state->Deserialize(std::move(data))) {
                return 0;
            }
 
            return static_cast<LRESULT>(lParam2);
        }
        case ICM_GETSTATE: {
            CodecState* state = reinterpret_cast<CodecState*>(dwDriverId);
            if (!state) return ICERR_MEMORY;
 
            state->Save();
            auto blob = state->Serialize();
 
            // Query mode - returns the size of the serialized data
            if (lParam1 == 0) {
                return static_cast<LRESULT>(blob.size());
            }
 
            // Copy mode - actually copies the data
            auto bufferSize = static_cast<DWORD>(lParam2);
            auto copySize = static_cast<DWORD>((blob.size() < bufferSize) ? blob.size() : bufferSize);
            if (copySize > 0) {
                memcpy(reinterpret_cast<void*>(lParam1), blob.data(), copySize);
            }
 
            return static_cast<LRESULT>(blob.size());
        }
    }

    return DefDriverProc(dwDriverId, hDriver, uMsg, lParam1, lParam2);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);

        Bridge::g_hInstance = hModule;
        
        Bridge::LoadAdjacentDLL(hModule, L"vfwbrdg-tmaudio.dll");
    }
    return TRUE;
}