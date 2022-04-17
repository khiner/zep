#pragma once

#include "syntax.h"

namespace Zep {

struct ZepSyntax_Markdown : public ZepSyntax {
    explicit ZepSyntax_Markdown(ZepBuffer &buffer,
                                const std::unordered_set<std::string> &keywords = std::unordered_set<std::string>{},
                                const std::unordered_set<std::string> &identifiers = std::unordered_set<std::string>{},
                                uint32_t flags = 0);
    explicit ZepSyntax_Markdown(ZepBuffer &buffer, uint32_t flags = 0);

    void UpdateSyntax() override;
};

} // namespace Zep
