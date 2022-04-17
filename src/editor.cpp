#include "zep/editor.h"
#include "zep/filesystem.h"
#include "zep/mode_search.h"
#include "zep/mode_standard.h"
#include "zep/mode_tree.h"
#include "zep/regress.h"
#include "zep/syntax.h"
#include "zep/syntax_providers.h"
#include "zep/syntax_tree.h"
#include "zep/syntax_markdown.h"
#include "zep/tab_window.h"

#include "config_app.h"

#include <unordered_set>

namespace Zep {
#ifdef _DEBUG
Zep::ZLogger logger = {true, Zep::ZLT::DBG};
#else
Zep::ZLogger logger = { false, Zep::ZLT::INFO };
#endif
bool Zep::ZLog::disabled = false;
} // namespace Zep

namespace Zep {

ZepComponent::ZepComponent(ZepEditor &editor) : editor(editor) {
    this->editor.RegisterCallback(this);
}

ZepComponent::~ZepComponent() {
    editor.UnRegisterCallback(this);
}


void ZepEditor::RegisterSyntaxProvider(const std::vector<std::string> &mappings, const SyntaxProvider &provider) {
    for (auto &m: mappings) syntaxProviders[string_tolower(m)] = provider;
}

void ZepEditor::RegisterSyntaxProviders() {
    RegisterSyntaxProvider({".dsp"},
        {"faust", ([](auto *buffer) { return std::make_shared<ZepSyntax>(*buffer, faust_keywords, faust_identifiers); })});
    RegisterSyntaxProvider({".vert", ".frag"},
        {"gl_shader", ([](auto *buffer) { return std::make_shared<ZepSyntax>(*buffer, glsl_keywords, glsl_identifiers); })});
    RegisterSyntaxProvider({".hlsl", ".hlsli", ".vs", ".ps"},
        {"hlsl_shader", ([](auto *buffer) { return std::make_shared<ZepSyntax>(*buffer, hlsl_keywords, hlsl_identifiers); })});
    RegisterSyntaxProvider({".cpp", ".cxx", ".h", ".c"},
        {"cpp", ([](auto *buffer) { return std::make_shared<ZepSyntax>(*buffer, cpp_keywords, cpp_identifiers); })});
    RegisterSyntaxProvider(
        {".toml"},
        {"cpp", ([](auto *buffer) { return std::make_shared<ZepSyntax>(*buffer, toml_keywords, toml_identifiers, ZepSyntaxFlags::CaseInsensitive); })});
    RegisterSyntaxProvider(
        {".tree"},
        {"tree", ([](auto *buffer) { return std::make_shared<ZepSyntax_Tree>(*buffer, ZepSyntaxFlags::CaseInsensitive); })});
    RegisterSyntaxProvider(
        {".md", ".markdown"},
        {"markdown", ([](auto *buffer) { return std::make_shared<ZepSyntax_Markdown>(*buffer, ZepSyntaxFlags::CaseInsensitive); })});
}

ZepEditor::ZepEditor(ZepDisplay *pDisplay, const ZepPath &configRoot, uint32_t flags, IZepFileSystem *pFileSystem)
    : display(pDisplay), fileSystem(pFileSystem), flags(flags) {

    if (fileSystem == nullptr) fileSystem = new ZepFileSystemCPP(configRoot);

    threadPool = flags & ZepEditorFlags::DisableThreads ? std::make_unique<ThreadPool>(1) : std::make_unique<ThreadPool>();

    LoadConfig(fileSystem->GetConfigPath() / "zep.cfg");

    theme = std::make_shared<ZepTheme>();

    assert(display != nullptr);
    RegisterGlobalMode(std::make_shared<ZepMode_Vim>(*this));
    RegisterGlobalMode(std::make_shared<ZepMode_Standard>(*this));
    SetGlobalMode(ZepMode_Vim::StaticName());

    timer_restart(m_cursorTimer);
    timer_restart(m_lastEditTimer);
    commandLines.emplace_back("");

    RegisterSyntaxProviders();

    editorRegion = std::make_shared<Region>();
    editorRegion->layoutType = RegionLayoutType::VBox;

    tabRegion = std::make_shared<Region>();
    tabRegion->layoutType = RegionLayoutType::HBox;
    tabRegion->margin = NVec4f(0, textBorder, 0, textBorder);

    tabContentRegion = std::make_shared<Region>();
    commandRegion = std::make_shared<Region>();

    editorRegion->children.push_back(tabRegion);
    editorRegion->children.push_back(tabContentRegion);
    editorRegion->children.push_back(commandRegion);

#ifdef IMPLEMENTED_INDEXER
    m_indexer = std::make_shared<Indexer>(*this);
    m_indexer->StartIndexing();
#endif

    Reset();
}

ZepEditor::~ZepEditor() {
    std::for_each(tabWindows.begin(), tabWindows.end(), [](ZepTabWindow *w) { delete w; });
    tabWindows.clear();
    delete display;
    delete fileSystem;
}

void ZepEditor::OnFileChanged(const ZepPath &path) {
    if (path.filename() == "zep.cfg") {
        ZLOG(INFO, "Reloading config");
        LoadConfig(path);
        Broadcast(std::make_shared<ZepMessage>(Msg::ConfigChanged));
    }
}

// If you pass a valid path to a 'zep.cfg' file, then editor settings will serialize from that
// You can even edit it inside zep for immediate changes :)
void ZepEditor::LoadConfig(const ZepPath &config_path) {
    if (!fileSystem->Exists(config_path)) return;

    try {
        std::shared_ptr<cpptoml::table> spConfig;
        spConfig = cpptoml::parse_file(config_path.string());
        if (spConfig == nullptr) return;

        LoadConfig(spConfig);
    }
    catch (cpptoml::parse_exception &ex) {
        std::ostringstream str;
        str << config_path.filename().string() << " : Failed to parse. " << ex.what();
        SetCommandText(str.str());
    } catch (...) {
        std::ostringstream str;
        str << config_path.filename().string() << " : Failed to parse. ";
        SetCommandText(str.str());
    }
}

void ZepEditor::LoadConfig(const std::shared_ptr<cpptoml::table> &spConfig) {
    try {
        config.showNormalModeKeyStrokes = spConfig->get_qualified_as<bool>("editor.show_normal_mode_keystrokes").value_or(false);
        config.showIndicatorRegion = spConfig->get_qualified_as<bool>("editor.show_indicator_region").value_or(true);
        config.showLineNumbers = spConfig->get_qualified_as<bool>("editor.show_line_numbers").value_or(true);
        config.autoHideCommandRegion = spConfig->get_qualified_as<bool>("editor.autohide_command_region").value_or(false);
        config.cursorLineSolid = spConfig->get_qualified_as<bool>("editor.cursor_line_solid").value_or(true);
        config.backgroundFadeTime = (float) spConfig->get_qualified_as<double>("editor.background_fade_time").value_or(60.0f);
        config.backgroundFadeWait = (float) spConfig->get_qualified_as<double>("editor.background_fade_wait").value_or(60.0f);
        config.showScrollBar = spConfig->get_qualified_as<uint32_t>("editor.show_scrollbar").value_or(1);
        config.lineMargins.x = (float) spConfig->get_qualified_as<double>("editor.line_margin_top").value_or(1);
        config.lineMargins.y = (float) spConfig->get_qualified_as<double>("editor.line_margin_bottom").value_or(1);
        config.widgetMargins.x = (float) spConfig->get_qualified_as<double>("editor.widget_margin_top").value_or(1);
        config.widgetMargins.y = (float) spConfig->get_qualified_as<double>("editor.widget_margin_bottom").value_or(1);
        config.shortTabNames = spConfig->get_qualified_as<bool>("editor.short_tab_names").value_or(false);
        auto styleStr = string_tolower(spConfig->get_qualified_as<std::string>("editor.style").value_or("normal"));
        if (styleStr == "normal") {
            config.style = EditorStyle::Normal;
        } else if (styleStr == "minimal") {
            config.style = EditorStyle::Minimal;
        }
    } catch (...) {}
}

// TODO get rid of this
void ZepEditor::SaveConfig(const std::shared_ptr<cpptoml::table> &spConfig) {
    auto table = spConfig->get_table("editor");
    if (!table) {
        table = cpptoml::make_table();
        spConfig->insert("editor", table);
    }

    table->insert("show_normal_mode_keystrokes", config.showNormalModeKeyStrokes);
    table->insert("show_indicator_region", config.showIndicatorRegion);
    table->insert("show_line_numbers", config.showLineNumbers);
    table->insert("autohide_command_region", config.autoHideCommandRegion);
    table->insert("cursor_line_solid", config.cursorLineSolid);
    table->insert("short_tab_names", config.shortTabNames);
    table->insert("background_fade_time", (double) config.backgroundFadeTime);
    table->insert("background_fade_wait", (double) config.backgroundFadeWait);
    table->insert("show_scrollbar", config.showScrollBar);

    table->insert("line_margin_top", config.lineMargins.x);
    table->insert("line_margin_bottom", config.lineMargins.y);
    table->insert("widget_margin_top", config.widgetMargins.x);
    table->insert("widget_margin_bottom", config.widgetMargins.y);

    table->insert("style", config.style == EditorStyle::Minimal ? "minimal" : "normal");

    /*
    Example Write:
    std::ofstream stream("d:/dev/out.txt");
    cpptoml::toml_writer writer(stream, "");
    writer.visit(*spConfig);
    */
}

void ZepEditor::SaveBuffer(ZepBuffer &buffer) {
    // TODO:
    // - What if the buffer has no associated file?  Prompt for one.
    // - We don't check for outside modification yet either, meaning this could overwrite
    std::ostringstream text;
    if (buffer.HasFileFlags(FileFlags::ReadOnly)) text << "Failed to save, Read Only: " << buffer.GetDisplayName();
    else if (buffer.HasFileFlags(FileFlags::Locked)) text << "Failed to save, Locked: " << buffer.GetDisplayName();
    else if (buffer.filePath.empty()) text << "Error: No file name";
    else {
        int64_t size;
        if (!buffer.Save(size)) text << "Failed to save: " << buffer.GetDisplayName() << " at: " << buffer.filePath.string();
        else text << "Wrote " << buffer.filePath.string() << ", " << size << " bytes";
    }
    SetCommandText(text.str());
}

std::vector<ZepWindow *> ZepEditor::FindBufferWindows(const ZepBuffer *buffer) const {
    std::vector<ZepWindow *> bufferWindows;
    for (auto &tab: tabWindows) {
        for (auto &win: tab->GetWindows()) {
            if (win->buffer == buffer) {
                bufferWindows.push_back(win);
            }
        }
    }
    return bufferWindows;
}

void ZepEditor::RemoveBuffer(ZepBuffer *buffer) {
    auto bufferWindows = FindBufferWindows(buffer);
    for (auto &window: bufferWindows) {
        window->tabWindow.RemoveWindow(window);
    }

    // Find the buffer in the list of buffers owned by the editor and remove it
    auto itr = std::find_if(buffers.begin(), buffers.end(), [buffer](const auto &spBuffer) {
        return spBuffer.get() == buffer;
    });

    if (itr != buffers.end()) {
        buffers.erase(itr);
    }
}

ZepBuffer *ZepEditor::GetEmptyBuffer(const std::string &name, uint32_t fileFlags) {
    auto buffer = CreateNewBuffer(name);
    buffer->SetFileFlags(fileFlags);
    return buffer;
}

ZepBuffer *ZepEditor::GetFileBuffer(const ZepPath &filePath, uint32_t fileFlags, bool create) {
    auto path = fileSystem->Exists(filePath) ? fileSystem->Canonical(filePath) : filePath;
    if (!path.empty()) {
        for (auto &pBuffer: buffers) {
            if (!pBuffer->filePath.empty() && fileSystem->Equivalent(pBuffer->filePath, path)) {
                return pBuffer.get();
            }
        }
    }

    if (!create) return nullptr;

    // Create buffer, try to load even if not present, the buffer represents the save path (it just isn't saved yet)
    auto buffer = CreateNewBuffer(filePath);
    buffer->SetFileFlags(fileFlags);
    return buffer;
}

ZepWindow *ZepEditor::AddTree() {
    auto tree = GetEmptyBuffer("Tree.tree", FileFlags::Locked | FileFlags::ReadOnly);
    auto treeWindow = activeTabWindow->AddWindow(tree, nullptr, RegionLayoutType::HBox);

    auto activeWindow = activeTabWindow->GetActiveWindow();
    activeWindow->SetBuffer(tree);

    auto treeModel = std::make_shared<ZepFileTree>();
    auto root = treeModel->root;

    root->AddChild(std::make_shared<ZepFileNode>("Child1", ZepTreeNodeFlags::IsFolder));
    auto child2 = root->AddChild(std::make_shared<ZepFileNode>("Child2", ZepTreeNodeFlags::IsFolder));
    child2->AddChild(std::make_shared<ZepFileNode>("Child2_1"));

    root->ExpandAll(true);

    auto mode = std::make_shared<ZepMode_Tree>(*this, treeModel, *activeWindow, *treeWindow);
    tree->SetMode(mode);
    mode->Begin(activeWindow);
    return activeWindow;
}

ZepWindow *ZepEditor::AddSearch() {
    if (!activeTabWindow) return nullptr;

    static std::unordered_set<std::string> search_keywords = {};
    static std::unordered_set<std::string> search_identifiers = {};

    auto *searchBuffer = GetEmptyBuffer("Search", FileFlags::Locked | FileFlags::ReadOnly);
    searchBuffer->type = BufferType::Search;
    searchBuffer->syntax = std::make_shared<ZepSyntax>(*searchBuffer, search_keywords, search_identifiers, ZepSyntaxFlags::CaseInsensitive);

    auto *activeWindow = activeTabWindow->GetActiveWindow();
    bool hasGit = false;
    const auto &searchPath = fileSystem->GetSearchRoot(activeWindow->buffer->filePath, hasGit);
    auto pSearchWindow = activeTabWindow->AddWindow(searchBuffer, nullptr, RegionLayoutType::VBox);
    pSearchWindow->SetWindowFlags(pSearchWindow->GetWindowFlags() | WindowFlags::Modal);

    auto mode = std::make_shared<ZepMode_Search>(*this, *activeWindow, *pSearchWindow, searchPath);
    searchBuffer->SetMode(mode);
    mode->Begin(pSearchWindow);
    return pSearchWindow;
}

ZepTabWindow *ZepEditor::EnsureTab() {
    return tabWindows.empty() ? AddTabWindow() : activeTabWindow ? activeTabWindow : tabWindows[0];
}

// Reset editor to start state; with a single tab, a single window and an empty unmodified buffer
void ZepEditor::Reset() {
    EnsureTab();
}

// TODO fix for directory startup; it won't work
ZepBuffer *ZepEditor::InitWithFileOrDir(const std::string &str) {
    ZepPath startPath(str);

    if (fileSystem->Exists(startPath)) {
        startPath = fileSystem->Canonical(startPath);
        // If a directory, just return the default already created buffer.
        if (fileSystem->IsDirectory(startPath)) {
            // Remember the working directory 
            fileSystem->SetWorkingDirectory(startPath);
            return activeTabWindow->GetActiveWindow()->buffer;
        }
        // Try to get the working directory from the parent path of the passed file
        auto parentDir = startPath.parent_path();
        if (fileSystem->Exists(parentDir) && fileSystem->IsDirectory(parentDir)) {
            fileSystem->SetWorkingDirectory(startPath.parent_path());
        }
    }

    // Get a buffer for the start file; even if the path is not valid; it can be created but not saved
    auto fileBuffer = GetFileBuffer(startPath);
    auto tab = EnsureTab();
    tab->AddWindow(fileBuffer, nullptr, RegionLayoutType::HBox);

    return fileBuffer;
}

ZepBuffer *ZepEditor::InitWithText(const std::string &strName, const std::string &strText) {
    auto tab = EnsureTab();
    auto buffer = GetEmptyBuffer(strName);
    buffer->SetText(strText);
    tab->AddWindow(buffer, nullptr, RegionLayoutType::HBox);

    return buffer;
}

// Here we ensure that the editor is in a valid state, and cleanup Default buffers
void ZepEditor::UpdateWindowState() {
    // If there is no active tab window, and we have one, set it.
    if (!activeTabWindow && !tabWindows.empty()) {
        SetCurrentTabWindow(tabWindows.back());
    }

    // If the tab window doesn't contain an active window, and there is one, set it
    if (activeTabWindow && !activeTabWindow->GetActiveWindow()) {
        if (!activeTabWindow->GetWindows().empty()) {
            activeTabWindow->SetActiveWindow(activeTabWindow->GetWindows().back());
            m_bRegionsChanged = true;
        }
    }

    // Clean up any default buffers
    std::vector<ZepBuffer *> victims;
    for (auto &buffer: buffers) {
        if (!buffer->HasFileFlags(FileFlags::DefaultBuffer) || buffer->HasFileFlags(FileFlags::Dirty)) {
            continue;
        }

        auto windows = FindBufferWindows(buffer.get());
        if (windows.empty()) {
            victims.push_back(buffer.get());
        }
    }

    for (auto &victim: victims) {
        RemoveBuffer(victim);
    }

    // If the display says we need a layout update, force it on all the windows
    if (display->layoutDirty) {
        for (auto &tabWindow: tabWindows) {
            for (auto &window: tabWindow->GetWindows()) {
                window->DirtyLayout();
            }
        }
        display->layoutDirty = false;
    }
}

void ZepEditor::ResetCursorTimer() { timer_restart(m_cursorTimer); }
void ZepEditor::ResetLastEditTimer() { timer_restart(m_lastEditTimer); }
float ZepEditor::GetLastEditElapsedTime() const { return (float) timer_get_elapsed_seconds(m_lastEditTimer); }

void ZepEditor::NextTabWindow() {
    auto itr = std::find(tabWindows.begin(), tabWindows.end(), activeTabWindow);
    if (itr != tabWindows.end()) itr++;
    if (itr == tabWindows.end()) itr = tabWindows.end() - 1;
    SetCurrentTabWindow(*itr);
}

void ZepEditor::PreviousTabWindow() {
    auto itr = std::find(tabWindows.begin(), tabWindows.end(), activeTabWindow);
    if (itr == tabWindows.end()) return;

    if (itr != tabWindows.begin()) itr--;
    SetCurrentTabWindow(*itr);
}

void ZepEditor::SetCurrentTabWindow(ZepTabWindow *pTabWindow) {
    // Sanity
    auto itr = std::find(tabWindows.begin(), tabWindows.end(), pTabWindow);
    if (itr != tabWindows.end()) {
        activeTabWindow = pTabWindow;
        // Force a reactivation of the active window to ensure buffer setup is correct
        activeTabWindow->SetActiveWindow(activeTabWindow->GetActiveWindow());
    }
}

void ZepEditor::UpdateTabs() const {
    tabRegion->children.clear();
    if (tabWindows.size() > 1) {
        // Tab region
        for (auto &window: tabWindows) {
            if (window->GetActiveWindow() == nullptr) continue;

            // Show active buffer in tab as tab name
            auto *buffer = window->GetActiveWindow()->buffer;
            auto name = buffer->name;
            if (config.shortTabNames) {
                auto pos = name.find_last_of('.');
                if (pos != std::string::npos) {
                    name = name.substr(0, pos);
                }
            }

            auto tabColor = theme->GetColor(ThemeColor::TabActive);
            if (buffer->HasFileFlags(FileFlags::HasWarnings)) {
                tabColor = theme->GetColor(ThemeColor::Warning);
            }

            // Errors win for coloring
            if (buffer->HasFileFlags(FileFlags::HasErrors)) {
                tabColor = theme->GetColor(ThemeColor::Error);
            }

            if (window != activeTabWindow) {
                // Desaturate unselected ones
                tabColor = tabColor * .55f;
                tabColor.w = 1.0f;
            }

            auto tabLength = display->GetFont(ZepTextType::Text).GetTextSize((const uint8_t *) name.c_str()).x + DpiX(textBorder) * 2;
            auto tabRegionTab = std::make_shared<TabRegionTab>();
            tabRegionTab->color = tabColor;
            tabRegionTab->name = name;
            tabRegionTab->pTabWindow = window;
            tabRegionTab->fixed_size = NVec2f(tabLength, 0.0f);
            tabRegionTab->layoutType = RegionLayoutType::HBox;
            tabRegionTab->padding = Dpi(NVec2f(textBorder, textBorder));
            tabRegionTab->flags = RegionFlags::Fixed;
            tabRegion->children.push_back(tabRegionTab);
            tabRegionTab->pParent = tabRegion.get();
        }
    }

    LayoutRegion(*tabRegion);
}

ZepTabWindow *ZepEditor::AddTabWindow() {
    auto pTabWindow = new ZepTabWindow(*this);
    tabWindows.push_back(pTabWindow);
    activeTabWindow = pTabWindow;

    auto pEmpty = GetEmptyBuffer("[No ExCommandName]", FileFlags::DefaultBuffer);
    pTabWindow->AddWindow(pEmpty, nullptr, RegionLayoutType::HBox);

    return pTabWindow;
}

void ZepEditor::RequestQuit() {
    Broadcast(std::make_shared<ZepMessage>(Msg::RequestQuit, "RequestQuit"));
}

void ZepEditor::RemoveTabWindow(ZepTabWindow *tabWindow) {
    assert(tabWindow);
    if (!tabWindow) return;

    auto itrFound = std::find(tabWindows.begin(), tabWindows.end(), tabWindow);
    if (itrFound == tabWindows.end()) assert(!"Not found?");

    delete tabWindow;
    tabWindows.erase(itrFound);

    if (tabWindows.empty()) {
        activeTabWindow = nullptr;
        // Reset the window state, but request a quit
        Reset();
        RequestQuit();
    } else {
        if (activeTabWindow == tabWindow) {
            activeTabWindow = tabWindows[tabWindows.size() - 1];
            // Force a reset of active to initialize the mode
            activeTabWindow->SetActiveWindow(activeTabWindow->GetActiveWindow());
        }
    }
}

void ZepEditor::RegisterGlobalMode(const std::shared_ptr<ZepMode> &spMode) {
    m_mapGlobalModes[spMode->Name()] = spMode;
    spMode->Init();
}

void ZepEditor::RegisterExCommand(const std::shared_ptr<ZepExCommand> &command) {
    m_mapExCommands[command->ExCommandName()] = command;
}

ZepExCommand *ZepEditor::FindExCommand(const std::string &commandName) {
    auto itr = m_mapExCommands.find(commandName);
    return itr != m_mapExCommands.end() ? itr->second.get() : nullptr;
}

ZepExCommand *ZepEditor::FindExCommand(const StringId &Id) {
    if (Id.id == 0) return nullptr;

    for (auto &[name, pEx]: m_mapExCommands) {
        if (pEx->ExCommandId() == Id) return pEx.get();
    }

    return nullptr;
}

void ZepEditor::RegisterBufferMode(const std::string &extension, const std::shared_ptr<ZepMode> &mode) {
    m_mapBufferModes[extension] = mode;
    mode->Init();
}

void ZepEditor::SetGlobalMode(const std::string &currentMode) {
    auto itrMode = m_mapGlobalModes.find(currentMode);
    if (itrMode != m_mapGlobalModes.end()) {
        auto *window = m_pCurrentMode ? m_pCurrentMode->currentWindow : nullptr;
        m_pCurrentMode = itrMode->second.get();
        if (window) m_pCurrentMode->Begin(window);
    }
}

ZepMode *ZepEditor::GetGlobalMode() {
    // The 'Mode' is typically vim or normal and determines how editing is done in a panel
    if (!m_pCurrentMode && !m_mapGlobalModes.empty()) {
        m_pCurrentMode = m_mapGlobalModes.begin()->second.get();
    }

    return m_pCurrentMode;
}

void ZepEditor::SetBufferMode(ZepBuffer &buffer) const {
    // Reset it in case we are changing the text in a buffer
    buffer.SetMode(nullptr);

    // TODO DRY
    std::string ext;
    std::string fileName;
    if (buffer.filePath.has_filename() && buffer.filePath.filename().has_extension()) {
        ext = string_tolower(buffer.filePath.filename().extension().string());
        fileName = string_tolower(buffer.filePath.filename().string());
    } else {
        const auto &bufferName = buffer.name;
        size_t dot_pos = bufferName.find_last_of('.');
        if (dot_pos != std::string::npos) {
            ext = string_tolower(bufferName.substr(dot_pos, bufferName.length() - dot_pos));
        }
    }

    auto itr = m_mapBufferModes.find(ext);
    if (itr != m_mapBufferModes.end()) {
        buffer.SetMode(itr->second);
    }
}

void ZepEditor::SetBufferSyntax(ZepBuffer &buffer) const {
    std::string ext;
    std::string fileName;
    if (buffer.filePath.has_filename() && buffer.filePath.filename().has_extension()) {
        ext = string_tolower(buffer.filePath.filename().extension().string());
        fileName = string_tolower(buffer.filePath.filename().string());
    } else {
        const auto &bufferName = buffer.name;
        size_t dot_pos = bufferName.find_last_of('.');
        if (dot_pos != std::string::npos) {
            ext = string_tolower(bufferName.substr(dot_pos, bufferName.length() - dot_pos));
        }
    }

    // first check file name
    if (!fileName.empty()) {
        auto itr = syntaxProviders.find(fileName);
        if (itr != syntaxProviders.end()) {
            buffer.SetSyntaxProvider(itr->second);
            return;
        }
    }

    auto itr = syntaxProviders.find(ext);
    if (itr != syntaxProviders.end()) {
        buffer.SetSyntaxProvider(itr->second);
    } else {
        itr = syntaxProviders.find(string_tolower(buffer.name));
        buffer.SetSyntaxProvider(itr != syntaxProviders.end() ? itr->second : SyntaxProvider{});
    }
}

float ZepEditor::DpiX(float value) const { return display->pixelScale.x * value; }
float ZepEditor::DpiY(float value) const { return display->pixelScale.y * value; }
NVec2f ZepEditor::Dpi(NVec2f value) const { return value * display->pixelScale; }
NRectf ZepEditor::Dpi(NRectf value) const { return value * display->pixelScale; }

// Inform clients of an event in the buffer
bool ZepEditor::Broadcast(const std::shared_ptr<ZepMessage> &message) {
    Notify(message);
    if (message->handled) return true;

    for (auto &client: notifyClients) {
        client->Notify(message);
        if (message->handled) break;
    }
    return message->handled;
}

// Do any special buffer processing
void ZepEditor::InitBuffer(ZepBuffer &buffer) const { SetBufferMode(buffer); }

ZepBuffer *ZepEditor::CreateNewBuffer(const std::string &str) {
    auto pBuffer = std::make_shared<ZepBuffer>(*this, str);
    // For a new buffer, set the syntax based on the string name
    SetBufferSyntax(*pBuffer);
    buffers.push_front(pBuffer);

    InitBuffer(*pBuffer);
    return pBuffer.get();
}

ZepBuffer *ZepEditor::CreateNewBuffer(const ZepPath &path) {
    auto buffer = std::make_shared<ZepBuffer>(*this, path);
    buffers.push_front(buffer);
    InitBuffer(*buffer);
    return buffer.get();
}

ZepBuffer *ZepEditor::GetMRUBuffer() const { return buffers.front().get(); }

void ZepEditor::ReadClipboard() {
    auto msg = std::make_shared<ZepMessage>(Msg::GetClipBoard);
    Broadcast(msg);
    if (msg->handled) {
        m_registers["+"] = msg->str;
        m_registers["*"] = msg->str;
        m_registers["\""] = msg->str;
    }
}

void ZepEditor::WriteClipboard() {
    auto msg = std::make_shared<ZepMessage>(Msg::SetClipBoard);
    msg->str = m_registers["+"].text;
    Broadcast(msg);
}

void ZepEditor::SetRegister(const char reg, const Register &val) {
    std::string str({reg});
    m_registers[str] = val;
    if (reg == '+' || reg == '*') WriteClipboard();
}

void ZepEditor::SetRegister(const char reg, const char *text) {
    std::string str({reg});
    m_registers[str] = Register(text);
    if (reg == '+' || reg == '*') WriteClipboard();
}

Register &ZepEditor::GetRegister(const char reg) {
    if (reg == '+' || reg == '*') ReadClipboard();
    std::string str({reg});
    return m_registers[str];
}
const std::map<std::string, Register> &ZepEditor::GetRegisters() {
    ReadClipboard();
    return m_registers;
}

void ZepEditor::Notify(const std::shared_ptr<ZepMessage> &msg) {
    if (msg->messageId != Msg::MouseDown) return;

    for (auto &tab: tabRegion->children) {
        if (tab->rect.Contains(msg->pos)) {
            auto pTabRegionTab = std::static_pointer_cast<TabRegionTab>(tab);
            SetCurrentTabWindow(pTabRegionTab->pTabWindow);
        }
    }
}

std::string ZepEditor::GetCommandText() const {
    std::ostringstream str;
    bool start = true;
    for (auto &line: commandLines) {
        if (!start) str << "\n";
        start = false;
        str << line;
    }
    return str.str();
}

void ZepEditor::SetCommandText(const std::string &command) {
    commandLines = string_split(command, "\n\r");
    if (commandLines.empty()) commandLines.emplace_back("");

    m_bRegionsChanged = true;
}

void ZepEditor::RequestRefresh() { m_bPendingRefresh = true; }

bool ZepEditor::RefreshRequired() {
    auto lastBlink = m_lastCursorBlink;
    if (m_bPendingRefresh || lastBlink != GetCursorBlinkState()) {
        if (!(flags & ZepEditorFlags::FastUpdate)) m_bPendingRefresh = false;
        return true;
    }

    return false;
}

bool ZepEditor::GetCursorBlinkState() const {
    m_lastCursorBlink = (int(timer_get_elapsed_seconds(m_cursorTimer) * 1.75f) & 1) ? true : false;
    return m_lastCursorBlink;
}

void ZepEditor::SetDisplayRegion(const NRectf &rect) const {
    editorRegion->rect = rect;
    UpdateSize();
}

void ZepEditor::UpdateSize() const {
    auto &uiFont = display->GetFont(ZepTextType::UI);
    auto commandCount = commandLines.size();
    const float commandSize = uiFont.GetPixelHeight() * commandCount + DpiX(textBorder) * 2.0f;
    auto displaySize = editorRegion->rect.Size();

    // Regions
    commandRegion->fixed_size = NVec2f(0.0f, commandSize);
    commandRegion->flags = RegionFlags::Fixed;

    // Add tabs for extra windows
    if (tabWindows.size() > 1) {
        tabRegion->fixed_size = NVec2f(0.0f, uiFont.GetPixelHeight() + DpiX(textBorder) * 2);
        tabRegion->flags = RegionFlags::Fixed;
    } else {
        tabRegion->fixed_size = NVec2f(0.0f);
        tabRegion->flags = RegionFlags::Fixed;
    }

    tabContentRegion->flags = RegionFlags::Expanding;

    LayoutRegion(*editorRegion);

    if (activeTabWindow) {
        activeTabWindow->SetDisplayRegion(tabContentRegion->rect);
    }
}

void ZepEditor::Display() {
    // Allow any components to update themselves
    Broadcast(std::make_shared<ZepMessage>(Msg::Tick));

    UpdateWindowState();

    if (m_bRegionsChanged) {
        m_bRegionsChanged = false;
        UpdateSize();
    }

    // Command plus output
    long commandCount = long(commandLines.size());

    auto &uiFont = display->GetFont(ZepTextType::UI);
    const float commandSize = uiFont.GetPixelHeight() * commandCount + DpiX(textBorder) * 2.0f;

    auto displaySize = editorRegion->rect.Size();

    auto commandSpace = commandCount;
    commandSpace = std::max(commandCount, 0l);

    // This fill will effectively fill the region around the tabs in Normal mode
    if (config.style == EditorStyle::Normal) {
        display->DrawRectFilled(editorRegion->rect, theme->GetColor(ThemeColor::Background));
    }

    // Background rect for CommandLine
    if (!GetCommandText().empty() || (config.autoHideCommandRegion == false)) {
        display->DrawRectFilled(commandRegion->rect, theme->GetColor(ThemeColor::Background));
    }

    // Draw command text
    auto screenPosYPx = commandRegion->rect.topLeftPx + NVec2f(0.0f, DpiX(textBorder));
    for (int i = 0; i < commandSpace; i++) {
        if (!commandLines[i].empty()) {
            auto textSize = uiFont.GetTextSize((const uint8_t *) commandLines[i].c_str(), (const uint8_t *) commandLines[i].c_str() + commandLines[i].size());
            display->DrawChars(uiFont, screenPosYPx, theme->GetColor(ThemeColor::Text), (const uint8_t *) commandLines[i].c_str());
        }

        screenPosYPx.y += uiFont.GetPixelHeight();
        screenPosYPx.x = commandRegion->rect.topLeftPx.x;
    }

    if (config.style == EditorStyle::Normal) {
        // A line along the bottom of the tab region
        display->DrawRectFilled(
            NRectf(
                NVec2f(tabRegion->rect.Left(), tabRegion->rect.Bottom() - DpiY(1)),
                NVec2f(tabRegion->rect.Right(), tabRegion->rect.Bottom())
            ),
            theme->GetColor(ThemeColor::TabInactive)
        );
    }

    // Figure out the active region
    auto pActiveTabWindow = activeTabWindow;
    NRectf tabRect;
    for (auto &tab: tabRegion->children) {
        if (std::static_pointer_cast<TabRegionTab>(tab)->pTabWindow == pActiveTabWindow) {
            tabRect = tab->rect;
            break;
        }
    }

    // Figure out the virtual vs real page size of the tabs
    float virtualSize = 0.0f;
    float tabRegionSize = tabRegion->rect.Width();
    if (!tabRegion->children.empty()) {
        virtualSize = tabRegion->children.back()->rect.Right();
    }

    // Move the tab bar origin if appropriate
    if (tabRect.Width() != 0.0f) {
        if ((tabRect.Left() - tabRect.Width() + m_tabOffsetX) < tabRegion->rect.Left()) {
            m_tabOffsetX += tabRegion->rect.Left() - (tabRect.Left() + m_tabOffsetX - tabRect.Width());
        } else if ((tabRect.Right() + m_tabOffsetX + tabRect.Width()) > tabRegion->rect.Right()) {
            m_tabOffsetX -= (tabRect.Right() + m_tabOffsetX - tabRegion->rect.Right() + tabRect.Width());
        }
    }

    // Clamp it
    m_tabOffsetX = std::min(m_tabOffsetX, 0.0f);
    m_tabOffsetX = std::max(std::min(tabRegionSize - virtualSize, 0.0f), m_tabOffsetX);

    // Now display the tabs
    for (auto &tab: tabRegion->children) {
        auto spTabRegionTab = std::static_pointer_cast<TabRegionTab>(tab);

        auto rc = spTabRegionTab->rect;
        rc.Adjust(m_tabOffsetX, 0.0f);

        // Tab background rect
        display->DrawRectFilled(rc, spTabRegionTab->color);

        auto lum = Luminosity(spTabRegionTab->color);
        auto textCol = NVec4f(1.0f);
        if (lum > .5f) {
            textCol.x = 0.0f;
            textCol.y = 0.0f;
            textCol.z = 0.0f;
        }

        // Tab text
        display->DrawChars(uiFont, rc.topLeftPx + Dpi(NVec2f(textBorder, 0.0f)), textCol, (const uint8_t *) spTabRegionTab->name.c_str());
    }

    if (activeTabWindow) activeTabWindow->Display();
} // namespace Zep

bool ZepEditor::OnMouseMove(const NVec2f &pos) {
    mousePos = pos;
    bool handled = Broadcast(std::make_shared<ZepMessage>(Msg::MouseMove, pos));
    m_bPendingRefresh = true;
    return handled;
}

bool ZepEditor::OnMouseDown(const NVec2f &pos, ZepMouseButton button) {
    mousePos = pos;
    bool handled = Broadcast(std::make_shared<ZepMessage>(Msg::MouseDown, pos, button));
    m_bPendingRefresh = true;
    return handled;
}

bool ZepEditor::OnMouseUp(const NVec2f &pos, ZepMouseButton button) {
    mousePos = pos;
    bool handled = Broadcast(std::make_shared<ZepMessage>(Msg::MouseUp, pos, button));
    m_bPendingRefresh = true;
    return handled;
}

void ZepEditor::SetFlags(uint32_t newFlags) {
    flags = newFlags;
    if ((flags & ZepEditorFlags::FastUpdate)) RequestRefresh();
}

std::vector<const KeyMap *> ZepEditor::GetGlobalKeyMaps(ZepMode &mode) {
    std::vector<const KeyMap *> maps;
    for (auto &[id, spMap]: m_mapExCommands) {
        auto pMap = spMap->GetKeyMappings(mode);
        if (pMap) maps.push_back(pMap);
    }
    return maps;
}

} // namespace Zep
