#pragma once

#include "syntax.h"
#include <list>
#include <string>
#include <unordered_map>

namespace Zep {

class ZepSyntaxAdorn_RainbowBrackets : public ZepSyntaxAdorn {
public:
    using TParent = ZepSyntaxAdorn;
    ZepSyntaxAdorn_RainbowBrackets(ZepSyntax &syntax, ZepBuffer &buffer);
    ~ZepSyntaxAdorn_RainbowBrackets() override;

    void Notify(const std::shared_ptr<ZepMessage> &message) override;
    SyntaxResult GetSyntaxAt(const GlyphIterator &offset, bool &found) const override;

    virtual void Clear(const GlyphIterator &start, const GlyphIterator &end);
    virtual void Insert(const GlyphIterator &start, const GlyphIterator &end);
    virtual void Update(const GlyphIterator &start, const GlyphIterator &end);

private:
    void RefreshBrackets();
    enum class BracketType {
        Bracket = 0,
        Brace = 1,
        Group = 2,
        Max = 3
    };

    struct Bracket {
        int32_t indent{};
        BracketType type{};
        bool is_open = false;
        bool valid = true;
    };
    std::map<long, Bracket> m_brackets;
};

} // namespace Zep
