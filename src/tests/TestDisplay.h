#pragma once

#include "zep/display.h"

using namespace Zep;

struct TestDisplay : public ZepDisplay {
    TestDisplay() : ZepDisplay() {}

    void DrawChars(ZepFont &font, const NVec2f &pos, const NVec4f &col, const uint8_t *text_begin, const uint8_t *text_end) const override {}
    void DrawLine(const NVec2f &start, const NVec2f &end, const NVec4f &color, float width) const override {}
    void DrawRectFilled(const NRectf &rc, const NVec4f &color) const override {}
    void SetClipRect(const NRectf &rc) override {}
    ZepFont &GetFont(ZepTextType type) override { return *fonts[(int) type]; }
};
