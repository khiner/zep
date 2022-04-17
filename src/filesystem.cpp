#include "zep/filesystem.h"
#include "zep/editor.h"
#include "zep/mcommon/logger.h"

#undef ERROR

#include <filesystem>

namespace cpp_fs = std::filesystem;

namespace Zep {
ZepFileSystem::ZepFileSystem(const ZepPath &configPath) : configPath(configPath) {
    workingDirectory = ZepPath(cpp_fs::current_path().string());

    // Didn't find the config path, try the working directory
    if (!Exists(configPath)) {
        this->configPath = workingDirectory;
    }

    ZLOG(INFO, "Config Dir: " << configPath.c_str());
    ZLOG(INFO, "Working Dir: " << workingDirectory.c_str());
}

bool ZepFileSystem::MakeDirectories(const ZepPath &path) { return cpp_fs::create_directories(path.c_str()); }
bool ZepFileSystem::IsDirectory(const ZepPath &path) { return Exists(path) ? cpp_fs::is_directory(path.string()) : false; }

bool ZepFileSystem::IsReadOnly(const ZepPath &path) {
    auto perms = cpp_fs::status(path.string()).permissions();
    return (perms & cpp_fs::perms::owner_write) == cpp_fs::perms::owner_write ? false : true;
}

std::string ZepFileSystem::Read(const ZepPath &fileName) {
    std::ifstream in(fileName, std::ios::in | std::ios::binary);
    if (in) {
        std::string contents;
        in.seekg(0, std::ios::end);
        contents.resize(size_t(in.tellg()));
        in.seekg(0, std::ios::beg);
        in.read(&contents[0], contents.size());
        in.close();
        return (contents);
    }

    ZLOG(ERROR, "File Not Found: " << fileName.string());
    return {};
}

bool ZepFileSystem::Write(const ZepPath &fileName, const void *pData, size_t size) {
    FILE *file = fopen(fileName.string().c_str(), "wb");
    if (!file) return false;

    fwrite(pData, sizeof(uint8_t), size, file);
    fclose(file);

    return true;
}

bool ZepFileSystem::Equivalent(const ZepPath &path1, const ZepPath &path2) {
    try {
        // The below API expects existing files! Best we can do is direct compare of paths
        return !cpp_fs::exists(path1.string()) || !cpp_fs::exists(path2.string()) ?
               Canonical(path1).string() == Canonical(path2).string() :
               cpp_fs::equivalent(path1.string(), path2.string());
    } catch (cpp_fs::filesystem_error &err) {
        ZEP_UNUSED(err);
        ZLOG(ERROR, "Exception: " << err.what());
        return path1 == path2;
    }
}

ZepPath ZepFileSystem::Canonical(const ZepPath &path) {
    try {
        return {cpp_fs::weakly_canonical(path.string()).string()};
    } catch (cpp_fs::filesystem_error &err) {
        ZEP_UNUSED(err);
        ZLOG(ERROR, "Exception: " << err.what());
        return path;
    }
}

ZepPath ZepFileSystem::GetSearchRoot(const ZepPath &start, bool &foundGit) const {
    foundGit = false;
    auto findStartPath = [&](const ZepPath &startPath) {
        if (!startPath.empty()) {
            auto testPath = IsDirectory(startPath) ? startPath : startPath.parent_path();
            while (!testPath.empty() && IsDirectory(testPath)) {
                foundGit = false;

                ScanDirectory(testPath, [&](const ZepPath &p, bool &recurse) -> bool {
                    // Not looking at sub folders
                    recurse = false;

                    // Found the .git repo
                    if (p.extension() == ".git" && IsDirectory(p)) {
                        foundGit = true;
                        return false;
                    }
                    return true;
                });

                if (foundGit) return testPath;

                testPath = testPath.parent_path();
            }
        }
        return startPath;
    };

    auto startPath = findStartPath(start);
    if (startPath.empty()) {
        startPath = findStartPath(workingDirectory);
        if (startPath.empty()) startPath = workingDirectory;
    }

    return startPath.empty() ? start : startPath;
}

void ZepFileSystem::ScanDirectory(const ZepPath &path, const std::function<bool(const ZepPath &path, bool &dont_recurse)> &fnScan) {
    for (auto itr = cpp_fs::recursive_directory_iterator(path.string()); itr != cpp_fs::recursive_directory_iterator(); itr++) {
        auto p = ZepPath(itr->path().string());

        bool recurse = true;
        if (!fnScan(p, recurse)) return;
    }
}

bool ZepFileSystem::Exists(const ZepPath &path) {
    try {
        return cpp_fs::exists(path.string());
    } catch (cpp_fs::filesystem_error &err) {
        ZEP_UNUSED(err);
        ZLOG(ERROR, "Exception: " << err.what());
        return false;
    }
}

} // namespace Zep
