#pragma once

#include <future>
#include <memory>
#include <regex>
#include <thread>

#include "zep/editor.h"

namespace Zep {

struct ZepEditor;

// List of files found in the directory search
struct FileIndexResult {
    ZepPath root;
    std::vector<ZepPath> paths;
    std::vector<std::string> lowerPaths;
    std::string errors;
};

struct Indexer : public ZepComponent {
    explicit Indexer(ZepEditor &editor);

    void Notify(const std::shared_ptr<ZepMessage> &message) override;

    bool StartIndexing();
    void StartSymbolSearch();

    static void GetSearchPaths(ZepEditor &editor, const ZepPath &path, std::vector<std::string> &ignore_patterns, std::vector<std::string> &include_patterns, std::string &errors);
    static std::future<std::shared_ptr<FileIndexResult>> IndexPaths(ZepEditor &editor, const ZepPath &startPath);

private:
    bool m_fileSearchActive = false;
    std::future<std::shared_ptr<FileIndexResult>> m_indexResult;
    std::shared_ptr<FileIndexResult> m_spFilePaths;

    std::mutex m_queueMutex;
    std::deque<ZepPath> m_searchQueue;

    ZepPath m_searchRoot;
};

} // Zep
