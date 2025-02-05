#pragma once

#include "zep/utf8/core.h"
#include "gap_buffer.h"

namespace Zep {

struct ZepBuffer;

using ByteIndex = long;
struct ByteRange {
    explicit ByteRange(ByteIndex a = 0, ByteIndex b = 0) : first(a), second(b) {}
    bool ContainsLocation(ByteIndex loc) const { return loc >= first && loc < second; }

    ByteIndex first;
    ByteIndex second;
};

enum class LineLocation {
    None, // Not any specific location
    LineFirstGraphChar, // First non blank character
    LineLastGraphChar, // Last non blank character
    LineLastNonCR, // Last character before the carriage return
    LineBegin, // Beginning of line
    BeyondLineEnd, // The line end of the buffer line (for wrapped lines).
    LineCRBegin, // The first carriage return character
};

// An iterator which is utf8-sensitve, and buffer sensitive; i.e. it knows how to
// walk the gap buffer and skip along in multi byte characters.
// This is the main 'pointer' into the text, used everywhere
// It will clamp automatically and is usually valid if not in the default uninitialized state
struct GlyphIterator {
    explicit GlyphIterator(const ZepBuffer *buffer = nullptr, long offset = 0);
    GlyphIterator(const GlyphIterator &itr);

    bool Valid() const;
    void Invalidate();
    bool operator<(const GlyphIterator &rhs) const;
    bool operator<=(const GlyphIterator &rhs) const;
    bool operator>(const GlyphIterator &rhs) const;
    bool operator>=(const GlyphIterator &rhs) const;
    bool operator==(const GlyphIterator &rhs) const;
    bool operator!=(const GlyphIterator &rhs) const;
    const GlyphIterator operator--(int);
    const GlyphIterator operator++(int);
    void operator-=(long count);
    void operator+=(long count);

    GlyphIterator operator-(long value) const;
    GlyphIterator operator+(long value) const;

    GlyphIterator &operator=(const GlyphIterator &rhs);
    uint8_t Char() const;
    uint8_t operator*() const;
    GlyphIterator &MoveClamped(long count, LineLocation clamp = LineLocation::LineLastNonCR);
    GlyphIterator &Move(long count);
    GlyphIterator &Clamp();

    GlyphIterator Peek(long count) const;
    GlyphIterator PeekLineClamped(long count, LineLocation clamp = LineLocation::LineLastNonCR) const;
    GlyphIterator PeekByteOffset(long count) const;
    GlyphIterator Clamped() const;

    long index = -1;
    const ZepBuffer *buffer = nullptr;
};

inline long CodePointDistance(const GlyphIterator &itr1, const GlyphIterator &itr2) {
    long count = 0;
    auto start = itr1;
    while (start != itr2) {
        count++;
        start++;
    }
    return count;
}

inline long ByteDistance(const GlyphIterator &itr1, const GlyphIterator &itr2) { return itr2.index - itr1.index; }

struct GlyphRange {
    GlyphIterator first;
    GlyphIterator second;

    GlyphRange(const GlyphIterator &a, const GlyphIterator &b);
    GlyphRange(const ZepBuffer *buffer, ByteRange range);
    GlyphRange();

    bool ContainsInclusiveLocation(const GlyphIterator &loc) const;
};

} // namespace Zep
