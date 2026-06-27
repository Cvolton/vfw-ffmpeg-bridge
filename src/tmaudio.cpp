#include <windows.h>
#include <dsound.h>
#include "MinHook.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

// Recording logic
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_encoder* pEncoder = (ma_encoder*)pDevice->pUserData;

    if (pInput != NULL) {
        ma_encoder_write_pcm_frames(pEncoder, pInput, frameCount, NULL);
    }
}

ma_result result;
ma_encoder_config encoderConfig;
ma_encoder encoder;
ma_device_config deviceConfig;
ma_device device;

void startRecording() {
    const char* outputFile = "c:\\temp\\desktop_audio.wav";

    encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 2, 48000);
    result = ma_encoder_init_file(outputFile, &encoderConfig, &encoder);
    if (result != MA_SUCCESS) {
        OutputDebugStringA("Failed to initialize the output file.\n");
        return;
    }

    deviceConfig = ma_device_config_init(ma_device_type_loopback);
    deviceConfig.capture.pDeviceID  = NULL;
    deviceConfig.capture.format     = encoder.config.format;
    deviceConfig.capture.channels   = encoder.config.channels;
    deviceConfig.sampleRate         = encoder.config.sampleRate;
    deviceConfig.dataCallback       = data_callback;
    deviceConfig.pUserData          = &encoder;

    result = ma_device_init(NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        OutputDebugStringA("[TMAudio] Failed to initialize loopback device.\n");
        ma_encoder_uninit(&encoder);
        return;
    }

    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        OutputDebugStringA("[TMAudio] Failed to start device.\n");
        ma_device_uninit(&device);
        ma_encoder_uninit(&encoder);
        return;
    }
}

void stopRecording() {
    ma_device_uninit(&device);
    ma_encoder_uninit(&encoder);
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
    
    startRecording();

    return Original_Start(pThis, dwFlags);
}

HRESULT WINAPI Hooked_Stop(LPDIRECTSOUNDCAPTUREBUFFER pThis)
{
    OutputDebugStringA("[TMAudio] Recording STOPPED!");
    
    stopRecording();

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
        
        OutputDebugStringA("[TMAudio] Successfully hooked Start and Stop!");
    }

    return hr;
}

HRESULT WINAPI Hooked_DirectSoundCaptureCreate8(LPCGUID pcGuidDevice, LPDIRECTSOUNDCAPTURE8 *ppDSC8, LPUNKNOWN pUnkOuter)
{
    OutputDebugStringA("[TMAudio] Intercepted DirectSoundCaptureCreate8 call.");

    HRESULT hr = Original_DirectSoundCaptureCreate8(pcGuidDevice, ppDSC8, pUnkOuter);
    
    if (SUCCEEDED(hr) && ppDSC8 && *ppDSC8)
    {
        void** captureVTable = *(void***)(*ppDSC8);
        void* realCreateBufAddr = captureVTable[3];

        if (MH_CreateHook(realCreateBufAddr, &Hooked_CreateCaptureBuffer, (LPVOID*)&Original_CreateCaptureBuffer) == MH_OK) {
            MH_EnableHook(realCreateBufAddr);
        }
        
        OutputDebugStringA("[TMAudio] Successfully hooked CreateCaptureBuffer!");
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