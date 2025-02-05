#include "zep/display.h"
#include "zep/logger.h"
#include "zep/stringutils.h"
#include "zep/utf8/unchecked.h"

// A 'window' is like a vim window; i.e. a region inside a tab
namespace Zep {

void ZepFont::InvalidateCharCache() {
    m_display.layoutDirty = true;
    m_charCacheDirty = true;
}

void ZepFont::BuildCharCache() {
    const char chA = 'A';
    m_defaultCharSize = GetTextSize((const uint8_t *) &chA, (const uint8_t *) &chA + 1);
    for (int i = 0; i < 256; i++) {
        auto ch = (uint8_t) i;
        m_charCacheASCII[i] = GetTextSize(&ch, &ch + 1);
    }
    m_charCacheDirty = false;

    dotSize = m_defaultCharSize / 8.0f;
    dotSize.x = std::min(dotSize.x, dotSize.y);
    dotSize.y = std::min(dotSize.x, dotSize.y);
    dotSize.x = std::max(1.0f, dotSize.x);
    dotSize.y = std::max(1.0f, dotSize.y);
}

const NVec2f &ZepFont::GetDefaultCharSize() {
    if (m_charCacheDirty) BuildCharCache();
    return m_defaultCharSize;
}

NVec2f ZepFont::GetCharSize(const uint8_t *pCh) {
    if (m_charCacheDirty) BuildCharCache();

    if (utf8_codepoint_length(*pCh) == 1) return m_charCacheASCII[*pCh];

    auto ch32 = utf8::unchecked::next(pCh);

    auto itr = m_charCache.find((uint32_t) ch32);
    if (itr != m_charCache.end()) return itr->second;

    auto sz = GetTextSize(pCh, pCh + utf8_codepoint_length(*pCh));
    m_charCache[(uint32_t) ch32] = sz;

    return sz;
}

ZepDisplay::ZepDisplay() : pixelScale(NVec2f(1.0f)) {
    for (auto &m_font: fonts) {
        m_font = nullptr;
    }
}

void ZepDisplay::DrawRect(const NRectf &rc, const NVec4f &col) const {
    DrawLine(rc.topLeftPx, rc.BottomLeft(), col);
    DrawLine(rc.topLeftPx, rc.TopRight(), col);
    DrawLine(rc.TopRight(), rc.bottomRightPx, col);
    DrawLine(rc.BottomLeft(), rc.bottomRightPx, col);
}

void ZepDisplay::SetFont(ZepTextType type, std::shared_ptr<ZepFont> font) {
    fonts[(int) type] = std::move(font);
}

void ZepDisplay::Bigger() {
    for (int i = 0; i < (int) fonts.size(); i++) {
        if (fonts[i] != nullptr) {
            switch ((ZepTextType) i) {
                case ZepTextType::Text:
                case ZepTextType::Heading1:
                case ZepTextType::Heading2:
                case ZepTextType::Heading3: {
                    auto &textFont = GetFont(ZepTextType(i));
                    textFont.SetPixelHeight((int) std::min((float) ceil(textFont.pixelHeight * 1.05), 800.0f));
                }
                default:break;
            }
        }
    }
}

void ZepDisplay::Smaller() {
    for (int i = 0; i < (int) fonts.size(); i++) {
        if (fonts[i] != nullptr) {
            switch ((ZepTextType) i) {
                case ZepTextType::Text:
                case ZepTextType::Heading1:
                case ZepTextType::Heading2:
                case ZepTextType::Heading3: {
                    auto &textFont = GetFont(ZepTextType(i));
                    textFont.SetPixelHeight((int) std::max(4.0f, (float) floor(textFont.pixelHeight / 1.05)));
                }
                default:break;
            }
        }
    }
}

} // namespace Zep
