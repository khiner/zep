#include "zep/syntax_tree.h"
#include "zep/theme.h"

#include <vector>

namespace Zep {

ZepSyntax_Tree::ZepSyntax_Tree(ZepBuffer &buffer,
                               const std::unordered_set<std::string> &keywords,
                               const std::unordered_set<std::string> &identifiers,
                               uint32_t flags) : ZepSyntax(buffer, keywords, identifiers, flags) {
    m_adornments.clear(); // Don't need default
}

ZepSyntax_Tree::ZepSyntax_Tree(ZepBuffer &buffer, uint32_t flags) : ZepSyntax_Tree(buffer, {}, {}, flags) {}

void ZepSyntax_Tree::UpdateSyntax() {
    const auto &buffer = m_buffer.workingBuffer;
    auto itrCurrent = buffer.begin();
    auto itrEnd = buffer.end();

    assert(std::distance(itrCurrent, itrEnd) <= int(m_syntax.size()));
    assert(m_syntax.size() == buffer.size());

    // Mark a region of the syntax buffer with the correct marker
    auto mark = [&](const GapBuffer<uint8_t>::const_iterator &itrA, const GapBuffer<uint8_t>::const_iterator &itrB, ThemeColor type, ThemeColor background) {
        std::fill(m_syntax.begin() + (itrA - buffer.begin()), m_syntax.begin() + (itrB - buffer.begin()), SyntaxData{type, background});
    };

    // Walk backwards to previous delimiter
    while (itrCurrent != itrEnd) {
        if (m_stop == true) return;

        // Update start location
        m_processedChar = long(itrCurrent - buffer.begin());

        if (*itrCurrent == '~' || *itrCurrent == '+') {
            mark(itrCurrent, itrCurrent + 1, ThemeColor::CursorNormal, ThemeColor::None);
            itrCurrent++;
            auto itrNext = itrCurrent;
            while (itrNext != itrEnd &&
                *itrNext != '\n') {
                itrNext++;
            }
            mark(itrCurrent, itrNext, ThemeColor::Comment, ThemeColor::None);
            itrCurrent = itrNext;
        }

        itrCurrent++;
    }

    // If we got here, we successfully completed
    // Reset the target to the beginning
    m_targetChar = long(0);
    m_processedChar = long(buffer.size() - 1);
}

} // namespace Zep
