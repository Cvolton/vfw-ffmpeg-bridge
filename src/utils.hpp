#pragma once
#include <windows.h>
#include <string>
#include <optional>

namespace Bridge {
    extern HINSTANCE g_hInstance;

    HMODULE LoadAdjacentDLL(HMODULE hModule, const wchar_t* targetDllName);
    std::wstring GetAdjacentPath(HMODULE hModule, const wchar_t* targetDllName);

    std::optional<std::wstring> SaveDialog(std::wstring defaultPath, std::wstring filters);
    std::optional<std::wstring> OpenDialog(std::wstring defaultPath, std::wstring filters);
    std::optional<std::wstring> FolderDialog(std::wstring defaultPath, HWND hwndDlg = nullptr);

    enum class DialogType {
        Save,
        Open,
        SelectFolder,
    };
    enum class LinuxDialogResult {
        Success,
        Cancel,
        Error
    };
    std::pair<LinuxDialogResult, std::wstring> LinuxDialog(DialogType type, std::wstring defaultPath, std::wstring filters);
    bool IsWine();

    std::wstring GetInstallDir();
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