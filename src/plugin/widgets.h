// Small custom widgets drawn with the host's style colours (UiStyle): segmented control, toggle switch,
// cards, chips, info tooltips, a big action button. They never change ImGui's global style.
#pragma once
#include <string>

#include "host.h"
#include "icons.h"
#include "imgui.h"

namespace lr {
namespace ui {

void setStyle(const UiStyle& s);  // once per frame, before using the widgets
const UiStyle& style();
ImU32 col(const ImVec4& c, float alphaMul = 1.0f);

bool segmented(const char* id, int* value, const char* const* labels, int count, float width = 0);
bool toggle(const char* label, bool* v, const char* tip = nullptr);
bool chip(const char* label, bool on);                 // a pill that toggles (multi-select)
bool triState(const char* label, int* v);              // Any / Yes / No  (-1 / 1 / 0)
void infoTip(const char* text);                        // (i) on the same line, tooltip on hover
void beginCard(const char* title, Icon ic);            // a rounded panel with a title row
void endCard();
bool bigButton(const char* label, Icon ic, ImVec2 size, bool enabled = true);
void label(const char* text);                          // muted label in front of a control (own line)
void textRight(const char* text, float rightX, const ImVec4* color = nullptr);  // same line, right-aligned
void medalDot(const ImVec4& c);

} // namespace ui
} // namespace lr
