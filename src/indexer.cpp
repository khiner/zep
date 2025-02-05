#include "zep/file/fnmatch.h"
#include "zep/logger.h"
#include "zep/stringutils.h"
#include "zep/threadutils.h"

#include "zep/filesystem.h"
#include "zep/indexer.h"

namespace Zep {

Indexer::Indexer(ZepEditor &editor) : ZepComponent(editor) {}

void Indexer::GetSearchPaths(ZepEditor &editor, const ZepPath &path, std::vector<std::string> &ignore_patterns, std::vector<std::string> &include_patterns, std::string &errors) {
    ZepPath configPath = path / ".zep" / "project.cfg";
    if (!ZepFileSystem::Exists(configPath)) {
        configPath = editor.configRoot / "zep.cfg";
    }

    if (ZepFileSystem::Exists(configPath)) {
        try {
            auto config = cpptoml::parse_file(configPath.string());
            if (config != nullptr) {
                ignore_patterns = config->get_qualified_array_of<std::string>("search.ignore").value_or(std::vector<std::string>{});
                include_patterns = config->get_qualified_array_of<std::string>("search.include").value_or(std::vector<std::string>{});
            }
        }
        catch (cpptoml::parse_exception &ex) {
            std::ostringstream str;
            str << configPath.filename().string() << " : Failed to parse. " << ex.what();
            errors = str.str();
        }
        catch (...) {
            std::ostringstream str;
            str << configPath.filename().string() << " : Failed to parse. ";
            errors = str.str();
        }
    }

    if (ignore_patterns.empty()) {
        ignore_patterns = {
            "[Bb]uild/*",
            "**/[Oo]bj/**",
            "**/[Bb]in/**",
            "[Bb]uilt*"
        };
    }
    if (include_patterns.empty()) {
        include_patterns = {"*.cpp", "*.c", "*.hpp", "*.h", "*.lsp", "*.scm", "*.cs", "*.cfg",};
    }
} // namespace Zep

std::future<std::shared_ptr<FileIndexResult>> Indexer::IndexPaths(ZepEditor &editor, const ZepPath &startPath) {
    std::vector<std::string> ignorePaths;
    std::vector<std::string> includePaths;
    std::string errors;
    GetSearchPaths(editor, startPath, ignorePaths, includePaths, errors);

    auto result = std::make_shared<FileIndexResult>();
    if (!errors.empty()) {
        result->errors = errors;
        return make_ready_future(result);
    }

    return editor.threadPool->enqueue([=](ZepPath root) {
            result->root = root;

            try {
                // Index the whole subtree, ignoring any patterns supplied to us
                ZepFileSystem::ScanDirectory(root, [&](const ZepPath &p, bool &recurse) -> bool {
                    recurse = true;

                    auto bDir = ZepFileSystem::IsDirectory(p);

                    // Add this one to our list
                    auto targetZep = ZepFileSystem::Canonical(p);
                    auto rel = path_get_relative(root, targetZep);

                    bool matched = true;
                    for (auto &proj: ignorePaths) {
                        auto res = fnmatch(proj.c_str(), rel.string().c_str(), 0);
                        if (res == 0) {
                            matched = false;
                            break;
                        }
                    }

                    if (!matched) {
                        if (bDir) recurse = false;
                        return true;
                    }

                    matched = false;
                    for (auto &proj: includePaths) {
                        auto res = fnmatch(proj.c_str(), rel.string().c_str(), 0);
                        if (res == 0) {
                            matched = true;
                            break;
                        }
                    }

                    if (!matched) return true;

                    // Not adding directories to the search list
                    if (bDir) return true;

                    result->paths.push_back(rel);
                    result->lowerPaths.push_back(string_tolower(rel.string()));

                    return true;
                });
            } catch (std::exception &) {}

            return result;
        },
        startPath);
}

void Indexer::Notify(const std::shared_ptr<ZepMessage> &message) {
    if (message->messageId == Msg::Tick) {
        if (m_fileSearchActive) {
            if (!is_future_ready(m_indexResult)) return;

            m_fileSearchActive = false;

            m_filePaths = m_indexResult.get();
            if (!m_filePaths->errors.empty()) {
                editor.SetCommandText(m_filePaths->errors);
                return;
            }

            {
                // Queue the files to be searched
                std::lock_guard<std::mutex> guard(m_queueMutex);
                for (auto &p: m_filePaths->paths) {
                    m_searchQueue.push_back(p);
                }
            }

            StartSymbolSearch(); // Kick off the thread
        }
    }
}

void Indexer::StartSymbolSearch() {
    editor.threadPool->enqueue([=]() {
        for (;;) {
            ZepPath path;
            {
                std::lock_guard<std::mutex> lock(m_queueMutex);
                if (m_searchQueue.empty()) return true;

                path = m_searchQueue.front();
                m_searchQueue.pop_front();
            }

            std::string strLast;
            auto fullPath = m_searchRoot / path;
            if (ZepFileSystem::Exists(fullPath)) {
                ZLOG(DBG, "Parsing: " << fullPath.c_str());
                auto strFile = ZepFileSystem::Read(fullPath);

                std::vector<std::string> tokens;
                string_split(strFile, ";()[] \t\n\r&!\"\'*:,<>", tokens);
            }
        }
    });
}

bool Indexer::StartIndexing() {
    bool foundGit = false;
    m_searchRoot = editor.fileSystem->GetSearchRoot(editor.fileSystem->workingDirectory, foundGit);
    if (!foundGit) {
        ZLOG(INFO, "Not a git project");
        return false;
    }

    auto indexDBRoot = m_searchRoot / ".zep";
    if (!ZepFileSystem::IsDirectory(indexDBRoot) && !ZepFileSystem::MakeDirectories(indexDBRoot)) {
        ZLOG(ERROR, "Can't get the index folder");
        return false;
    }

    int v = 0;
    ZepFileSystem::Write(indexDBRoot / "indexdb", &v, 1);

    m_fileSearchActive = true;
    m_indexResult = Indexer::IndexPaths(editor, m_searchRoot);

    return true;
}

} // namespace Zep
