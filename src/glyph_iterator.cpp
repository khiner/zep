#include "zep/glyph_iterator.h"
#include "zep/buffer.h"

namespace Zep {

GlyphIterator::GlyphIterator(const ZepBuffer *buffer, unsigned long offset)
    : m_pBuffer(buffer) {
    if (buffer) m_index = offset;
}

GlyphIterator::GlyphIterator(const GlyphIterator &itr) = default;

long GlyphIterator::Index() const { return m_index; }

bool GlyphIterator::Valid() const {
    if (!m_pBuffer || m_index < 0 || m_index >= m_pBuffer->workingBuffer.size()) return false;

    // We should never have a valid buffer index but be outside the start of a 
    // utf8 glyph
    assert(!utf8_is_trailing(Char()));
    return true;
}

bool GlyphIterator::operator<(const GlyphIterator &rhs) const { return m_index < rhs.m_index; }
bool GlyphIterator::operator<=(const GlyphIterator &rhs) const { return m_index <= rhs.m_index; }
bool GlyphIterator::operator>(const GlyphIterator &rhs) const { return m_index > rhs.m_index; }
bool GlyphIterator::operator>=(const GlyphIterator &rhs) const { return m_index >= rhs.m_index; }
bool GlyphIterator::operator==(const GlyphIterator &rhs) const { return m_index == rhs.m_index; }
bool GlyphIterator::operator!=(const GlyphIterator &rhs) const { return m_index != rhs.m_index; }

GlyphIterator &GlyphIterator::operator=(const GlyphIterator &rhs) = default;

uint8_t GlyphIterator::Char() const { return !m_pBuffer ? 0 : m_pBuffer->workingBuffer[m_index]; }

uint8_t GlyphIterator::operator*() const { return !m_pBuffer ? 0 : m_pBuffer->workingBuffer[m_index]; }

GlyphIterator &GlyphIterator::MoveClamped(long count, LineLocation clamp) {
    if (!m_pBuffer) return *this;

    const auto &gapBuffer = m_pBuffer->workingBuffer;

    if (count >= 0) {
        auto lineEnd = m_pBuffer->GetLinePos(*this, clamp);
        for (long c = 0; c < count; c++) {
            if (m_index >= lineEnd.m_index) {
                break;
            }
            m_index += utf8_codepoint_length(gapBuffer[m_index]);
        }
    } else {
        auto lineBegin = m_pBuffer->GetLinePos(*this, LineLocation::LineBegin);
        for (long c = count; c < 0; c++) {
            while ((m_index > lineBegin.Index()) && utf8_is_trailing(gapBuffer[--m_index]));
        }
    }

    Clamp();

    return *this;
}

GlyphIterator &GlyphIterator::Move(long count) {
    if (!m_pBuffer) return *this;

    const auto &gapBuffer = m_pBuffer->workingBuffer;
    if (count >= 0) {
        for (long c = 0; c < count; c++) {
            m_index += utf8_codepoint_length(gapBuffer[m_index]);
        }
    } else {
        for (long c = count; c < 0; c++) {
            while ((m_index > 0) && utf8::internal::is_trail(gapBuffer[--m_index]));
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
    if (!m_pBuffer) return *this;

    // Clamp to the 0 on the end of the buffer 
    // Since indices are usually exclusive, this allows selection of everything but the 0
    m_index = std::min(m_index, long(m_pBuffer->workingBuffer.size()) - 1);
    m_index = std::max(m_index, 0l);
    return *this;
}

void GlyphIterator::Invalidate() {
    m_index = -1;
    m_pBuffer = nullptr;
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
    return GlyphIterator(m_pBuffer, m_index + count);
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

void GlyphIterator::operator+=(long count) {
    Move(count);
}

void GlyphIterator::operator-=(long count) {
    Move(-count);
}

GlyphRange::GlyphRange(const GlyphIterator &a, const GlyphIterator &b) : first(a), second(b) {}
GlyphRange::GlyphRange(const ZepBuffer *pBuffer, ByteRange range) : first(pBuffer, range.first), second(pBuffer, range.second) {}
GlyphRange::GlyphRange() = default;

bool GlyphRange::ContainsInclusiveLocation(const GlyphIterator &loc) const { return loc >= first && loc <= second; }

} // namespace Zep
