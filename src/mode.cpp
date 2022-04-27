#include "zep/mode.h"
#include "zep/buffer.h"
#include "zep/editor.h"
#include "zep/filesystem.h"
#include "zep/logger.h"
#include "zep/mode_search.h"
#include "zep/regress.h"
#include "zep/syntax.h"
#include "zep/tab_window.h"
#include "../../imgui/imgui_internal.h"

namespace Zep {
CommandContext::CommandContext(std::string commandIn, ZepMode &md, EditorMode editorMode)
    : owner(md), fullCommand(std::move(commandIn)), buffer(*md.currentWindow->buffer), bufferCursor(md.currentWindow->GetBufferCursor()), tempReg("", false), currentMode(editorMode) {
    registers.push('"');
    pRegister = &tempReg;

    bool needMore = false;
    auto extraMaps = md.editor.GetGlobalKeyMaps(md);
    for (auto &extra: extraMaps) {
        keymap_find(*extra, fullCommand, keymap);
        if (keymap.foundMapping.id != 0) break;
    }

    if (keymap.foundMapping.id == 0) {
        keymap_find(owner.GetKeyMappings(currentMode), fullCommand, keymap);
        if (keymap.foundMapping.id == 0 && needMore) {
            keymap.needMoreChars = true;
        }
    }

    GetCommandRegisters();
}

void CommandContext::UpdateRegisters() {
    // Store in a register
    if (registers.empty()) return;

    if (op == CommandOperation::Delete || op == CommandOperation::DeleteLines) {
        beginRange.Clamp();
        endRange.Clamp();
        if (beginRange > endRange) {
            std::swap(beginRange, endRange);
        }

        // TODO: Make a helper for this
        auto str = std::string(buffer.workingBuffer.begin() + beginRange.index, buffer.workingBuffer.begin() + endRange.index);

        // Delete commands fill up 1-9 registers
        if (keymap.commandWithoutGroups[0] == 'd' || keymap.commandWithoutGroups[0] == 'D') {
            for (int i = 9; i > 1; i--) {
                owner.editor.SetRegister('0' + (char) i, owner.editor.GetRegister('0' + (char) i - 1));
            }
            owner.editor.SetRegister('1', Register(str, (op == CommandOperation::DeleteLines)));
        }

        // Fill up any other required registers
        while (!registers.empty()) {
            owner.editor.SetRegister(registers.top(), Register(str, (op == CommandOperation::DeleteLines)));
            registers.pop();
        }
    } else if (op == CommandOperation::Copy || op == CommandOperation::CopyLines) {
        beginRange = beginRange.Clamp();
        endRange = endRange.Clamp();
        if (beginRange > endRange) {
            std::swap(beginRange, endRange);
        }

        auto str = std::string(buffer.workingBuffer.begin() + beginRange.index, buffer.workingBuffer.begin() + endRange.index);
        while (!registers.empty()) {
            auto &ed = owner.editor;

            // Capital letters append to registers instead of replacing them
            if (registers.top() >= 'A' && registers.top() <= 'Z') {
                auto chlow = (char) std::tolower(registers.top());
                ed.SetRegister(chlow, Register(ed.GetRegister(chlow).text + str, (op == CommandOperation::CopyLines)));
            } else {
                ed.SetRegister(registers.top(), Register(str, op == CommandOperation::CopyLines));
            }
            registers.pop();
        }
    }
}

void CommandContext::GetCommandRegisters() {
    // No specified register, so use the default
    if (keymap.RegisterName() == 0) {
        registers.push('*');
        registers.push('+');
    } else {
        if (keymap.RegisterName() == '_') {
            std::stack<char> temp;
            registers.swap(temp);
        } else {
            registers.push(keymap.RegisterName());
            char reg = keymap.RegisterName();

            // Demote capitals to lower registers when pasting (all both)
            if (reg >= 'A' && reg <= 'Z') {
                reg = (char) std::tolower((char) reg);
            }

            if (owner.editor.GetRegisters().find(std::string({reg})) != owner.editor.GetRegisters().end()) {
                pRegister = &owner.editor.GetRegister(reg);
            }
        }
    }

    // Default register
    if (!pRegister || pRegister->text.empty()) {
        pRegister = &owner.editor.GetRegister('"');
    }
}

ZepMode::ZepMode(ZepEditor &editor) : ZepComponent(editor) {}

void ZepMode::AddCommandText(const std::string &text) {
    if (currentWindow == nullptr) return;

    for (auto &ch: text) {
        AddKeyPress(ch, ImGuiKeyModFlags_None);
    }
}

void ZepMode::ClampCursorForMode() {
    if (currentWindow == nullptr) return;

    // Normal mode cursor is never on a CR/0
    // This stops an edit, such as an undo from leaving the cursor on the CR.
    if (currentMode == EditorMode::Normal) {
        currentWindow->SetBufferCursor(currentWindow->buffer->ClampToVisibleLine(currentWindow->GetBufferCursor()));
    }
}

void ZepMode::SwitchMode(EditorMode editorMode) {
    if (currentWindow == nullptr) return;

    // Don't switch to invalid mode
    if (editorMode == EditorMode::None) return;

    // Don't switch to the same thing again
    if (editorMode == currentMode) return;

    auto pWindow = currentWindow;
    auto *buffer = pWindow->buffer;
    auto cursor = pWindow->GetBufferCursor();

    // Force normal mode if the file is read only
    if (editorMode == EditorMode::Insert && buffer->HasFileFlags(FileFlags::ReadOnly)) {
        editorMode = DefaultMode();
    }

    // When leaving Ex mode, reset search markers
    if (currentMode == EditorMode::Ex) {
        buffer->HideMarkers(RangeMarkerType::Search);

        // Bailed out of ex mode; reset the start location
        /*if (mode != EditorMode::Ex)
        {
            pWindow->SetBufferCursor(m_exCommandStartLocation);
        }
        */
    } else if (editorMode == EditorMode::Insert) {
        // When switching back to normal mode, put the cursor on the last character typed
        if (editorMode == EditorMode::Normal) {
            // Move back, but not to the previous line
            GlyphIterator itr(cursor);
            itr.MoveClamped(-1);
            currentWindow->SetBufferCursor(itr);
        }
    }

    currentMode = editorMode;

    switch (currentMode) {
        case EditorMode::Normal: {
            buffer->ClearSelection();
            ClampCursorForMode();
            ResetCommand();
        }
            break;
        case EditorMode::Insert:buffer->ClearSelection();
            ResetCommand();
            break;
        case EditorMode::Visual: {
            ResetCommand();
        }
            break;
        case EditorMode::Ex: {
            m_exCommandStartLocation = cursor;
            // Ensure we show the command at the bottom
            editor.SetCommandText(m_currentCommand);
        }
            break;
        default:
        case EditorMode::None:break;
    }
}

std::string ZepMode::ConvertInputToMapString(ImGuiKey key, ImGuiKeyModFlags modifierFlags) {
    std::string str;
    bool closeBracket = false;
    if (ImGui::GetIO().KeyCtrl) {
        str += "<C-";
        if (ImGui::GetIO().KeyShift) {
            // Add the S- modifier for shift enabled special keys
            str += "S-";
        }
        closeBracket = true;
    } else if (ImGui::GetIO().KeyShift) {
        if (key < ImGuiKey_Space) {
            str += "<S-";
            closeBracket = true;
        }
    }

    std::string mapped;

#define COMPARE_STR(a, b) \
    if (key == (b))       \
        mapped = #a;

    COMPARE_STR(Return, ImGuiKey_Enter)
    COMPARE_STR(Escape, ImGuiKey_Escape)
    COMPARE_STR(Backspace, ImGuiKey_Backspace)
    COMPARE_STR(Left, ImGuiKey_LeftArrow)
    COMPARE_STR(Right, ImGuiKey_RightArrow)
    COMPARE_STR(Up, ImGuiKey_UpArrow)
    COMPARE_STR(Down, ImGuiKey_DownArrow)
    COMPARE_STR(Tab, ImGuiKey_Tab)
    COMPARE_STR(Del, ImGuiKey_Delete)
    COMPARE_STR(Home, ImGuiKey_Home)
    COMPARE_STR(End, ImGuiKey_End)
    COMPARE_STR(PageDown, ImGuiKey_PageDown)
    COMPARE_STR(PageUp, ImGuiKey_PageUp)
    COMPARE_STR(F1, ImGuiKey_F1)
    COMPARE_STR(F2, ImGuiKey_F2)
    COMPARE_STR(F3, ImGuiKey_F3)
    COMPARE_STR(F4, ImGuiKey_F4)
    COMPARE_STR(F5, ImGuiKey_F5)
    COMPARE_STR(F6, ImGuiKey_F6)
    COMPARE_STR(F7, ImGuiKey_F7)
    COMPARE_STR(F8, ImGuiKey_F8)
    COMPARE_STR(F9, ImGuiKey_F9)
    COMPARE_STR(F10, ImGuiKey_F10)
    COMPARE_STR(F11, ImGuiKey_F11)
    COMPARE_STR(F12, ImGuiKey_F12)

    if (!mapped.empty()) {
        if (!closeBracket) {
            str += "<" + mapped;
            closeBracket = true;
        } else {
            str += mapped;
        }
    } else {
        str += (char) (key - ImGuiKey_A + 'a');
    }

    if (closeBracket) str += ">";

    return str;
}

// Handle a key press, convert it to an input command and context, and return it.
void ZepMode::AddKeyPress(ImGuiKey key, ImGuiKeyModFlags modifierKeys) {
    if (currentWindow == nullptr) return;

    m_lastKey = key;

    // Get the new command by parsing out the keys
    // We convert CTRL + f to a string: "<C-f>"
    HandleMappedInput(ConvertInputToMapString(key, modifierKeys));

    if (currentWindow->buffer->postKeyNotifier != nullptr) {
        currentWindow->buffer->postKeyNotifier(key, modifierKeys);
    }

    timer_restart(m_lastKeyPressTimer);
}

void ZepMode::HandleMappedInput(const std::string &input) {
    if (input.empty()) return;

    // Special case, dot command (do last edit again)
    // Dot command is complicated, this is my second attempt at implementing it and is less
    // complex.  The approach is to store relevant keystrokes for the last edit operation,
    // and replay them when the user uses the dot.
    if (currentMode == EditorMode::Normal && input[input.size() - 1] == '.') {
        // Save and restore the last command while doing it.
        auto lastCommand = m_dotCommand;
        for (auto &last: lastCommand) {
            HandleMappedInput(std::string(1, last));
        }
        m_dotCommand = lastCommand;

        SwitchMode(EditorMode::Normal);
        return;
    }

    // The current command is our currently typed multi-key operation
    m_currentCommand += input;

    // Reset the timer for the last edit, for time-sensitive keystrokes
    editor.ResetLastEditTimer();

    // Reset the cursor to keep it visible during typing, and not flashing
    editor.ResetCursorTimer();

    // Reset command text - it may get updated later.
    editor.SetCommandText("");

    // Figure out the command we have typed. foundCommand means that the command was interpreted and understood.
    // If command is returned, then there is an atomic command operation that needs to be done.
    auto context = std::make_shared<CommandContext>(m_currentCommand, *this, currentMode);

    // Before handling the command, change the command text, since the command might override it
    if (editor.config.showNormalModeKeyStrokes && (currentMode == EditorMode::Normal || currentMode == EditorMode::Visual)) {
        editor.SetCommandText(context->keymap.searchPath);
    }

    context->foundCommand = GetCommand(*context);

    // Stay in insert mode unless commanded otherwise
    if (context->commandResult.modeSwitch == EditorMode::None && context->foundCommand && m_modeFlags & ModeFlags::StayInInsertMode) {
        context->commandResult.modeSwitch = EditorMode::Insert;
    }

    // A lambda to check for a pending mode switch after the command
    auto enteringMode = [=](auto mode) {
        return currentMode != context->commandResult.modeSwitch && context->commandResult.modeSwitch == mode;
    };

    // Escape Nukes the current command - we handle it in the keyboard mappings after that
    // TODO: This feels awkward
    if (m_lastKey == ImGuiKey_Escape) {
        m_currentCommand.clear();
    }

    // Did we find something to do?
    if (context->foundCommand) {
        // It's an undoable command  - add it
        // Note: a command here is something that modifies the text.  It can be something like a delete
        // or a simple insert
        if (context->commandResult.command) {
            // If not in insert mode, begin the group, because we have started a new operation
            if (currentMode != EditorMode::Insert || (context->commandResult.flags & CommandResultFlags::BeginUndoGroup)) {
                AddCommand(std::make_shared<ZepCommand_GroupMarker>(context->buffer));

                // Record for the dot command
                m_dotCommand = m_currentCommand;
            } else {
                // In insert mode keep the text for the dot command.  An insert adds a command too!
                m_dotCommand += input;
            }

            // Do the command
            AddCommand(context->commandResult.command);
        } else {
            // This command didn't change anything, but switched into insert mode, so
            // remember the dot command that did it
            if (enteringMode(EditorMode::Insert)) {
                AddCommand(std::make_shared<ZepCommand_GroupMarker>(context->buffer));
                m_dotCommand = m_currentCommand;
            }
        }

        // If the command can't manage the count, we do it
        // Maybe all commands should handle the count?  What are the implications of that?  This bit is a bit messy
        if (!(context->commandResult.flags & CommandResultFlags::HandledCount)) {
            // Ignore count == 1, we already did it
            for (int i = 1; i < context->keymap.TotalCount(); i++) {
                // May immediate execute and not return a command...
                // Create a new 'inner' context-> for the next command, because we need to re-initialize the command
                // context-> for 'after' what just happened!
                CommandContext contextInner(m_currentCommand, *this, currentMode);
                if (GetCommand(contextInner) && contextInner.commandResult.command) {
                    // Actually queue/do command
                    AddCommand(contextInner.commandResult.command);
                }
            }
        }

        // A mode to switch to after the command is done
        SwitchMode(context->commandResult.modeSwitch);

        // If not in ex mode, wait for a new command
        // Can this be cleaner?
        if (currentMode != EditorMode::Ex) {
            ResetCommand();
        }

        // Motions can update the visual selection
        UpdateVisualSelection();
    } else {
        // If not found, and there was no request for more characters, and we aren't in Ex mode
        if (currentMode != EditorMode::Ex) {
            if (HandleIgnoredInput(*context) || !context->keymap.needMoreChars) {
                ResetCommand();
            }
        }
    }
    ClampCursorForMode();
}

void ZepMode::AddCommand(const std::shared_ptr<ZepCommand> &cmd) {
    if (currentWindow == nullptr) return;

    // Ignore commands on buffers because we are view only,
    // and all commands currently modify the buffer!
    if (currentWindow->buffer->HasFileFlags(FileFlags::Locked)) return;

    cmd->Redo();
    m_undoStack.push(cmd);

    // Can't redo anything beyond this point
    std::stack<std::shared_ptr<ZepCommand>> empty;
    m_redoStack.swap(empty);

    if (cmd->cursorAfter.Valid()) {
        currentWindow->SetBufferCursor(cmd->cursorAfter);
    }
}

void ZepMode::Redo() {
    if (currentWindow == nullptr || m_redoStack.empty()) return;

    if (std::dynamic_pointer_cast<ZepCommand_GroupMarker>(m_redoStack.top()) != nullptr) {
        m_undoStack.push(m_redoStack.top());
        m_redoStack.pop();
    }

    while (!m_redoStack.empty()) {
        auto &command = m_redoStack.top();
        command->Redo();

        if (command->cursorAfter.Valid()) {
            currentWindow->SetBufferCursor(command->cursorAfter);
        }

        m_undoStack.push(command);
        m_redoStack.pop();

        if (std::dynamic_pointer_cast<ZepCommand_GroupMarker>(command) != nullptr) break;
    };
}

void ZepMode::Undo() {
    if (currentWindow == nullptr || m_undoStack.empty()) return;

    if (std::dynamic_pointer_cast<ZepCommand_GroupMarker>(m_undoStack.top()) != nullptr) {
        m_redoStack.push(m_undoStack.top());
        m_undoStack.pop();
    }

    while (!m_undoStack.empty()) {
        auto &command = m_undoStack.top();
        command->Undo();

        if (command->cursorBefore.Valid()) {
            currentWindow->SetBufferCursor(command->cursorBefore);
        }

        m_redoStack.push(command);
        m_undoStack.pop();

        if (std::dynamic_pointer_cast<ZepCommand_GroupMarker>(command) != nullptr) break;
    };
}

GlyphRange ZepMode::GetInclusiveVisualRange() const {
    // Clamp and orient the correct way around
    auto startOffset = m_visualBegin.Clamped();
    auto endOffset = m_visualEnd.Clamped();

    if (startOffset > endOffset) {
        std::swap(startOffset, endOffset);
    }

    if (DefaultMode() == EditorMode::Insert) {
        // In standard/insert mode, selections exclude the last character
        endOffset.Move(-1);
    }

    return {startOffset, endOffset};
}

bool ZepMode::GetCommand(CommandContext &context) {
    auto bufferCursor = currentWindow->GetBufferCursor();
    auto *buffer = currentWindow->buffer;

    if (currentMode == EditorMode::Ex) {
        // TODO: Is it possible extend our key mapping to better process ex commands?  Or are these too specialized?
        if (HandleExCommand(context.fullCommand)) {
            // buffer.GetMode()->Begin(currentWindow);
            SwitchMode(DefaultMode());
            ResetCommand();
            return true;
        }

        editor.SetCommandText(m_currentCommand);
        return false;
    }

    // The keymapper is waiting for more input
    if (context.keymap.needMoreChars) return false;

    // This flag is for non-modal editors which like to break insertions into undo groups.
    // Vim, for example, doesn't do that; an insert mode operation is a single 'group'
    bool shouldGroupInserts = m_modeFlags & ModeFlags::InsertModeGroupUndo;

    GlyphIterator cursorItr = bufferCursor;

    auto mappedCommand = context.keymap.foundMapping;

    auto pEx = editor.FindExCommand(mappedCommand);
    if (pEx) {
        pEx->Run();
        return true;
    }

    if (mappedCommand == id_NormalMode) {
        // TODO: I think this should be a 'command' which would get replayed with dot;
        // instead of special casing it later, we could just insert it into the stream of commands
        context.commandResult.modeSwitch = EditorMode::Normal;
        return true;
    } else if (mappedCommand == id_ExMode) {
        context.commandResult.modeSwitch = EditorMode::Ex;
        return true;
    }
        // Control
    else if (mappedCommand == id_MotionNextMarker) {
        auto pFound = buffer->FindNextMarker(currentWindow->GetBufferCursor(), Direction::Forward, RangeMarkerType::Mark);
        if (pFound) {
            currentWindow->SetBufferCursor(GlyphIterator(&context.buffer, pFound->range.first));
        }
        return true;
    } else if (mappedCommand == id_MotionPreviousMarker) {
        auto pFound = buffer->FindNextMarker(currentWindow->GetBufferCursor(), Direction::Backward, RangeMarkerType::Mark);
        if (pFound) {
            currentWindow->SetBufferCursor(GlyphIterator(&context.buffer, pFound->range.first));
        }
        return true;
    } else if (mappedCommand == id_MotionNextSearch) {
        auto pFound = buffer->FindNextMarker(currentWindow->GetBufferCursor(), m_lastSearchDirection, RangeMarkerType::Search);
        if (pFound) {
            currentWindow->SetBufferCursor(GlyphIterator(&context.buffer, pFound->range.first));
        }
        return true;
    } else if (mappedCommand == id_MotionPreviousSearch) {
        auto pFound = buffer->FindNextMarker(currentWindow->GetBufferCursor(), m_lastSearchDirection == Direction::Forward ? Direction::Backward : Direction::Forward, RangeMarkerType::Search);
        if (pFound) {
            currentWindow->SetBufferCursor(GlyphIterator(&context.buffer, pFound->range.first));
        }
        return true;
    } else if (mappedCommand == id_SwitchToAlternateFile) {
        // This is a quick and easy 'alternate file swap'.
        // It searches a preset list of useful folder targets around the current file.
        // A better alternative might be a wildcard list of relations, but this works well enough
        // It also only looks for a file with the same name and different extension!
        // it is good enough for my current needs...
        const auto &path = buffer->filePath;
        if (!path.empty() && ZepFileSystem::Exists(path)) {
            auto ext = path.extension();
            auto searchPaths = std::vector<ZepPath>{
                path.parent_path(),
                path.parent_path().parent_path(),
                path.parent_path().parent_path().parent_path()
            };

            auto ignoreFolders = std::vector<std::string>{"build", ".git", "obj", "debug", "release"};

            auto priorityFolders = std::vector<std::string>{"source", "include", "src", "inc", "lib"};

            // Search the paths, starting near and widening
            for (auto &p: searchPaths) {
                if (p.empty()) continue;

                bool found = false;
                // Look for the priority folder locations
                std::vector<ZepPath> searchFolders{path.parent_path()};
                for (auto &priorityFolder: priorityFolders) {
                    ZepFileSystem::ScanDirectory(p, [&](const ZepPath &currentPath, bool &recurse) {
                        recurse = false;
                        if (ZepFileSystem::IsDirectory(currentPath)) {
                            auto lower = string_tolower(currentPath.filename().string());
                            if (std::find(ignoreFolders.begin(), ignoreFolders.end(), lower) != ignoreFolders.end()) {
                                return true;
                            }

                            if (priorityFolder == lower) {
                                searchFolders.push_back(currentPath);
                            }
                        }
                        return true;
                    });
                }

                for (auto &folder: searchFolders) {
                    ZLOG(INFO, "Searching: " << folder.string());

                    ZepFileSystem::ScanDirectory(folder, [&](const ZepPath &currentPath, bool &recurse) {
                        recurse = true;
                        if (path.stem() == currentPath.stem() && !(currentPath.extension() == path.extension())) {
                            auto load = editor.GetFileBuffer(currentPath, 0, true);
                            if (load != nullptr) {
                                currentWindow->SetBuffer(load);
                                found = true;
                                return false;
                            }
                        }
                        return true;
                    });
                    if (found) return true;
                }
            }
        }
    } else if (mappedCommand == id_FontBigger) {
        editor.display->Bigger();
        return true;
    } else if (mappedCommand == id_FontSmaller) {
        editor.display->Smaller();
        return true;
    }
        // Moving between splits
    else if (mappedCommand == id_MotionDownSplit) {
        currentWindow->tabWindow.DoMotion(WindowMotion::Down);
        return true;
    } else if (mappedCommand == id_MotionUpSplit) {
        currentWindow->tabWindow.DoMotion(WindowMotion::Up);
        return true;
    } else if (mappedCommand == id_MotionLeftSplit) {
        currentWindow->tabWindow.DoMotion(WindowMotion::Left);
        return true;
    } else if (mappedCommand == id_MotionRightSplit) {
        currentWindow->tabWindow.DoMotion(WindowMotion::Right);
        return true;
    }
        // global search
    else if (mappedCommand == id_QuickSearch) {
        editor.AddSearch();
        return true;
    } else if (mappedCommand == id_Redo) {
        context.commandResult.modeSwitch = DefaultMode();
        Redo();
        return true;
    } else if (mappedCommand == id_Undo) {
        context.commandResult.modeSwitch = DefaultMode();
        Undo();
        return true;
    } else if (mappedCommand == id_MotionLineEnd) {
        currentWindow->SetBufferCursor(context.buffer.GetLinePos(bufferCursor, LineLocation::LineLastNonCR));
        return true;
    } else if (mappedCommand == id_MotionLineBeyondEnd) {
        currentWindow->SetBufferCursor(context.buffer.GetLinePos(bufferCursor, LineLocation::LineCRBegin));
        return true;
    } else if (mappedCommand == id_MotionLineBegin) {
        currentWindow->SetBufferCursor(context.buffer.GetLinePos(bufferCursor, LineLocation::LineBegin));
        return true;
    } else if (mappedCommand == id_MotionLineFirstChar) {
        currentWindow->SetBufferCursor(context.buffer.GetLinePos(bufferCursor, LineLocation::LineFirstGraphChar));
        return true;
    } else if (mappedCommand == id_MotionLineHomeToggle) {
        auto newCursorPos = context.buffer.GetLinePos(bufferCursor, LineLocation::LineFirstGraphChar);
        if (bufferCursor == newCursorPos) {
            newCursorPos = context.buffer.GetLinePos(bufferCursor, LineLocation::LineBegin);
        }
        currentWindow->SetBufferCursor(newCursorPos);
        return true;
    }
        // Moving between tabs
    else if (mappedCommand == id_PreviousTabWindow) {
        editor.PreviousTabWindow();
        return true;
    } else if (mappedCommand == id_NextTabWindow) {
        editor.NextTabWindow();
        return true;
    } else if (mappedCommand == id_MotionDown) {
        currentWindow->MoveCursorY(context.keymap.TotalCount());
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    } else if (mappedCommand == id_MotionUp) {
        currentWindow->MoveCursorY(-context.keymap.TotalCount());
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    } else if (mappedCommand == id_MotionRight) {
        currentWindow->SetBufferCursor(cursorItr.MoveClamped(context.keymap.TotalCount()));
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    } else if (mappedCommand == id_MotionLeft) {
        currentWindow->SetBufferCursor(cursorItr.MoveClamped(-context.keymap.TotalCount()));
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    } else if (mappedCommand == id_MotionStandardRight) {
        currentWindow->SetBufferCursor(cursorItr.Move(context.keymap.TotalCount()));
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    } else if (mappedCommand == id_MotionStandardLeft) {
        currentWindow->SetBufferCursor(cursorItr.Move(-context.keymap.TotalCount()));
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    } else if (mappedCommand == id_MotionStandardUp) {
        currentWindow->MoveCursorY(-1, LineLocation::LineCRBegin);
        return true;
    } else if (mappedCommand == id_MotionStandardDown) {
        currentWindow->MoveCursorY(1, LineLocation::LineCRBegin);
        return true;
    } else if (mappedCommand == id_StandardSelectAll) {
        context.commandResult.modeSwitch = EditorMode::Visual;
        m_visualBegin = context.buffer.Begin();
        m_visualEnd = context.buffer.End();
        auto range = GetInclusiveVisualRange();
        currentWindow->buffer->SetSelection(range);
        currentWindow->SetBufferCursor(range.second);
        return true;
    } else if (mappedCommand == id_MotionStandardRightSelect) {
        context.commandResult.modeSwitch = EditorMode::Visual;
        if (currentMode != EditorMode::Visual) {
            m_visualBegin = currentWindow->GetBufferCursor();
        }
        currentWindow->SetBufferCursor(bufferCursor + 1);
        UpdateVisualSelection();
        return true;
    } else if (mappedCommand == id_MotionStandardLeftSelect) {
        context.commandResult.modeSwitch = EditorMode::Visual;
        if (currentMode != EditorMode::Visual) {
            m_visualBegin = currentWindow->GetBufferCursor();
        }
        currentWindow->SetBufferCursor(bufferCursor - 1);
        UpdateVisualSelection();
        return true;
    } else if (mappedCommand == id_MotionStandardUpSelect) {
        context.commandResult.modeSwitch = EditorMode::Visual;
        if (currentMode != EditorMode::Visual) {
            m_visualBegin = currentWindow->GetBufferCursor();
        }
        currentWindow->MoveCursorY(-1, LineLocation::LineCRBegin);
        UpdateVisualSelection();
        return true;
    } else if (mappedCommand == id_MotionStandardDownSelect) {
        context.commandResult.modeSwitch = EditorMode::Visual;
        if (currentMode != EditorMode::Visual) {
            m_visualBegin = currentWindow->GetBufferCursor();
        }
        currentWindow->MoveCursorY(1, LineLocation::LineCRBegin);
        UpdateVisualSelection();
        return true;
    } else if (mappedCommand == id_MotionStandardRightWord) {
        auto target = buffer->StandardCtrlMotion(bufferCursor, Direction::Forward);
        currentWindow->SetBufferCursor(target.second);
        return true;
    } else if (mappedCommand == id_MotionStandardLeftWord) {
        auto target = buffer->StandardCtrlMotion(bufferCursor, Direction::Backward);
        currentWindow->SetBufferCursor(target.second);
        return true;
    } else if (mappedCommand == id_MotionStandardRightWordSelect) {
        context.commandResult.modeSwitch = EditorMode::Visual;
        if (currentMode != EditorMode::Visual) {
            m_visualBegin = currentWindow->GetBufferCursor();
        }
        auto target = buffer->StandardCtrlMotion(bufferCursor, Direction::Forward);
        currentWindow->SetBufferCursor(target.second);
        UpdateVisualSelection();
        return true;
    } else if (mappedCommand == id_MotionStandardLeftWordSelect) {
        context.commandResult.modeSwitch = EditorMode::Visual;
        if (currentMode != EditorMode::Visual) {
            m_visualBegin = currentWindow->GetBufferCursor();
        }
        auto target = buffer->StandardCtrlMotion(bufferCursor, Direction::Backward);
        currentWindow->SetBufferCursor(target.second);
        UpdateVisualSelection();
        return true;
    } else if (mappedCommand == id_MotionPageForward) {
        // Note: the vim spec says 'visible lines - 2' for a 'page'.
        // We jump the max possible lines, which might hit the end of the text; this matches observed vim behavior
        currentWindow->MoveCursorY((currentWindow->GetMaxDisplayLines() - 2) * context.keymap.TotalCount());
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    } else if (mappedCommand == id_MotionHalfPageForward) {
        // Note: the vim spec says 'half visible lines' for up/down
        currentWindow->MoveCursorY((currentWindow->GetNumDisplayedLines() / 2) * context.keymap.TotalCount());
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    } else if (mappedCommand == id_MotionPageBackward) {
        // Note: the vim spec says 'visible lines - 2' for a 'page'
        currentWindow->MoveCursorY(-(currentWindow->GetMaxDisplayLines() - 2) * context.keymap.TotalCount());
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    } else if (mappedCommand == id_MotionHalfPageBackward) {
        currentWindow->MoveCursorY(-(currentWindow->GetNumDisplayedLines() / 2) * context.keymap.TotalCount());
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    } else if (mappedCommand == id_MotionGotoLine) {
        if (!context.keymap.captureNumbers.empty()) {
            // In Vim, 0G means go to end!  1G is the first line...
            long count = std::max(std::min(long(context.buffer.lineEnds.size()) - 1l, context.keymap.TotalCount() - 1l), 0l);

            ByteRange range;
            if (context.buffer.GetLineOffsets(count, range)) {
                currentWindow->SetBufferCursor(GlyphIterator(&context.buffer, range.first));
            }
        } else {
            // Move right to the end
            auto lastLine = context.buffer.GetLinePos(context.buffer.End(), LineLocation::LineBegin);
            currentWindow->SetBufferCursor(lastLine);
            context.commandResult.flags |= CommandResultFlags::HandledCount;
        }
        return true;
    } else if (mappedCommand == id_Backspace) {
        // In insert mode, we are 'on' the character after the one we want to delete
        context.beginRange = cursorItr.Peek(-1);
        context.endRange = cursorItr;
        context.op = CommandOperation::Delete;
    } else if (mappedCommand == id_MotionWord) {
        auto target = context.buffer.WordMotion(context.bufferCursor, SearchType::Word, Direction::Forward);
        currentWindow->SetBufferCursor(target);
        return true;
    } else if (mappedCommand == id_MotionWORD) {
        auto target = context.buffer.WordMotion(context.bufferCursor, SearchType::WORD, Direction::Forward);
        currentWindow->SetBufferCursor(target);
        return true;
    } else if (mappedCommand == id_MotionBackWord) {
        auto target = context.buffer.WordMotion(context.bufferCursor, SearchType::Word, Direction::Backward);
        currentWindow->SetBufferCursor(target);
        return true;
    } else if (mappedCommand == id_MotionBackWORD) {
        auto target = context.buffer.WordMotion(context.bufferCursor, SearchType::WORD, Direction::Backward);
        currentWindow->SetBufferCursor(target);
        return true;
    } else if (mappedCommand == id_MotionEndWord) {
        auto target = context.buffer.EndWordMotion(context.bufferCursor, SearchType::Word, Direction::Forward);
        currentWindow->SetBufferCursor(target);
        return true;
    } else if (mappedCommand == id_MotionEndWORD) {
        auto target = context.buffer.EndWordMotion(context.bufferCursor, SearchType::WORD, Direction::Forward);
        currentWindow->SetBufferCursor(target);
        return true;
    } else if (mappedCommand == id_MotionBackEndWord) {
        auto target = context.buffer.EndWordMotion(context.bufferCursor, SearchType::Word, Direction::Backward);
        currentWindow->SetBufferCursor(target);
        return true;
    } else if (mappedCommand == id_MotionBackEndWORD) {
        auto target = context.buffer.EndWordMotion(context.bufferCursor, SearchType::WORD, Direction::Backward);
        currentWindow->SetBufferCursor(target);
        return true;
    } else if (mappedCommand == id_MotionGotoBeginning) {
        currentWindow->SetBufferCursor(context.buffer.Begin());
        return true;
    } else if (mappedCommand == id_JoinLines) {
        // Special case, join on empty line, just pull out the newline
        if (context.bufferCursor.Char() == '\n') {
            context.beginRange = context.bufferCursor;
            context.endRange = context.bufferCursor.PeekByteOffset(1);
            context.op = CommandOperation::Delete;
        } else {
            // Replace the CR (and thus join lines)
            context.beginRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineCRBegin);
            context.endRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::BeyondLineEnd);

            // Replace all white space (as the J append command does)
            context.tempReg.text = " ";
            context.pRegister = &context.tempReg;
            context.endRange = std::max(context.endRange, buffer->GetLinePos(context.endRange, LineLocation::LineFirstGraphChar));
            context.replaceRangeMode = ReplaceRangeMode::Replace;

            context.op = CommandOperation::Replace;
            context.commandResult.flags = ZSetFlags(context.commandResult.flags, CommandResultFlags::BeginUndoGroup);
        }
    } else if (mappedCommand == id_VisualMode || mappedCommand == id_VisualLineMode) {
        if (currentMode == EditorMode::Visual) {
            context.commandResult.modeSwitch = DefaultMode();
        } else {
            if (mappedCommand == id_VisualLineMode) {
                m_visualBegin = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineBegin);
                m_visualEnd = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineCRBegin);
            } else {
                m_visualBegin = context.bufferCursor;
                m_visualEnd = m_visualBegin;
            }
            context.commandResult.modeSwitch = EditorMode::Visual;
        }
        m_lineWise = (mappedCommand == id_VisualLineMode ? true : false);
        return true;
    } else if (mappedCommand == id_Delete) {
        if (currentMode == EditorMode::Visual) {
            auto range = GetInclusiveVisualRange();
            context.beginRange = range.first;
            context.endRange = range.second.Peek(1);
            context.op = CommandOperation::Delete;
            context.commandResult.modeSwitch = DefaultMode();
        } else {
            context.beginRange = cursorItr;
            if (currentMode == EditorMode::Normal) {
                // Normal/Vim mode clamped to end of line
                context.endRange = cursorItr.PeekLineClamped(context.keymap.TotalCount(), LineLocation::LineCRBegin);
            } else {
                context.endRange = cursorItr.Peek(context.keymap.TotalCount());
            }
            context.op = CommandOperation::Delete;
            context.commandResult.flags |= CommandResultFlags::HandledCount;
        }
    } else if (mappedCommand == id_OpenLineBelow) {
        context.beginRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineCRBegin);
        context.tempReg.text = "\n";
        context.pRegister = &context.tempReg;
        context.op = CommandOperation::Insert;
        context.commandResult.modeSwitch = EditorMode::Insert;
        context.commandResult.flags = ZSetFlags(context.commandResult.flags, CommandResultFlags::BeginUndoGroup);
    } else if (mappedCommand == id_InsertCarriageReturn) {
        context.beginRange = context.bufferCursor;
        context.tempReg.text = "\n";
        context.pRegister = &context.tempReg;
        context.op = CommandOperation::Insert;
        context.commandResult.modeSwitch = EditorMode::Insert;
        context.commandResult.flags = ZSetFlags(context.commandResult.flags, CommandResultFlags::BeginUndoGroup, shouldGroupInserts);
    } else if (mappedCommand == id_InsertTab) {
        context.beginRange = context.bufferCursor;
        if (buffer->HasFileFlags(FileFlags::InsertTabs)) {
            context.tempReg.text = "\t";
        } else {
            context.tempReg.text = "    ";
        }
        context.pRegister = &context.tempReg;
        context.op = CommandOperation::Insert;
        context.commandResult.modeSwitch = EditorMode::Insert;
        context.commandResult.flags = ZSetFlags(context.commandResult.flags, CommandResultFlags::BeginUndoGroup, shouldGroupInserts);
    } else if (mappedCommand == id_OpenLineAbove) {
        context.beginRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineBegin);
        context.tempReg.text = "\n";
        context.pRegister = &context.tempReg;
        context.op = CommandOperation::Insert;
        context.commandResult.modeSwitch = EditorMode::Insert;
        context.cursorAfterOverride = context.bufferCursor;
        context.commandResult.flags = ZSetFlags(context.commandResult.flags, CommandResultFlags::BeginUndoGroup, shouldGroupInserts);
    } else if (mappedCommand == id_YankLine) {
        // Copy the whole line, including the CR
        context.registers.push('0');
        context.registers.push('*');
        context.registers.push('+');
        context.beginRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineBegin);
        context.endRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::BeyondLineEnd);
        context.op = CommandOperation::CopyLines;
        context.commandResult.modeSwitch = DefaultMode();
        context.cursorAfterOverride = context.beginRange;
    } else if (mappedCommand == id_Yank) {
        context.registers.push('0');
        context.registers.push('*');
        context.registers.push('+');
        auto range = GetInclusiveVisualRange();
        context.beginRange = range.first;
        context.endRange = range.second.Peek(1);
        // Note: select line wise yank if we started in linewise copy mode
        context.op = m_lineWise ? CommandOperation::CopyLines : CommandOperation::Copy;
        context.commandResult.modeSwitch = DefaultMode();
        context.cursorAfterOverride = context.beginRange;
    } else if (mappedCommand == id_StandardCopy) {
        // Ignore empty copy
        auto range = GetInclusiveVisualRange();
        context.beginRange = range.first;
        context.endRange = range.second.Peek(1);
        if (context.beginRange == context.endRange) {
            return true;
        }

        // Copy in standard mode stays in visual mode
        context.commandResult.modeSwitch = EditorMode::Visual;
        context.registers.push('0');
        context.registers.push('*');
        context.registers.push('+');
        context.cursorAfterOverride = context.bufferCursor;

        // Note: select line wise yank if we started in linewise copy mode
        context.op = CommandOperation::Copy;
    } else if (mappedCommand == id_StandardPaste) {
        if (context.currentMode == EditorMode::Visual) {
            context.replaceRangeMode = ReplaceRangeMode::Replace;
            context.op = CommandOperation::Replace;
            context.pRegister = &editor.GetRegister('"');
            auto range = GetInclusiveVisualRange();
            context.beginRange = range.first;
            context.endRange = range.second.Peek(1);
            context.cursorAfterOverride = context.beginRange.PeekByteOffset(long(context.pRegister->text.size()));
            context.commandResult.modeSwitch = EditorMode::Insert;
        } else {
            context.beginRange = context.bufferCursor;
            context.op = CommandOperation::Insert;
        }
        context.commandResult.flags = ZSetFlags(context.commandResult.flags, CommandResultFlags::BeginUndoGroup);
    } else if (mappedCommand == id_PasteAfter) {
        if (!context.pRegister->text.empty()) {
            // Already in visual mode, so replace the selection
            if (context.currentMode == EditorMode::Visual) {
                context.replaceRangeMode = ReplaceRangeMode::Replace;
                context.op = CommandOperation::Replace;
                context.pRegister = &editor.GetRegister('"');
                auto range = GetInclusiveVisualRange();
                context.beginRange = range.first;
                context.endRange = range.second.Peek(1);
                context.cursorAfterOverride = context.beginRange.PeekByteOffset(long(context.pRegister->text.size()));
                context.commandResult.modeSwitch = EditorMode::Insert;
            } else {
                if (context.pRegister->lineWise) {
                    context.beginRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::BeyondLineEnd);
                    context.cursorAfterOverride = context.beginRange;
                } else {
                    context.beginRange = cursorItr.PeekLineClamped(1, LineLocation::LineCRBegin);
                }
                context.op = CommandOperation::Insert;
            }
        }
        context.commandResult.flags = ZSetFlags(context.commandResult.flags, CommandResultFlags::BeginUndoGroup);
    } else if (mappedCommand == id_PasteBefore) {
        if (!context.pRegister->text.empty()) {
            // Already in visual mode, so replace the selection with whatever we copied
            if (context.currentMode == EditorMode::Visual) {
                context.pRegister = &editor.GetRegister('"');
                auto range = GetInclusiveVisualRange();
                context.beginRange = range.first;
                context.endRange = range.second.Peek(1);
                context.cursorAfterOverride = context.beginRange.PeekByteOffset(long(context.pRegister->text.size()));
                context.commandResult.modeSwitch = EditorMode::Insert;
                context.replaceRangeMode = ReplaceRangeMode::Replace;
                context.op = CommandOperation::Replace;
            } else {
                if (context.pRegister->lineWise) {
                    context.beginRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineBegin);
                } else {
                    context.beginRange = context.bufferCursor;
                }
                context.op = CommandOperation::Insert;
            }
        }
        context.commandResult.flags = ZSetFlags(context.commandResult.flags, CommandResultFlags::BeginUndoGroup);
    } else if (mappedCommand == id_InsertMode) {
        // TODO: I think this should be a 'command' which would get replayed with dot;
        // instead of special casing it later, we could just insert it into the stream of commands
        // Then undo/redo would replay the insert operation neatly
        context.commandResult.modeSwitch = EditorMode::Insert;
        return true;
    } else if (mappedCommand == id_VisualSelectInnerWORD) {
        if (GetOperationRange("iW", context.currentMode, context.beginRange, context.endRange)) {
            m_visualBegin = context.beginRange;
            currentWindow->SetBufferCursor(context.endRange - 1);
            UpdateVisualSelection();
            return true;
        }
        return true;
    } else if (mappedCommand == id_VisualSelectInnerWord) {
        if (GetOperationRange("iw", context.currentMode, context.beginRange, context.endRange)) {
            m_visualBegin = context.beginRange;
            currentWindow->SetBufferCursor(context.endRange - 1);
            UpdateVisualSelection();
            return true;
        }
    } else if (mappedCommand == id_VisualSelectAWord) {
        if (GetOperationRange("aw", context.currentMode, context.beginRange, context.endRange)) {
            m_visualBegin = context.beginRange;
            currentWindow->SetBufferCursor(context.endRange - 1);
            UpdateVisualSelection();
            return true;
        }
    } else if (mappedCommand == id_VisualSelectAWORD) {
        if (GetOperationRange("aW", context.currentMode, context.beginRange, context.endRange)) {
            m_visualBegin = context.beginRange;
            currentWindow->SetBufferCursor(context.endRange - 1);
            UpdateVisualSelection();
            return true;
        }
        return true;
    } else if (mappedCommand == id_DeleteToLineEnd) {
        if (GetOperationRange("$", context.currentMode, context.beginRange, context.endRange)) {
            context.op = CommandOperation::Delete;
        }
    } else if (mappedCommand == id_VisualDelete) {
        // Only in visual mode; delete selected block
        if (GetOperationRange("visual", context.currentMode, context.beginRange, context.endRange)) {
            context.op = CommandOperation::Delete;
            context.commandResult.modeSwitch = DefaultMode();
        }
    } else if (mappedCommand == id_DeleteLine) {
        if (GetOperationRange("line", context.currentMode, context.beginRange, context.endRange)) {
            context.op = CommandOperation::DeleteLines;
            context.commandResult.modeSwitch = DefaultMode();
            context.cursorAfterOverride = context.buffer.GetLinePos(context.beginRange, LineLocation::LineBegin);
        }
    } else if (mappedCommand == id_DeleteWord) {
        if (GetOperationRange("w", context.currentMode, context.beginRange, context.endRange)) {
            context.op = CommandOperation::Delete;
        }
    } else if (mappedCommand == id_DeleteWORD) {
        if (GetOperationRange("W", context.currentMode, context.beginRange, context.endRange)) {
            context.op = CommandOperation::Delete;
        }
    } else if (mappedCommand == id_DeleteAWord) {
        if (GetOperationRange("aw", context.currentMode, context.beginRange, context.endRange)) {
            context.op = CommandOperation::Delete;
        }
    } else if (mappedCommand == id_DeleteAWORD) {
        if (GetOperationRange("aW", context.currentMode, context.beginRange, context.endRange)) {
            context.op = CommandOperation::Delete;
        }
    } else if (mappedCommand == id_DeleteInnerWord) {
        if (GetOperationRange("iw", context.currentMode, context.beginRange, context.endRange)) {
            context.op = CommandOperation::Delete;
        }
    } else if (mappedCommand == id_DeleteInnerWORD) {
        if (GetOperationRange("iW", context.currentMode, context.beginRange, context.endRange)) {
            context.op = CommandOperation::Delete;
        }
    } else if (mappedCommand == id_ChangeToLineEnd) {
        if (GetOperationRange("$", context.currentMode, context.beginRange, context.endRange)) {
            context.op = CommandOperation::Delete;
            context.commandResult.modeSwitch = EditorMode::Insert;
        }
    } else if (mappedCommand == id_VisualChange) {
        // Only in visual mode; delete selected block
        if (GetOperationRange("visual", context.currentMode, context.beginRange, context.endRange)) {
            context.op = CommandOperation::Delete;
            context.commandResult.modeSwitch = EditorMode::Insert;
        }
    } else if (mappedCommand == id_ChangeLine) {
        if (GetOperationRange("line", context.currentMode, context.beginRange, context.endRange)) {
            context.op = CommandOperation::DeleteLines;
            context.commandResult.modeSwitch = EditorMode::Insert;
        }
    } else if (mappedCommand == id_ChangeWord) {
        if (GetOperationRange("cw", context.currentMode, context.beginRange, context.endRange)) {
            context.op = CommandOperation::Delete;
            context.commandResult.modeSwitch = EditorMode::Insert;
        }
    } else if (mappedCommand == id_ChangeWORD) {
        if (GetOperationRange("cW", context.currentMode, context.beginRange, context.endRange)) {
            context.op = CommandOperation::Delete;
            context.commandResult.modeSwitch = EditorMode::Insert;
        }
    } else if (mappedCommand == id_ChangeAWord) {
        if (GetOperationRange("aw", context.currentMode, context.beginRange, context.endRange)) {
            context.op = CommandOperation::Delete;
            context.commandResult.modeSwitch = EditorMode::Insert;
        }
    } else if (mappedCommand == id_ChangeAWORD) {
        if (GetOperationRange("aW", context.currentMode, context.beginRange, context.endRange)) {
            context.op = CommandOperation::Delete;
            context.commandResult.modeSwitch = EditorMode::Insert;
        }
    } else if (mappedCommand == id_ChangeInnerWord) {
        if (GetOperationRange("iw", context.currentMode, context.beginRange, context.endRange)) {
            context.op = CommandOperation::Delete;
            context.commandResult.modeSwitch = EditorMode::Insert;
        }
    } else if (mappedCommand == id_ChangeInnerWORD) {
        if (GetOperationRange("iW", context.currentMode, context.beginRange, context.endRange)) {
            context.op = CommandOperation::Delete;
            context.commandResult.modeSwitch = EditorMode::Insert;
        }
    } else if (mappedCommand == id_ChangeIn) {
        if (!context.keymap.captureChars.empty()) {
            auto range = buffer->FindMatchingPair(bufferCursor, context.keymap.captureChars[0]);
            if (range.first.Valid() && range.second.Valid()) {
                if ((range.first + 1) == range.second) {
                    // A closed pair (); so insert between them 
                    currentWindow->SetBufferCursor(range.first + 1);
                    context.commandResult.modeSwitch = EditorMode::Insert;
                    return true;
                } else {
                    GlyphIterator lineEnd = context.buffer.GetLinePos(range.first, LineLocation::LineCRBegin);
                    if (lineEnd.Valid() && lineEnd < range.second) {
                        GlyphIterator lineStart = context.buffer.GetLinePos(range.first, LineLocation::LineBegin);
                        auto offsetStart = (range.first.index - lineStart.index);

                        // If change in a pair of delimiters that are on separate lines, then
                        // we remove everything and replace with 2 CRs and an indent based on the start bracket
                        // Since Zep doesn't auto indent, this is the best we can do for now.
                        context.replaceRangeMode = ReplaceRangeMode::Replace;
                        context.op = CommandOperation::Replace;

                        auto offsetText = std::string(offsetStart + 4, ' ');
                        auto offsetBracket = std::string(offsetStart, ' ');
                        context.tempReg.text = std::string("\n") + offsetText + "\n" + offsetBracket;
                        context.pRegister = &context.tempReg;
                        context.beginRange = range.first + 1;
                        context.endRange = range.second;
                        context.cursorAfterOverride = range.first + (long) offsetText.length() + 2;
                        context.commandResult.modeSwitch = EditorMode::Insert;
                    } else {
                        context.beginRange = range.first + 1; // returned range is inclusive
                        context.endRange = range.second;
                        context.op = CommandOperation::Delete;
                        context.commandResult.modeSwitch = EditorMode::Insert;
                    }
                }
            }
        }
    } else if (mappedCommand == id_SubstituteLine) {
        // Delete whole line and go to insert mode
        context.beginRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineBegin);
        context.endRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineCRBegin);
        context.op = CommandOperation::Delete;
        context.commandResult.modeSwitch = EditorMode::Insert;
    } else if (mappedCommand == id_Substitute) {
        // Only in visual mode; delete selected block and go to insert mode
        // Just delete under m_bufferCursor and insert
        if (GetOperationRange("cursor", context.currentMode, context.beginRange, context.endRange)) {
            context.op = CommandOperation::Delete;
            context.commandResult.modeSwitch = EditorMode::Insert;
        }
    } else if (mappedCommand == id_VisualSubstitute) {
        if (GetOperationRange("visual", context.currentMode, context.beginRange, context.endRange)) {
            context.op = CommandOperation::Delete;
            context.commandResult.modeSwitch = EditorMode::Insert;
        }
    } else if (mappedCommand == id_Find) {
        if (!context.keymap.captureChars.empty()) {
            currentWindow->SetBufferCursor(context.buffer.FindOnLineMotion(bufferCursor, (const uint8_t *) &context.keymap.captureChars[0], Direction::Forward));
            m_lastFind = context.keymap.captureChars[0];
            m_lastFindDirection = Direction::Forward;
        }
        return true;
    } else if (mappedCommand == id_FindBackwards) {
        if (!context.keymap.captureChars.empty()) {
            currentWindow->SetBufferCursor(context.buffer.FindOnLineMotion(bufferCursor, (const uint8_t *) &context.keymap.captureChars[0], Direction::Backward));
            m_lastFind = context.keymap.captureChars[0];
            m_lastFindDirection = Direction::Backward;
        }
        return true;
    } else if (mappedCommand == id_FindNext) {
        currentWindow->SetBufferCursor(context.buffer.FindOnLineMotion(bufferCursor, (const uint8_t *) m_lastFind.c_str(), m_lastFindDirection));
        return true;
    } else if (mappedCommand == id_FindNextDelimiter) {
        int32_t findIndex = 0;
        std::string delims = "\n(){}[]";
        Direction dir = Direction::Forward;

        GlyphIterator loc;
        loc = context.buffer.FindFirstCharOf(bufferCursor, delims, findIndex, dir);

        if (findIndex > 0) {
            // Make a new end location
            auto end_loc = loc;

            std::string closing;
            std::string opening = std::string(1, delims[findIndex]);

            // opening bracket
            if (findIndex & 0x1) {
                end_loc++;
                closing = delims[findIndex + 1];
            } else {
                end_loc--;
                closing = delims[findIndex - 1];
                dir = Direction::Backward;
            }
            std::string openClose = opening + closing;

            // Track open/close bracket pairs
            int closingCount = 1;

            for (;;) {
                // Find the next open or close of the current delim type
                int newIndex;
                end_loc = context.buffer.FindFirstCharOf(end_loc, openClose, newIndex, dir);

                // Fell off, no find
                if (newIndex < 0) break;

                // Found another opener/no good
                if (newIndex == 0) {
                    closingCount++;
                }
                    // Found a closer
                else if (newIndex == 1) {
                    closingCount--;
                    if (closingCount == 0) {
                        currentWindow->SetBufferCursor(end_loc);
                        return true;
                    }
                }

                if (dir == Direction::Forward) {
                    if (end_loc == context.buffer.End()) break;
                    end_loc++;
                } else {
                    if (end_loc == context.buffer.Begin()) break;
                    end_loc--;
                }
            }
        }
        return false;
    } else if (mappedCommand == id_Append) {
        // Cursor append
        cursorItr.MoveClamped(1, LineLocation::LineCRBegin);
        currentWindow->SetBufferCursor(cursorItr);
        context.commandResult.modeSwitch = EditorMode::Insert;
        return true;
    } else if (mappedCommand == id_AppendToLine) {
        GlyphIterator appendItr = context.buffer.GetLinePos(bufferCursor, LineLocation::LineLastNonCR);
        appendItr.MoveClamped(1, LineLocation::LineCRBegin);
        currentWindow->SetBufferCursor(appendItr);
        context.commandResult.modeSwitch = EditorMode::Insert;
        return true;
    } else if (mappedCommand == id_InsertAtFirstChar) {
        currentWindow->SetBufferCursor(context.buffer.GetLinePos(bufferCursor, LineLocation::LineFirstGraphChar));
        context.commandResult.modeSwitch = EditorMode::Insert;
        return true;
    } else if (mappedCommand == id_MotionNextFirstChar) {
        currentWindow->MoveCursorY(1);
        currentWindow->SetBufferCursor(context.buffer.GetLinePos(currentWindow->GetBufferCursor(), LineLocation::LineBegin));
        return true;
    } else if (mappedCommand == id_Replace) {
        if (!context.keymap.captureChars.empty()) {
            context.commandResult.flags |= CommandResultFlags::HandledCount;

            if (!bufferCursor.PeekByteOffset(context.keymap.TotalCount()).Valid()) {
                // Outside the valid buffer; an invalid replace with count!
                return true;
            }

            context.replaceRangeMode = ReplaceRangeMode::Fill;
            context.op = CommandOperation::Replace;
            context.tempReg.text = context.keymap.captureChars[0];
            context.pRegister = &context.tempReg;

            // Get the range from visual, or use the cursor location
            if (!GetOperationRange("visual", context.currentMode, context.beginRange, context.endRange)) {
                context.beginRange = cursorItr;
                context.endRange = cursorItr.PeekLineClamped(context.keymap.TotalCount(), LineLocation::LineCRBegin);
            }

            context.commandResult.modeSwitch = DefaultMode();
        }
    } else if (mappedCommand == id_ChangeToChar) {
        if (!context.keymap.captureChars.empty()) {
            context.beginRange = bufferCursor;
            context.endRange = buffer->FindOnLineMotion(bufferCursor, (const uint8_t *) &context.keymap.captureChars[0], Direction::Forward);
            context.op = CommandOperation::Delete;
            context.commandResult.modeSwitch = EditorMode::Insert;
        }
    } else if (mappedCommand == id_DeleteToChar) {
        if (!context.keymap.captureChars.empty()) {
            context.beginRange = bufferCursor;
            context.endRange = buffer->FindOnLineMotion(bufferCursor, (const uint8_t *) &context.keymap.captureChars[0], Direction::Forward);
            context.op = CommandOperation::Delete;
        }
    } else if (currentMode == EditorMode::Insert) {
        // If not a single char, then we are trying to input a special, which isn't allowed
        // TODO: Cleaner detection of this?
        // Special case for 'j + another character' which is an insert
        if (true) // context.keymap.commandWithoutGroups.size() == 1 || ((context.keymap.commandWithoutGroups.size() == 2) && context.keymap.commandWithoutGroups[0] == 'j'))
        {
            context.beginRange = context.bufferCursor;
            context.tempReg.text = context.keymap.commandWithoutGroups;
            context.pRegister = &context.tempReg;
            context.op = CommandOperation::Insert;
            context.commandResult.modeSwitch = EditorMode::Insert;
            context.commandResult.flags |= CommandResultFlags::HandledCount;

            // Insert grouping command if necessary
            if (context.fullCommand == " ") {
                context.commandResult.flags = ZSetFlags(context.commandResult.flags, CommandResultFlags::BeginUndoGroup, shouldGroupInserts);
            }
        } else {
            return false;
        }
    }

    // Update the registers based on context state
    context.UpdateRegisters();

    // Setup command, if any
    if (context.op == CommandOperation::Delete || context.op == CommandOperation::DeleteLines) {
        auto cmd = std::make_shared<ZepCommand_DeleteRange>(
            context.buffer,
            context.beginRange,
            context.endRange,
            context.bufferCursor,
            context.cursorAfterOverride);
        context.commandResult.command = std::static_pointer_cast<ZepCommand>(cmd);
        context.commandResult.flags = ZSetFlags(context.commandResult.flags, CommandResultFlags::BeginUndoGroup);
        return true;
    } else if (context.op == CommandOperation::Insert && !context.pRegister->text.empty()) {
        auto cmd = std::make_shared<ZepCommand_Insert>(
            context.buffer,
            context.beginRange,
            context.pRegister->text,
            context.bufferCursor,
            context.cursorAfterOverride);
        context.commandResult.command = std::static_pointer_cast<ZepCommand>(cmd);
        return true;
    } else if (context.op == CommandOperation::Replace && !context.pRegister->text.empty()) {
        auto cmd = std::make_shared<ZepCommand_ReplaceRange>(
            context.buffer,
            context.replaceRangeMode,
            context.beginRange,
            context.endRange,
            context.pRegister->text,
            context.bufferCursor,
            context.cursorAfterOverride);
        context.commandResult.command = std::static_pointer_cast<ZepCommand>(cmd);
        return true;
    } else if (context.op == CommandOperation::Copy || context.op == CommandOperation::CopyLines) {
        // Put the cursor where the command says it should be
        currentWindow->SetBufferCursor(context.cursorAfterOverride);
        return true;
    }

    return false;
} // namespace Zep

void ZepMode::ResetCommand() {
    m_currentCommand.clear();
}

bool ZepMode::GetOperationRange(const std::string &op, EditorMode currentMode, GlyphIterator &beginRange, GlyphIterator &endRange) const {
    auto *buffer = currentWindow->buffer;
    const auto bufferCursor = currentWindow->GetBufferCursor();

    beginRange = GlyphIterator();
    if (op == "visual") {
        if (currentMode == EditorMode::Visual) {
            auto range = GetInclusiveVisualRange();
            beginRange = range.first;
            endRange = range.second.Peek(1);
        }
    }
        // Whole line
    else if (op == "line") {
        beginRange = buffer->GetLinePos(bufferCursor, LineLocation::LineBegin);
        endRange = buffer->GetLinePos(bufferCursor, LineLocation::BeyondLineEnd);

        // Special case; if this is the last line, remove the previous newline in favour of the terminator
        if (endRange.Char() == 0) {
            beginRange.Move(-1);
        }
    } else if (op == "$") {
        beginRange = bufferCursor;
        endRange = buffer->GetLinePos(bufferCursor, LineLocation::LineCRBegin);
    } else if (op == "w") {
        beginRange = bufferCursor;
        endRange = buffer->WordMotion(bufferCursor, SearchType::Word, Direction::Forward);
    } else if (op == "cw") {
        // Change word doesn't extend over the next space
        beginRange = bufferCursor;
        endRange = buffer->ChangeWordMotion(bufferCursor, SearchType::Word, Direction::Forward);
    } else if (op == "cW") {
        beginRange = bufferCursor;
        endRange = buffer->ChangeWordMotion(bufferCursor, SearchType::WORD, Direction::Forward);
    } else if (op == "W") {
        beginRange = bufferCursor;
        endRange = buffer->WordMotion(bufferCursor, SearchType::WORD, Direction::Forward);
    } else if (op == "aw") {
        auto range = buffer->AWordMotion(bufferCursor, SearchType::Word);
        beginRange = range.first;
        endRange = range.second;
    } else if (op == "aW") {
        auto range = buffer->AWordMotion(bufferCursor, SearchType::WORD);
        beginRange = range.first;
        endRange = range.second;
    } else if (op == "iw") {
        auto range = buffer->InnerWordMotion(bufferCursor, SearchType::Word);
        beginRange = range.first;
        endRange = range.second;
    } else if (op == "iW") {
        auto range = buffer->InnerWordMotion(bufferCursor, SearchType::WORD);
        beginRange = range.first;
        endRange = range.second;
    } else if (op == "cursor") {
        const auto &cursorItr = bufferCursor;
        beginRange = cursorItr;
        endRange = cursorItr.PeekLineClamped(1);
    }
    return beginRange.Valid();
}

void ZepMode::UpdateVisualSelection() {
    // Visual mode update - after a command
    if (currentMode == EditorMode::Visual) {
        // Update the visual range
        m_visualEnd = m_lineWise ?
                      currentWindow->buffer->GetLinePos(currentWindow->GetBufferCursor(), LineLocation::LineCRBegin) :
                      currentWindow->GetBufferCursor();

        auto range = GetInclusiveVisualRange();
        currentWindow->buffer->SetSelection(range);
    }
}

bool ZepMode::HandleExCommand(std::string strCommand) {
    if (strCommand.empty()) return false;

    auto eraseExtKey = [](std::string &str) {
        auto pos = str.find_last_of('<');
        if (pos != std::string::npos) {
            str.erase(pos, str.size() - pos);
        }
    };

    if (m_lastKey == ImGuiKey_Backspace) {
        eraseExtKey(strCommand);

        // Remove the previous character
        if (!strCommand.empty()) strCommand.pop_back();

        if (strCommand.empty()) {
            currentWindow->SetBufferCursor(m_exCommandStartLocation);
            return true;
        }

        m_currentCommand = strCommand;
        return false;
    }

    if (m_lastKey == ImGuiKey_Escape) {
        currentWindow->SetBufferCursor(m_exCommandStartLocation);
        return true;
    }

    if (m_lastKey == ImGuiKey_Enter) {
        assert(currentWindow);

        auto *buffer = currentWindow->buffer;
        auto bufferCursor = currentWindow->GetBufferCursor();

        // Just exit Ex mode when finished the search
        if (strCommand[0] == '/' || strCommand[0] == '?') return true;

        // Remove the return
        eraseExtKey(strCommand);
        if (strCommand.empty()) return false;

        if (editor.Broadcast(std::make_shared<ZepMessage>(Msg::HandleCommand, strCommand))) return true;

        auto pCommand = editor.FindExCommand(strCommand.substr(1));
        if (pCommand) {
            auto strTok = string_split(strCommand, " ");
            pCommand->Run(strTok);
        } else if (strCommand == ":reg") {
            std::ostringstream str;
            str << "--- Registers ---" << '\n';
            for (auto &reg: editor.GetRegisters()) {
                if (!reg.second.text.empty()) {
                    std::string displayText = reg.second.text;
                    displayText = string_replace(displayText, "\n", "^J");
                    displayText = string_replace(displayText, "\r", "");
                    str << "\"" << reg.first << "   " << displayText << '\n';
                }
            }
            editor.SetCommandText(str.str());
        } else if (strCommand == ":map") {
            std::ostringstream str;

            // TODO: CM: this overflows; need to page the output
            str << "--- Mappings ---" << std::endl;
            str << "Normal Maps:" << std::endl;
            keymap_dump(m_normalMap, str);
            str << "Insert Maps:" << std::endl;
            keymap_dump(m_insertMap, str);
            str << "Visual Maps:" << std::endl;
            keymap_dump(m_visualMap, str);

            auto pMapBuffer = editor.GetEmptyBuffer("Mappings");

            pMapBuffer->SetFileFlags(FileFlags::Locked | FileFlags::ReadOnly);
            pMapBuffer->SetText(str.str());
            editor.activeTabWindow->AddWindow(pMapBuffer, nullptr, RegionLayoutType::VBox);
        } else if (strCommand.find(":tabedit") == 0) {
            auto pTab = editor.AddTabWindow();
            auto strTok = string_split(strCommand, " ");
            if (strTok.size() > 1) {
                if (strTok[1] == "%") {
                    pTab->AddWindow(buffer, nullptr, RegionLayoutType::HBox);
                } else {
                    auto fname = strTok[1];
                    auto pBuffer = editor.GetFileBuffer(fname);
                    pTab->AddWindow(pBuffer, nullptr, RegionLayoutType::HBox);
                }
            }
            editor.SetCurrentTabWindow(pTab);
        } else if (strCommand.find(":vsplit") == 0) {
            auto pTab = editor.activeTabWindow;
            auto strTok = string_split(strCommand, " ");
            if (strTok.size() > 1) {
                if (strTok[1] == "%") {
                    pTab->AddWindow(currentWindow->buffer, currentWindow, RegionLayoutType::HBox);
                } else {
                    auto fname = strTok[1];
                    auto pBuffer = editor.GetFileBuffer(fname);
                    pTab->AddWindow(pBuffer, currentWindow, RegionLayoutType::HBox);
                }
            } else {
                pTab->AddWindow(currentWindow->buffer, currentWindow, RegionLayoutType::HBox);
            }
        } else if (strCommand.find(":hsplit") == 0 || strCommand.find(":split") == 0) {
            auto pTab = editor.activeTabWindow;
            auto strTok = string_split(strCommand, " ");
            if (strTok.size() > 1) {
                if (strTok[1] == "%") {
                    pTab->AddWindow(currentWindow->buffer, currentWindow, RegionLayoutType::VBox);
                } else {
                    auto fname = strTok[1];
                    auto pBuffer = editor.GetFileBuffer(fname);
                    pTab->AddWindow(pBuffer, currentWindow, RegionLayoutType::VBox);
                }
            } else {
                pTab->AddWindow(currentWindow->buffer, currentWindow, RegionLayoutType::VBox);
            }
        } else if (strCommand.find(":e") == 0) {
            auto strTok = string_split(strCommand, " ");
            if (strTok.size() > 1) {
                auto fname = strTok[1];
                auto pBuffer = editor.GetFileBuffer(fname);
                currentWindow->SetBuffer(pBuffer);
            }
        } else if (strCommand.find(":w") == 0) {
            auto strTok = string_split(strCommand, " ");
            if (strTok.size() > 1) {
                auto fname = strTok[1];
                currentWindow->buffer->SetFilePath(fname);
            }
            editor.SaveBuffer(*currentWindow->buffer);
        } else if (strCommand == ":close" || strCommand == ":clo") {
            editor.activeTabWindow->CloseActiveWindow();
        } else if (strCommand[1] == 'q') {
            if (strCommand == ":q") {
                editor.activeTabWindow->CloseActiveWindow();
            }
        } else if (strCommand.find(":ZConfigPath") == 0) {
            editor.SetCommandText(editor.fileSystem->configPath.string());
        } else if (strCommand.find(":ZConfig") == 0) {
            auto pBuffer = editor.GetFileBuffer(editor.fileSystem->configPath / "zep.cfg");
            currentWindow->SetBuffer(pBuffer);
        } else if (strCommand.find(":cd") == 0) {
            editor.SetCommandText(editor.fileSystem->configPath.string());
        } else if (strCommand.find(":ZTestFlash") == 0) {
            if (buffer->syntax) {
                FlashType flashType = FlashType::Flash;
                float time = 1.0f;
                auto strTok = string_split(strCommand, " ");
                if (strTok.size() > 1 && std::stoi(strTok[1]) > 0) {
                    flashType = FlashType::Flash;
                }
                if (strTok.size() > 2) {
                    try {
                        time = std::stof(strTok[2]);
                    } catch (std::exception &) {}
                }
                buffer->BeginFlash(time, flashType, GlyphRange(buffer->Begin(), buffer->End()));
            }
        } else if (strCommand.find(":ZTestMarkers") == 0) {
            static uint32_t unique = 0;
            int markerSelection = 0;
            auto strTok = string_split(strCommand, " ");
            if (strTok.size() > 1) {
                markerSelection = std::stoi(strTok[1]);
            }
            auto marker = std::make_shared<RangeMarker>(*buffer);
            GlyphIterator start;
            GlyphIterator end;

            if (currentWindow->buffer->HasSelection()) {
                // Markers are exclusive
                auto range = currentWindow->buffer->selection;
                start = range.first;
                end = range.second.Peek(1);
            } else {
                start = buffer->GetLinePos(bufferCursor, LineLocation::LineFirstGraphChar);
                end = buffer->GetLinePos(bufferCursor, LineLocation::LineLastGraphChar) + 1;
            }
            marker->SetRange(ByteRange(start.index, end.index));
            switch (markerSelection) {
                case 5:marker->SetColors(ThemeColor::Error, ThemeColor::Text, ThemeColor::Error);
                    marker->name = "All Marker";
                    marker->description = "This is an example tooltip\nThey can be added to any range of characters";
                    marker->displayType = RangeMarkerDisplayType::All;
                    break;
                case 4:marker->SetColors(ThemeColor::Error, ThemeColor::Text, ThemeColor::Error);
                    marker->name = "Filled Marker";
                    marker->description = "This is an example tooltip\nThey can be added to any range of characters";
                    marker->displayType = RangeMarkerDisplayType::Tooltip | RangeMarkerDisplayType::Underline | RangeMarkerDisplayType::Indicator | RangeMarkerDisplayType::Background;
                    break;
                case 3:marker->SetColors(ThemeColor::Background, ThemeColor::Text, ZepTheme::GetUniqueColor(unique++));
                    marker->name = "Underline Marker";
                    marker->description = "This is an example tooltip\nThey can be added to any range of characters";
                    marker->displayType = RangeMarkerDisplayType::Tooltip | RangeMarkerDisplayType::Underline | RangeMarkerDisplayType::CursorTip;
                    break;
                case 2:marker->SetColors(ThemeColor::Background, ThemeColor::Text, ThemeColor::Warning);
                    marker->name = "Tooltip";
                    marker->description = "This is an example tooltip\nThey can be added to any range of characters";
                    marker->displayType = RangeMarkerDisplayType::Tooltip;
                    break;
                case 1:marker->SetColors(ThemeColor::Background, ThemeColor::Text, ThemeColor::Warning);
                    marker->name = "Warning";
                    marker->description = "This is an example warning mark";
                    break;
                case 0:
                default:marker->SetColors(ThemeColor::Background, ThemeColor::Text, ThemeColor::Error);
                    marker->name = "Error";
                    marker->description = "This is an example error mark";
            }
            SwitchMode(DefaultMode());
        } else if (strCommand == ":ZTabs") {
            buffer->ToggleFileFlag(FileFlags::InsertTabs);
        } else if (strCommand == ":ZShowCR") {
            currentWindow->ToggleFlag(WindowFlags::ShowCR);
        } else if (strCommand == ":ZShowLineNumbers") {
            currentWindow->ToggleFlag(WindowFlags::ShowLineNumbers);
        } else if (strCommand == ":ZWrapText") {
            // Wrapping is not fully supported yet, but useful for the Orca optional mode.
            // To enable wrapping fully, the editor needs to scroll in X as well as Y...
            currentWindow->ToggleFlag(WindowFlags::WrapText);
        } else if (strCommand == ":ZShowIndicators") {
            currentWindow->ToggleFlag(WindowFlags::ShowIndicators);
        } else if (strCommand == ":ZShowInput") {
            editor.config.showNormalModeKeyStrokes = !editor.config.showNormalModeKeyStrokes;
        } else if (strCommand == ":ls") {
            std::ostringstream str;
            str << "--- buffers ---" << '\n';
            int index = 0;
            for (auto &editor_buffer: editor.buffers) {
                if (!editor_buffer->name.empty()) {
                    str << (editor_buffer->IsHidden() ? "h" : " ");
                    str << (currentWindow->buffer == editor_buffer.get() ? "*" : " ");
                    str << (editor_buffer->HasFileFlags(FileFlags::Dirty) ? "+" : " ");
                    str << index++ << " : " << string_replace(editor_buffer->name, "\n", "^J") << '\n';
                }
            }
            editor.SetCommandText(str.str());
        } else if (strCommand.find(":bu") == 0) {
            auto strTok = string_split(strCommand, " ");

            if (strTok.size() > 1) {
                try {
                    auto index = std::stoi(strTok[1]);
                    auto current = 0;
                    for (auto &editor_buffer: editor.buffers) {
                        if (index == current) {
                            currentWindow->SetBuffer(editor_buffer.get());
                        }
                        current++;
                    }
                } catch (std::exception &) {}
            }
        } else {
            editor.SetCommandText("Not a command");
        }
        return true;
    } else if (!m_currentCommand.empty() && (m_currentCommand[0] == '/' || m_currentCommand[0] == '?')) {
        // TODO: Busy editing the search string; do the search
        if (m_currentCommand.length() > 0) {
            auto pWindow = currentWindow;
            auto *buffer = pWindow->buffer;
            auto searchString = m_currentCommand.substr(1);

            buffer->ClearRangeMarkers(RangeMarkerType::Search);

            uint32_t numMarkers = 0;
            GlyphIterator start = buffer->Begin();

            if (!searchString.empty()) {
                static const uint32_t MaxMarkers = 1000;
                while (numMarkers < MaxMarkers) {
                    auto found = buffer->Find(start, (uint8_t *) &searchString[0], (uint8_t *) &searchString[searchString.length()]);
                    if (!found.Valid()) break;

                    start = found + 1;

                    auto marker = std::make_shared<RangeMarker>(*buffer);
                    marker->SetColors(ThemeColor::VisualSelectBackground, ThemeColor::Text);
                    marker->SetRange(ByteRange(found.index, found.PeekByteOffset(long(searchString.size())).index));
                    marker->displayType = RangeMarkerDisplayType::Background;
                    marker->markerType = RangeMarkerType::Search;

                    numMarkers++;
                }
            }

            Direction dir = (m_currentCommand[0] == '/') ? Direction::Forward : Direction::Backward;
            m_lastSearchDirection = dir;

            // Find the one on or in front of the cursor, in either direction.
            auto startLocation = m_exCommandStartLocation;
            if (dir == Direction::Forward) startLocation--;
            else startLocation++;

            auto pMark = buffer->FindNextMarker(startLocation, dir, RangeMarkerType::Search);
            if (pMark) {
                pWindow->SetBufferCursor(GlyphIterator(buffer, pMark->range.first));
                pMark->SetBackgroundColor(ThemeColor::Info);
            } else {
                pWindow->SetBufferCursor(m_exCommandStartLocation);
            }
        }
    }
    return false;
}

const KeyMap &ZepMode::GetKeyMappings(EditorMode mode) const {
    return mode == EditorMode::Visual ? m_visualMap : mode == EditorMode::Normal ? m_normalMap : m_insertMap;
}

void ZepMode::AddKeyMapWithCountRegisters(const std::vector<KeyMap *> &maps, const std::vector<std::string> &commands, const StringId &id) {
    for (auto &m: maps) {
        for (auto &c: commands) {
            keymap_add({m}, {"<D><R>" + c}, id);
            keymap_add({m}, {"<R>" + c}, id);
            keymap_add({m}, {"<D>" + c}, id);
            keymap_add({m}, {c}, id);
        }
    }
}

void ZepMode::AddNavigationKeyMaps(bool allowInVisualMode) {
    std::vector<KeyMap *> navigationMaps = {&m_normalMap};
    if (allowInVisualMode) {
        navigationMaps.push_back(&m_visualMap);
    }

    // Up/Down/Left/Right
    AddKeyMapWithCountRegisters(navigationMaps, {"j", "<Down>"}, id_MotionDown);
    AddKeyMapWithCountRegisters(navigationMaps, {"k", "<Up>"}, id_MotionUp);
    AddKeyMapWithCountRegisters(navigationMaps, {"l", "<Right>"}, id_MotionRight);
    AddKeyMapWithCountRegisters(navigationMaps, {"h", "<Left>"}, id_MotionLeft);

    // Page Motions
    AddKeyMapWithCountRegisters(navigationMaps, {"<C-f>", "<PageDown>"}, id_MotionPageForward);
    AddKeyMapWithCountRegisters(navigationMaps, {"<C-b>", "<PageUp>"}, id_MotionPageBackward);
    AddKeyMapWithCountRegisters(navigationMaps, {"<C-d>"}, id_MotionHalfPageForward);
    AddKeyMapWithCountRegisters(navigationMaps, {"<C-u>"}, id_MotionHalfPageBackward);
    AddKeyMapWithCountRegisters(navigationMaps, {"G"}, id_MotionGotoLine);

    // Line Motions
    AddKeyMapWithCountRegisters(navigationMaps, {"$", "<End>"}, id_MotionLineEnd);
    AddKeyMapWithCountRegisters(navigationMaps, {"^"}, id_MotionLineFirstChar);
    keymap_add(navigationMaps, {"0", "<Home>"}, id_MotionLineBegin);

    // Word motions
    AddKeyMapWithCountRegisters(navigationMaps, {"w"}, id_MotionWord);
    AddKeyMapWithCountRegisters(navigationMaps, {"b"}, id_MotionBackWord);
    AddKeyMapWithCountRegisters(navigationMaps, {"W"}, id_MotionWORD);
    AddKeyMapWithCountRegisters(navigationMaps, {"B"}, id_MotionBackWORD);
    AddKeyMapWithCountRegisters(navigationMaps, {"e"}, id_MotionEndWord);
    AddKeyMapWithCountRegisters(navigationMaps, {"E"}, id_MotionEndWORD);
    AddKeyMapWithCountRegisters(navigationMaps, {"ge"}, id_MotionBackEndWord);
    AddKeyMapWithCountRegisters(navigationMaps, {"gE"}, id_MotionBackEndWORD);
    AddKeyMapWithCountRegisters(navigationMaps, {"gg"}, id_MotionGotoBeginning);

    // Navigate between splits
    keymap_add(navigationMaps, {"<C-j>"}, id_MotionDownSplit);
    keymap_add(navigationMaps, {"<C-l>"}, id_MotionRightSplit);
    keymap_add(navigationMaps, {"<C-k>"}, id_MotionUpSplit);
    keymap_add(navigationMaps, {"<C-h>"}, id_MotionLeftSplit);

    // Arrows always navigate in insert mode
    keymap_add({&m_insertMap}, {"<Down>"}, id_MotionDown);
    keymap_add({&m_insertMap}, {"<Up>"}, id_MotionUp);
    keymap_add({&m_insertMap}, {"<Right>"}, id_MotionRight);
    keymap_add({&m_insertMap}, {"<Left>"}, id_MotionLeft);

    keymap_add({&m_insertMap}, {"<End>"}, id_MotionLineBeyondEnd);
    keymap_add({&m_insertMap}, {"<Home>"}, id_MotionLineBegin);
}

void ZepMode::AddSearchKeyMaps() {
    // Normal mode searching
    AddKeyMapWithCountRegisters({&m_normalMap}, {"f<.>"}, id_Find);
    AddKeyMapWithCountRegisters({&m_normalMap}, {"F<.>"}, id_FindBackwards);
    AddKeyMapWithCountRegisters({&m_normalMap}, {";"}, id_FindNext);
    AddKeyMapWithCountRegisters({&m_normalMap}, {"%"}, id_FindNextDelimiter);
    AddKeyMapWithCountRegisters({&m_normalMap}, {"n"}, id_MotionNextSearch);
    AddKeyMapWithCountRegisters({&m_normalMap}, {"N"}, id_MotionPreviousSearch);
    keymap_add({&m_normalMap}, {"<F8>"}, id_MotionNextMarker);
    keymap_add({&m_normalMap}, {"<S-F8>"}, id_MotionPreviousMarker);
}

void ZepMode::AddGlobalKeyMaps() {
    // Global bits
    keymap_add({&m_normalMap, &m_insertMap}, {"<C-p>", "<C-,>"}, id_QuickSearch);
    keymap_add({&m_normalMap}, {":", "/", "?"}, id_ExMode);
    keymap_add({&m_normalMap}, {"H"}, id_PreviousTabWindow);
    keymap_add({&m_normalMap}, {"L"}, id_NextTabWindow);
    keymap_add({&m_normalMap}, {"<C-i><C-o>"}, id_SwitchToAlternateFile);
    keymap_add({&m_normalMap}, {"+"}, id_FontBigger);
    keymap_add({&m_normalMap}, {"-"}, id_FontSmaller);
}

CursorType ZepMode::GetCursorType() const {
    switch (currentMode) {
        default:
        case EditorMode::None:
        case EditorMode::Ex:return CursorType::None;
        case EditorMode::Insert:return CursorType::Insert;
        case EditorMode::Normal:return CursorType::Normal;
        case EditorMode::Visual:return m_visualCursorType;
    }
}

void ZepMode::Begin(ZepWindow *pWindow) {
    timer_restart(m_lastKeyPressTimer);

    currentWindow = pWindow;

    if (pWindow) {
        m_visualBegin = pWindow->buffer->Begin();
        m_visualEnd = pWindow->buffer->End();
        pWindow->buffer->ClearSelection();
    }

    // If we are an overlay mode, make sure that the global mode is also begun on the new window
    if (editor.GetGlobalMode() != this) {
        editor.GetGlobalMode()->Begin(pWindow);
    }
}

} // namespace Zep
