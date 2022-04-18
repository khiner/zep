#pragma once

#include "splits.h"
#include "editor.h"
#include "zep/mcommon/animation/timer.h"

namespace Zep {
struct ZepTheme;
struct ZepEditor;

struct Scroller : public ZepComponent {
    Scroller(ZepEditor &editor, Region &parent);

    void Display(ZepTheme &theme);
    void Notify(const std::shared_ptr<ZepMessage> &message) override;

    float vScrollVisiblePercent = 1.0f;
    float vScrollPosition = 0.0f;
    float vScrollLinePercent = 0.0f;
    float vScrollPagePercent = 0.0f;
    bool vertical = true;

private:
    void CheckState();
    void ClickUp();
    void ClickDown();
    void PageUp();
    void PageDown();
    void DoMove(NVec2f pos);

    float ThumbSize() const;
    NRectf ThumbRect() const;

private:
    std::shared_ptr<Region> m_region;
    std::shared_ptr<Region> m_topButtonRegion;
    std::shared_ptr<Region> m_bottomButtonRegion;
    std::shared_ptr<Region> m_mainRegion;
    Timer m_start_delay_timer;
    Timer m_reclick_timer;
    enum class ScrollState {
        None,
        ScrollDown,
        ScrollUp,
        PageUp,
        PageDown,
        Drag
    };
    ScrollState m_scrollState = ScrollState::None;
    NVec2f m_mouseDownPos;
    float m_mouseDownPercent = 0;
};

}; // namespace Zep
