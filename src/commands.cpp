#include "zep/commands.h"

namespace Zep {

// Delete Range of chars
ZepCommand_DeleteRange::ZepCommand_DeleteRange(ZepBuffer &buffer, const GlyphIterator &start, const GlyphIterator &end, const GlyphIterator &cursor, const GlyphIterator &cursorAfter)
    : ZepCommand(buffer, cursor, cursorAfter.Valid() ? cursorAfter : start), startIndex(start), endIndex(end) {
    assert(startIndex.Valid());
    assert(endIndex.Valid());

    // We never allow deletion of the '0' at the end of the buffer
    if (buffer.workingBuffer.empty()) endIndex = startIndex;
    else endIndex.Clamp();
}

void ZepCommand_DeleteRange::Redo() {
    if (startIndex != endIndex) {
        changeRecord.Clear();
        buffer.Delete(startIndex, endIndex, changeRecord);
    }
}

void ZepCommand_DeleteRange::Undo() {
    if (changeRecord.strDeleted.empty()) return;

    ChangeRecord tempRecord;
    buffer.Insert(startIndex, changeRecord.strDeleted, tempRecord);
}

// Insert a string
ZepCommand_Insert::ZepCommand_Insert(ZepBuffer &buffer, const GlyphIterator &start, const std::string &str, const GlyphIterator &cursor, const GlyphIterator &cursorAfter)
    : ZepCommand(buffer, cursor, cursorAfter.Valid() ? cursorAfter : (start.PeekByteOffset(long(str.size())))), startIndex(start), insert(str) {
    startIndex.Clamp();
}

void ZepCommand_Insert::Redo() {
    changeRecord.Clear();
    bool ret = buffer.Insert(startIndex, insert, changeRecord);
    assert(ret);
    if (ret) endIndexInserted = startIndex.PeekByteOffset(long(insert.size()));
    else endIndexInserted.Invalidate();
}

void ZepCommand_Insert::Undo() {
    if (endIndexInserted.Valid()) {
        ChangeRecord tempRecord;
        buffer.Delete(startIndex, endIndexInserted, tempRecord);
    }
}

// Replace
ZepCommand_ReplaceRange::ZepCommand_ReplaceRange(ZepBuffer &buffer, ReplaceRangeMode currentMode, const GlyphIterator &startIndex, const GlyphIterator &endIndex, std::string strReplace,
                                                 const GlyphIterator &cursor, const GlyphIterator &cursorAfter)
    : ZepCommand(buffer, cursor.Valid() ? cursor : endIndex, cursorAfter.Valid() ? cursorAfter : startIndex), startIndex(startIndex), endIndex(endIndex), replace(std::move(strReplace)), mode(currentMode) {
    this->startIndex.Clamp();
}

void ZepCommand_ReplaceRange::Redo() {
    if (startIndex != endIndex) {
        changeRecord.Clear();
        buffer.Replace(startIndex, endIndex, replace, mode, changeRecord);
    }
}

void ZepCommand_ReplaceRange::Undo() {
    if (startIndex != endIndex) {
        // Replace the range we replaced previously with the old thing
        ChangeRecord temp;
        buffer.Replace(startIndex, mode == ReplaceRangeMode::Fill ? endIndex : startIndex.PeekByteOffset((long) replace.length()), changeRecord.strDeleted, ReplaceRangeMode::Replace, temp);
    }
}

} // namespace Zep
