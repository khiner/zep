#pragma once

#include <array>
#include "buffer.h"

namespace Zep {

struct ZepTabWindow;

enum class ZepTextType {
    UI,
    Text,
    Heading1,
    Heading2,
    Heading3,
    Count
};

struct ZepFont {
    explicit ZepFont(ZepDisplay &display) : m_display(display) {}
    virtual ~ZepFont() = default;

    // Implemented in API specific ways
    virtual void SetPixelHeight(int height) = 0;
    virtual NVec2f GetTextSize(const uint8_t *pBegin, const uint8_t *pEnd = nullptr) const = 0;

    virtual const NVec2f &GetDefaultCharSize();
    virtual void BuildCharCache();
    virtual void InvalidateCharCache();
    virtual NVec2f GetCharSize(const uint8_t *pChar);

    NVec2f dotSize;
    int pixelHeight{};

protected:
    bool m_charCacheDirty = true;
    std::unordered_map<uint32_t, NVec2f> m_charCache;
    NVec2f m_charCacheASCII[256];
    NVec2f m_defaultCharSize;
    ZepDisplay &m_display;
};

struct ZepDisplay {
    virtual ~ZepDisplay() = default;
    ZepDisplay();

    // Renderer specific overrides
    // Implement these to draw the buffer using whichever system you prefer
    virtual void DrawLine(const NVec2f &start, const NVec2f &end, const NVec4f &color = NVec4f(1.0f), float width = 1.0f) const = 0;
    virtual void DrawChars(ZepFont &font, const NVec2f &pos, const NVec4f &col, const uint8_t *text_begin, const uint8_t *text_end = nullptr) const = 0;
    virtual void DrawRectFilled(const NRectf &rc, const NVec4f &col = NVec4f(1.0f)) const = 0;
    virtual void SetClipRect(const NRectf &rc) = 0;

    virtual void DrawRect(const NRectf &rc, const NVec4f &col = NVec4f(1.0f)) const;

    virtual void SetFont(ZepTextType type, std::shared_ptr<ZepFont> font);
    virtual ZepFont &GetFont(ZepTextType type) = 0;

    void Bigger();
    void Smaller();

    NVec2f pixelScale;
    bool layoutDirty = false;
    std::array<std::shared_ptr<ZepFont>, (int) ZepTextType::Count> fonts;
};

} // namespace Zep
