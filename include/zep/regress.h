#pragma once

#include "timer.h"

namespace Zep {

struct ZepEditor;
struct ZepRegressExCommand : public ZepExCommand {
    explicit ZepRegressExCommand(ZepEditor &editor);

    static void Register(ZepEditor &editor);

    void Tick();
    void Run(const std::vector<std::string> &tokens) override;
    void Notify(const std::shared_ptr<ZepMessage> &message) override;
    const char *ExCommandName() const override { return "ZRegress"; }

private:
    Timer m_timer;
    bool m_enable = false;
    uint32_t m_windowOperationCount = 0;
};

}
