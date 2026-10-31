#pragma once

// Reports an unrecoverable error to the user and terminates the process.
// `code` is an optional platform error code (e.g. HRESULT), shown when non-zero.
[[noreturn]] void Fatal(const char* msg, long code = 0);

// Fatal error for a required asset (path relative to the data folder) that couldn't be loaded:
// tells the user how to cook the data and where to run the game from.
[[noreturn]] void FatalMissingAsset(const char* path);

// Returns `asset` (anything testable for null, e.g. the unique_ptr from LoadMesh), or calls
// FatalMissingAsset(path) if it's null:
//   m_mesh = LoadOrFatal(LoadMesh(device, path), path);
template <typename T>
T LoadOrFatal(T asset, const char* path)
{
    if (!asset)
        FatalMissingAsset(path);
    return asset;
}
