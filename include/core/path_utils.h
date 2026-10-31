#pragma once

// Returns the file name part of a path (everything after the last '/' or '\').
inline const char* PathFileName(const char* path)
{
    const char* name = path;
    for (const char* p = path; *p; ++p)
    {
        if (*p == '/' || *p == '\\')
            name = p + 1;
    }
    return name;
}
