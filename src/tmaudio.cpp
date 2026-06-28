#include <windows.h>
#include <dsound.h>
#include <atomic>
#include <string>
#include <format>
#include <fstream>
#include "MinHook.h"
#include "subprocess.hpp"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

static std::atomic_bool g_isRecording = false;
static ma_encoder g_encoder;
static ma_device g_device;
static bool g_deviceInitialized = false;
static bool g_recordedAudio = false;
static std::string g_outputPath = "c:\\temp\\output.wav";
static std::string g_videoPath = "";
static std::wstring g_aviPath = L"";

// Interfacing with the main application
std::string wideToUtf8(std::wstring_view path) {
    // geode::utils::string::wideToUtf8
    int count = WideCharToMultiByte(CP_UTF8, 0, path.data(), path.size(), NULL, 0, NULL, NULL);
    std::string str(count, 0);
    WideCharToMultiByte(CP_UTF8, 0, path.data(), path.size(), &str[0], count, NULL, NULL);
    return str;
}

std::wstring utf8ToWide(std::string_view str) {
    // geode::utils::string::utf8ToWide
    int count = MultiByteToWideChar(CP_UTF8, 0, str.data(), str.size(), NULL, 0);
    std::wstring wstr(count, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), str.size(), &wstr[0], count);
    return wstr;
}

extern "C" __declspec(dllexport) void SetOutputFilePath(const wchar_t* path) {
    if (g_isRecording) {
        return;
    }

    g_outputPath = wideToUtf8(path);
}

extern "C" __declspec(dllexport) void SetVideoFilePath(const wchar_t* path) {
    if (g_isRecording) {
        return;
    }

    g_videoPath = wideToUtf8(path);
    SetOutputFilePath(std::format(L"{}_audio.wav", path).c_str());
}

double getMediaDuration(const std::wstring& path) {
    std::wstring cmd = std::format(
        L"ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"{}\"", 
        path
    );

    subprocess::Popen proc(cmd, true);
    
    std::string output = proc.m_stdout.read();
    proc.wait();
    proc.m_stdout.close();

    try {
        return std::stod(output);
    } catch (...) {
        return 0.0;
    }
}

void muxAudio() {
    if (!g_videoPath.empty() && !g_outputPath.empty()) {
        std::wstring wVideoPath = utf8ToWide(g_videoPath);
        std::wstring wAudioPath = !g_recordedAudio ? g_aviPath : utf8ToWide(g_outputPath);
        
        std::wstring tmpVideoPath = wVideoPath + L"_tmp.mp4"; 
        MoveFileExW(wVideoPath.c_str(), tmpVideoPath.c_str(), MOVEFILE_REPLACE_EXISTING);

        double v_dur = getMediaDuration(tmpVideoPath);
        double a_dur = getMediaDuration(wAudioPath);
        double diff = v_dur - a_dur;

        std::wstring cmd;

        if (diff > 0.0) {
            int delay_ms = static_cast<int>(diff * 1000.0);
            cmd = std::format(
                L"\"ffmpeg\" -y -i \"{}\" -i \"{}\" -c:v copy -c:a aac -b:a 320k -af \"adelay={}|{},apad\" -shortest \"{}\"", 
                tmpVideoPath, wAudioPath, delay_ms, delay_ms, wVideoPath
            );
        } else {
            double skip_seconds = std::abs(diff);
            cmd = std::format(
                L"\"ffmpeg\" -y -i \"{}\" -ss {:.3f} -i \"{}\" -c:v copy -c:a aac -b:a 320k -shortest \"{}\"", 
                tmpVideoPath, skip_seconds, wAudioPath, wVideoPath
            );
        }

        subprocess::Popen muxProcess(cmd);
        muxProcess.wait(); 

        DeleteFileW(tmpVideoPath.c_str());
        DeleteFileW(wAudioPath.c_str());

        g_outputPath.clear();
        g_aviPath.clear();
        g_recordedAudio = false;
    }
}

// Recording logic
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    if (g_isRecording && pInput != NULL) {
        ma_encoder_write_pcm_frames(&g_encoder, pInput, frameCount, NULL);
    }
}

void init_wasapi_device() {
    if (g_deviceInitialized) return;

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_loopback);
    //deviceConfig.periodSizeInFrames = 256; 
    deviceConfig.performanceProfile = ma_performance_profile_low_latency;
    deviceConfig.sampleRate = 48000; 
    deviceConfig.wasapi.noHardwareOffloading = MA_TRUE; 
    deviceConfig.dataCallback = data_callback;
    deviceConfig.capture.format = ma_format_s16;
    deviceConfig.capture.channels = 2;

    if (ma_device_init(NULL, &deviceConfig, &g_device) == MA_SUCCESS) {
        if (ma_device_start(&g_device) == MA_SUCCESS) {
            g_deviceInitialized = true;
        }
    }
}

void start_audio_recording() {
    OutputDebugStringA("[TMAudio] Recording STARTED!");

    g_recordedAudio = true;
    
    if (!g_isRecording) {
        const char* outputFile = g_outputPath.c_str();
        ma_encoder_config encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, ma_format_s16, 2, 48000);
        
        if (ma_encoder_init_file(outputFile, &encoderConfig, &g_encoder) == MA_SUCCESS) {
            g_isRecording = true;
        }
    }
}

void stop_audio_recording() {
    OutputDebugStringA("[TMAudio] Recording STOPPED!");
    
    if (g_isRecording.exchange(false)) {
        
        Sleep(20); 
        
        ma_encoder_uninit(&g_encoder);

        muxAudio();
    }
}

/**
 * TMN ESWC
 */

// Hooks
typedef HRESULT(WINAPI *pDirectSoundCaptureCreate8)(LPCGUID, LPDIRECTSOUNDCAPTURE8*, LPUNKNOWN);
typedef HRESULT(WINAPI *pCreateCaptureBuffer)(LPDIRECTSOUNDCAPTURE, LPCDSCBUFFERDESC, LPDIRECTSOUNDCAPTUREBUFFER*, LPUNKNOWN);
typedef HRESULT(WINAPI *pStart)(LPDIRECTSOUNDCAPTUREBUFFER, DWORD);
typedef HRESULT(WINAPI *pStop)(LPDIRECTSOUNDCAPTUREBUFFER);

pDirectSoundCaptureCreate8 Original_DirectSoundCaptureCreate8 = nullptr;
pCreateCaptureBuffer Original_CreateCaptureBuffer = nullptr;
pStart Original_Start = nullptr;
pStop Original_Stop = nullptr;

HRESULT WINAPI Hooked_Start(LPDIRECTSOUNDCAPTUREBUFFER pThis, DWORD dwFlags)
{
    start_audio_recording();

    return Original_Start(pThis, dwFlags);
}

HRESULT WINAPI Hooked_Stop(LPDIRECTSOUNDCAPTUREBUFFER pThis)
{
    stop_audio_recording();

    return Original_Stop(pThis);
}

HRESULT WINAPI Hooked_CreateCaptureBuffer(LPDIRECTSOUNDCAPTURE pThis, LPCDSCBUFFERDESC pcDSCBufferDesc, LPDIRECTSOUNDCAPTUREBUFFER *ppDSCBuffer, LPUNKNOWN pUnkOuter)
{
    OutputDebugStringA("[TMAudio] Intercepted CreateCaptureBuffer call.");

    HRESULT hr = Original_CreateCaptureBuffer(pThis, pcDSCBufferDesc, ppDSCBuffer, pUnkOuter);
    
    if (SUCCEEDED(hr) && ppDSCBuffer && *ppDSCBuffer)
    {
        void** bufferVTable = *(void***)(*ppDSCBuffer);

        void* realStartAddr = bufferVTable[9];
        void* realStopAddr = bufferVTable[10];

        if (MH_CreateHook(realStartAddr, &Hooked_Start, (LPVOID*)&Original_Start) == MH_OK) {
            MH_EnableHook(realStartAddr);
        }
        if (MH_CreateHook(realStopAddr, &Hooked_Stop, (LPVOID*)&Original_Stop) == MH_OK) {
            MH_EnableHook(realStopAddr);
        }
        
        init_wasapi_device();

        OutputDebugStringA("[TMAudio] Successfully hooked Start and Stop!");
    }

    return hr;
}

HRESULT WINAPI Hooked_DirectSoundCaptureCreate8(LPCGUID pcGuidDevice, LPDIRECTSOUNDCAPTURE8 *ppDSC8, LPUNKNOWN pUnkOuter)
{
    HRESULT hr = Original_DirectSoundCaptureCreate8(pcGuidDevice, ppDSC8, pUnkOuter);
    
    if (SUCCEEDED(hr) && ppDSC8 && *ppDSC8)
    {
        void** captureVTable = *(void***)(*ppDSC8);
        void* realCreateBufAddr = captureVTable[3];

        if (MH_CreateHook(realCreateBufAddr, &Hooked_CreateCaptureBuffer, (LPVOID*)&Original_CreateCaptureBuffer) == MH_OK) {
            MH_EnableHook(realCreateBufAddr);
        }
    }

    return hr;
}

// Setup
void SetupDirectSoundHook()
{
    HMODULE hDSound = LoadLibraryA("dsound.dll");
    if (!hDSound) return;

    if (MH_Initialize() != MH_OK) return;

    void* pCreateAddr = (void*)GetProcAddress(hDSound, "DirectSoundCaptureCreate8");
    if (pCreateAddr) {
        MH_CreateHook(pCreateAddr, &Hooked_DirectSoundCaptureCreate8, (LPVOID*)&Original_DirectSoundCaptureCreate8);
        MH_EnableHook(pCreateAddr);
    }
}

/**
 * TMUF
 */
typedef void* ALCdevice;
typedef void (__cdecl *palcCaptureStart)(ALCdevice device);
typedef void (__cdecl *palcCaptureStop)(ALCdevice device);

palcCaptureStart Original_alcCaptureStart = nullptr;
palcCaptureStop Original_alcCaptureStop = nullptr;

void __cdecl Hooked_alcCaptureStart(ALCdevice device)
{
    start_audio_recording();

    Original_alcCaptureStart(device);
}

void __cdecl Hooked_alcCaptureStop(ALCdevice device)
{
    stop_audio_recording();

    Original_alcCaptureStop(device);
}

void SetupOpenALHook()
{
    HMODULE hOpenAL = GetModuleHandleA("OPENAL32.DLL");
    if (!hOpenAL) {
        hOpenAL = LoadLibraryA("OPENAL32.DLL"); 
    }
    
    if (!hOpenAL) return;

    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) return;

    void* pStartAddr = (void*)GetProcAddress(hOpenAL, "alcCaptureStart");
    void* pStopAddr = (void*)GetProcAddress(hOpenAL, "alcCaptureStop");

    if (pStartAddr && pStopAddr) {
        MH_CreateHook(pStartAddr, &Hooked_alcCaptureStart, (LPVOID*)&Original_alcCaptureStart);
        MH_CreateHook(pStopAddr, &Hooked_alcCaptureStop, (LPVOID*)&Original_alcCaptureStop);
        
        MH_EnableHook(pStartAddr);
        MH_EnableHook(pStopAddr);
        
        init_wasapi_device();
    }
}

/**
 * TM2
 */
typedef HANDLE(WINAPI *CREATEFILEW_T)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
CREATEFILEW_T Original_CreateFileW = nullptr;

HANDLE WINAPI Hooked_CreateFileW(
    LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) 
{
    if (lpFileName != nullptr) {
        std::wstring path(lpFileName);
        
        if (path.length() >= 4 && 
            path.substr(path.length() - 4) == L".avi" && 
            (dwDesiredAccess & GENERIC_WRITE)
        ) {
            g_aviPath = path;
        }
    }

    return Original_CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, 
                                 lpSecurityAttributes, dwCreationDisposition, 
                                 dwFlagsAndAttributes, hTemplateFile);
}

void InitializeHook() {
    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) return;

    MH_CreateHook(&CreateFileW, &Hooked_CreateFileW, reinterpret_cast<LPVOID*>(&Original_CreateFileW));
    MH_EnableHook(&CreateFileW);
}

/**
 * Shared
 */

DWORD WINAPI InitHooksThread(LPVOID lpParam)
{
    SetupDirectSoundHook();
    SetupOpenALHook();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        
        HMODULE hDummy;
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCSTR)&DllMain, 
            &hDummy
        );

        CreateThread(nullptr, 0, InitHooksThread, nullptr, 0, nullptr);
        break;
        
    case DLL_PROCESS_DETACH:
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        break;
    }
    return TRUE;
}