#pragma once

#include <memory>
#include <map>
#include <string>
#include "zep/mcommon/file/path.h"
#include <functional>

namespace Zep {

// Zep's view of the outside world in terms of files
// Below there is a version of this that will work on most platforms using `std::filesystem` for file operations.
// If you want to expose your app's view of the world, you need to implement this minimal set of functions.
class IZepFileSystem {
public:
    virtual ~IZepFileSystem() = default;
    virtual std::string Read(const ZepPath &filePath) = 0;
    virtual bool Write(const ZepPath &filePath, const void *pData, size_t size) = 0;

    // This is the application config path, where the executable configuration files live
    // (and most likely the .exe too).
    virtual ZepPath GetConfigPath() const = 0;

    // The root path is either the git working directory or the app current working directory
    virtual ZepPath GetSearchRoot(const ZepPath &start, bool &foundGit) const = 0;

    // The working directory is typically the root of the current project that is being edited;
    // i.e. it is set to the path of the first thing that is passed to zep, or is the zep startup folder
    virtual const ZepPath &GetWorkingDirectory() const = 0;
    virtual void SetWorkingDirectory(const ZepPath &path) = 0;
    virtual bool MakeDirectories(const ZepPath &path) = 0;

    virtual bool IsDirectory(const ZepPath &path) const = 0;
    virtual bool IsReadOnly(const ZepPath &path) const = 0;
    virtual bool Exists(const ZepPath &path) const = 0;

    // A callback API for scaning 
    virtual void ScanDirectory(const ZepPath &path, std::function<bool(const ZepPath &path, bool &dont_recurse)> fnScan) const = 0;

    // Equivalent means 'the same file'
    virtual bool Equivalent(const ZepPath &path1, const ZepPath &path2) const = 0;
    virtual ZepPath Canonical(const ZepPath &path) const = 0;
};

// CPP File system - part of the standard C++ libraries
#if defined(ZEP_FEATURE_CPP_FILE_SYSTEM)

// A generic file system using cross-platform `fs::` and `tinydir` for searches.
// This is typically the only one that is used for normal desktop usage.
// But you could make your own if your files were stored in a compressed folder, or the target system didn't have a traditional file system...
struct ZepFileSystemCPP : public IZepFileSystem {
    explicit ZepFileSystemCPP(const ZepPath &configPath);
    ~ZepFileSystemCPP() override;
    std::string Read(const ZepPath &filePath) override;
    bool Write(const ZepPath &filePath, const void *pData, size_t size) override;
    void ScanDirectory(const ZepPath &path, std::function<bool(const ZepPath &path, bool &dont_recurse)> fnScan) const override;
    void SetWorkingDirectory(const ZepPath &path) override;
    bool MakeDirectories(const ZepPath &path) override;
    const ZepPath &GetWorkingDirectory() const override;
    ZepPath GetConfigPath() const override;
    ZepPath GetSearchRoot(const ZepPath &start, bool &foundGit) const override;
    bool IsDirectory(const ZepPath &path) const override;
    bool IsReadOnly(const ZepPath &path) const override;
    bool Exists(const ZepPath &path) const override;
    bool Equivalent(const ZepPath &path1, const ZepPath &path2) const override;
    ZepPath Canonical(const ZepPath &path) const override;

private:
    ZepPath m_workingDirectory;
    ZepPath m_configPath;
};
#endif // CPP File system

} // namespace Zep
