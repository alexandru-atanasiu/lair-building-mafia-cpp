#include "../platform_dev.h"

#if defined(GAME_DEV_TOOLS_ENABLED)

#include "core/log.h"

#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <utility>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace
{
    // PIPE_NOWAIT: every call returns at once, so the game polls it each frame with no thread.
    class Win32DevPipe final : public IDevPipe
    {
    public:
        explicit Win32DevPipe(HANDLE handle) : m_handle(handle) {}
        ~Win32DevPipe() override
        {
            DisconnectNamedPipe(m_handle);
            CloseHandle(m_handle);
        }

        bool ReadLine(std::string& line) override
        {
            Poll();
            const size_t end = m_buffer.find('\n');
            if (end == std::string::npos)
                return false;
            line = m_buffer.substr(0, end);
            m_buffer.erase(0, end + 1);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            return true;
        }

        void Write(const std::string& text) override
        {
            DWORD written = 0;
            if (!WriteFile(m_handle, text.data(), (DWORD)text.size(), &written, nullptr))
                LOG_WARN("Dev pipe: write failed (%lu)", GetLastError());
        }

    private:
        void Poll()
        {
            if (!ConnectNamedPipe(m_handle, nullptr))
            {
                const DWORD error = GetLastError();
                if (error == ERROR_NO_DATA)
                {
                    DisconnectNamedPipe(m_handle); // the last client left
                    return;
                }
                if (error != ERROR_PIPE_CONNECTED)
                    return; // listening
            }

            char  chunk[4096];
            DWORD count = 0;
            while (ReadFile(m_handle, chunk, sizeof(chunk), &count, nullptr) && count > 0)
                m_buffer.append(chunk, count);
            if (GetLastError() == ERROR_BROKEN_PIPE)
                DisconnectNamedPipe(m_handle);
        }

        HANDLE      m_handle;
        std::string m_buffer;
    };
}

std::unique_ptr<IDevPipe> CreateDevPipe(const char* name)
{
    const std::string path = std::string("\\\\.\\pipe\\") + name;
    const HANDLE handle = CreateNamedPipeA(path.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT, 1,
                                           65536, 65536, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
    {
        LOG_ERR("Can't create pipe %s (%lu)", path.c_str(), GetLastError());
        return nullptr;
    }
    LOG_INFO("Dev pipe: %s", path.c_str());
    return std::make_unique<Win32DevPipe>(handle);
}

bool PlatformWritePng(const char* path, const uint8_t* rgba, uint32_t width, uint32_t height)
{
    wchar_t widePath[1024];
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, widePath, ARRAYSIZE(widePath)))
        return false;

    const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(init) && init != RPC_E_CHANGED_MODE)
        return false;

    bool ok = false;
    {
        // WIC's PNG encoder takes BGRA.
        std::vector<uint8_t> bgra(rgba, rgba + (size_t)width * height * 4);
        for (size_t i = 0; i < bgra.size(); i += 4)
            std::swap(bgra[i], bgra[i + 2]);

        ComPtr<IWICImagingFactory>    factory;
        ComPtr<IWICStream>            stream;
        ComPtr<IWICBitmapEncoder>     encoder;
        ComPtr<IWICBitmapFrameEncode> frame;
        WICPixelFormatGUID            format = GUID_WICPixelFormat32bppBGRA;
        ok = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
             SUCCEEDED(factory->CreateStream(&stream)) &&
             SUCCEEDED(stream->InitializeFromFilename(widePath, GENERIC_WRITE)) &&
             SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) &&
             SUCCEEDED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) &&
             SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr)) &&
             SUCCEEDED(frame->Initialize(nullptr)) &&
             SUCCEEDED(frame->SetSize(width, height)) &&
             SUCCEEDED(frame->SetPixelFormat(&format)) &&
             IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA) &&
             SUCCEEDED(frame->WritePixels(height, width * 4, (UINT)bgra.size(), bgra.data())) &&
             SUCCEEDED(frame->Commit()) &&
             SUCCEEDED(encoder->Commit());
    }

    if (SUCCEEDED(init))
        CoUninitialize();
    return ok;
}

#endif
