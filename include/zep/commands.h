#pragma once

#include "zep/buffer.h"

namespace Zep {

class ZepCommand {
public:
    explicit ZepCommand(ZepBuffer &currentMode, const GlyphIterator &cursorBefore = GlyphIterator(), const GlyphIterator &cursorAfter = GlyphIterator())
        : m_buffer(currentMode), m_cursorBefore(cursorBefore), m_cursorAfter(cursorAfter) {
    }

    virtual ~ZepCommand() = default;

    virtual void Redo() = 0;
    virtual void Undo() = 0;

    virtual GlyphIterator GetCursorAfter() const {
        return m_cursorAfter;
    }
    virtual GlyphIterator GetCursorBefore() const {
        return m_cursorBefore;
    }

protected:
    ZepBuffer &m_buffer;
    GlyphIterator m_cursorBefore;
    GlyphIterator m_cursorAfter;
    ChangeRecord m_changeRecord;
};

class ZepCommand_GroupMarker : public ZepCommand {
public:
    explicit ZepCommand_GroupMarker(ZepBuffer &currentMode) : ZepCommand(currentMode) {}
    void Redo() override {};
    void Undo() override {};
};

class ZepCommand_DeleteRange : public ZepCommand {
public:
    ZepCommand_DeleteRange(ZepBuffer &buffer, const GlyphIterator &startIndex, const GlyphIterator &endIndex, const GlyphIterator &cursor = GlyphIterator(), const GlyphIterator &cursorAfter = GlyphIterator());
    ~ZepCommand_DeleteRange() override = default;;

    void Redo() override;
    void Undo() override;

    GlyphIterator m_startIndex;
    GlyphIterator m_endIndex;
};

class ZepCommand_ReplaceRange : public ZepCommand {
public:
    ZepCommand_ReplaceRange(ZepBuffer &buffer, ReplaceRangeMode replaceMode, const GlyphIterator &startIndex, const GlyphIterator &endIndex, std::string ch, const GlyphIterator &cursor = GlyphIterator(),
                            const GlyphIterator &cursorAfter = GlyphIterator());
    ~ZepCommand_ReplaceRange() override = default;;

    void Redo() override;
    void Undo() override;

    GlyphIterator m_startIndex;
    GlyphIterator m_endIndex;

    std::string m_strReplace;
    ReplaceRangeMode m_mode;
};

class ZepCommand_Insert : public ZepCommand {
public:
    ZepCommand_Insert(ZepBuffer &buffer, const GlyphIterator &startIndex, const std::string &str, const GlyphIterator &cursor = GlyphIterator(), const GlyphIterator &cursorAfter = GlyphIterator());
    ~ZepCommand_Insert() override = default;;

    void Redo() override;
    void Undo() override;

    GlyphIterator m_startIndex;
    std::string m_strInsert;
    GlyphIterator m_endIndexInserted;
};

} // namespace Zep
