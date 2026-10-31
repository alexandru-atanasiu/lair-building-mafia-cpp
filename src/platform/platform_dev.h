#pragma once

#if defined(GAME_DEV_TOOLS_ENABLED)

#include <cstdint>
#include <memory>
#include <string>

// What the dev driver needs from the platform (dbg and opt only).

// A server end of a named pipe, polled from the game's thread. One client at a time; a client
// leaving makes it wait for the next.
class IDevPipe
{
public:
    virtual ~IDevPipe() = default;

    // Gets the next complete line sent by the client, without its line ending. False if none yet.
    virtual bool ReadLine(std::string& line) = 0;
    // Sends to the client, if there is one.
    virtual void Write(const std::string& text) = 0;
};

// `name`: the bare pipe name; \\.\pipe\ is added. Null (logged) if it can't be created.
std::unique_ptr<IDevPipe> CreateDevPipe(const char* name);

// Writes RGBA8 rows (top to bottom, opaque) as a PNG. False on failure.
bool PlatformWritePng(const char* path, const uint8_t* rgba, uint32_t width, uint32_t height);

#endif
