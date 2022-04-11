#pragma once

#include "syntax.h"

namespace Zep {

class ZepSyntax_Markdown : public ZepSyntax {
public:
    explicit ZepSyntax_Markdown(ZepBuffer &buffer,
                                const std::unordered_set<std::string> &keywords = std::unordered_set<std::string>{},
                                const std::unordered_set<std::string> &identifiers = std::unordered_set<std::string>{},
                                uint32_t flags = 0);
    ZepSyntax_Markdown(ZepBuffer &buffer, uint32_t flags = 0);

    void UpdateSyntax() override;
};

} // namespace Zep
