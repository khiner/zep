#pragma once

#include "buffer.h"
#include <deque>

namespace Zep {

struct ZepWindow;
struct ZepDisplay;
struct Region;

enum class WindowMotion { Left, Right, Up, Down };

// Display state for a single pane of text.
// Editor operations such as select and change are local to a displayed pane
struct ZepTabWindow : public ZepComponent {
    explicit ZepTabWindow(ZepEditor &editor);
    ~ZepTabWindow() override;

    void Notify(const std::shared_ptr<ZepMessage> &message) override;

    ZepWindow *DoMotion(WindowMotion motion);
    ZepWindow *AddWindow(ZepBuffer *pBuffer, ZepWindow *pParent = nullptr, RegionLayoutType layoutType = RegionLayoutType::HBox);
    void RemoveWindow(ZepWindow *pWindow);
    void SetActiveWindow(ZepWindow *pBuffer);
    ZepWindow *GetActiveWindow() const { return m_pActiveWindow; }
    void CloseActiveWindow();

    using tWindows = std::vector<ZepWindow *>;
    using tWindowRegions = std::map<ZepWindow *, std::shared_ptr<Region>>;
    const tWindows &GetWindows() const { return m_windows; }

    void SetDisplayRegion(const NRectf &region, bool force = false);

    void Display();

    NRectf m_lastRegionRect;

    tWindows m_windows;
    tWindowRegions m_windowRegions;
    std::shared_ptr<Region> m_rootRegion;
    ZepWindow *m_pActiveWindow = nullptr;
};

} // namespace Zep
