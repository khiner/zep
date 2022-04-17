#pragma once

#include <memory>
#include <map>
#include <string>
#include "zep/mcommon/file/path.h"
#include <functional>

namespace Zep {

// A generic file system using cross-platform `fs::` and `tinydir` for searches.
// This is typically the only one that is used for normal desktop usage.
// But you could make your own if your files were stored in a compressed folder, or the target system didn't have a traditional file system...
struct ZepFileSystem {
    explicit ZepFileSystem(const ZepPath &configPath);
    static std::string Read(const ZepPath &filePathstatic);
    static bool Write(const ZepPath &filePath, const void *pData, size_t size);
    // A callback API for scannistatic ng
    static bool MakeDirectories(const ZepPath &path);
    // The root path is either the git working directory or the app current working directory
    ZepPath GetSearchRoot(const ZepPath &start, bool &foundGit) const;
    static bool IsDirectory(const ZepPath &path);
    static bool IsReadOnly(const ZepPath &path);
    // Equivalent means 'the same file'
    static bool Equivalent(const ZepPath &path1, const ZepPath &path2);
    static ZepPath Canonical(const ZepPath &path);

    // The working directory is typically the root of the current project that is being edited;
    // i.e. it is set to the path of the first thing that is passed to zep, or is the zep startup folder
    ZepPath workingDirectory;
    // This is the application config path, where the executable configuration files live (and most likely the .exe too).
    ZepPath configPath;

    static void ScanDirectory(const ZepPath &path, const std::function<bool(const ZepPath &path, bool &dont_recurse)> &fnScan);
    static bool Exists(const ZepPath &path);
};

} // namespace Zep
