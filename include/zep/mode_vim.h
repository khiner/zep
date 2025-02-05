#pragma once

#include "mode.h"

namespace Zep {

struct ZepMode_Vim : public ZepMode {
    explicit ZepMode_Vim(ZepEditor &editor);
    ~ZepMode_Vim() override;

    static const char *StaticName() { return "Vim"; }

    // Zep Mode
    void Init() override;
    void Begin(ZepWindow *pWindow) override;
    const char *Name() const override { return StaticName(); }
    EditorMode DefaultMode() const override { return EditorMode::Normal; }
    void PreDisplay(ZepWindow &win) override;
    void SetupKeyMaps();
    bool UsesRelativeLines() const override { return true; }
};

} // namespace Zep
