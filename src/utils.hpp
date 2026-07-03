#pragma once
#include <windows.h>
#include <string>

namespace Bridge {
    extern HINSTANCE g_hInstance;

    void LoadAdjacentDLL(HMODULE hModule, const wchar_t* targetDllName);
    std::wstring GetAdjacentPath(HMODULE hModule, const wchar_t* targetDllName);
}