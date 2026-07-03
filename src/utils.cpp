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

void Bridge::LoadAdjacentDLL(HMODULE hModule, const wchar_t* targetDllName)
{
    auto targetDllPath = GetAdjacentPath(hModule, targetDllName);

    if (!targetDllPath.empty())
    {
        LoadLibraryW(targetDllPath.c_str());
    }
}