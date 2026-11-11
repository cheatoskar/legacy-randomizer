// TrackMania-formatted text ("$f00Red $oBold$z $wWide") for Dear ImGui.
// Applied: $rgb colours, $g (default colour), $o bold (bold font if given, else drawn twice), $i italic
// (sheared vertices), $s shadow, $w wide / $n narrow / $m normal (scaled vertices), $t upper case, $z reset.
// Stripped: $l / $h / $p links (with an optional [target]), $< $>, unknown codes. "$$" is a dollar sign.
// Very dark colours are lifted so the text stays readable on dark panels.
#pragma once
#include <string>

#include "imgui.h"

namespace lr {

struct TmTextStyle {
    ImFont* font = nullptr;      // null: current font
    float size = 0;              // 0: font's size (with the window's font scale)
    ImFont* boldFont = nullptr;  // for $o; null: faux bold
    float maxWidth = 0;          // > 0: cut with "..." at this width
    ImU32 defaultColor = 0;      // 0: ImGuiCol_Text
    bool liftDark = true;
};

std::string tmPlain(const std::string& raw);  // all codes removed, links' targets dropped
ImVec2 tmTextSize(const std::string& raw, const TmTextStyle& s = TmTextStyle());
// Draws at pos without layout.
void tmTextAt(ImDrawList* dl, ImVec2 pos, const std::string& raw, const TmTextStyle& s = TmTextStyle());
// Draws at the cursor and advances it like ImGui::Text.
void tmText(const std::string& raw, const TmTextStyle& s = TmTextStyle());

} // namespace lr
