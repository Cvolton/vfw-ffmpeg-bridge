#include "utils.hpp"
#include "CodecState.hpp"
#include "subprocess.hpp"
#include <ShObjIdl_core.h>
#include <format>
HINSTANCE Bridge::g_hInstance = nullptr;

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

std::optional<std::wstring> Bridge::SaveDialog(std::wstring defaultPath, std::wstring filters) {
    if(Bridge::IsWine()) {
        auto res = LinuxDialog(DialogType::Save, defaultPath, L"");
        if(res.first != LinuxDialogResult::Error) {
            if(res.first == LinuxDialogResult::Success) {
                return res.second;
            } else {
                return std::nullopt;
            }
        }

        // fallback to wine built in file picker if linux dialog fails to open
    }

    wchar_t fileBuffer[MAX_PATH] = {};
    wcsncpy_s(fileBuffer, defaultPath.c_str(), _TRUNCATE);

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFilter = filters.c_str();
    ofn.lpstrFile = fileBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"mp4";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetSaveFileNameW(&ofn)) {
        return std::wstring(fileBuffer);
    }
    return std::nullopt;
}

std::optional<std::wstring> Bridge::OpenDialog(std::wstring defaultPath, std::wstring filters) {
    if(Bridge::IsWine()) {
        auto res = LinuxDialog(DialogType::Open, defaultPath, L"");
        if(res.first != LinuxDialogResult::Error) {
            if(res.first == LinuxDialogResult::Success) {
                return res.second;
            } else {
                return std::nullopt;
            }
        }

        // fallback to wine built in file picker if linux dialog fails to open
    }

    wchar_t fileBuffer[MAX_PATH] = {};
    wcsncpy_s(fileBuffer, defaultPath.c_str(), _TRUNCATE);

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFilter = filters.c_str();
    ofn.lpstrFile = fileBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"mp4";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        return std::wstring(fileBuffer);
    }
    return std::nullopt;
}

std::optional<std::wstring> Bridge::FolderDialog(std::wstring defaultPath, HWND hwndDlg) {
    if(Bridge::IsWine()) {
        auto res = LinuxDialog(DialogType::SelectFolder, defaultPath, L"");
        if(res.first != LinuxDialogResult::Error) {
            if(res.first == LinuxDialogResult::Success) {
                return res.second;
            } else {
                return std::nullopt;
            }
        }

        // fallback to wine built in file picker if linux dialog fails to open
    }


    std::optional<std::wstring> ret = std::nullopt;

    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool comInitializedHere = SUCCEEDED(hrInit);
    bool comUsable = SUCCEEDED(hrInit) || hrInit == RPC_E_CHANGED_MODE;

    if (comUsable) {
        IFileOpenDialog* pDialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&pDialog));

        if (SUCCEEDED(hr)) {
            DWORD options = 0;
            pDialog->GetOptions(&options);
            pDialog->SetOptions(options | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM);

            if (!defaultPath.empty()) {
                IShellItem* pDefaultFolder = nullptr;
                if (SUCCEEDED(SHCreateItemFromParsingName(defaultPath.c_str(), nullptr, IID_PPV_ARGS(&pDefaultFolder)))) {
                    pDialog->SetFolder(pDefaultFolder);
                    pDefaultFolder->Release();
                }
            }

            hr = pDialog->Show(hwndDlg);
            if (SUCCEEDED(hr)) {
                IShellItem* pResult = nullptr;
                if (SUCCEEDED(pDialog->GetResult(&pResult))) {
                    PWSTR path = nullptr;
                    if (SUCCEEDED(pResult->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                        ret = std::wstring(path);
                        CoTaskMemFree(path);
                    }
                    pResult->Release();
                }
            }

            pDialog->Release();
        }
    }

    if (comInitializedHere) {
        CoUninitialize();
    }

    return ret;
}

std::pair<Bridge::LinuxDialogResult, std::wstring> Bridge::LinuxDialog(DialogType type, std::wstring defaultPath, std::wstring filters) {
    if(!Bridge::IsWine()) {
        return {LinuxDialogResult::Error, L""};
    }

    auto loc = Bridge::GetInstallDir() + L"\\wine\\linux-file-picker.exe";
    
    typedef std::wstring (WINAPI *wine_get_unix_file_name_func)(const wchar_t*);
    wine_get_unix_file_name_func wine_get_unix_file_name = (wine_get_unix_file_name_func)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "wine_get_unix_file_name");
    if(!wine_get_unix_file_name) {
        return {LinuxDialogResult::Error, L""};
    }
    std::wstring unixPath = wine_get_unix_file_name(defaultPath.c_str());

    std::wstring cmd = std::format(L"\"{}\" {} \"{}\" \"{}\"", loc, static_cast<int>(type), unixPath, filters);

    subprocess::Popen proc(cmd, true);
    
    std::string output = proc.m_stdout.read();
    auto code = proc.wait();
    proc.m_stdout.close();

    switch(code) {
        case 0: {
            std::wstring result = utf8ToWide(output);
            return {LinuxDialogResult::Success, result};
        }
        case 1: {
            return {LinuxDialogResult::Cancel, L""};
        }
        default:
            return {LinuxDialogResult::Error, L""};
    }
}

bool Bridge::IsWine() {
    static const char * (CDECL *pwine_get_version)(void);
    HMODULE hntdll = GetModuleHandle("ntdll.dll");
    if(!hntdll) return false;

    pwine_get_version = (const char *(__cdecl *)(void))GetProcAddress(hntdll, "wine_get_version");
    if(!pwine_get_version) return false;

    return true;
}

std::wstring Bridge::GetInstallDir() {
    WCHAR installDir[MAX_PATH];
    DWORD size = sizeof(installDir);

    LONG result = RegGetValueW(
        HKEY_LOCAL_MACHINE,
        L"Software\\vfw-ffmpeg-bridge",
        L"InstallDir",
        RRF_RT_REG_SZ,
        nullptr,
        installDir,
        &size
    );

    if (result != ERROR_SUCCESS) {
        return L"";
    }

    return std::wstring(installDir);
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
        
        typedef void (*DeleteAviFilePathBufferFunc)(const wchar_t*);
        DeleteAviFilePathBufferFunc pDeleteAviFilePathBuffer = (DeleteAviFilePathBufferFunc)GetProcAddress(g_hModule, "DeleteAviFilePathBuffer");
        if (pDeleteAviFilePathBuffer) {
            pDeleteAviFilePathBuffer(aviPath);
        }
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