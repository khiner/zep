#include "zep/mode_search.h"
#include "zep/filesystem.h"
#include "zep/tab_window.h"
#include "zep/window.h"

#include "zep/logger.h"
#include "zep/threadutils.h"

#include "zep/file/fnmatch.h"

namespace Zep {

ZepMode_Search::ZepMode_Search(ZepEditor &editor, ZepWindow &launchWindow, ZepWindow &window, ZepPath path)
    : ZepMode(editor), m_launchWindow(launchWindow), m_window(window), m_startPath(std::move(path)) {}

ZepMode_Search::~ZepMode_Search() {
    // Ensure threads have finished
    if (m_indexResult.valid()) m_indexResult.wait();
    if (m_searchResult.valid()) m_searchResult.wait();
}

void ZepMode_Search::AddKeyPress(ImGuiKey key, ImGuiModFlags modifiers) {
    (void) modifiers;
    if (key == ImGuiKey_Escape) {
        // CM TODO:
        // Note that the Repl represents the new way to do these commands; and this mode should be ported
        // to do the same thing.  It should also use the keymapper

        // We choose to rearrange the window and return to the previous order here.
        // If we just delete the buffer, it would have the same effect, but the editor
        // does not currently maintain a list of window orderings; so this is a second best for now.
        // TODO: Add window order tracking along with cursors for CTRL+i/o support
        auto *buffer = m_window.buffer;
        editor.activeTabWindow->RemoveWindow(&m_window);
        editor.activeTabWindow->SetActiveWindow(&m_launchWindow);
        editor.RemoveBuffer(buffer);
        return;
    }
    if (key == ImGuiKey_Enter) {
        OpenSelection(OpenType::Replace);
        return;
    }
    if (key == ImGuiKey_Backspace) {
        if (m_searchTerm.length() > 0) {
            m_searchTerm = m_searchTerm.substr(0, m_searchTerm.length() - 1);
            UpdateTree();
        }
    } else {
        if (ImGui::GetIO().KeyCtrl) {
            if (key == ImGuiKey_K || key == ImGuiKey_DownArrow) m_window.MoveCursorY(1);
            if (key == ImGuiKey_J || key == ImGuiKey_UpArrow) m_window.MoveCursorY(-1);
            if (key == ImGuiKey_V) {
                OpenSelection(OpenType::VSplit);
                return;
            }
            if (key == ImGuiKey_X) {
                OpenSelection(OpenType::HSplit);
                return;
            }
            if (key == ImGuiKey_T) {
                OpenSelection(OpenType::Tab);
                return;
            }
        } else if (key == ImGuiKey_DownArrow) {
            m_window.MoveCursorY(1);
        } else if (key == ImGuiKey_UpArrow) {
            m_window.MoveCursorY(-1);
        } else if (std::isgraph(key)) {
            // TODO: UTF8
            m_searchTerm += char(key);
            UpdateTree();
        }
    }

    std::ostringstream str;
    str << ">>> " << m_searchTerm;

    if (!m_indexTree.empty()) {
        str << " (" << m_indexTree[m_indexTree.size() - 1]->size() << " / " << m_indexTree[0]->size() << ")";
    }

    editor.SetCommandText(str.str());
}


void ZepMode_Search::Begin(ZepWindow *pWindow) {
    ZepMode::Begin(pWindow);

    m_searchTerm = "";
    editor.SetCommandText(">>> ");

    m_indexResult = Indexer::IndexPaths(editor, m_startPath);
    m_window.buffer->SetText("Indexing: " + m_startPath.string());

    fileSearchActive = true;
}

void ZepMode_Search::Notify(const std::shared_ptr<ZepMessage> &message) {
    ZepMode::Notify(message);
    if (message->messageId == Msg::Tick) {
        if (fileSearchActive) {
            if (!is_future_ready(m_indexResult)) return;

            fileSearchActive = false;

            m_filePaths = m_indexResult.get();
            if (!m_filePaths->errors.empty()) {
                editor.SetCommandText(m_filePaths->errors);
                return;
            }

            InitSearchTree();
            ShowTreeResult();
            UpdateTree();

            editor.RequestRefresh();
        }

        if (treeSearchActive) {
            UpdateTree();
        }
    }
}

void ZepMode_Search::InitSearchTree() {
    m_indexTree.clear();
    auto pInitSet = std::make_shared<IndexSet>();
    for (uint32_t i = 0; i < (uint32_t) m_filePaths->paths.size(); i++) {
        pInitSet->insert(std::make_pair(0, SearchResult{i, 0}));
    }
    m_indexTree.push_back(pInitSet);
}

void ZepMode_Search::ShowTreeResult() {
    std::ostringstream str;
    bool start = true;
    for (auto &index: *m_indexTree.back()) {
        if (!start) {
            str << std::endl;
        }
        str << m_filePaths->paths[index.second.index].string();
        start = false;
    }
    m_window.buffer->SetText(str.str());
    m_window.SetBufferCursor(m_window.buffer->Begin());
}

void ZepMode_Search::OpenSelection(OpenType type) {
    if (m_indexTree.empty()) return;

    auto cursor = m_window.GetBufferCursor();
    auto line = m_window.buffer->GetBufferLine(cursor);
    auto paths = m_indexTree[m_indexTree.size() - 1];
    auto *buffer = m_window.buffer;

    editor.activeTabWindow->SetActiveWindow(&m_launchWindow);

    long count = 0;
    for (auto &index: *m_indexTree.back()) {
        if (count == line) {
            auto path = m_filePaths->paths[index.second.index];
            auto full_path = m_filePaths->root / path;
            auto pBuffer = editor.GetFileBuffer(full_path, 0, true);
            if (pBuffer != nullptr) {
                switch (type) {
                    case OpenType::Replace: {
                        auto win = editor.FindBufferWindows(pBuffer);
                        // If they just hit enter, then jump to existing if possible.
                        if (!win.empty()) {
                            editor.SetCurrentTabWindow(&win[0]->tabWindow);
                            win[0]->tabWindow.SetActiveWindow(win[0]);
                        } else {
                            m_launchWindow.SetBuffer(pBuffer);
                        }
                    }
                        break;
                    case OpenType::VSplit:editor.activeTabWindow->AddWindow(pBuffer, &m_launchWindow, RegionLayoutType::HBox);
                        break;
                    case OpenType::HSplit:editor.activeTabWindow->AddWindow(pBuffer, &m_launchWindow, RegionLayoutType::VBox);
                        break;
                    case OpenType::Tab:editor.AddTabWindow()->AddWindow(pBuffer, nullptr, RegionLayoutType::HBox);
                        break;
                }
            }
        }
        count++;
    }

    // Removing the buffer will also kill this mode and its window; this is the last thing we can do here
    editor.RemoveBuffer(buffer);
}

void ZepMode_Search::UpdateTree() {
    if (fileSearchActive) return;

    if (treeSearchActive) {
        if (!is_future_ready(m_searchResult)) return;

        m_indexTree.push_back(m_searchResult.get());
        treeSearchActive = false;
    }

    // If the user is typing capitals, he cares about them in the search!
    m_caseImportant = string_tolower(m_searchTerm) != m_searchTerm;

    assert(!m_indexTree.empty());

    auto treeDepth = uint32_t(m_indexTree.size() - 1);
    if (m_searchTerm.size() < treeDepth) {
        while (m_searchTerm.size() < treeDepth) {
            m_indexTree.pop_back();
            treeDepth--;
        };
    } else if (m_searchTerm.size() > treeDepth) {
        std::shared_ptr<IndexSet> startSet;
        startSet = m_indexTree[m_indexTree.size() - 1];
        char startChar = m_searchTerm[m_indexTree.size() - 1];

        // Search for a match at the next level of the search tree
        m_searchResult = editor.threadPool->enqueue([&](const std::shared_ptr<IndexSet> &startSet, const char startChar) {
                auto result = std::make_shared<IndexSet>();
                for (auto &searchPair: *startSet) {
                    auto index = searchPair.second.index;
                    auto loc = searchPair.second.location;
                    auto dist = searchPair.first;

                    size_t pos;
                    if (m_caseImportant) {
                        auto str = m_filePaths->paths[index].string();
                        pos = str.find_first_of(startChar, loc);
                    } else {
                        auto str = m_filePaths->lowerPaths[index];
                        pos = str.find_first_of(startChar, loc);
                    }

                    if (pos != std::string::npos) {
                        // this approach 'clumps things together'
                        // It rewards more for strings of subsequent characters
                        uint32_t newDist = ((uint32_t) pos - loc);
                        newDist = dist == 0 ? 1 : newDist == 1 ? dist : dist + 1;
                        result->insert(std::make_pair(newDist, SearchResult{index, (uint32_t) pos}));
                    }
                }
                return result;
            },
            startSet, startChar);

        treeSearchActive = true;
    }

    ShowTreeResult();
    editor.RequestRefresh();
}

CursorType ZepMode_Search::GetCursorType() const { return CursorType::LineMarker; }

} // namespace Zep
