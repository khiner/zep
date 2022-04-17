#pragma once

#include <stack>

#include "zep/buffer.h"
#include "zep/display.h"
#include "zep/window.h"
#include "zep/commands.h"
#include "zep/keymap.h"

namespace Zep {

struct ZepEditor;

// NOTE: These are input keys mapped to Zep's internal keymapping; they live below 'space'/32
// Key mapping needs a rethink for international keyboards.  But for modes, this is the remapped key definitions for anything that isn't
// basic ascii symbol.  ASCII 0-31 are mostly unused these days anyway.
struct ExtKeys {
    enum Key {
        RETURN = 0, // NOTE: Do not change this value
        ESCAPE = 1,
        BACKSPACE = 2,
        LEFT = 3,
        RIGHT = 4,
        UP = 5,
        DOWN = 6,
        TAB = 7,
        DEL = 8,
        HOME = 9,
        END = 10,
        PAGEDOWN = 11,
        PAGEUP = 12,
        F1 = 13,
        F2 = 14,
        F3 = 15,
        F4 = 16,
        F5 = 17,
        F6 = 18,
        F7 = 19,
        F8 = 20,
        F9 = 21,
        F10 = 22,
        F11 = 23,
        F12 = 24,

        NONE = 32
    };
};

struct ModifierKey {
    enum Key {
        None = (0),
        Ctrl = (1 << 0),
        Alt = (1 << 1),
        Shift = (1 << 2)
    };
}; // ModifierKey

enum class EditorMode {
    None,
    Normal,
    Insert,
    Visual,
    Ex
};

enum class CommandOperation {
    None,
    Delete,
    DeleteLines,
    Insert,
    Copy,
    CopyLines,
    Replace,
    Paste
};

namespace ModeFlags {
enum {
    None = (0),
    InsertModeGroupUndo = (1 << 0),
    StayInInsertMode = (1 << 1)
};
}

namespace CommandResultFlags {
enum {
    None = 0,
    HandledCount = (1 << 2), // Command implements the count, no need to recall it.
    BeginUndoGroup = (1 << 4)
};
} // CommandResultFlags

struct CommandResult {
    uint32_t flags = CommandResultFlags::None;
    EditorMode modeSwitch = EditorMode::None;
    std::shared_ptr<ZepCommand> command;
};

struct CommandContext {
    CommandContext(std::string commandIn, ZepMode &md, EditorMode editorMode);

    void GetCommandRegisters();
    void UpdateRegisters();

    ZepMode &owner;

    std::string fullCommand;
    KeyMapResult keymap;

    ReplaceRangeMode replaceRangeMode = ReplaceRangeMode::Fill;
    GlyphIterator beginRange;
    GlyphIterator endRange;
    ZepBuffer &buffer;

    // Cursor State
    GlyphIterator bufferCursor;
    GlyphIterator cursorAfterOverride;

    // Register state
    std::stack<char> registers;
    Register tempReg;
    const Register *pRegister = nullptr;

    // Input State
    EditorMode currentMode = EditorMode::None;

    // Output result
    CommandResult commandResult;
    CommandOperation op = CommandOperation::None;

    bool foundCommand = false;
};

struct ZepMode : public ZepComponent {
    explicit ZepMode(ZepEditor &editor);

    virtual void Init() {};
    virtual void AddKeyPress(uint32_t key, uint32_t modifierKeys = ModifierKey::None);
    virtual const char *Name() const = 0;
    virtual void Begin(ZepWindow *pWindow);
    void Notify(const std::shared_ptr<ZepMessage> &) override {}
    virtual uint32_t ModifyWindowFlags(uint32_t windowFlags) { return windowFlags; }
    virtual EditorMode DefaultMode() const = 0;
    virtual bool UsesRelativeLines() const { return false; }

    // About to display this window, which is associated with this mode
    virtual void PreDisplay(ZepWindow &) {};

    // Called when we begin editing in this mode
    void Undo();
    void Redo();

    virtual CursorType GetCursorType() const;

    virtual void SwitchMode(EditorMode editorMode);

    const KeyMap &GetKeyMappings(EditorMode mode) const;

    // Keys handled by modes
    virtual void AddCommandText(const std::string &text);

    virtual GlyphRange GetInclusiveVisualRange() const;

    virtual std::vector<Airline> GetAirlines(ZepWindow &) const { return std::vector<Airline>{}; }

    ZepWindow *currentWindow = nullptr;
    EditorMode currentMode = EditorMode::Normal;

protected:
    // Do the actual input handling
    void HandleMappedInput(const std::string &input);

    void AddCommand(const std::shared_ptr<ZepCommand> &cmd);

    bool GetCommand(CommandContext &context);
    void ResetCommand();

    bool GetOperationRange(const std::string &op, EditorMode currentMode, GlyphIterator &beginRange, GlyphIterator &endRange) const;

    void UpdateVisualSelection();

    void AddGlobalKeyMaps();
    void AddNavigationKeyMaps(bool allowInVisualMode = true);
    void AddSearchKeyMaps();
    static void AddKeyMapWithCountRegisters(const std::vector<KeyMap *> &maps, const std::vector<std::string> &commands, const StringId &id);

    void ClampCursorForMode();
    bool HandleExCommand(std::string strCommand);
    static std::string ConvertInputToMapString(uint32_t key, uint32_t modifierKeys);

    virtual bool HandleIgnoredInput(CommandContext &) { return false; };

protected:
    std::stack<std::shared_ptr<ZepCommand>> m_undoStack;
    std::stack<std::shared_ptr<ZepCommand>> m_redoStack;
    bool m_lineWise = false;
    GlyphIterator m_visualBegin;
    GlyphIterator m_visualEnd;
    std::string m_dotCommand;

    // Keyboard mappings
    KeyMap m_normalMap;
    KeyMap m_visualMap;
    KeyMap m_insertMap;

    Direction m_lastFindDirection = Direction::Forward;
    Direction m_lastSearchDirection = Direction::Forward;

    std::string m_currentCommand;
    std::string m_lastInsertString;
    std::string m_lastFind;

    GlyphIterator m_exCommandStartLocation;
    CursorType m_visualCursorType = CursorType::Visual;
    uint32_t m_modeFlags = ModeFlags::None;
    uint32_t m_lastKey = 0;

    timer m_lastKeyPressTimer;
};

} // namespace Zep
