#include "zep/range_markers.h"
#include "zep/buffer.h"

namespace Zep {

RangeMarker::RangeMarker(ZepBuffer &buffer) : buffer(buffer) {
    onPreBufferInsert = buffer.sigPreInsert.connect([=](ZepBuffer &buffer, const GlyphIterator &itrStart, const std::string &str) {
        HandleBufferInsert(buffer, itrStart, str);
    });
    buffer.sigPreDelete.connect([=](ZepBuffer &buffer, const GlyphIterator &itrStart, const GlyphIterator &itrEnd) {
        HandleBufferDelete(buffer, itrStart, itrEnd);
    });
}

bool RangeMarker::ContainsLocation(const GlyphIterator &loc) const {
    return range.ContainsLocation(loc.index);
}

bool RangeMarker::IntersectsRange(const ByteRange &i) const {
    return i.first < range.second && i.second > range.first;
}

void RangeMarker::SetBackgroundColor(ThemeColor color) { backgroundColor = color; }

void RangeMarker::SetColors(ThemeColor back, ThemeColor text, ThemeColor highlight) {
    backgroundColor = back;
    textColor = text;
    highlightColor = highlight;
}

void RangeMarker::SetAlpha(float a) { alpha = a; }

void RangeMarker::SetRange(ByteRange newRange) {
    auto marker = shared_from_this();
    buffer.ClearRangeMarker(marker);

    range = newRange;
    buffer.AddRangeMarker(marker);
}

// By Default Markers will:
// - Move down if text is inserted before them.
// - Move up if text is deleted before them.
// - Remove themselves from the buffer if text is edited _inside_ them.
// Derived markers can modify this behavior.
// It's up to marker owners to update this behavior if necessary.
// Markers do not act inside the undo/redo system.  They live on the buffer but are not stored with it.  They are adornments that 
// must be managed externally.
void RangeMarker::HandleBufferInsert(ZepBuffer &buf, const GlyphIterator &itrStart, const std::string &str) {
    if (!enabled || itrStart.index > range.second) return;

    auto itrEnd = itrStart + long(str.size());
    if (itrEnd.index <= (range.first + 1)) {
        auto distance = itrEnd.index - itrStart.index;
        auto currentRange = range;
        SetRange(ByteRange(currentRange.first + distance, currentRange.second + distance));
    } else {
        buf.ClearRangeMarker(shared_from_this());
        enabled = false;
    }
}

void RangeMarker::HandleBufferDelete(ZepBuffer &buf, const GlyphIterator &itrStart, const GlyphIterator &itrEnd) {
    if (!enabled || itrStart.index > range.second) return;

    // It's OK to move on the first char; since that is like a shove
    if (itrEnd.index < (range.first + 1)) {
        auto distance = std::min(itrEnd.index, range.first) - itrStart.index;
        auto currentRange = range;
        SetRange(ByteRange(currentRange.first - distance, currentRange.second - distance));
    } else {
        buf.ClearRangeMarker(shared_from_this());
        enabled = false;
    }
}

}; // namespace Zep
