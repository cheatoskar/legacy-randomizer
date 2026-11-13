// A small set of line icons drawn with ImDrawList. Plugins can't add an icon font to the host's shared
// font atlas, so the icons are geometry; a host icon font can replace drawIcon() later (one file).
#pragma once
#include "imgui.h"

namespace lr {

enum class Icon {
    None, Play, Timer, Flag, Skip, Medal, Filter, Sliders, Layout, History, Info, List, Map, Controls, PopOut, Card, Stop,
    ChevronDown, ChevronRight, Sunrise, Sun, Sunset, Moon, Route, Gauge,
    RouteSingle, RouteMulti, RouteSymmetric  // TMX routes: one path, branching paths, mirrored paths
};

// Draws icon into the square [min, min + size].
void drawIcon(ImDrawList* dl, Icon icon, ImVec2 min, float size, ImU32 col);
// At the cursor, as an item of size x size (default: text line height).
void icon(Icon icon, ImU32 col = 0, float size = 0);

} // namespace lr
