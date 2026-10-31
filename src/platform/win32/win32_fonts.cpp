#include "../platform_fonts.h"

#include <windows.h>
#include <shlobj.h>

#include <cstdio>

bool PlatformFindSystemFont(const char* fileName, char* outPath, size_t outPathSize)
{
    PWSTR fontsDir = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Fonts, 0, nullptr, &fontsDir)))
        return false;

    char fontsDirUtf8[MAX_PATH * 4];
    const int converted = WideCharToMultiByte(CP_UTF8, 0, fontsDir, -1, fontsDirUtf8, sizeof(fontsDirUtf8),
                                              nullptr, nullptr);
    CoTaskMemFree(fontsDir);
    if (converted == 0)
        return false;

    const int written = snprintf(outPath, outPathSize, "%s\\%s", fontsDirUtf8, fileName);
    if (written < 0 || (size_t)written >= outPathSize)
        return false;

    wchar_t widePath[MAX_PATH * 2];
    if (MultiByteToWideChar(CP_UTF8, 0, outPath, -1, widePath, ARRAYSIZE(widePath)) == 0)
        return false;
    return GetFileAttributesW(widePath) != INVALID_FILE_ATTRIBUTES;
}
