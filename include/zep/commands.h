#pragma once

#include "zep/buffer.h"

namespace Zep {

struct ZepCommand {
    explicit ZepCommand(ZepBuffer &currentMode, const GlyphIterator &cursorBefore = GlyphIterator(), const GlyphIterator &cursorAfter = GlyphIterator())
        : cursorBefore(cursorBefore), cursorAfter(cursorAfter), m_buffer(currentMode) {}

    virtual ~ZepCommand() = default;

    virtual void Redo() = 0;
    virtual void Undo() = 0;

    GlyphIterator cursorBefore;
    GlyphIterator cursorAfter;

protected:
    ZepBuffer &m_buffer;
    ChangeRecord m_changeRecord;
};

struct ZepCommand_GroupMarker : public ZepCommand {
    explicit ZepCommand_GroupMarker(ZepBuffer &currentMode) : ZepCommand(currentMode) {}
    void Redo() override {};
    void Undo() override {};
};

struct ZepCommand_DeleteRange : public ZepCommand {
    ZepCommand_DeleteRange(ZepBuffer &buffer, const GlyphIterator &startIndex, const GlyphIterator &endIndex, const GlyphIterator &cursor = GlyphIterator(), const GlyphIterator &cursorAfter = GlyphIterator());
    ~ZepCommand_DeleteRange() override = default;;

    void Redo() override;
    void Undo() override;

    GlyphIterator m_startIndex;
    GlyphIterator m_endIndex;
};

struct ZepCommand_ReplaceRange : public ZepCommand {
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

struct ZepCommand_Insert : public ZepCommand {
    ZepCommand_Insert(ZepBuffer &buffer, const GlyphIterator &startIndex, const std::string &str, const GlyphIterator &cursor = GlyphIterator(), const GlyphIterator &cursorAfter = GlyphIterator());
    ~ZepCommand_Insert() override = default;;

    void Redo() override;
    void Undo() override;

    GlyphIterator m_startIndex;
    std::string m_strInsert;
    GlyphIterator m_endIndexInserted;
};

} // namespace Zep
