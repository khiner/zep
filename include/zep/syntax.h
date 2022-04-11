#pragma once

#include "buffer.h"

#include <atomic>
#include <future>
#include <memory>
#include <unordered_set>
#include <vector>

#include "zep/mcommon/animation/timer.h"
#include "zep/mcommon/math/math.h"

namespace Zep {

enum class ThemeColor;

namespace ZepSyntaxFlags {
enum {
    CaseInsensitive = (1 << 0),
    IgnoreLineHighlight = (1 << 1),
    LispLike = (1 << 2)
};
};

struct SyntaxData {
    ThemeColor foreground = ThemeColor::Normal;
    ThemeColor background = ThemeColor::None;
    bool underline = false;
};

struct SyntaxResult : SyntaxData {
    NVec4f customBackgroundColor;
    NVec4f customForegroundColor;
};

class ZepSyntaxAdorn;
class ZepSyntax : public ZepComponent {
public:
    explicit ZepSyntax(ZepBuffer &buffer,
                       std::unordered_set<std::string> keywords = std::unordered_set<std::string>{},
                       std::unordered_set<std::string> identifiers = std::unordered_set<std::string>{},
                       uint32_t flags = 0);
    ~ZepSyntax() override;

    virtual SyntaxResult GetSyntaxAt(const GlyphIterator &index) const;
    virtual void UpdateSyntax();
    virtual void Interrupt();
    virtual void Wait() const;

    void Notify(const std::shared_ptr<ZepMessage> &message) override;

    const NVec4f &ToBackgroundColor(const SyntaxResult &res) const;
    const NVec4f &ToForegroundColor(const SyntaxResult &res) const;

    virtual void IgnoreLineHighlight() { m_flags |= ZepSyntaxFlags::IgnoreLineHighlight; }

private:
    virtual void QueueUpdateSyntax(const GlyphIterator &startLocation, const GlyphIterator &endLocation);

protected:
    ZepBuffer &m_buffer;
    std::vector<SyntaxData> m_syntax;
    std::future<void> m_syntaxResult;
    std::atomic<long> m_processedChar = {0};
    std::atomic<long> m_targetChar = {0};
    std::vector<uint32_t> m_multiCommentStarts;
    std::vector<uint32_t> m_multiCommentEnds;
    std::unordered_set<std::string> m_keywords;
    std::unordered_set<std::string> m_identifiers;
    std::atomic<bool> m_stop;
    std::vector<std::shared_ptr<ZepSyntaxAdorn>> m_adornments;
    uint32_t m_flags;
};

class ZepSyntaxAdorn : public ZepComponent {
public:
    ZepSyntaxAdorn(ZepSyntax &syntax, ZepBuffer &buffer) : ZepComponent(syntax.editor), m_buffer(buffer) {}
    virtual SyntaxResult GetSyntaxAt(const GlyphIterator &offset, bool &found) const = 0;

protected:
    ZepBuffer &m_buffer;
};

} // namespace Zep
