#include <windows.h>
#include <dsound.h>
#include "MinHook.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

ma_pcm_rb g_ringBuffer;
BOOL g_isRecording = FALSE;

// Recording logic
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    if (pInput == NULL) return;

    ma_uint32 framesToAcquire = frameCount;
    void* pWriteBuffer;
    
    ma_result result = ma_pcm_rb_acquire_write(&g_ringBuffer, &framesToAcquire, &pWriteBuffer);
    if (result == MA_SUCCESS && framesToAcquire > 0) {
        ma_uint32 bytesPerFrame = ma_get_bytes_per_frame(pDevice->capture.format, pDevice->capture.channels);
        memcpy(pWriteBuffer, pInput, framesToAcquire * bytesPerFrame);
        
        ma_pcm_rb_commit_write(&g_ringBuffer, framesToAcquire);
    }
}

ma_device_config deviceConfig;
ma_device device;

// Hooks
typedef HRESULT(WINAPI *pDirectSoundCaptureCreate8)(LPCGUID, LPDIRECTSOUNDCAPTURE8*, LPUNKNOWN);
typedef HRESULT(WINAPI *pCreateCaptureBuffer)(LPDIRECTSOUNDCAPTURE, LPCDSCBUFFERDESC, LPDIRECTSOUNDCAPTUREBUFFER*, LPUNKNOWN);
typedef HRESULT(WINAPI *pStart)(LPDIRECTSOUNDCAPTUREBUFFER, DWORD);
typedef HRESULT(WINAPI *pStop)(LPDIRECTSOUNDCAPTUREBUFFER);
typedef HRESULT(WINAPI *pLock)(LPDIRECTSOUNDCAPTUREBUFFER, DWORD, DWORD, LPVOID*, LPDWORD, LPVOID*, LPDWORD, DWORD);

pDirectSoundCaptureCreate8 Original_DirectSoundCaptureCreate8 = nullptr;
pCreateCaptureBuffer Original_CreateCaptureBuffer = nullptr;
pStart Original_Start = nullptr;
pStop Original_Stop = nullptr;
pLock Original_Lock = nullptr;

HRESULT WINAPI Hooked_Lock(LPDIRECTSOUNDCAPTUREBUFFER pThis, DWORD dwOffset, DWORD dwBytes, 
                           LPVOID *ppvAudioPtr1, LPDWORD pdwAudioBytes1, 
                           LPVOID *ppvAudioPtr2, LPDWORD pdwAudioBytes2, DWORD dwFlags)
{
    HRESULT hr = Original_Lock(pThis, dwOffset, dwBytes, ppvAudioPtr1, pdwAudioBytes1, ppvAudioPtr2, pdwAudioBytes2, dwFlags);
    
    if (SUCCEEDED(hr) && g_isRecording) {
        if (ppvAudioPtr1 && pdwAudioBytes1 && *pdwAudioBytes1 > 0) {
            ma_uint32 bytesPerFrame = ma_get_bytes_per_frame(device.capture.format, device.capture.channels);
            ma_uint32 framesReq = *pdwAudioBytes1 / bytesPerFrame;
            void* pReadBuffer;
            
            if (ma_pcm_rb_acquire_read(&g_ringBuffer, &framesReq, &pReadBuffer) == MA_SUCCESS && framesReq > 0) {
                memcpy(*ppvAudioPtr1, pReadBuffer, framesReq * bytesPerFrame);
                ma_pcm_rb_commit_read(&g_ringBuffer, framesReq);
            } else {
                memset(*ppvAudioPtr1, 0, *pdwAudioBytes1);
            }
        }

        if (ppvAudioPtr2 && pdwAudioBytes2 && *pdwAudioBytes2 > 0) {
            ma_uint32 bytesPerFrame = ma_get_bytes_per_frame(device.capture.format, device.capture.channels);
            ma_uint32 framesReq = *pdwAudioBytes2 / bytesPerFrame;
            void* pReadBuffer;
            
            if (ma_pcm_rb_acquire_read(&g_ringBuffer, &framesReq, &pReadBuffer) == MA_SUCCESS && framesReq > 0) {
                memcpy(*ppvAudioPtr2, pReadBuffer, framesReq * bytesPerFrame);
                ma_pcm_rb_commit_read(&g_ringBuffer, framesReq);
            } else {
                memset(*ppvAudioPtr2, 0, *pdwAudioBytes2);
            }
        }
    }

    return hr;
}

HRESULT WINAPI Hooked_Start(LPDIRECTSOUNDCAPTUREBUFFER pThis, DWORD dwFlags)
{
    if (!g_isRecording) {
        ma_device_start(&device);
        g_isRecording = TRUE;
    }
    return Original_Start(pThis, dwFlags);
}

HRESULT WINAPI Hooked_Stop(LPDIRECTSOUNDCAPTUREBUFFER pThis)
{
    if (g_isRecording) {
        ma_device_stop(&device);
        g_isRecording = FALSE;
    }
    return Original_Stop(pThis);
}

HRESULT WINAPI Hooked_CreateCaptureBuffer(LPDIRECTSOUNDCAPTURE pThis, LPCDSCBUFFERDESC pcDSCBufferDesc, LPDIRECTSOUNDCAPTUREBUFFER *ppDSCBuffer, LPUNKNOWN pUnkOuter)
{
    HRESULT hr = Original_CreateCaptureBuffer(pThis, pcDSCBufferDesc, ppDSCBuffer, pUnkOuter);
    
    if (SUCCEEDED(hr) && ppDSCBuffer && *ppDSCBuffer)
    {
        void** bufferVTable = *(void***)(*ppDSCBuffer);

        void* realLockAddr  = bufferVTable[8];
        void* realStartAddr = bufferVTable[9];
        void* realStopAddr  = bufferVTable[10];

        if (MH_CreateHook(realLockAddr, &Hooked_Lock, (LPVOID*)&Original_Lock) == MH_OK) MH_EnableHook(realLockAddr);
        if (MH_CreateHook(realStartAddr, &Hooked_Start, (LPVOID*)&Original_Start) == MH_OK) MH_EnableHook(realStartAddr);
        if (MH_CreateHook(realStopAddr, &Hooked_Stop, (LPVOID*)&Original_Stop) == MH_OK) MH_EnableHook(realStopAddr);

        if (pcDSCBufferDesc && pcDSCBufferDesc->lpwfxFormat) {
            WAVEFORMATEX* wfx = pcDSCBufferDesc->lpwfxFormat;
            
            ma_format maFormat;
            if (wfx->wBitsPerSample == 16) maFormat = ma_format_s16;
            else if (wfx->wBitsPerSample == 32) maFormat = ma_format_f32;
            else maFormat = ma_format_u8;

            ma_pcm_rb_init(maFormat, wfx->nChannels, wfx->nSamplesPerSec, NULL, NULL, &g_ringBuffer);

            deviceConfig = ma_device_config_init(ma_device_type_loopback);
            deviceConfig.capture.format   = maFormat;
            deviceConfig.capture.channels = wfx->nChannels;
            deviceConfig.sampleRate       = wfx->nSamplesPerSec;
            deviceConfig.dataCallback     = data_callback;

            ma_device_init(NULL, &deviceConfig, &device);
        }
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