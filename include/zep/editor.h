#pragma once

#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "zep/mcommon/math/math.h"
#include "zep/mcommon/animation/timer.h"
#include "zep/mcommon/threadpool.h"
#include "zep/mcommon/file/path.h"
#include "zep/mcommon/file/cpptoml.h"

#include "zep/keymap.h"

#include "splits.h"

// Basic Architecture

// Editor
//      Buffers
//      Modes -> (Active BufferRegion)
// Display
//      BufferRegions (->Buffers)
//
// A buffer is just an array of chars in a gap buffer, with simple operations to insert, delete and search
// A display is something that can display a collection of regions and the editor controls in a window
// A buffer region is a single view onto a buffer inside the main display
//
// The editor has a list of ZepBuffers.
// The editor has different editor modes (vim/standard)
// ZepDisplay can render the editor (with imgui or something else)
// The display has multiple BufferRegions, each a window onto a buffer.
// Multiple regions can refer to the same buffer (N Regions : N Buffers)
// The Modes receive key presses and act on a buffer region
namespace Zep {

class ZepBuffer;
class ZepMode;
class ZepMode_Vim;
class ZepMode_Standard;
class ZepEditor;
class ZepSyntax;
class ZepTabWindow;
class ZepWindow;
class ZepTheme;
class ZepDisplay;
class IZepFileSystem;
class Indexer;

struct Region;

#define ZEP_UNUSED(var) (void)(var);

// Helpers 
inline uint32_t ZSetFlags(const uint32_t &flags, uint32_t value, bool set = true) { return set ? flags | value : flags; }
inline uint32_t ZClearFlags(const uint32_t &flags, uint32_t value) { return flags & ~value; }

namespace ZepEditorFlags {
enum {
    None = (0),
    DisableThreads = (1 << 0),
    FastUpdate = (1 << 1)
};
};

enum class ZepMouseButton {
    Left,
    Middle,
    Right,
    Unknown
};

enum class Msg {
    HandleCommand,
    RequestQuit,
    GetClipBoard,
    SetClipBoard,
    MouseMove,
    MouseDown,
    MouseUp,
    Buffer,
    ComponentChanged,
    Tick,
    ConfigChanged,
    ToolTip
};

struct IZepComponent;
class ZepMessage {
public:
    explicit ZepMessage(Msg id, std::string strIn = std::string()) : messageId(id), str(std::move(strIn)) {}
    ZepMessage(Msg id, const NVec2f &p, ZepMouseButton b = ZepMouseButton::Unknown) : messageId(id), pos(p), button(b) {}
    ZepMessage(Msg id, IZepComponent *pComp) : messageId(id), pComponent(pComp) {}

    Msg messageId; // Message ID
    std::string str;       // Generic string for simple messages
    bool handled = false;  // If the message was handled
    NVec2f pos;
    ZepMouseButton button = ZepMouseButton::Unknown;
    IZepComponent *pComponent = nullptr;
};

struct IZepComponent {
    virtual void Notify(const std::shared_ptr<ZepMessage> &message) { ZEP_UNUSED(message); };
};

class ZepComponent : public IZepComponent {
public:
    explicit ZepComponent(ZepEditor &editor);
    virtual ~ZepComponent();

    ZepEditor &editor;
};

// Registers are used by the editor to store/retrieve text fragments
struct Register {
    Register() : lineWise(false) {}

    explicit Register(const char *ch, bool lw = false) : text(ch), lineWise(lw) {}
    explicit Register(uint8_t *ch, bool lw = false) : text((const char *) ch), lineWise(lw) {}
    Register(std::string str, bool lw = false) : text(std::move(str)), lineWise(lw) {}

    std::string text;
    bool lineWise = false;
};

struct SyntaxProvider {
    std::string syntaxID;
    std::function<std::shared_ptr<ZepSyntax>(ZepBuffer *)> factory = nullptr;
};

const float bottomBorder = 2.0f;
const float textBorder = 2.0f;
const float tabSpacing = 1.0f;
const float leftBorderChars = 3;

enum class EditorStyle { Normal = 0, Minimal };

struct EditorConfig {
    uint32_t showScrollBar = 1;
    EditorStyle style{EditorStyle::Normal};
    NVec2f lineMargins{1.0f};
    NVec2f widgetMargins{1.0f};
    NVec2f inlineWidgetMargins{2.0f};
    float underlineHeight = 3.0f;
    bool showLineNumbers = true;
    bool shortTabNames = true;
    bool showIndicatorRegion = true;
    bool autoHideCommandRegion = false;
    bool cursorLineSolid = false;
    bool showNormalModeKeyStrokes = false;
    float backgroundFadeTime = 60.0f;
    float backgroundFadeWait = 60.0f;
};

class ZepExCommand : public ZepComponent {
public:
    explicit ZepExCommand(ZepEditor &editor) : ZepComponent(editor) {}
    ~ZepExCommand() override = default;
    virtual void Run(const std::vector<std::string> &args = {}) = 0;
    virtual const char *ExCommandName() const = 0;
    virtual StringId ExCommandId() const { return StringId(ExCommandName()); }
    virtual void Init() {};
    virtual const KeyMap *GetKeyMappings(ZepMode &) const { return nullptr; };
};

struct TabRegionTab : public Region {
    NVec4f color;
    std::string name;
    ZepTabWindow *pTabWindow = nullptr;
};


class ZepEditor {
public:
    // Root path is the path to search for a config file
    ZepEditor(ZepDisplay *pDisplay, const ZepPath &root, uint32_t flags = 0, IZepFileSystem *pFileSystem = nullptr);
    ~ZepEditor();

    void LoadConfig(const ZepPath &config_path);
    void LoadConfig(const std::shared_ptr<cpptoml::table> &spConfig);
    void SaveConfig(const std::shared_ptr<cpptoml::table> &spConfig);
    void RequestQuit();

    void Reset();
    ZepBuffer *InitWithFileOrDir(const std::string &str);
    ZepBuffer *InitWithText(const std::string &strName, const std::string &strText);

    ZepMode *GetGlobalMode();
    void RegisterGlobalMode(const std::shared_ptr<ZepMode> &spMode);
    void RegisterExCommand(const std::shared_ptr<ZepExCommand> &command);
    ZepExCommand *FindExCommand(const std::string &commandName);
    ZepExCommand *FindExCommand(const StringId &strName);
    void SetGlobalMode(const std::string &currentMode);
    std::vector<const KeyMap *> GetGlobalKeyMaps(ZepMode &mode);

    void RegisterBufferMode(const std::string &strExtension, const std::shared_ptr<ZepMode> &mode);

    void Display();

    bool Broadcast(const std::shared_ptr<ZepMessage> &payload);
    void RegisterCallback(IZepComponent *pClient) { m_notifyClients.insert(pClient); }
    void UnRegisterCallback(IZepComponent *pClient) { m_notifyClients.erase(pClient); }

    ZepBuffer *GetMRUBuffer() const;
    void SaveBuffer(ZepBuffer &buffer);
    ZepBuffer *GetFileBuffer(const ZepPath &filePath, uint32_t fileFlags = 0, bool create = true);
    ZepBuffer *GetEmptyBuffer(const std::string &name, uint32_t fileFlags = 0);
    void RemoveBuffer(ZepBuffer *buffer);
    std::vector<ZepWindow *> FindBufferWindows(const ZepBuffer *buffer) const;

    void SetRegister(char reg, const Register &val);
    void SetRegister(char reg, const char *text);
    Register &GetRegister(const std::string &reg);
    Register &GetRegister(char reg);
    const std::map<std::string, Register> &GetRegisters();

    void ReadClipboard();
    void WriteClipboard();

    void Notify(const std::shared_ptr<ZepMessage> &msg);
    void SetFlags(uint32_t flags);

    void NextTabWindow();
    void PreviousTabWindow();
    void SetCurrentTabWindow(ZepTabWindow *pTabWindow);
    ZepTabWindow *AddTabWindow();
    void RemoveTabWindow(ZepTabWindow *pTabWindow);

    void UpdateTabs() const;

    ZepWindow *AddTree();
    ZepWindow *AddSearch();

    void ResetCursorTimer();
    bool GetCursorBlinkState() const;

    void ResetLastEditTimer();
    float GetLastEditElapsedTime() const;

    void RequestRefresh();
    bool RefreshRequired();

    void SetCommandText(const std::string &command);

    std::string GetCommandText() const;

    void UpdateWindowState();

    void SetDisplayRegion(const NRectf &rect);
    void UpdateSize();

    bool OnMouseMove(const NVec2f &pos);
    bool OnMouseDown(const NVec2f &pos, ZepMouseButton button);
    bool OnMouseUp(const NVec2f &pos, ZepMouseButton button);

    void SetBufferSyntax(ZepBuffer &buffer) const;
    void SetBufferMode(ZepBuffer &buffer) const;

    float DpiX(float value) const;
    float DpiY(float value) const;
    NVec2f Dpi(NVec2f value) const;
    NRectf Dpi(NRectf value) const;

    // Used to inform when a file changes - called from outside zep by the platform specific code, if possible
    virtual void OnFileChanged(const ZepPath &path);
    virtual void HandleInput() {};

    ZepDisplay *display;
    std::deque<std::shared_ptr<ZepBuffer>> buffers; // May or may not be visible
    std::shared_ptr<ZepTheme> theme;
    EditorConfig config;
    IZepFileSystem *fileSystem;
    std::unique_ptr<ThreadPool> threadPool;
    uint32_t flags = 0;
    NVec2f mousePos = NVec2f(0.0f);
    std::shared_ptr<Region> editorRegion, tabContentRegion, commandRegion, tabRegion;
    std::map<std::string, SyntaxProvider> syntaxProviders;
    std::vector<ZepTabWindow *> tabWindows;
    ZepTabWindow *activeTabWindow = nullptr;
    std::vector<std::string> commandLines; // Command information, shown under the buffer

private:
    // Call GetBuffer publicly, to stop creation of duplicate buffers referring to the same file
    ZepBuffer *CreateNewBuffer(const std::string &bufferName);
    ZepBuffer *CreateNewBuffer(const ZepPath &path);

    void InitBuffer(ZepBuffer &buffer) const;

    // Ensure there is a valid tab window and return it
    ZepTabWindow *EnsureTab();
    void RegisterSyntaxProvider(const std::vector<std::string> &mappings, const SyntaxProvider &provider);
    void RegisterSyntaxProviders();

private:
    std::set<IZepComponent *> m_notifyClients;
    mutable std::map<std::string, Register> m_registers;
    std::map<std::string, std::shared_ptr<ZepMode>> m_mapGlobalModes;
    std::map<std::string, std::shared_ptr<ZepMode>> m_mapBufferModes;
    std::map<std::string, std::shared_ptr<ZepExCommand>> m_mapExCommands;
    timer m_cursorTimer;
    timer m_lastEditTimer;
    ZepMode *m_pCurrentMode = nullptr;
    mutable std::atomic_bool m_bPendingRefresh = true;
    mutable bool m_lastCursorBlink = false;
    bool m_bRegionsChanged = false;
    float m_tabOffsetX = 0.0f;
    std::shared_ptr<Indexer> m_indexer; // Define `IMPLEMENTED_INDEXER` to initialize
};

} // namespace Zep
