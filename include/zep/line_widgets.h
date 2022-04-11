#pragma once

#include "zep/editor.h"

namespace Zep {

struct IWidget {
    virtual NVec2f GetSize() const = 0; // Required size of the widget
    virtual void MouseDown(const NVec2f &pos, ZepMouseButton button) = 0;
    virtual void MouseUp(const NVec2f &pos, ZepMouseButton button) = 0;
    virtual void MouseMove(const NVec2f &pos) = 0;
    virtual void Draw(const ZepBuffer &buffer, const NVec2f &location) = 0;
    virtual void DrawInline(const ZepBuffer &buffer, const NRectf &location) = 0;
    virtual void Set(const NVec4f &value) = 0;
    virtual const NVec4f &Get() const = 0;
};

using fnWidgetValueChanged = std::function<void(IWidget *pWidget)>;
class FloatSlider : public IWidget {
public:
    FloatSlider(ZepEditor &editor, uint32_t dimension, fnWidgetValueChanged fnChanged = nullptr)
        : editor(editor),
          m_dimension(dimension),
          m_fnChanged(std::move(std::move(fnChanged))) {

    }
    NVec2f GetSize() const override;
    void MouseDown(const NVec2f &pos, ZepMouseButton button) override;
    void MouseUp(const NVec2f &pos, ZepMouseButton button) override;
    void MouseMove(const NVec2f &pos) override;
    void Draw(const ZepBuffer &buffer, const NVec2f &location) override;
    void DrawInline(const ZepBuffer &buffer, const NRectf &location) override;
    void Set(const NVec4f &value) override;
    const NVec4f &Get() const override;

private:
    ZepEditor &editor;
    uint32_t m_dimension = 1;
    NVec2f m_range = NVec2f(0.0f, 1.0f);
    NVec4f m_value = NVec4f(0.0f, 0.0f, 0.0f, 0.0f);
    float m_sliderGap = 5.0f;
    fnWidgetValueChanged m_fnChanged = nullptr;
};

class ColorPicker : public IWidget {
public:
    explicit ColorPicker(ZepEditor &editor) : editor(editor) {}

    NVec2f GetSize() const override;
    void MouseDown(const NVec2f &pos, ZepMouseButton button) override;
    void MouseUp(const NVec2f &pos, ZepMouseButton button) override;
    void MouseMove(const NVec2f &pos) override;
    void Draw(const ZepBuffer &buffer, const NVec2f &location) override;
    void DrawInline(const ZepBuffer &buffer, const NRectf &location) override;
    void Set(const NVec4f &value) override;
    const NVec4f &Get() const override;

    ZepEditor &editor;
private:
    NVec4f m_color;
};
} // Zep
