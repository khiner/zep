#include "zep/glyph_iterator.h"
#include "zep/buffer.h"

namespace Zep {

GlyphIterator::GlyphIterator(const ZepBuffer *buffer, long offset) : buffer(buffer) {
    if (buffer) index = offset;
}

GlyphIterator::GlyphIterator(const GlyphIterator &itr) = default;


bool GlyphIterator::Valid() const {
    if (!buffer || index < 0 || index >= buffer->workingBuffer.size()) return false;

    // We should never have a valid buffer index but be outside the start of a utf8 glyph
    assert(!utf8_is_trailing(Char()));
    return true;
}

bool GlyphIterator::operator<(const GlyphIterator &rhs) const { return index < rhs.index; }
bool GlyphIterator::operator<=(const GlyphIterator &rhs) const { return index <= rhs.index; }
bool GlyphIterator::operator>(const GlyphIterator &rhs) const { return index > rhs.index; }
bool GlyphIterator::operator>=(const GlyphIterator &rhs) const { return index >= rhs.index; }
bool GlyphIterator::operator==(const GlyphIterator &rhs) const { return index == rhs.index; }
bool GlyphIterator::operator!=(const GlyphIterator &rhs) const { return index != rhs.index; }

GlyphIterator &GlyphIterator::operator=(const GlyphIterator &rhs) = default;

uint8_t GlyphIterator::Char() const { return !buffer ? 0 : buffer->workingBuffer[index]; }

uint8_t GlyphIterator::operator*() const { return !buffer ? 0 : buffer->workingBuffer[index]; }

GlyphIterator &GlyphIterator::MoveClamped(long count, LineLocation clamp) {
    if (!buffer) return *this;

    const auto &gapBuffer = buffer->workingBuffer;
    if (count >= 0) {
        auto lineEnd = buffer->GetLinePos(*this, clamp);
        for (long c = 0; c < count; c++) {
            if (index >= lineEnd.index) {
                break;
            }
            index += utf8_codepoint_length(gapBuffer[index]);
        }
    } else {
        auto lineBegin = buffer->GetLinePos(*this, LineLocation::LineBegin);
        for (long c = count; c < 0; c++) {
            while ((index > lineBegin.index) && utf8_is_trailing(gapBuffer[--index]));
        }
    }

    Clamp();

    return *this;
}

GlyphIterator &GlyphIterator::Move(long count) {
    if (!buffer) return *this;

    const auto &gapBuffer = buffer->workingBuffer;
    if (count >= 0) {
        for (long c = 0; c < count; c++) {
            index += utf8_codepoint_length(gapBuffer[index]);
        }
    } else {
        for (long c = count; c < 0; c++) {
            while ((index > 0) && utf8::internal::is_trail(gapBuffer[--index]));
        }
    }
    Clamp();
    return *this;
}

GlyphIterator GlyphIterator::Clamped() const {
    GlyphIterator itr(*this);
    itr.Clamp();
    return itr;
}

GlyphIterator &GlyphIterator::Clamp() {
    // Invalid thing is still invalid
    if (!buffer) return *this;

    // Clamp to the 0 on the end of the buffer 
    // Since indices are usually exclusive, this allows selection of everything but the 0
    index = std::min(index, long(buffer->workingBuffer.size()) - 1);
    index = std::max(index, 0l);
    return *this;
}

void GlyphIterator::Invalidate() {
    index = -1;
    buffer = nullptr;
}

GlyphIterator GlyphIterator::Peek(long count) const {
    GlyphIterator copy(*this);
    copy.Move(count);
    return copy;
}

GlyphIterator GlyphIterator::PeekLineClamped(long count, LineLocation clamp) const {
    GlyphIterator copy(*this);
    copy.MoveClamped(count, clamp);
    return copy;
}

GlyphIterator GlyphIterator::PeekByteOffset(long count) const {
    return GlyphIterator(buffer, index + count);
}

const GlyphIterator GlyphIterator::operator--(int) {
    GlyphIterator ret(*this);
    Move(-1);
    return ret;
}

const GlyphIterator GlyphIterator::operator++(int) {
    GlyphIterator ret(*this);
    Move(1);
    return ret;
}

GlyphIterator GlyphIterator::operator+(long value) const {
    GlyphIterator ret(*this);
    ret.Move(value);
    return ret;
}

GlyphIterator GlyphIterator::operator-(long value) const {
    GlyphIterator ret(*this);
    ret.Move(-value);
    return ret;
}

void GlyphIterator::operator+=(long count) { Move(count); }
void GlyphIterator::operator-=(long count) { Move(-count); }

GlyphRange::GlyphRange(const GlyphIterator &a, const GlyphIterator &b) : first(a), second(b) {}
GlyphRange::GlyphRange(const ZepBuffer *pBuffer, ByteRange range) : first(pBuffer, range.first), second(pBuffer, range.second) {}
GlyphRange::GlyphRange() = default;

bool GlyphRange::ContainsInclusiveLocation(const GlyphIterator &loc) const { return loc >= first && loc <= second; }

} // namespace Zep
