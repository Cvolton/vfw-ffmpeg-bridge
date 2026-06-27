#include <string>
#include <windows.h>
#include <vfw.h>

#include <fstream>
#include <sstream>

#include "subprocess.hpp"

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

    std::string codec = "av1_nvenc";
    std::string bitrate = "0"; 
    std::string extra_args = "-preset p1 -tune ull -rc vbr -cq 28 -multipass 0 -pix_fmt yuv420p";
    std::string path = "c:\\temp\\output.mp4";

    subprocess::Popen* ffmpegProcess = nullptr;
};

void ffmpegBegin(CodecState* state) {
    if(state->ffmpegProcess) return;

    std::stringstream stream;
    stream << '"' << "ffmpeg" << '"' << " -y -f rawvideo -pix_fmt bgr24 -s " << state->width << "x" << state->height << " -r " << state->fpsNum << "/" << state->fpsDen
    << " -i - "; 
    if (!state->codec.empty())
        stream << "-c:v " << state->codec << " ";
    if (!state->bitrate.empty())
        stream << "-b:v " << state->bitrate << " ";
    if (!state->extra_args.empty())
        stream << state->extra_args << " ";
    else
        stream << "-pix_fmt yuv420p ";
    stream << "-vf \"vflip\" -an \"" << state->path << "\" "; // i hope just putting it in "" escapes it

    state->ffmpegProcess = new subprocess::Popen(stream.str());
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
            ffmpegBegin(state);
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
            delete state->ffmpegProcess;
            state->ffmpegProcess = nullptr;
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
    }

    return DefDriverProc(dwDriverId, hDriver, uMsg, lParam1, lParam2);
}