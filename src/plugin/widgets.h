// Small custom widgets drawn with the host's style colours (UiStyle), in the look of TM Vibrant Shaders'
// menu: cards with a title mark, switch rows, labels above controls, a range slider, dropdowns without
// the arrow box, a disclosure row, chips. Local style pushes only; the global style stays the host's.
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

// cards
void beginCard(const char* title, Icon ic, float width = 0);  // width 0: the rest of the row
void endCard();
void emptyState(Icon ic, const char* line);  // a large muted icon and one line

// labels and rows
void fieldLabel(const char* label, const char* tip = nullptr);  // small muted label above the next control
bool switchRow(const char* label, bool* v, const char* tip = nullptr);  // label left, switch at the right edge
bool toggle(const char* id, bool* v);                                   // just the switch
void infoTip(const char* text);                                          // (i) on the same line

// inputs (full width of the content region)
bool segmented(const char* id, int* value, const char* const* labels, int count, float width = 0);
bool chip(const char* label, bool on, Icon ic = Icon::None, ImU32 iconColor = 0);  // iconColor 0: the text colour
bool triState(const char* label, int* v);
// author-time window in ms on a square-root scale up to maxMs, 5 s steps; 0 = open end. Values beyond the
// scale (typed) are drawn clamped and only change when their knob is dragged.
bool rangeSlider(const char* id, int* loMs, int* hiMs, int maxMs, float width = 0);
// a time field: m:ss, m:ss.mmm or h:mm:ss, empty = 0; applied when the field loses focus, red while invalid
bool durationInput(const char* id, int* ms, const char* hint, float width);
bool combo(const char* id, int* current, const char* const* items, int count);
bool beginCombo(const char* id, const char* preview);  // + ImGui::EndCombo() when true
bool disclosure(const char* label, bool* open);       // a subtle row with a chevron

bool bigButton(const char* label, Icon ic, ImVec2 size, bool enabled = true);
void textRight(const char* text, float rightX, const ImVec4* color = nullptr);  // same line, right-aligned
void medalDot(const ImVec4& c);

} // namespace ui
} // namespace lr
