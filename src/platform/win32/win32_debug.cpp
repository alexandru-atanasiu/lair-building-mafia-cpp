#include "../platform_debug.h"

#include "core/path_utils.h"

#include <windows.h>
#include <commctrl.h>
#include <dbghelp.h>
#include <intrin.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <mutex>

// TaskDialog lives in Common Controls v6, which must be requested via the manifest.
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' "  \
                        "version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' " \
                        "language='*'\"")

// ---------------------------------------------------------------------------
// Callstack
// ---------------------------------------------------------------------------

void PlatformCaptureCallstack(char* buffer, size_t bufferSize, int skipFrames)
{
    if (bufferSize == 0)
        return;
    buffer[0] = '\0';

    void*        frames[64];
    const USHORT frameCount = RtlCaptureStackBackTrace((DWORD)(skipFrames + 1), ARRAYSIZE(frames), frames, nullptr);

    // DbgHelp is single-threaded.
    static std::mutex dbgHelpMutex;
    std::lock_guard<std::mutex> lock(dbgHelpMutex);

    const HANDLE process = GetCurrentProcess();
    static const bool symbolsReady = [process] {
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
        return SymInitialize(process, nullptr, TRUE) != FALSE;
    }();

    size_t used = 0;
    for (USHORT i = 0; i < frameCount; ++i)
    {
        // Frames hold return addresses; step back into the call instruction so the
        // line lookup lands on the call site rather than the line after it.
        const DWORD64 address = (DWORD64)frames[i] - 1;

        alignas(SYMBOL_INFO) char symbolStorage[sizeof(SYMBOL_INFO) + 256];
        SYMBOL_INFO* symbol  = reinterpret_cast<SYMBOL_INFO*>(symbolStorage);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen   = 255;

        DWORD64     symbolDisplacement = 0;
        const char* name = (symbolsReady && SymFromAddr(process, address, &symbolDisplacement, symbol))
                               ? symbol->Name
                               : "???";

        IMAGEHLP_LINE64 lineInfo     = {};
        lineInfo.SizeOfStruct        = sizeof(lineInfo);
        DWORD           lineDisplacement = 0;

        int written;
        if (symbolsReady && SymGetLineFromAddr64(process, address, &lineDisplacement, &lineInfo))
            written = snprintf(buffer + used, bufferSize - used, "    %s  (%s:%lu)\n",
                               name, PathFileName(lineInfo.FileName), lineInfo.LineNumber);
        else
            written = snprintf(buffer + used, bufferSize - used, "    %s  (0x%016llX)\n",
                               name, (unsigned long long)address);

        if (written < 0 || used + (size_t)written >= bufferSize)
            break;
        used += (size_t)written;

        // Everything below main is CRT/OS startup noise.
        if (strcmp(name, "main") == 0)
            break;
    }

    if (used > 0 && buffer[used - 1] == '\n')
        buffer[used - 1] = '\0';
}

// ---------------------------------------------------------------------------
// Assert dialog
// ---------------------------------------------------------------------------

static HRESULT CALLBACK AssertDialogCallback(HWND hwnd, UINT msg, WPARAM, LPARAM, LONG_PTR)
{
    if (msg == TDN_CREATED)
    {
        // Make sure it's visible above the (possibly fullscreen) game window.
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetForegroundWindow(hwnd);
    }
    return S_OK;
}

AssertDialogResult PlatformShowAssertDialog(const char* title, const char* body, const char* callstack,
                                            bool allowContinue)
{
    enum { kRetry = 100, kContinue, kAbort };

    static wchar_t wTitle[256];
    static wchar_t wBody[2048];
    static wchar_t wCallstack[8192];
    MultiByteToWideChar(CP_UTF8, 0, title, -1, wTitle, ARRAYSIZE(wTitle));
    MultiByteToWideChar(CP_UTF8, 0, body, -1, wBody, ARRAYSIZE(wBody));
    static const wchar_t kCallstackHeader[] = L"Callstack:\n";
    wcscpy_s(wCallstack, kCallstackHeader);
    MultiByteToWideChar(CP_UTF8, 0, callstack, -1, wCallstack + ARRAYSIZE(kCallstackHeader) - 1,
                        ARRAYSIZE(wCallstack) - (ARRAYSIZE(kCallstackHeader) - 1));

    TASKDIALOG_BUTTON buttons[3];
    UINT              buttonCount = 0;
    buttons[buttonCount++] = { kRetry, L"Retry" };
    if (allowContinue)
        buttons[buttonCount++] = { kContinue, L"Continue" };
    buttons[buttonCount++] = { kAbort, L"Abort" };

    // No TDF_ALLOW_DIALOG_CANCELLATION: the close button / Esc are disabled, a choice must be made.
    TASKDIALOGCONFIG config        = {};
    config.cbSize                  = sizeof(config);
    config.dwFlags                 = TDF_EXPANDED_BY_DEFAULT | TDF_SIZE_TO_CONTENT;
    config.pszWindowTitle          = L"Capo di Tutti Capi";
    config.pszMainIcon             = TD_ERROR_ICON;
    config.pszMainInstruction      = wTitle;
    config.pszContent              = wBody;
    config.pszExpandedInformation  = wCallstack;
    config.pszExpandedControlText  = L"Hide callstack";
    config.pszCollapsedControlText = L"Show callstack";
    config.pButtons                = buttons;
    config.cButtons                = buttonCount;
    config.nDefaultButton          = allowContinue ? kContinue : kRetry;
    config.pfCallback              = AssertDialogCallback;

    // Statics above: only one dialog at a time.
    static std::mutex dialogMutex;
    std::lock_guard<std::mutex> lock(dialogMutex);

    // An assert during mouse-look would otherwise leave the cursor hidden and pinned.
    PlatformReleaseCursor();

    int pressed = 0;
    if (FAILED(TaskDialogIndirect(&config, &pressed, nullptr, nullptr)))
        return allowContinue ? AssertDialogResult::Continue : AssertDialogResult::Abort;

    switch (pressed)
    {
    case kRetry:    return AssertDialogResult::Retry;
    case kContinue: return AssertDialogResult::Continue;
    default:        return AssertDialogResult::Abort;
    }
}

void PlatformReleaseCursor()
{
    ClipCursor(nullptr);
    ReleaseCapture();
    // The cursor shows while the display counter is >= 0.
    while (ShowCursor(TRUE) < 0)
    {
    }
}

// ---------------------------------------------------------------------------
// Crash handler
// ---------------------------------------------------------------------------

namespace
{
    // %LOCALAPPDATA%\LairBuildingMafiaCPP\crashes\game_<date>_<time>_<pid>.dmp (created as needed).
    bool MakeDumpPath(wchar_t* path, size_t pathSize)
    {
        wchar_t base[MAX_PATH];
        const DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", base, ARRAYSIZE(base));
        if (len == 0 || len >= ARRAYSIZE(base))
            return false;

        swprintf(path, pathSize, L"%ls\\LairBuildingMafiaCPP", base);
        CreateDirectoryW(path, nullptr);
        swprintf(path, pathSize, L"%ls\\LairBuildingMafiaCPP\\crashes", base);
        CreateDirectoryW(path, nullptr);

        SYSTEMTIME t;
        GetLocalTime(&t);
        const int written = swprintf(path, pathSize, L"%ls\\LairBuildingMafiaCPP\\crashes\\game_%04u-%02u-%02u_%02u-%02u-%02u_%lu.dmp",
                                     base, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond,
                                     GetCurrentProcessId());
        return written > 0;
    }

    bool WriteMinidump(EXCEPTION_POINTERS* exception, const wchar_t* path)
    {
        const HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;

        MINIDUMP_EXCEPTION_INFORMATION info = {};
        info.ThreadId          = GetCurrentThreadId();
        info.ExceptionPointers = exception;
        info.ClientPointers    = FALSE;

        // Stacks, plus the memory they point to (enough to inspect most locals), thread and
        // module info. Matching symbols: the .pdb build.bat keeps next to the exe.
        const MINIDUMP_TYPE type = (MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo |
                                                   MiniDumpWithUnloadedModules);
        const BOOL ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, type, &info, nullptr, nullptr);
        CloseHandle(file);
        if (!ok)
            DeleteFileW(path);
        return ok != FALSE;
    }

    LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* exception)
    {
        // A second crash (another thread, or inside this handler) just terminates.
        static std::atomic<bool> handling = false;
        if (handling.exchange(true))
            return EXCEPTION_EXECUTE_HANDLER;

        // No logging here: the crash may have happened with the log lock held.
        PlatformReleaseCursor();

        static wchar_t path[MAX_PATH * 2];
        static wchar_t message[MAX_PATH * 3];
        const DWORD    code = exception && exception->ExceptionRecord ? exception->ExceptionRecord->ExceptionCode : 0;
        if (MakeDumpPath(path, ARRAYSIZE(path)) && WriteMinidump(exception, path))
            swprintf(message, ARRAYSIZE(message), L"The game crashed (exception 0x%08lX).\n\nA crash dump was written to:\n%ls",
                     code, path);
        else
            swprintf(message, ARRAYSIZE(message), L"The game crashed (exception 0x%08lX).\n\nNo crash dump could be written.",
                     code);

        OutputDebugStringW(message);
        MessageBoxW(nullptr, message, L"Capo di Tutti Capi", MB_OK | MB_ICONERROR | MB_TOPMOST);
        return EXCEPTION_EXECUTE_HANDLER; // terminate the process
    }
}

void PlatformInstallCrashHandler()
{
    // Only reached without a debugger attached: a debugger gets unhandled exceptions first.
    SetUnhandledExceptionFilter(UnhandledExceptionHandler);
}

// ---------------------------------------------------------------------------
// Termination
// ---------------------------------------------------------------------------

void PlatformCrash()
{
    __fastfail(FAST_FAIL_FATAL_APP_EXIT);
}

void PlatformExit(int exitCode)
{
    ExitProcess((UINT)exitCode);
}
