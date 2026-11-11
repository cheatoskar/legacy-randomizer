#include "icons.h"

#include <cmath>

namespace lr {

void drawIcon(ImDrawList* dl, Icon ic, ImVec2 o, float s, ImU32 c) {
    const float t = s * 0.09f < 1.2f ? 1.2f : s * 0.09f;  // line thickness
    auto P = [&](float x, float y) { return ImVec2(o.x + x * s, o.y + y * s); };
    const float pi = 3.14159265f;
    switch (ic) {
    case Icon::Play:
        dl->AddTriangleFilled(P(0.28f, 0.18f), P(0.28f, 0.82f), P(0.84f, 0.5f), c);
        break;
    case Icon::Stop:
        dl->AddRectFilled(P(0.24f, 0.24f), P(0.76f, 0.76f), c, s * 0.08f);
        break;
    case Icon::Timer:
        dl->AddCircle(P(0.5f, 0.56f), s * 0.36f, c, 24, t);
        dl->AddLine(P(0.5f, 0.56f), P(0.5f, 0.34f), c, t);
        dl->AddLine(P(0.5f, 0.56f), P(0.66f, 0.62f), c, t);
        dl->AddLine(P(0.4f, 0.1f), P(0.6f, 0.1f), c, t);
        break;
    case Icon::Flag: {
        dl->AddLine(P(0.22f, 0.1f), P(0.22f, 0.92f), c, t);
        // checkered flag
        const float x0 = 0.25f, y0 = 0.12f, cw = 0.16f, ch = 0.14f;
        for (int r = 0; r < 3; ++r)
            for (int k = 0; k < 4; ++k)
                if ((r + k) % 2 == 0) dl->AddRectFilled(P(x0 + k * cw, y0 + r * ch), P(x0 + (k + 1) * cw, y0 + (r + 1) * ch), c);
        dl->AddRect(P(x0, y0), P(x0 + 4 * cw, y0 + 3 * ch), c, 0, 0, t * 0.7f);
        break;
    }
    case Icon::Skip:
        dl->AddTriangleFilled(P(0.14f, 0.2f), P(0.14f, 0.8f), P(0.52f, 0.5f), c);
        dl->AddTriangleFilled(P(0.46f, 0.2f), P(0.46f, 0.8f), P(0.84f, 0.5f), c);
        dl->AddRectFilled(P(0.84f, 0.2f), P(0.92f, 0.8f), c);
        break;
    case Icon::Medal:
        dl->AddLine(P(0.32f, 0.08f), P(0.46f, 0.42f), c, t);
        dl->AddLine(P(0.68f, 0.08f), P(0.54f, 0.42f), c, t);
        dl->AddCircle(P(0.5f, 0.64f), s * 0.25f, c, 20, t);
        dl->AddCircleFilled(P(0.5f, 0.64f), s * 0.11f, c, 12);
        break;
    case Icon::Filter:
        dl->AddTriangle(P(0.12f, 0.16f), P(0.88f, 0.16f), P(0.5f, 0.56f), c, t);
        dl->AddLine(P(0.5f, 0.56f), P(0.5f, 0.88f), c, t);
        break;
    case Icon::Sliders:
        for (int i = 0; i < 3; ++i) {
            float y = 0.22f + i * 0.28f, k = i == 1 ? 0.66f : (i == 0 ? 0.34f : 0.5f);
            dl->AddLine(P(0.1f, y), P(0.9f, y), c, t);
            dl->AddCircleFilled(P(k, y), s * 0.1f, c, 12);
        }
        break;
    case Icon::Layout:
        dl->AddRect(P(0.1f, 0.14f), P(0.9f, 0.86f), c, s * 0.08f, 0, t);
        dl->AddLine(P(0.42f, 0.14f), P(0.42f, 0.86f), c, t);
        dl->AddLine(P(0.42f, 0.5f), P(0.9f, 0.5f), c, t);
        break;
    case Icon::History:
        dl->PathArcTo(P(0.52f, 0.52f), s * 0.36f, pi * 0.85f, pi * 2.6f, 20);
        dl->PathStroke(c, 0, t);
        dl->AddTriangleFilled(P(0.06f, 0.42f), P(0.28f, 0.42f), P(0.17f, 0.62f), c);
        dl->AddLine(P(0.52f, 0.52f), P(0.52f, 0.32f), c, t);
        dl->AddLine(P(0.52f, 0.52f), P(0.66f, 0.6f), c, t);
        break;
    case Icon::Info:
        dl->AddCircle(P(0.5f, 0.5f), s * 0.4f, c, 20, t);
        dl->AddCircleFilled(P(0.5f, 0.3f), s * 0.06f, c, 8);
        dl->AddLine(P(0.5f, 0.44f), P(0.5f, 0.72f), c, t * 1.2f);
        break;
    case Icon::List:
        for (int i = 0; i < 3; ++i) {
            float y = 0.24f + i * 0.26f;
            dl->AddCircleFilled(P(0.16f, y), s * 0.06f, c, 8);
            dl->AddLine(P(0.3f, y), P(0.9f, y), c, t);
        }
        break;
    case Icon::Map:
        dl->AddQuad(P(0.08f, 0.2f), P(0.36f, 0.1f), P(0.36f, 0.8f), P(0.08f, 0.9f), c, t);
        dl->AddQuad(P(0.36f, 0.1f), P(0.64f, 0.2f), P(0.64f, 0.9f), P(0.36f, 0.8f), c, t);
        dl->AddQuad(P(0.64f, 0.2f), P(0.92f, 0.1f), P(0.92f, 0.8f), P(0.64f, 0.9f), c, t);
        break;
    case Icon::Controls:
        dl->AddRect(P(0.08f, 0.28f), P(0.92f, 0.76f), c, s * 0.16f, 0, t);
        dl->AddLine(P(0.24f, 0.52f), P(0.4f, 0.52f), c, t);
        dl->AddLine(P(0.32f, 0.44f), P(0.32f, 0.6f), c, t);
        dl->AddCircleFilled(P(0.64f, 0.46f), s * 0.05f, c, 8);
        dl->AddCircleFilled(P(0.74f, 0.58f), s * 0.05f, c, 8);
        break;
    case Icon::Card:
        dl->AddRect(P(0.14f, 0.1f), P(0.86f, 0.9f), c, s * 0.08f, 0, t);
        dl->AddRectFilled(P(0.24f, 0.2f), P(0.76f, 0.44f), c, s * 0.04f);
        dl->AddLine(P(0.24f, 0.58f), P(0.76f, 0.58f), c, t);
        dl->AddLine(P(0.24f, 0.74f), P(0.6f, 0.74f), c, t);
        break;
    case Icon::PopOut:
        dl->AddRect(P(0.12f, 0.3f), P(0.7f, 0.88f), c, 2.0f, 0, t);
        dl->AddLine(P(0.45f, 0.55f), P(0.9f, 0.1f), c, t);
        dl->AddLine(P(0.58f, 0.1f), P(0.9f, 0.1f), c, t);
        dl->AddLine(P(0.9f, 0.1f), P(0.9f, 0.42f), c, t);
        break;
    }
}

void icon(Icon ic, ImU32 col, float size) {
    if (size <= 0) size = ImGui::GetTextLineHeight();
    if (!col) col = ImGui::GetColorU32(ImGuiCol_Text);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float lh = ImGui::GetTextLineHeight();
    float dy = size < lh ? (lh - size) * 0.5f : 0.0f;
    drawIcon(ImGui::GetWindowDrawList(), ic, ImVec2(p.x, p.y + dy), size, col);
    ImGui::Dummy(ImVec2(size, size > lh ? size : lh));
}

} // namespace lr
