#pragma once
#include <windows.h>
#include <string>

namespace Bridge {
    extern HINSTANCE g_hInstance;

    HMODULE LoadAdjacentDLL(HMODULE hModule, const wchar_t* targetDllName);
    std::wstring GetAdjacentPath(HMODULE hModule, const wchar_t* targetDllName);
}

namespace TMAudio {
    extern HMODULE g_hModule;

    void SetVideoFilePath(const wchar_t* path);
    void SetFfmpegPath(const wchar_t* path);
    void SetAudioEncoderArgs(const wchar_t* args);
    std::wstring GetAviFilePath();
    void EnableTMAudioHooks();
    void DisableTMAudioHooks();
    void CancelRender();
}