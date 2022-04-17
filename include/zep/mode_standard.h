#pragma once

#include "mode.h"

namespace Zep {

struct ZepMode_Standard : public ZepMode {
    explicit ZepMode_Standard(ZepEditor &editor);
    ~ZepMode_Standard() override;

    void Init() override;
    void Begin(ZepWindow *pWindow) override;
    EditorMode DefaultMode() const override { return EditorMode::Insert; }

    static const char *StaticName() { return "Standard"; }
    const char *Name() const override { return StaticName(); }
};

} // namespace Zep
