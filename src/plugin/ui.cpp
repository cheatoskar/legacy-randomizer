// Menu pages (Run, Filters, Rules, HUD & layout, History, About) and the HUD panels (Timer, Map, Medals,
// Controls, Session, Compact card). Which page shows where is panels.cpp's job. Colours come from the
// host's style; the only own colours are the medal colours.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"  // current window flags (locked card)
#include "plugin.h"
#include "tmtext.h"
#include "util.h"
#include "widgets.h"

namespace lr {

namespace {

const ImVec4 kAuthor(0.30f, 0.82f, 0.47f, 1.0f);
const ImVec4 kGold(1.00f, 0.80f, 0.22f, 1.0f);
const ImVec4 kSilver(0.78f, 0.80f, 0.85f, 1.0f);
const ImVec4 kBronze(0.82f, 0.53f, 0.28f, 1.0f);
const ImVec4 kNoMedal(0.42f, 0.44f, 0.50f, 1.0f);

ImVec4 medalColor(Medal m) {
    switch (m) {
    case Medal::Author: return kAuthor;
    case Medal::Gold: return kGold;
    case Medal::Silver: return kSilver;
    case Medal::Bronze: return kBronze;
    case Medal::None: break;
    }
    return kNoMedal;
}

const char* medalShort(Medal m) {
    switch (m) {
    case Medal::Author: return "AT";
    case Medal::Gold: return "Gold";
    case Medal::Silver: return "Silver";
    case Medal::Bronze: return "Bronze";
    case Medal::None: break;
    }
    return "-";
}

Medal medalFromKey(const std::string& k) {
    if (k == "author") return Medal::Author;
    if (k == "gold") return Medal::Gold;
    if (k == "silver") return Medal::Silver;
    if (k == "bronze") return Medal::Bronze;
    return Medal::None;
}

Medal goalMedal(Goal g) {
    switch (g) {
    case Goal::Author: return Medal::Author;
    case Goal::Gold: return Medal::Gold;
    case Goal::Silver: return Medal::Silver;
    case Goal::Bronze: return Medal::Bronze;
    case Goal::Finished: break;
    }
    return Medal::None;
}

int medalTime(const MapMedals& m, Medal which) {
    switch (which) {
    case Medal::Author: return m.authorMs;
    case Medal::Gold: return m.goldMs;
    case Medal::Silver: return m.silverMs;
    case Medal::Bronze: return m.bronzeMs;
    case Medal::None: break;
    }
    return 0;
}

const char* modeTitle(const std::string& key) {
    if (key == "rms") return "Random Map Survival";
    if (key == "free") return "Free play";
    return "Random Map Challenge";
}

const char* modeShort(Mode m) { return m == Mode::RMC ? "Challenge" : m == Mode::RMS ? "Survival" : "Free play"; }

const char* resultTitle(const std::string& k) {
    if (k == "author") return "Author";
    if (k == "gold") return "Gold";
    if (k == "gold_skip") return "Gold skip";
    if (k == "skipped") return "Skipped";
    if (k == "free_skip") return "Free skip";
    if (k == "finished") return "Finished";
    return "Unfinished";
}

const char* const kDifficulty[] = {"Any", "Beginner", "Intermediate", "Expert", "Lunatic"};
// TMX tag ids, identical on all five sites (checked /api/meta/tags 2026-11-11)
const char* const kTags[] = {"Race", "Stunt", "Maze", "Offroad", "Multilap", "FullSpeed", "LOL", "Tech", "SpeedTech", "RPG",
                             "PressForward", "Trial", "Grass", "Story", "Nascar", "Speedfun", "Endurance", "Altered Nadeo",
                             "Transitional"};
const int kTagCount = static_cast<int>(sizeof kTags / sizeof kTags[0]);
const char* const kSiteChoices[] = {"Auto (game)", "tmnf.exchange", "tmuf.exchange", "nations.tm-exchange.com",
                                    "sunrise.tm-exchange.com", "original.tm-exchange.com"};
const char* const kMoods[] = {"Sunrise", "Day", "Sunset", "Night"};
const char* const kRoutes[] = {"Single", "Multi", "Symmetric"};
const char* const kLbTypes[] = {"Any", "Standard", "Classic", "Nadeo", "Uncompetitive", "Beta", "Star"};
const char* const kFlagNames[] = {"Screenshot", "Featured", "Has a record", "Has replays", "Author's latest", "By a supporter"};

std::string clockText(int seconds) {
    char b[32];
    std::snprintf(b, sizeof b, "%d:%02d", seconds / 60, seconds % 60);
    return b;
}

bool sliderSeconds(const char* label, int* v, int maxS, const char* offText) {
    std::string fmt = *v == 0 ? std::string(offText) : clockText(*v);
    return ImGui::SliderInt(label, v, 0, maxS, fmt.c_str(), ImGuiSliderFlags_AlwaysClamp);
}

bool inputString(const char* label, std::string& s, const char* hint) {
    char buf[256];
    std::snprintf(buf, sizeof buf, "%s", s.c_str());
    if (ImGui::InputTextWithHint(label, hint, buf, sizeof buf)) {
        s = buf;
        return true;
    }
    return false;
}

// right-aligned, so columns of times line up (Segoe UI digits are tabular)
void timeRight(int ms, float rightX, const ImVec4& c) {
    std::string t = ms > 0 ? formatMs(ms) : std::string("-");
    ui::textRight(t.c_str(), rightX, &c);
}

} // namespace

// ------------------------------------------------------------------ helpers

ImFont* Plugin::titleFont() { return style_.title ? style_.title : ImGui::GetFont(); }

// two columns of cards need about 2 x 340 px; below that the cards stack (popped-out or narrow menu)
bool Plugin::stacked() const { return ImGui::GetContentRegionAvail().x < 680 * style_.scale; }

void Plugin::ensureTexture(PreparedMap* m) {
    if (!m || m->tex || m->texTried || !m->thumb.ok()) return;
    m->texTried = true;
    if (!host_.canCreateTextures()) return;
    m->tex = host_.createTextureRGBA(m->thumb.w, m->thumb.h, m->thumb.rgba.data());
    if (m->tex) {
        m->thumb.rgba.clear();  // keep w/h for the aspect ratio
        m->thumb.rgba.shrink_to_fit();
    }
}

// ------------------------------------------------------------------ stop

// One confirm for the Run page and the HUD buttons: the buttons only set a flag, drawOverlay opens and
// submits the popup every frame in a fixed ID scope (independent of the window the button was in).
static const ImGuiID kStopScope = 0x4C525354;  // "LRST"

void Plugin::requestStop() {
    // a run with progress asks first; one that has hardly begun just stops
    const Session* s = run_.session();
    bool progress = s && (!s->history().empty() || s->elapsedMs() > 60000);
    if (progress) stopAsk_ = true;
    else run_.stop();
}

void Plugin::stopButton(ImVec2 size) {
    if (ImGui::Button("Stop", size)) requestStop();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("End the run and save it");
}

void Plugin::stopPopup() {
    const float sc = style_.scale;
    ImGui::PushOverrideID(kStopScope);
    if (stopAsk_) {
        stopAsk_ = false;
        if (run_.active()) ImGui::OpenPopup("Stop run?");
    }
    if (ImGui::BeginPopupModal("Stop run?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        if (!run_.active()) ImGui::CloseCurrentPopup();  // ended by itself meanwhile
        ImGui::TextColored(style_.muted, "Results are saved.");
        if (ImGui::Button("Stop", ImVec2(120 * sc, 0))) {
            run_.stop();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Keep playing", ImVec2(120 * sc, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

// ------------------------------------------------------------------ menu shell

void Plugin::drawMenu() {
    style_ = host_.style();
    ui::setStyle(style_);
    const float sc = style_.scale;
    // the page's width decides the layout: docked or popped out, any size. Hosts with ui_metrics (ABI minor 4)
    // report it (page_w, 0 before the first page); the ImGui region is the ground truth for drawing.
    float pageW = ImGui::GetContentRegionAvail().x;
    UiMetrics m;
    if (host_.uiMetrics(&m) && m.pageW > 0 && m.pageW < pageW) pageW = m.pageW;
    const float logical = pageW / sc;
    const bool compact = logical < 800;    // header on two lines, narrower navigation
    const bool iconsOnly = logical < 600;  // navigation: icons only
    drawHeader(compact);
    const float navW = iconsOnly ? 46 * sc : compact ? 156 * sc : 190 * sc;
    ImGui::BeginChild("##lr_nav", ImVec2(navW, 0), 0, iconsOnly ? ImGuiWindowFlags_NoScrollbar : 0);
    panels_.drawNavigation(iconsOnly);
    ImGui::EndChild();
    ImGui::SameLine(0, (compact ? 10 : 14) * sc);
    ImGui::BeginChild("##lr_page", ImVec2(0, 0));
    panels_.drawCurrentPage();
    ImGui::EndChild();
    if (dirty_ && !ImGui::IsAnyItemActive()) {
        settings_.save(host_);
        dirty_ = false;
    }
    panels_.saveIfDirty(host_);
}

void Plugin::siteBadge(Site site, float size) {
    ImTextureID t = logos_.texture(host_, site);
    if (t) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        float lh = ImGui::GetTextLineHeight();
        float y = size < lh ? (lh - size) * 0.5f : 0.0f;
        ImGui::GetWindowDrawList()->AddImageRounded(t, ImVec2(p.x, p.y + y), ImVec2(p.x + size, p.y + y + size), ImVec2(0, 0), ImVec2(1, 1),
                                                     IM_COL32_WHITE, size * 0.18f);
        ImGui::Dummy(ImVec2(size, size > lh ? size : lh));
    } else {
        icon(Icon::Map, ui::col(style_.text2), size);
    }
}

void Plugin::siteBadges(const std::vector<Site>& sites, float size) {
    for (size_t i = 0; i < sites.size(); ++i) {
        if (i) ImGui::SameLine(0, 4 * style_.scale);
        siteBadge(sites[i], size);
    }
}

// Wide: one line (title, version, notice chips; logos, site and game on the right), as long as it fits.
// Compact or too narrow: the title alone, the rest as small chips on a second line that wrap.
void Plugin::drawHeader(bool compact) {
    const float sc = style_.scale;
    const HostGame g = host_.game();
    const std::vector<Site> sites = sourceSites(settings_, g);
    const std::string game = host_.gameName();
    const std::string host = sitesLabel(sites);
    const bool experimental = isForever(g) || g == HostGame::Original;
    const char* expTip = g == HostGame::Original ? "TM Original support is still experimental."
                                                 : "TMNF/TMUF support is experimental; maps change only in menus.";
    const bool paused = host_.isOnline() || !host_.actionsAllowed();
    const char* version = LR_PLUGIN_VERSION;
    const float avail = ImGui::GetContentRegionAvail().x;
    const float sp = ImGui::GetStyle().ItemSpacing.x;
    const float flag = ImGui::GetTextLineHeight() * 1.2f;

    ImGui::PushFont(titleFont());
    const float titleW = ImGui::CalcTextSize("LEGACY RANDOMIZER").x;
    ImGui::PopFont();
    float chipsW = ui::pillWidth(version) + sp;
    if (experimental) chipsW += ui::pillWidth("Experimental") + sp;
    if (paused) chipsW += ui::pillWidth("Paused online") + sp;
    // the site / game chip on the right: logo(s), site(s), game
    const float logo = ImGui::GetTextLineHeight() * 1.25f;
    const float n = static_cast<float>(sites.size());
    const float rightW = logo * n + 4 * sc * (n - 1) + ImGui::CalcTextSize((host + "   " + game).c_str()).x + 30 * sc;
    const bool oneLine = !compact && flag + sp + titleW + sp + chipsW + 24 * sc + rightW <= avail;

    icon(Icon::Flag, ui::col(style_.accent), flag);
    ImGui::SameLine();
    ImGui::PushFont(titleFont());
    ui::textFit("LEGACY RANDOMIZER", ImGui::GetContentRegionAvail().x, &style_.text);
    ImGui::PopFont();
    if (!oneLine) ImGui::Dummy(ImVec2(0, 2 * sc));

    // one line: the chips centred on the title's height
    const float yOff = oneLine ? std::max(0.0f, (titleFont()->FontSize - ImGui::GetTextLineHeight() - 4 * sc) * 0.5f) : 0.0f;
    auto next = [&] {
        ImGui::SameLine();
        if (yOff > 0) ImGui::SetCursorPosY(ImGui::GetCursorPosY() + yOff);
    };
    auto chips = [&] {
        next();
        ui::pill(version, style_.muted, false, "Legacy Randomizer version");
        if (experimental) {
            next();
            ui::pill("Experimental", style_.warn, true, expTip);
        }
        if (paused) {
            next();
            ui::pill("Paused online", style_.warn, false, "Net play: the randomizer does nothing.");
        }
    };
    if (oneLine) {
        chips();
        ImGui::SameLine(ImGui::GetContentRegionMax().x - rightW + 8 * sc);
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x - 8 * sc, p.y - 3 * sc), ImVec2(p.x + rightW - 8 * sc, p.y + logo + 3 * sc),
                                                  IM_COL32(255, 255, 255, 12), 12 * sc);
        siteBadges(sites, logo);
        ImGui::SameLine();
        ImGui::TextUnformatted(host.c_str());
        ImGui::SameLine();
        ImGui::TextColored(g == HostGame::Unknown ? style_.bad : style_.muted, "%s", game.c_str());
    } else {
        // line 2: site(s) with logo, game, version, notices
        for (size_t i = 0; i < sites.size(); ++i) {
            if (i) ImGui::SameLine();
            ui::pill(siteHost(sites[i]), style_.text2, false, "TMX site", logos_.texture(host_, sites[i]));
        }
        ImGui::SameLine();
        ui::pill(game.c_str(), g == HostGame::Unknown ? style_.bad : style_.muted, false, "Game");
        chips();
    }
    ImGui::Dummy(ImVec2(0, 6 * sc));
}

// ------------------------------------------------------------------ pages

void Plugin::pageRun() {
    const float sc = style_.scale;
    const float gap = 12 * sc;
    const Session* s = run_.session();
    if (run_.active() && s) {
        ui::beginCard("Run in progress", Icon::Timer);
        panelTimer();
        char b[32];
        ui::medalDot(kAuthor);
        ImGui::Text("AT %d", s->authorCount());
        std::snprintf(b, sizeof b, "Gold %d", s->goldCount());
        ui::flowNext(ImGui::CalcTextSize(b).x + ImGui::GetTextLineHeight(), 16 * sc);
        ui::medalDot(kGold);
        ImGui::TextUnformatted(b);
        std::snprintf(b, sizeof b, "Skips %d", s->skipCount());
        ui::flowNext(ImGui::CalcTextSize(b).x, 16 * sc);
        ImGui::TextUnformatted(b);
        ui::flowNext(ImGui::CalcTextSize(phaseName(run_.phase())).x, 16 * sc);
        ImGui::TextColored(style_.muted, "%s", phaseName(run_.phase()));
        stopButton(ImVec2(120 * sc, 0));
        ui::endCard();
        const bool stack = stacked();
        float half = stack ? 0.0f : (ImGui::GetContentRegionAvail().x - gap) * 0.5f;
        ImGui::BeginGroup();
        ui::beginCard("Current map", Icon::Map, half);
        std::shared_ptr<PreparedMap> m = run_.displayMap();
        if (m) drawMapCard(m.get(), ImGui::GetContentRegionAvail().x, true);
        else ui::textFit(run_.fetchStatus().c_str(), 0, run_.fetchFailing() ? &style_.bad : &style_.muted);
        ui::endCard();
        ImGui::EndGroup();
        if (!stack) ImGui::SameLine(0, gap);
        ImGui::BeginGroup();
        ui::beginCard("Medals", Icon::Medal);
        panelMedals();
        ui::endCard();
        ui::beginCard("Controls", Icon::Controls);
        panelControls(false);
        ui::endCard();
        ImGui::EndGroup();
        RunRecord snap = run_.snapshot();
        if (!snap.maps.empty()) {
            ui::beginCard("Maps so far", Icon::List);
            drawMapsTable(snap, "##lr_live_maps", 0);
            ui::endCard();
        }
        return;
    }
    if (run_.phase() == Phase::Over) {
        drawResults(run_.lastRun(), true);
        return;
    }

    const std::vector<Site> sites = sourceSites(settings_, host_.game());
    const bool stack = stacked();
    const float colW = stack ? 0.0f : (ImGui::GetContentRegionAvail().x - gap) * 0.5f;
    ImGui::BeginGroup();
    ui::beginCard("Start a run", Icon::Play, colW);
    {
        float logo = ImGui::GetTextLineHeight() * 2.4f;
        siteBadges(sites, logo);
        ImGui::SameLine(0, 12 * sc);
        ImGui::BeginGroup();
        ImGui::PushFont(titleFont());
        ui::textFit(settings_.mode == Mode::RMC ? "Random Map Challenge" : settings_.mode == Mode::RMS ? "Random Map Survival" : "Free play");
        ImGui::PopFont();
        // the settings summary wraps at its " · " separators, never inside a word
        std::string sum = settings_.summary(sites, host_.game());
        const float wrapW = ImGui::GetContentRegionAvail().x;
        std::string line;
        size_t p = 0;
        while (p <= sum.size()) {
            size_t q = sum.find(" \xC2\xB7 ", p);
            std::string part = sum.substr(p, q == std::string::npos ? std::string::npos : q - p);
            std::string cand = line.empty() ? part : line + " \xC2\xB7 " + part;
            if (!line.empty() && ImGui::CalcTextSize(cand.c_str()).x > wrapW) {
                ui::textFit(line.c_str(), wrapW, &style_.muted);
                line = part;
            } else {
                line = cand;
            }
            if (q == std::string::npos) break;
            p = q + 4;
        }
        if (!line.empty()) ui::textFit(line.c_str(), wrapW, &style_.muted);
        ImGui::EndGroup();
    }
    ImGui::Dummy(ImVec2(0, 6 * sc));
    bool blocked = host_.isOnline() || !host_.actionsAllowed();
    static std::string startError;
    if (ui::bigButton("Start run", Icon::Play, ImVec2(ImGui::GetContentRegionAvail().x, 46 * sc), !blocked)) {
        if (dirty_) {
            settings_.save(host_);
            dirty_ = false;
        }
        startError.clear();
        if (!run_.start(&startError)) host_.log(LogWarn, "start: " + startError);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(blocked ? "Solo only: not in net play" : "The first map loads from the main menu");
    if (blocked) ui::textFit("Solo only", 0, &style_.warn);
    else if (!startError.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
        ImGui::TextColored(style_.bad, "%s", startError.c_str());
        ImGui::PopTextWrapPos();
    }
    ui::endCard();
    ImGui::EndGroup();
    const float leftH = ImGui::GetItemRectSize().y;

    if (!stack) ImGui::SameLine(0, gap);
    ImGui::BeginGroup();
    const float rightTop = ImGui::GetCursorScreenPos().y;
    ui::beginCard("Last run", Icon::History);
    if (history_.runs().empty()) {
        ui::emptyState(Icon::Flag, "No runs yet");
    } else {
        const RunRecord& r = history_.runs().front();
        ImGui::PushFont(titleFont());
        ui::medalDot(kAuthor);
        ImGui::Text("%d", r.authors);
        ImGui::SameLine(0, 18 * sc);
        ui::medalDot(kGold);
        ImGui::Text("%d", r.golds);
        ImGui::SameLine(0, 18 * sc);
        ImGui::TextColored(style_.text2, "%d skips", r.skips);
        ImGui::PopFont();
        std::string when = r.started;
        for (char& c : when)
            if (c == 'T') c = ' ';
        char b[160];
        std::snprintf(b, sizeof b, "%s, goal %s", modeTitle(r.mode), goalTitle(Session::goalFromKey(r.goal.c_str())));
        ui::textFit(b, 0, &style_.text2);
        std::snprintf(b, sizeof b, "%d maps in %s, %s", static_cast<int>(r.maps.size()), formatClock(r.elapsedMs).c_str(), when.c_str());
        ui::textFit(b, 0, &style_.muted);
        // the last maps, newest first, as a short strip
        float w = ImGui::GetContentRegionAvail().x;
        int shown = 0;
        for (size_t i = r.maps.size(); i-- > 0 && shown < 3; ++shown) {
            const RunMapRecord& m = r.maps[i];
            ui::medalDot(medalColor(medalFromKey(m.medal)));
            TmTextStyle ts;
            ts.maxWidth = w - 70 * sc;
            ts.defaultColor = ImGui::ColorConvertFloat4ToU32(style_.text2);
            tmText(m.name, ts);
        }
        if (ImGui::SmallButton("Open in History")) {
            histSel_ = 0;
            panels_.select("history");
        }
    }
    if (!stack) {
        // as tall as the start card (bottom padding 12 and the 4 px endCard adds)
        float want = rightTop + leftH - 24 * sc;
        float y = ImGui::GetCursorScreenPos().y;
        if (want > y + 1) ImGui::Dummy(ImVec2(1, want - y));
    }
    ui::endCard();
    ImGui::EndGroup();

    // under the two cards: how a run goes (first time) or the recent runs
    if (history_.runs().size() > 1) {
        ui::beginCard("Recent runs", Icon::List);
        if (ImGui::BeginTable("##lr_recent", 5, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Started", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthStretch, 2.4f);
            ImGui::TableSetupColumn("AT", ImGuiTableColumnFlags_WidthStretch, 0.5f);
            ImGui::TableSetupColumn("Gold", ImGuiTableColumnFlags_WidthStretch, 0.5f);
            ImGui::TableSetupColumn("Maps", ImGuiTableColumnFlags_WidthStretch, 0.5f);
            ImGui::TableHeadersRow();
            for (size_t i = 0; i < history_.runs().size() && i < 6; ++i) {
                const RunRecord& r = history_.runs()[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                std::string when = r.started;
                for (char& c : when)
                    if (c == 'T') c = ' ';
                ui::textFit(when.c_str(), 0, &style_.text2);
                ImGui::TableNextColumn();
                ui::textFit(modeTitle(r.mode));
                int vals[] = {r.authors, r.golds, static_cast<int>(r.maps.size())};
                for (int v : vals) {
                    ImGui::TableNextColumn();
                    std::string t = std::to_string(v);
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(t.c_str()).x);
                    ImGui::TextUnformatted(t.c_str());
                }
            }
            ImGui::EndTable();
        }
        ui::endCard();
    } else {
        ui::beginCard("How it works", Icon::Info);
        struct Step { Icon ic; const char* title; const char* tip; };
        const Step steps[] = {
            {Icon::Filter, "1  Pick maps", "Filters and Rules: site, length, goal."},
            {Icon::Play, "2  Start", "Loads from the main menu."},
            {Icon::Medal, "3  Reach the goal", "Then the next map follows."},
        };
        float w3 = (ImGui::GetContentRegionAvail().x - gap * 2) / 3.0f;
        for (int i = 0; i < 3; ++i) {
            if (i) ImGui::SameLine(0, gap);
            ImGui::BeginGroup();
            icon(steps[i].ic, ui::col(style_.text2), ImGui::GetTextLineHeight() * 1.4f);
            ui::textFit(steps[i].title, w3);
            ImGui::Dummy(ImVec2(w3, 0));
            ImGui::EndGroup();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", steps[i].tip);
        }
        ui::endCard();
    }
}

void Plugin::pageFilters() {
    const float sc = style_.scale;
    const float gap = 12 * sc;
    const HostGame g = host_.game();
    const std::vector<Site> sites = sourceSites(settings_, g);
    const Site site = sites[0];
    const bool stack = stacked();
    const float colW = stack ? 0.0f : (ImGui::GetContentRegionAvail().x - gap) * 0.5f;

    ImGui::BeginGroup();
    ui::beginCard("Source", Icon::Map, colW);
    if (isForever(g)) {
        // Forever: the game's own exchange, the other one, or both (a site per map)
        ui::fieldLabel("TMX source", g == HostGame::Tmnf ? "TMNF drives Stadium only; Both mixes the two sites."
                                                         : "tmnf.exchange adds its Stadium maps.");
        siteBadges(sites, ImGui::GetFrameHeight());
        ImGui::SameLine();
        if (g == HostGame::Tmnf) {
            static const char* const opts[] = {"TMNF-X", "TMUF-X (Stadium)", "Both"};
            int v = settings_.foreverSource;
            if (ui::segmented("fsrc", &v, opts, 3, -1)) {
                settings_.foreverSource = v;
                markDirty();
            }
        } else {
            static const char* const opts[] = {"TMUF-X", "TMUF-X + TMNF-X"};
            int v = settings_.foreverSource == 2 ? 1 : 0;
            if (ui::segmented("fsrc", &v, opts, 2, -1)) {
                settings_.foreverSource = v ? 2 : 0;
                markDirty();
            }
        }
        if (sites.size() > 1 && ui::switchRow("Even site mix", &settings_.evenSites, "On: the site with fewer maps goes next."))
            markDirty();
    } else {
        ui::fieldLabel("TMX site", "Auto: the TMX site of your game.");
        siteBadge(site, ImGui::GetFrameHeight());
        ImGui::SameLine();
        int sc0 = static_cast<int>(settings_.site);
        if (ui::combo("##site", &sc0, kSiteChoices, 6)) {
            settings_.site = static_cast<SiteChoice>(sc0);
            settings_.envMask = 0;
            markDirty();
        }
    }
    ui::endCard();

    ui::beginCard("Length and environment", Icon::Timer, colW);
    ui::fieldLabel("Author time", "Drag, or type m:ss (also past 10:00); empty = any.");
    {
        const float fw = 84 * sc, sp = ImGui::GetStyle().ItemSpacing.x;
        bool ch = ui::durationInput("atmin", &settings_.atMinMs, "min", fw);
        ImGui::SameLine();
        ch |= ui::rangeSlider("at", &settings_.atMinMs, &settings_.atMaxMs, 600000, ImGui::GetContentRegionAvail().x - fw - sp);
        ImGui::SameLine();
        ch |= ui::durationInput("atmax", &settings_.atMaxMs, "max", fw);
        if (ch) markDirty();
        if (settings_.atMinMs > 0 && settings_.atMaxMs > 0 && settings_.atMinMs > settings_.atMaxMs)
            ui::textFit("Min above max", 0, &style_.warn);
    }
    const int* envs = nullptr;
    int nEnv = sourceEnvironments(sites, g, &envs);
    ui::fieldLabel("Environments", nEnv > 1 ? "None = all; several are picked evenly." : nullptr);
    if (nEnv > 1) {
        for (int i = 0; i < nEnv; ++i) {
            if (i) ImGui::SameLine();
            int bit = 1 << (envs[i] - 1);
            if (ui::chip(environmentName(envs[i]), (settings_.envMask & bit) != 0)) {
                settings_.envMask ^= bit;
                markDirty();
            }
        }
    } else {
        ui::textFit("Stadium only", 0, &style_.text2);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", g == HostGame::Tmnf ? "TMNF plays Stadium maps." : ("Only Stadium on " + sitesLabel(sites)).c_str());
    }
    ui::fieldLabel("Difficulty");
    for (int i = 0; i < 5; ++i) {
        if (i) ImGui::SameLine();
        if (ui::chip(kDifficulty[i], settings_.difficulty == i - 1)) {
            settings_.difficulty = i - 1;
            markDirty();
        }
    }
    ui::endCard();
    ImGui::EndGroup();

    if (!stack) ImGui::SameLine(0, gap);
    ImGui::BeginGroup();
    ui::beginCard("Look", Icon::Sun);
    ui::fieldLabel("Mood", "None = any.");
    static const Icon moodIcons[] = {Icon::Sunrise, Icon::Sun, Icon::Sunset, Icon::Moon};
    for (int i = 0; i < 4; ++i) {
        if (i) ImGui::SameLine();
        if (ui::chip(kMoods[i], (settings_.moodMask & (1 << i)) != 0, moodIcons[i])) {
            settings_.moodMask ^= 1 << i;
            markDirty();
        }
    }
    ui::fieldLabel("Routes");
    for (int i = 0; i < 3; ++i) {
        if (i) ImGui::SameLine();
        static const Icon routeIcons[] = {Icon::RouteSingle, Icon::RouteMulti, Icon::RouteSymmetric};
        if (ui::chip(kRoutes[i], (settings_.routesMask & (1 << i)) != 0, routeIcons[i])) {
            settings_.routesMask ^= 1 << i;
            markDirty();
        }
    }
    ui::fieldLabel("Style");
    const char* tagName = settings_.tag >= 0 && settings_.tag < kTagCount ? kTags[settings_.tag] : "Any";
    if (ui::beginCombo("##style", tagName)) {
        if (ImGui::Selectable("Any", settings_.tag < 0)) { settings_.tag = -1; markDirty(); }
        for (int i = 0; i < kTagCount; ++i)
            if (ImGui::Selectable(kTags[i], settings_.tag == i)) { settings_.tag = i; markDirty(); }
        ImGui::EndCombo();
    }
    ui::endCard();

    ui::beginCard("Map checks", Icon::Stop);
    if (ui::switchRow("Skip Unlimiter maps", &settings_.excludeUnlimiter, "They need the Unlimiter mod to load.")) markDirty();
    bool needAt = settings_.goal == Goal::Author;
    bool noAt = !(settings_.requireAuthorTime || needAt);
    bool skipNoAt = !noAt;
    ImGui::BeginDisabled(needAt);
    if (ui::switchRow("Need author time", &skipNoAt, needAt ? "Always on with goal Author." : "Skip maps without an author time.")) {
        settings_.requireAuthorTime = skipNoAt;
        markDirty();
    }
    ImGui::EndDisabled();
    ui::endCard();
    ImGui::EndGroup();

    ui::disclosure("Advanced TMX search", &advFilters_);
    if (advFilters_) {
        ui::beginCard("TMX search", Icon::Sliders);
        if (ImGui::BeginTable("##adv", 2, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextColumn();
            ui::fieldLabel("Name");
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (inputString("##name", settings_.name, "part of the map name")) markDirty();
            ImGui::TableNextColumn();
            ui::fieldLabel("Author", "Exact uploader name.");
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (inputString("##author", settings_.author, "uploader")) markDirty();
            ImGui::TableNextColumn();
            ui::fieldLabel("Uploaded after");
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (inputString("##after", settings_.uploadedAfter, "YYYY-MM-DD")) markDirty();
            ImGui::TableNextColumn();
            ui::fieldLabel("Uploaded before");
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (inputString("##before", settings_.uploadedBefore, "YYYY-MM-DD")) markDirty();
            ImGui::TableNextColumn();
            ui::fieldLabel("Leaderboard");
            int lb = settings_.lbType + 1;
            if (ui::combo("##lb", &lb, kLbTypes, 7)) {
                settings_.lbType = lb - 1;
                markDirty();
            }
            ImGui::TableNextColumn();
            if (site == Site::TMUF) {
                ui::fieldLabel("Vehicle", "TMUF only: maps driven with this car.");
                const char* cars[] = {"Any", "Snow", "Desert", "Rally", "Island", "Coast", "Bay", "Stadium"};
                if (ui::combo("##vehicle", &settings_.vehicle, cars, 8)) markDirty();
            }
            ImGui::EndTable();
        }
        ImGui::Dummy(ImVec2(0, 4 * sc));
        for (int i = 0; i < FlagCount; ++i)
            if (i != FlagReplays && ui::triState(kFlagNames[i], &settings_.flags[i])) markDirty();
        ui::fieldLabel("Race maps only", "Stunts, Platform and Puzzle are planned.");
        ui::endCard();
    }
}

void Plugin::pageRules() {
    const float sc = style_.scale;
    const float gap = 12 * sc;
    const bool stack = stacked();
    const float colW = stack ? 0.0f : (ImGui::GetContentRegionAvail().x - gap) * 0.5f;
    ImGui::BeginGroup();
    ui::beginCard("Mode", Icon::Timer, colW);
    static const char* const modes[] = {"Challenge", "Survival", "Free play"};
    static const char* const modeTips[] = {"Most goal medals before the time runs out.", "Medals add time, skips cost time.",
                                           "No timer; medals still count."};
    int mode = static_cast<int>(settings_.mode);
    if (ui::segmented("mode", &mode, modes, 3, -1, modeTips)) {
        settings_.mode = static_cast<Mode>(mode);
        markDirty();
    }
    if (settings_.mode == Mode::FreePlay) ui::textFit("No timer", 0, &style_.muted);
    if (settings_.mode == Mode::RMC) {
        ui::fieldLabel("Time limit");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::SliderInt("##rmc", &settings_.rmcMinutes, 5, 180, "%d min", ImGuiSliderFlags_AlwaysClamp)) markDirty();
    } else if (settings_.mode == Mode::RMS) {
        ui::fieldLabel("Start time");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::SliderInt("##rms", &settings_.rmsStartMinutes, 1, 60, "%d min", ImGuiSliderFlags_AlwaysClamp)) markDirty();
    }
    ui::endCard();

    ui::beginCard("Goal", Icon::Medal, colW, "The medal that completes a map; better ones count too.");
    static const char* const goals[] = {"Author", "Gold", "Silver", "Bronze", "Finished"};
    static const Medal goalMedals[] = {Medal::Author, Medal::Gold, Medal::Silver, Medal::Bronze, Medal::None};
    int goal = static_cast<int>(settings_.goal);
    for (int i = 0; i < 5; ++i) {
        if (i) ImGui::SameLine();
        ImU32 ic = i < 4 && goal != i ? ImGui::ColorConvertFloat4ToU32(medalColor(goalMedals[i])) : 0;
        if (ui::chip(goals[i], goal == i, i < 4 ? Icon::Medal : Icon::Flag, ic)) {
            settings_.goal = static_cast<Goal>(i);
            markDirty();
        }
    }
    ui::endCard();

    ui::beginCard("Skips", Icon::Skip, colW);
    bool unlimited = settings_.freeSkips < 0;
    if (ui::switchRow("Unlimited free skips", &unlimited)) {
        settings_.freeSkips = unlimited ? -1 : 1;
        markDirty();
    }
    if (!unlimited) {
        ui::fieldLabel("Free skips", "Gives back the time spent on that map.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::SliderInt("##free", &settings_.freeSkips, 0, 10, "%d", ImGuiSliderFlags_AlwaysClamp)) markDirty();
    }
    ImGui::BeginDisabled(settings_.goal != Goal::Author);
    if (ui::switchRow("Gold skip", &settings_.goldSkip, "Goal Author only: skip with gold, counts as gold."))
        markDirty();
    ImGui::EndDisabled();
    ui::endCard();
    ImGui::EndGroup();

    if (!stack) ImGui::SameLine(0, gap);
    ImGui::BeginGroup();
    ui::beginCard("Map choice", Icon::Filter);
    if (ui::switchRow("Even environments", &settings_.equalEnvironments, "Environment first (evenly), then a map of it."))
        markDirty();
    if (ui::switchRow("Skip played maps", &settings_.excludePlayed, "Maps in your saved runs on the same site.")) markDirty();
    if (ui::switchRow("Avoid skipped maps", &settings_.avoidSkipped, "Maps you skipped in saved runs.")) markDirty();
    if (ui::switchRow("Skip finished maps", &settings_.excludeFinished, "Maps with a replay in the game's Autosaves."))
        markDirty();
    ui::endCard();

    ui::beginCard("Between maps", Icon::Flag);
    if (ui::switchRow("Auto next map", &settings_.autoAdvance, "After a goal. TMNF/TMUF: from the end-race screen only."))
        markDirty();
    ImGui::BeginDisabled(!settings_.autoAdvance);
    ui::fieldLabel("Delay");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::SliderInt("##delay", &settings_.advanceDelayS, 0, 15, "%d s", ImGuiSliderFlags_AlwaysClamp)) markDirty();
    ImGui::EndDisabled();
    bool canSave = host_.canSaveReplay();
    ImGui::BeginDisabled(!canSave);
    bool rs = settings_.replayAutosave && canSave;
    if (ui::switchRow("Save goal replays", &rs, canSave ? "Into the game's Replays folder." : "Not supported by this Horizon Client yet.")) {
        settings_.replayAutosave = rs;
        markDirty();
    }
    ImGui::EndDisabled();
    ui::endCard();
    ImGui::EndGroup();

    ui::disclosure("Advanced rules", &advRules_);
    if (advRules_) {
        float half = stack ? 0.0f : (ImGui::GetContentRegionAvail().x - gap) * 0.5f;
        ImGui::BeginGroup();
        ui::beginCard("Survival numbers", Icon::Sliders, half);
        ui::fieldLabel("Author bonus");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::SliderInt("##ab", &settings_.rmsAuthorBonusS, 0, 600, "%d s")) markDirty();
        ui::fieldLabel("Gold bonus", "Gold medal or gold skip.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::SliderInt("##gb", &settings_.rmsGoldBonusS, 0, 600, "%d s")) markDirty();
        ui::fieldLabel("Skip penalty");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::SliderInt("##sp", &settings_.rmsSkipPenaltyS, 0, 600, "%d s")) markDirty();
        ui::endCard();
        ImGui::EndGroup();
        if (!stack) ImGui::SameLine(0, gap);
        ImGui::BeginGroup();
        ui::beginCard("Banned maps", Icon::Stop);
        ui::fieldLabel("TMX ids", "Never picked, on any site. Commas or spaces.");
        char buf[2048];
        std::snprintf(buf, sizeof buf, "%s", settings_.banned.c_str());
        if (ImGui::InputTextMultiline("##banned", buf, sizeof buf, ImVec2(-FLT_MIN, 70 * sc))) {
            settings_.banned = buf;
            markDirty();
        }
        ImGui::TextColored(style_.muted, "%d banned", static_cast<int>(parseIdList(settings_.banned).size()));
        ui::endCard();
        ImGui::EndGroup();
    }
}

void Plugin::pageLayout() {
    const float sc = style_.scale;
    const float gap = 12 * sc;
    const bool stack = stacked();
    const float colW = stack ? 0.0f : (ImGui::GetContentRegionAvail().x - gap) * 0.42f;
    ImGui::BeginGroup();
    ui::beginCard("HUD", Icon::Layout, colW);
    if (ui::switchRow("Show HUD", &settings_.hudVisible, "During a run.")) markDirty();
    ui::fieldLabel("Layout");
    static const char* const presets[] = {"Compact card", "Split"};
    static const char* const presetTips[] = {"Everything in one card.", "A panel per corner."};
    int preset = static_cast<int>(settings_.hudPreset);
    if (ui::segmented("preset", &preset, presets, 2, -1, presetTips)) {
        settings_.hudPreset = static_cast<HudPreset>(preset);
        if (settings_.hudPreset == HudPreset::Split) panels_.applySplitPreset();
        else panels_.applyCompactPreset();
        markDirty();
    }
    ImGui::Dummy(ImVec2(0, 2 * sc));
    if (ui::switchRow("Edit layout", &settings_.layoutEdit, "Move and resize the panels while the menu is open.")) markDirty();
    if (ImGui::Button("Reset layout")) {
        if (settings_.hudPreset == HudPreset::Split) panels_.applySplitPreset();
        else panels_.applyCompactPreset();
    }
    ui::endCard();

    ui::beginCard("Thumbnail", Icon::Map, colW);
    static const char* const tsrc[] = {"TMX screenshot", "Map file", "Off"};
    static const char* const tsrcTips[] = {"One extra request per map.", "TMNF/TMUF maps carry one, ESWC maps don't.", nullptr};
    int ts = static_cast<int>(settings_.thumbSource);
    if (ui::segmented("thumb", &ts, tsrc, 3, -1, tsrcTips)) {
        settings_.thumbSource = static_cast<ThumbSource>(ts);
        markDirty();
    }
    if (!host_.canCreateTextures()) {
        ui::textFit("Not supported", 0, &style_.warn);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("This host can't create textures.");
    }
    ui::endCard();

    ui::beginCard("Hotkeys", Icon::Controls, colW, "Change them in the Horizon menu, Hotkeys.");
    struct Hk { int h; const char* what; };
    const Hk hks[] = {{hkSkip_, "Skip map"}, {hkGoldSkip_, "Gold skip"}, {hkHud_, "Toggle HUD"}, {hkNext_, "Next map"}};
    for (const Hk& k : hks) {
        std::string t = hotkey(k.h);
        float x0 = ImGui::GetCursorPosX();
        const char* key = t.empty() ? "none" : t.c_str();
        float keyW = ImGui::CalcTextSize(key).x;
        ui::textFit(k.what, ImGui::GetContentRegionAvail().x - keyW - ImGui::GetStyle().ItemSpacing.x);
        ImVec4 c = t.empty() ? style_.muted : style_.text2;
        ui::textRight(key, x0 + ImGui::GetContentRegionAvail().x, &c);
    }
    ui::endCard();
    ImGui::EndGroup();

    if (!stack) ImGui::SameLine(0, gap);
    ImGui::BeginGroup();
    ui::beginCard("Panels", Icon::Card, 0, "During a run the HUD is the panels shown here.");
    int n = 0;
    const PanelDef* defs = Panels::table(&n);
    if (ImGui::BeginTable("##lr_panels", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX)) {
        ImGui::TableSetupColumn("Panel", ImGuiTableColumnFlags_WidthStretch, 1.5f);
        ImGui::TableSetupColumn("Show", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupColumn("Opacity", ImGuiTableColumnFlags_WidthStretch, 1.3f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 1.1f);
        ImGui::TableHeadersRow();
        for (int i = 0; i < n; ++i) {
            const PanelDef& d = defs[i];
            if (!d.hud) continue;
            PanelState& w = panels_.state(d);
            ImGui::PushID(d.key);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            icon(d.icon, ui::col(style_.text2));
            ImGui::SameLine();
            ui::textFit(d.title);
            ImGui::TableNextColumn();
            bool shown = w.detached && w.keepOpen;
            if (ui::toggle("##show", &shown)) {
                if (shown) {
                    if (!w.detached) panels_.detach(d);
                    w.keepOpen = true;
                } else {
                    w.keepOpen = false;
                }
                panels_.markDirty();
            }
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::SliderInt("##op", &w.opacity, 20, 100, "%d%%")) panels_.markDirty();
            ImGui::TableNextColumn();
            if (w.detached) {
                if (ImGui::SmallButton("Dock")) panels_.attach(d);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Back to the menu");
            } else if (ImGui::SmallButton("Pop out")) {
                panels_.detach(d);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ui::endCard();
    ImGui::EndGroup();
}

void Plugin::pageHistory() {
    const auto& runs = history_.runs();
    if (ImGui::Button("Reload")) {
        history_.reload();
        histSel_ = -1;
    }
    ImGui::SameLine();
    ImGui::TextColored(style_.muted, "%d runs", static_cast<int>(runs.size()));
    ui::infoTip(("Saved in " + history_.runsFolder()).c_str());
    if (runs.empty()) {
        ui::emptyState(Icon::History, "No runs yet");
        return;
    }
    // narrow: without the Game and Skips columns
    const bool narrow = stacked();
    float listH = ImGui::GetContentRegionAvail().y * (histSel_ >= 0 ? 0.32f : 1.0f);
    if (histSel_ >= 0 && listH < 120 * style_.scale) listH = 120 * style_.scale;
    ImGuiTableFlags f = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY |
                        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX;
    if (ImGui::BeginTable("##lr_runs", narrow ? 5 : 7, f, ImVec2(0, listH))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Started", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        if (!narrow) ImGui::TableSetupColumn("Game", ImGuiTableColumnFlags_WidthStretch, 1.8f);
        ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthStretch, 2.4f);
        ImGui::TableSetupColumn("AT", ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupColumn("Gold", ImGuiTableColumnFlags_WidthStretch, 0.5f);
        if (!narrow) ImGui::TableSetupColumn("Skips", ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupColumn("Maps", ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableHeadersRow();
        for (int i = 0; i < static_cast<int>(runs.size()); ++i) {
            const RunRecord& r = runs[i];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushID(i);
            std::string when = r.started;
            for (char& c : when)
                if (c == 'T') c = ' ';
            std::string whenFit = ui::ellipsize(when.c_str(), ImGui::GetContentRegionAvail().x);
            if (ImGui::Selectable(whenFit.c_str(), histSel_ == i, ImGuiSelectableFlags_SpanAllColumns)) histSel_ = histSel_ == i ? -1 : i;
            ImGui::PopID();
            if (!narrow) {
                ImGui::TableNextColumn();
                ui::textFit(r.game.c_str());
            }
            ImGui::TableNextColumn();
            char mb[96];
            std::snprintf(mb, sizeof mb, "%s, %s", narrow ? (r.mode == "rms" ? "Survival" : r.mode == "free" ? "Free play" : "Challenge")
                                                          : modeTitle(r.mode),
                          goalTitle(Session::goalFromKey(r.goal.c_str())));
            ui::textFit(mb);
            int vals[] = {r.authors, r.golds, r.skips, static_cast<int>(r.maps.size())};
            for (int k = 0; k < 4; ++k) {
                if (narrow && k == 2) continue;
                int v = vals[k];
                ImGui::TableNextColumn();
                char b[16];
                std::snprintf(b, sizeof b, "%d", v);
                float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
                ImGui::SetCursorPosX(right - ImGui::CalcTextSize(b).x);
                ImGui::TextUnformatted(b);
            }
        }
        ImGui::EndTable();
    }
    if (histSel_ >= 0 && histSel_ < static_cast<int>(runs.size())) {
        ImGui::Spacing();
        drawResults(runs[histSel_], false);
    }
}

void Plugin::pageAbout() {
    // label / value rows, the value cut with an ellipsis (full text on hover)
    auto row = [&](const char* label, const char* value, const char* tip = nullptr, const ImVec4* vc = nullptr) {
        const float lw = std::min(120 * style_.scale, ImGui::GetContentRegionAvail().x * 0.4f);
        float x0 = ImGui::GetCursorPosX();
        ui::textFit(label, lw - 8 * style_.scale, &style_.muted);
        ImGui::SameLine(x0 + lw);
        ui::textFit(value, ImGui::GetContentRegionAvail().x - (tip ? ImGui::GetTextLineHeight() : 0.0f), vc ? vc : &style_.text2);
        if (tip) ui::infoTip(tip);
    };
    ui::beginCard("Legacy Randomizer", Icon::Info);
    row("Modes", "Challenge, Survival, Free play");
    row("Maps", "TrackMania Exchange", "The next map downloads while you drive.");
    row("Net play", "Off", "Does nothing online.", &style_.ok);
    row("Physics", "Untouched", "Never changes physics, timing or replays.");
    if (isForever(host_.game()) || host_.game() == HostGame::Original)
        row("This game", "Experimental", isForever(host_.game()) ? "Maps change only in menus (Competition Patch)." : nullptr, &style_.warn);
    row("Version", LR_PLUGIN_VERSION);
    row("Licence", "MIT");
    row("Credits", "TMX, RandomizerTMF", "Map data and screenshots: TMX. Rules after RandomizerTMF.");
    ui::endCard();
    ui::beginCard("Files", Icon::List);
    const std::string data = host_.dataFolder();
    row("Data folder", data.c_str());
    if (ImGui::SmallButton("Copy path")) ImGui::SetClipboardText(data.c_str());
    char n[32];
    std::snprintf(n, sizeof n, "%d", static_cast<int>(history_.runs().size()));
    row("Saved runs", n);
    ui::endCard();
}

// ------------------------------------------------------------------ results / tables

void Plugin::drawMapsTable(const RunRecord& r, const char* id, float height) {
    ImGuiTableFlags f = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp |
                        ImGuiTableFlags_PadOuterX;
    if (height > 0) f |= ImGuiTableFlags_ScrollY;
    // narrow: without the Author and TMX columns
    const bool narrow = stacked();
    if (!ImGui::BeginTable(id, narrow ? 5 : 7, f, ImVec2(0, height))) return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Map", ImGuiTableColumnFlags_WidthStretch, 3.0f);
    if (!narrow) ImGui::TableSetupColumn("Author", ImGuiTableColumnFlags_WidthStretch, 1.6f);
    ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthStretch, 1.6f);
    ImGui::TableSetupColumn("Best", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("On map", ImGuiTableColumnFlags_WidthStretch, 0.9f);
    if (!narrow) ImGui::TableSetupColumn("TMX", ImGuiTableColumnFlags_WidthStretch, 0.9f);
    ImGui::TableHeadersRow();
    for (size_t i = 0; i < r.maps.size(); ++i) {
        const RunMapRecord& m = r.maps[i];
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%d", static_cast<int>(i + 1));
        ImGui::TableNextColumn();
        TmTextStyle ts;
        ts.maxWidth = ImGui::GetContentRegionAvail().x;
        tmText(m.name, ts);
        if (!narrow) {
            ImGui::TableNextColumn();
            ui::textFit(m.author.c_str());
        }
        ImGui::TableNextColumn();
        ui::medalDot(medalColor(medalFromKey(m.medal)));
        ui::textFit(resultTitle(m.result));
        auto right = [](const std::string& t, const ImVec4& c) {
            float rx = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(rx - ImGui::CalcTextSize(t.c_str()).x);
            ImGui::TextColored(c, "%s", t.c_str());
        };
        ImGui::TableNextColumn();
        right(m.bestMs > 0 ? formatMs(m.bestMs) : std::string("-"), m.bestMs > 0 ? style_.text : style_.muted);
        ImGui::TableNextColumn();
        right(formatClock(m.spentMs), style_.muted);
        if (!narrow) {
            ImGui::TableNextColumn();
            right(std::to_string(m.tmxId), style_.muted);
        }
    }
    ImGui::EndTable();
}

void Plugin::drawResults(const RunRecord& r, bool live) {
    const float sc = style_.scale;
    ui::beginCard(live ? "Run finished" : "Run", Icon::Flag);
    char b[200];
    if (r.timeLimitMs > 0 && r.mode == "rmc")
        std::snprintf(b, sizeof b, "%s, %d min, goal %s", modeTitle(r.mode), r.timeLimitMs / 60000, goalTitle(Session::goalFromKey(r.goal.c_str())));
    else
        std::snprintf(b, sizeof b, "%s, goal %s", modeTitle(r.mode), goalTitle(Session::goalFromKey(r.goal.c_str())));
    ui::textFit(b, 0, &style_.text2);
    // the counters wrap as a whole ("Gold 3"), never in the middle
    ImGui::PushFont(titleFont());
    const float dotW = ImGui::GetTextLineHeight() * 0.44f + 2 + ImGui::GetStyle().ItemInnerSpacing.x;
    ui::medalDot(kAuthor);
    ImGui::Text("AT %d", r.authors);
    std::snprintf(b, sizeof b, "Gold %d", r.golds);
    ui::flowNext(dotW + ImGui::CalcTextSize(b).x, 22 * sc);
    ui::medalDot(kGold);
    ImGui::TextUnformatted(b);
    std::snprintf(b, sizeof b, "Skips %d", r.skips);
    ui::flowNext(ImGui::CalcTextSize(b).x, 22 * sc);
    ImGui::TextUnformatted(b);
    std::snprintf(b, sizeof b, "Maps %d", static_cast<int>(r.maps.size()));
    ui::flowNext(ImGui::CalcTextSize(b).x, 22 * sc);
    ImGui::TextUnformatted(b);
    ImGui::PopFont();
    const char* by = r.endedBy == "timer" ? "time ran out" : r.endedBy == "user" ? "stopped" : r.endedBy.c_str();
    // the details as pieces that wrap whole: played time, site, how it ended, start
    std::string when = r.started;
    for (char& c : when)
        if (c == 'T') c = ' ';
    const std::string parts[] = {"Played " + formatClock(r.elapsedMs), r.site, by, when};
    for (size_t i = 0; i < 4; ++i) {
        std::string t = parts[i] + (i < 3 ? "," : "");
        if (i) ui::flowNext(ImGui::CalcTextSize(t.c_str()).x, ImGui::CalcTextSize(" ").x);
        ui::textFit(t.c_str(), 0, &style_.muted);
    }
    if (live) {
        if (ImGui::Button("New run", ImVec2(140 * sc, 0))) run_.newRun();
        ImGui::SameLine();
        if (!run_.lastRunPath().empty()) {
            ImGui::TextColored(style_.muted, "Saved");
            ui::infoTip(run_.lastRunPath().c_str());
        } else if (!run_.error().empty()) {
            ui::textFit(run_.error().c_str(), 0, &style_.bad);
        }
    }
    ui::endCard();
    float h = ImGui::GetContentRegionAvail().y - 8 * sc;
    drawMapsTable(r, live ? "##lr_results" : "##lr_hist_maps", h > 120 * sc ? h : 0);
}

// ------------------------------------------------------------------ HUD panels

void Plugin::drawMapCard(PreparedMap* m, float width, bool thumb) {
    ensureTexture(m);
    if (thumb && m->tex && m->thumb.w > 0) {
        float iw = std::min(width, 480 * style_.scale);  // a stacked card is wide: keep the picture sensible
        float h = iw * static_cast<float>(m->thumb.h) / static_cast<float>(m->thumb.w);
        if (h > iw * 0.62f) h = iw * 0.62f;
        ImGui::Image(m->tex, ImVec2(iw, h));
    }
    TmTextStyle ts;
    ts.font = style_.title;
    ts.size = style_.title ? style_.title->FontSize * 0.85f : 0;
    ts.maxWidth = width;
    ts.defaultColor = ImGui::ColorConvertFloat4ToU32(style_.text);
    tmText(m->name, ts);
    float x0 = ImGui::GetCursorPosX();
    ImGui::TextColored(style_.muted, "by %s", tmPlain(m->author).c_str());
    if (m->tmx.id > 0) {
        std::string id = "TMX " + std::to_string(m->tmx.id);
        ImVec4 c = style_.muted;
        ui::textRight(id.c_str(), x0 + width, &c);
    }
}

void Plugin::drawMedalRows(PreparedMap* m, float width) {
    const Session* s = run_.session();
    Goal goal = s ? s->config().goal : settings_.goal;
    bool isCur = run_.phase() != Phase::Loading && run_.phase() != Phase::Preparing;
    Medal best = isCur ? run_.bestMedal() : Medal::None;
    int pb = isCur ? run_.bestMs() : 0;
    Medal gm = goalMedal(goal);
    const Medal rows[] = {Medal::Author, Medal::Gold, Medal::Silver, Medal::Bronze};
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float lineH = ImGui::GetTextLineHeightWithSpacing();
    for (Medal md : rows) {
        int t = medalTime(m->medals, md);
        ImVec2 p = ImGui::GetCursorScreenPos();
        float x0 = ImGui::GetCursorPosX();
        bool reached = best == md;
        if (reached) {
            ImVec4 bg = medalColor(md);
            bg.w = 0.20f;
            dl->AddRectFilled(ImVec2(p.x - 4, p.y - 1), ImVec2(p.x + width + 4, p.y + lineH - 1), ImGui::ColorConvertFloat4ToU32(bg),
                              4.0f * style_.scale);
        }
        ui::medalDot(medalColor(md));
        ImGui::TextColored(reached ? style_.text : style_.text2, "%s", medalShort(md));
        if (md == gm) {
            ImGui::SameLine();
            ImGui::TextColored(style_.accent, "goal");
        }
        timeRight(t, x0 + width, reached ? medalColor(md) : style_.text2);
    }
    float x0 = ImGui::GetCursorPosX();
    ImGui::TextColored(style_.muted, "Personal best");
    if (pb > 0) {
        timeRight(pb, x0 + width, best != Medal::None ? medalColor(best) : style_.text);
    } else {
        ImVec4 c = style_.muted;
        ui::textRight("no finish yet", x0 + width, &c);
    }
}

void Plugin::panelTimer() {
    const Session* s = run_.session();
    bool live = s && (run_.active() || run_.phase() == Phase::Over);
    int ms = !live ? 0 : (s->countsDown() ? s->remainingMs() : s->elapsedMs());
    ImGui::PushFont(titleFont());
    ImGui::SetWindowFontScale(1.7f);
    ImVec4 c = !live ? style_.muted : (s->countsDown() && ms < 60000 ? style_.bad : style_.accent);
    ImGui::TextColored(c, "%s", live ? formatClock(ms).c_str() : "--:--");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopFont();
    if (live) ImGui::TextColored(style_.muted, "%s, goal %s", modeShort(s->config().mode), goalTitle(s->config().goal));
    else ImGui::TextColored(style_.muted, "No run");
}

void Plugin::panelMap() {
    std::shared_ptr<PreparedMap> m = run_.displayMap();
    float w = ImGui::GetContentRegionAvail().x;
    if (m) drawMapCard(m.get(), w, settings_.thumbSource != ThumbSource::Off);
    else if (run_.active()) ImGui::TextColored(run_.fetchFailing() ? style_.bad : style_.muted, "%s", run_.fetchStatus().c_str());
    else ImGui::TextColored(style_.muted, "No map yet");
}

void Plugin::panelMedals() {
    std::shared_ptr<PreparedMap> m = run_.displayMap();
    float w = ImGui::GetContentRegionAvail().x;
    if (m) drawMedalRows(m.get(), w);
    else ImGui::TextColored(style_.muted, "No map yet");
    const Session* s = run_.session();
    if (!s) return;
    ImGui::Separator();
    ui::medalDot(kAuthor);
    ImGui::Text("AT %d", s->authorCount());
    ImGui::SameLine(0, 12 * style_.scale);
    ui::medalDot(kGold);
    ImGui::Text("Gold %d", s->goldCount());
    ImGui::SameLine(0, 12 * style_.scale);
    int left = s->freeSkipsLeft();
    if (left < 0) ImGui::Text("Skips %d", s->skipCount());
    else ImGui::Text("Skips %d (%d free)", s->skipCount(), left);
    if (s->finishedCount() > 0) {
        ImGui::SameLine(0, 12 * style_.scale);
        ImGui::Text("Finished %d", s->finishedCount());
    }
}

void Plugin::drawControls(float width, bool withStop) {
    const Session* s = run_.session();
    float sp = ImGui::GetStyle().ItemSpacing.x;
    bool showGold = s && s->config().goal == Goal::Author && s->config().allowGoldSkip;
    // Skip [Gold skip] Stop: Stop as wide as its label, the others share the rest
    const float stopW = withStop ? ImGui::CalcTextSize("Stop").x + ImGui::GetStyle().FramePadding.x * 2 + 8 * style_.scale : 0.0f;
    const float rest = withStop ? width - stopW - sp : width;
    float bw = showGold ? (rest - sp) * 0.5f : rest;
    if (!s || !run_.active()) {
        ImGui::BeginDisabled();
        ImGui::Button("Skip", ImVec2(width, 0));
        ImGui::EndDisabled();
        return;
    }
    if (run_.phase() == Phase::Leaving) {
        if (ImGui::Button("Next map", ImVec2(bw, 0))) run_.nextNow();
    } else if (run_.leftMap()) {
        if (ImGui::Button("Resume map", ImVec2(bw, 0))) run_.resume();
    } else if (run_.phase() == Phase::Loading && run_.pendingLoadAgeMs() > 15000) {
        if (ImGui::Button("Another map", ImVec2(bw, 0))) run_.dropPendingMap();
    } else {
        ImGui::BeginDisabled(!run_.canSkip());
        if (ImGui::Button(s->freeSkipsLeft() != 0 ? "Free skip" : "Skip", ImVec2(bw, 0))) run_.skip();
        ImGui::EndDisabled();
    }
    if (showGold) {
        ImGui::SameLine(0, sp);
        ImGui::BeginDisabled(!run_.canGoldSkip());
        if (ImGui::Button("Gold skip", ImVec2(bw, 0))) run_.goldSkip();
        ImGui::EndDisabled();
    }
    if (withStop) {
        ImGui::SameLine(0, sp);
        stopButton(ImVec2(stopW, 0));
    }
}

void Plugin::panelControls(bool withStop) {
    float w = ImGui::GetContentRegionAvail().x;
    drawControls(w, withStop);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + w);
    if (run_.goalJustReached()) {
        Medal md = run_.bestMedal();
        ImGui::TextColored(medalColor(md), "%s reached", md == Medal::None ? "Goal" : medalShort(md));
    }
    if (!run_.notice().empty()) ImGui::TextColored(style_.warn, "%s", run_.notice().c_str());
    if (!run_.error().empty()) ImGui::TextColored(style_.bad, "%s", run_.error().c_str());
    if (run_.active() && run_.phase() != Phase::Preparing)
        ImGui::TextColored(run_.fetchFailing() ? style_.bad : style_.muted, "Next map: %s", run_.fetchStatus().c_str());
    ImGui::PopTextWrapPos();
    std::string hs = hotkey(hkSkip_), hg = hotkey(hkGoldSkip_);
    if (!hs.empty() || !hg.empty()) ImGui::TextColored(style_.muted, "%s skip   %s gold skip", hs.c_str(), hg.c_str());
}

void Plugin::panelSession() {
    RunRecord snap = run_.snapshot();
    float w = ImGui::GetContentRegionAvail().x;
    if (snap.maps.empty() && !run_.displayMap()) {
        ImGui::TextColored(style_.muted, "No maps yet");
        return;
    }
    float timeW = ImGui::CalcTextSize("0:00.000").x + 8 * style_.scale;
    for (size_t i = 0; i < snap.maps.size(); ++i) {
        const RunMapRecord& m = snap.maps[i];
        float x0 = ImGui::GetCursorPosX();
        ui::medalDot(medalColor(medalFromKey(m.medal)));
        TmTextStyle ts;
        ts.maxWidth = w - timeW - ImGui::GetTextLineHeight();
        ts.defaultColor = ImGui::ColorConvertFloat4ToU32(style_.text2);
        tmText(m.name, ts);
        bool skipped = m.result == "skipped" || m.result == "free_skip" || m.result == "unfinished";
        std::string t = skipped ? (m.result == "unfinished" ? "-" : "skip") : (m.bestMs > 0 ? formatMs(m.bestMs) : "-");
        ImVec4 c = skipped ? style_.muted : medalColor(medalFromKey(m.medal));
        ui::textRight(t.c_str(), x0 + w, &c);
    }
    std::shared_ptr<PreparedMap> cur = run_.displayMap();
    if (cur && run_.active() && run_.phase() != Phase::Leaving) {
        float x0 = ImGui::GetCursorPosX();
        ui::medalDot(style_.accent);
        TmTextStyle ts;
        ts.maxWidth = w - timeW - ImGui::GetTextLineHeight();
        tmText(cur->name, ts);
        ImVec4 c = style_.accent;
        ui::textRight("now", x0 + w, &c);
    }
}

void Plugin::panelCard() {
    const Session* s = run_.session();
    if (!s || !(run_.active() || run_.phase() == Phase::Over)) {
        panelTimer();
        return;
    }
    const bool compact = settings_.hudCompact;
    float w = ImGui::GetContentRegionAvail().x;
    float x0 = ImGui::GetCursorPosX();
    int ms = s->countsDown() ? s->remainingMs() : s->elapsedMs();
    ImGui::PushFont(titleFont());
    ImGui::SetWindowFontScale(compact ? 1.25f : 1.55f);
    ImGui::TextColored(s->countsDown() && ms < 60000 ? style_.bad : style_.accent, "%s", formatClock(ms).c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopFont();
    char modeText[64];
    std::snprintf(modeText, sizeof modeText, "%s, %s", modeShort(s->config().mode), goalTitle(s->config().goal));
    ImVec4 mc = style_.muted;
    ui::textRight(modeText, x0 + w, &mc);
    std::shared_ptr<PreparedMap> m = run_.displayMap();
    if (m) {
        drawMapCard(m.get(), w, !compact && settings_.thumbSource != ThumbSource::Off);
        if (!compact) drawMedalRows(m.get(), w);
    } else {
        ImGui::TextColored(run_.fetchFailing() ? style_.bad : style_.muted, "%s", run_.fetchStatus().c_str());
    }
    ImGui::Separator();
    ui::medalDot(kAuthor);
    ImGui::Text("AT %d", s->authorCount());
    ImGui::SameLine(0, 12 * style_.scale);
    ui::medalDot(kGold);
    ImGui::Text("Gold %d", s->goldCount());
    ImGui::SameLine(0, 12 * style_.scale);
    ImGui::Text("Skips %d", s->skipCount());
    ImGui::PushTextWrapPos(x0 + w);
    if (!run_.notice().empty()) ImGui::TextColored(style_.warn, "%s", run_.notice().c_str());
    if (!run_.error().empty()) ImGui::TextColored(style_.bad, "%s", run_.error().c_str());
    ImGui::PopTextWrapPos();
    // In a locked (click-through) window the buttons live in a small window of their own.
    bool clickThrough = (ImGui::GetCurrentWindow()->Flags & ImGuiWindowFlags_NoInputs) != 0;
    if (!clickThrough) {
        drawControls(w);
        return;
    }
    ImVec2 at = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(w, ImGui::GetFrameHeight()));
    ImGui::SetNextWindowPos(at, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    if (ImGui::Begin("##lr_card_buttons", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_AlwaysAutoResize))
        drawControls(w);
    ImGui::End();
    ImGui::PopStyleVar(2);
}

// ------------------------------------------------------------------ overlay

void Plugin::drawResultsToast() {
    const RunRecord& r = run_.lastRun();
    const float sc = style_.scale;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + 24 * sc), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.92f);
    const ImGuiWindowFlags f = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                               ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("##lr_toast", nullptr, f)) {
        ImGui::PushFont(titleFont());
        ImGui::TextColored(style_.accent, "RUN FINISHED");
        ImGui::PopFont();
        ui::medalDot(kAuthor);
        ImGui::Text("AT %d", r.authors);
        ImGui::SameLine(0, 14 * sc);
        ui::medalDot(kGold);
        ImGui::Text("Gold %d", r.golds);
        ImGui::SameLine(0, 14 * sc);
        ImGui::Text("Skips %d   Maps %d", r.skips, static_cast<int>(r.maps.size()));
        ImGui::TextColored(style_.muted, "Results: Horizon menu");
        if (ImGui::Button("Dismiss", ImVec2(110 * sc, 0))) toastDismissed_ = true;
    }
    ImGui::End();
}

void Plugin::drawOverlay() {
    style_ = host_.style();
    ui::setStyle(style_);
    Phase before = run_.phase();
    run_.update();
    if (run_.phase() == Phase::Over && before != Phase::Over) toastDismissed_ = false;
    if (run_.phase() != Phase::Over) toastDismissed_ = true;
    if (dirty_ && !ImGui::IsAnyItemActive()) {
        settings_.save(host_);
        dirty_ = false;
    }
    bool menu = host_.menuOpen();
    if (run_.phase() == Phase::Over && !toastDismissed_ && !menu) drawResultsToast();
    panels_.drawWindows(menu, run_.active(), settings_.hudVisible, settings_.layoutEdit);
    stopPopup();
    panels_.saveIfDirty(host_);
}

} // namespace lr
