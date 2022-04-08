#pragma once

#include <set>
#include <map>

#include "timer.h"
#include "zep/signals.h"

#include "zep/glyph_iterator.h"
#include "zep/theme.h"


// Range Markers are adornments over the text; they represent any additional marks over the existing text buffer.
// For example, tooltips, underlines, inline widgets, etc.
// Try :ZTestMarkers 5 or :ZTestMarkers 3 after selecting a region of text
namespace Zep {
struct ZepBuffer;

namespace RangeMarkerType {
enum {
    Mark = (1 << 0),
    Search = (1 << 1),
    Widget = (1 << 2),
    LineWidget = (1 << 3),
    All = (Mark | Search | LineWidget | Widget)
};
};

enum class FlashType {
    Flash
};

namespace RangeMarkerDisplayType {
enum {
    Hidden = 0,
    Underline = (1 << 0), // Underline the range
    Background = (1 << 1), // Add a background to the range
    Tooltip = (1 << 2), // Show a tooltip using the name/description
    TooltipAtLine = (1 << 3), // Tooltip shown if the user hovers the line
    CursorTip = (1 << 4), // Tooltip shown if the user cursor is on the Mark
    CursorTipAtLine = (1 << 5), // Tooltip shown if the user cursor is on the Mark line
    Indicator = (1 << 6), // Show an indicator on the left side
    Timed = (1 << 7),
    All = Underline | Tooltip | TooltipAtLine | CursorTip | CursorTipAtLine | Indicator | Background,
};
};

enum class ToolTipPos {
    AboveLine = 0,
    BelowLine = 1,
    RightLine = 2,
    Count = 3
};

struct RangeMarker : std::enable_shared_from_this<RangeMarker> {
    explicit RangeMarker(ZepBuffer &buffer);

    bool ContainsLocation(const GlyphIterator &loc) const;
    bool IntersectsRange(const ByteRange &i) const;

    void SetRange(ByteRange range);
    void SetBackgroundColor(ThemeColor color);
    void SetColors(ThemeColor back = ThemeColor::None, ThemeColor text = ThemeColor::Text, ThemeColor highlight = ThemeColor::Text);
    void SetAlpha(float a);

    void HandleBufferInsert(ZepBuffer &buffer, const GlyphIterator &itrStart, const std::string &str);
    void HandleBufferDelete(ZepBuffer &buffer, const GlyphIterator &itr, const GlyphIterator &itrEnd);

    ZepBuffer &buffer;
    std::string name;
    std::string description;
    uint32_t displayType = RangeMarkerDisplayType::All;
    uint32_t markerType = RangeMarkerType::Mark;
    uint32_t displayRow = 0;
    mutable ThemeColor backgroundColor = ThemeColor::Background;
    mutable ThemeColor highlightColor = ThemeColor::Background; // Used for lines around tip box, underline, etc.
    mutable ThemeColor textColor = ThemeColor::Text;
    mutable float alpha = 1.0f;
    ToolTipPos tipPos = ToolTipPos::AboveLine;
    float duration = 1.0f;
    NVec2f inlineSize;
    bool enabled = true;
    Timer timer;

    ByteRange range;

protected:
    scoped_connection onPreBufferInsert;
};

using tRangeMarkers = std::map<ByteIndex, std::set<std::shared_ptr<RangeMarker>>>;

}; // Zep
