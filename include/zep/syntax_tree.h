#pragma once

#include "syntax.h"

namespace Zep {

struct ZepSyntax_Tree : public ZepSyntax {
    explicit ZepSyntax_Tree(ZepBuffer &buffer,
                            const std::unordered_set<std::string> &keywords = std::unordered_set<std::string>{},
                            const std::unordered_set<std::string> &identifiers = std::unordered_set<std::string>{},
                            uint32_t flags = 0);
    ZepSyntax_Tree(ZepBuffer &buffer, uint32_t flags);

    void UpdateSyntax() override;
};

} // namespace Zep
