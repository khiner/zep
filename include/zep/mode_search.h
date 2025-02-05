#pragma once

#include "mode.h"
#include <future>
#include <memory>
#include <regex>

#include <zep/indexer.h>

namespace Zep {

struct ZepWindow;
struct ZepMode_Search : public ZepMode {
    ZepMode_Search(ZepEditor &editor, ZepWindow &previousWindow, ZepWindow &window, ZepPath startPath);
    ~ZepMode_Search() override;

    void AddKeyPress(ImGuiKey key, ImGuiModFlags modifiers) override;
    void Begin(ZepWindow *pWindow) override;
    void Notify(const std::shared_ptr<ZepMessage> &message) override;
    EditorMode DefaultMode() const override { return EditorMode::Normal; }

    static const char *StaticName() { return "Search"; }
    const char *Name() const override { return StaticName(); }

    CursorType GetCursorType() const override;

private:
    void InitSearchTree();
    void ShowTreeResult();
    void UpdateTree();

    enum class OpenType {
        Replace,
        VSplit,
        HSplit,
        Tab
    };
    void OpenSelection(OpenType type);

private:

    // List of lines in the file result, with last found char
    struct SearchResult {
        uint32_t index = 0;
        uint32_t location = 0;
    };

    // A mapping from character distance to a list of lines
    using IndexSet = std::multimap<uint32_t, SearchResult>;

    bool fileSearchActive = false;
    bool treeSearchActive = false;

    // Results of the file search and the indexing threads
    std::future<std::shared_ptr<FileIndexResult>> m_indexResult;
    std::future<std::shared_ptr<IndexSet>> m_searchResult;

    // All files that can potentially match
    std::shared_ptr<FileIndexResult> m_filePaths;

    // A hierarchy of index results.
    // The 'top' of the tree is the narrowest finding from a set of 'n' characters
    // index a,b,c -> index b,c -> index c
    std::vector<std::shared_ptr<IndexSet>> m_indexTree;

    // What we are searching for 
    std::string m_searchTerm;
    bool m_caseImportant = false;

    ZepWindow &m_launchWindow;
    ZepWindow &m_window;
    ZepPath m_startPath;
};

} // namespace Zep
