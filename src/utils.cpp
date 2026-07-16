#include "utils.hpp"
HINSTANCE Bridge::g_hInstance = nullptr;

std::wstring Bridge::GetAdjacentPath(HMODULE hModule, const wchar_t* targetDllName)
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
            
            return targetDllPath;
        }
    }
    
    return L"";
}

HMODULE Bridge::LoadAdjacentDLL(HMODULE hModule, const wchar_t* targetDllName)
{
    auto targetDllPath = GetAdjacentPath(hModule, targetDllName);

    if (!targetDllPath.empty())
    {
        return LoadLibraryW(targetDllPath.c_str());
    }
    return nullptr;
}

HMODULE TMAudio::g_hModule = nullptr;

void TMAudio::SetVideoFilePath(const wchar_t* path) {
    if(!g_hModule) return;

    typedef void (*SetVideoFilePathFunc)(const wchar_t*); 
    SetVideoFilePathFunc pSetVideoFilePath = (SetVideoFilePathFunc)GetProcAddress(g_hModule, "SetVideoFilePath");
    if (pSetVideoFilePath) {
        pSetVideoFilePath(path);
    }
}

void TMAudio::SetFfmpegPath(const wchar_t* path) {
    if(!g_hModule) return;

    typedef void (*SetFfmpegPathFunc)(const wchar_t*); 
    SetFfmpegPathFunc pSetFfmpegPath = (SetFfmpegPathFunc)GetProcAddress(g_hModule, "SetFfmpegPath");
    if (pSetFfmpegPath) {
        pSetFfmpegPath(path);
    }
}

void TMAudio::SetAudioEncoderArgs(const wchar_t* args) {
    if(!g_hModule) return;

    typedef void (*SetAudioEncoderArgsFunc)(const wchar_t*);
    SetAudioEncoderArgsFunc pSetAudioEncoderArgs = (SetAudioEncoderArgsFunc)GetProcAddress(g_hModule, "SetAudioEncoderArgs");
    if (pSetAudioEncoderArgs) {
        pSetAudioEncoderArgs(args);
    }
}

std::wstring TMAudio::GetAviFilePath() {
    if(!g_hModule) return L"";

    std::wstring ret = L"";
    typedef const wchar_t* (*GetAviFilePathFunc)();
    GetAviFilePathFunc pGetAviFilePath = (GetAviFilePathFunc)GetProcAddress(g_hModule, "GetAviFilePath");
    if (pGetAviFilePath) {
        const wchar_t* aviPath = pGetAviFilePath();
        if (aviPath) {
            ret = std::wstring(aviPath);
        }
        delete[] aviPath;
    }
    return ret;
}

void TMAudio::EnableTMAudioHooks() {
    if(!g_hModule) return;

    typedef void (*EnableTMAudioHooksFunc)(); 
    EnableTMAudioHooksFunc pEnableTMAudioHooks = (EnableTMAudioHooksFunc)GetProcAddress(g_hModule, "EnableTMAudioHooks");
    if (pEnableTMAudioHooks) {
        pEnableTMAudioHooks();
    }
}
void TMAudio::DisableTMAudioHooks() {
    if(!g_hModule) return;

    typedef void (*DisableTMAudioHooksFunc)(); 
    DisableTMAudioHooksFunc pDisableTMAudioHooks = (DisableTMAudioHooksFunc)GetProcAddress(g_hModule, "DisableTMAudioHooks");
    if (pDisableTMAudioHooks) {
        pDisableTMAudioHooks();
    }
}

void TMAudio::CancelRender() {
    if(!g_hModule) return;

    typedef void (*CancelRenderFunc)(); 
    CancelRenderFunc pCancelRender = (CancelRenderFunc)GetProcAddress(g_hModule, "CancelRender");
    if (pCancelRender) {
        pCancelRender();
    }
}