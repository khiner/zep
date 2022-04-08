#pragma once

#include "mode.h"
#include "zep/keymap.h"

class Timer;

namespace Zep {

struct SpanInfo;

enum class VimMotion {
    LineEnd
};

class ZepMode_Vim : public ZepMode {
public:
    explicit ZepMode_Vim(ZepEditor &editor);
    ~ZepMode_Vim() override;

    static const char *StaticName() { return "Vim"; }

    // Zep Mode
    void Init() override;
    void Begin(ZepWindow *pWindow) override;
    const char *Name() const override { return StaticName(); }
    EditorMode DefaultMode() const override { return EditorMode::Normal; }
    void PreDisplay(ZepWindow &win) override;
    virtual void SetupKeyMaps();
    virtual void AddOverStrikeMaps();
    virtual void AddCopyMaps();
    virtual void AddPasteMaps();
    bool UsesRelativeLines() const override { return true; }
};

} // namespace Zep
