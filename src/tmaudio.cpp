#include <windows.h>
#include <dsound.h>
#include <atomic>
#include "MinHook.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

static std::atomic_bool g_isRecording = false;
static ma_encoder g_encoder;
static ma_device g_device;
static bool g_deviceInitialized = false;

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
    deviceConfig.capture.pDeviceID  = NULL;
    deviceConfig.capture.format     = ma_format_f32;
    deviceConfig.capture.channels   = 2;
    deviceConfig.sampleRate         = 48000;
    deviceConfig.dataCallback       = data_callback;
    deviceConfig.performanceProfile = ma_performance_profile_low_latency;

    if (ma_device_init(NULL, &deviceConfig, &g_device) == MA_SUCCESS) {
        if (ma_device_start(&g_device) == MA_SUCCESS) {
            g_deviceInitialized = true;
        }
    }
}

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
    OutputDebugStringA("[TMAudio] Recording STARTED!");
    
    if (!g_isRecording) {
        const char* outputFile = "c:\\temp\\output.wav";
        ma_encoder_config encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 2, 48000);
        
        if (ma_encoder_init_file(outputFile, &encoderConfig, &g_encoder) == MA_SUCCESS) {
            g_isRecording = true;
        }
    }

    return Original_Start(pThis, dwFlags);
}

HRESULT WINAPI Hooked_Stop(LPDIRECTSOUNDCAPTUREBUFFER pThis)
{
    OutputDebugStringA("[TMAudio] Recording STOPPED!");
    
    if (g_isRecording.exchange(false)) {
        
        Sleep(20); 
        
        ma_encoder_uninit(&g_encoder);
    }

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

        SetupDirectSoundHook();
        break;
        
    case DLL_PROCESS_DETACH:
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        break;
    }
    return TRUE;
}