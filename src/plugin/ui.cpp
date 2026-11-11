// Menu pages (Run, Filters, Rules, HUD & layout, History, About) and the HUD panels (Timer, Map, Medals,
// Controls, Session, Compact card). Which page shows where is panels.cpp's job. Colours come from the
// host's style; the only own colours are the medal colours.
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
    if (k == "author") return "Author medal";
    if (k == "gold") return "Gold medal";
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
const char* const kSiteChoices[] = {"Auto (from the game)", "tmnf.exchange", "tmuf.exchange", "nations.tm-exchange.com",
                                    "sunrise.tm-exchange.com", "original.tm-exchange.com"};
const char* const kMoods[] = {"Sunrise", "Day", "Sunset", "Night"};
const char* const kRoutes[] = {"Single", "Multi", "Symmetric"};
const char* const kLbTypes[] = {"Any", "Standard", "Classic", "Nadeo", "Uncompetitive", "Beta", "Star"};
const char* const kFlagNames[] = {"Has a screenshot", "Featured", "Has a record", "Has replays", "Author's latest map", "By a supporter"};

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

// ------------------------------------------------------------------ menu shell

void Plugin::drawMenu() {
    style_ = host_.style();
    ui::setStyle(style_);
    const float sc = style_.scale;
    drawHeader();
    ImGui::BeginChild("##lr_nav", ImVec2(190 * sc, 0));
    panels_.drawNavigation();
    ImGui::EndChild();
    ImGui::SameLine(0, 14 * sc);
    ImGui::BeginChild("##lr_page", ImVec2(0, 0));
    panels_.drawCurrentPage();
    ImGui::EndChild();
    if (dirty_ && !ImGui::IsAnyItemActive()) {
        settings_.save(host_);
        dirty_ = false;
    }
    panels_.saveIfDirty(host_);
}

void Plugin::drawHeader() {
    const float sc = style_.scale;
    icon(Icon::Flag, ui::col(style_.accent), ImGui::GetTextLineHeight() * 1.2f);
    ImGui::SameLine();
    ImGui::PushFont(titleFont());
    ImGui::TextColored(style_.accent, "LEGACY RANDOMIZER");
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextColored(style_.muted, "v" LR_PLUGIN_VERSION);
    HostGame g = host_.game();
    std::string game = host_.gameName();
    std::string site = siteHost(siteFor(settings_.site, g));
    float w = ImGui::CalcTextSize((game + "   " + site).c_str()).x + 20 * sc;
    ImGui::SameLine(ImGui::GetContentRegionMax().x - w);
    ui::medalDot(g == HostGame::Unknown ? style_.bad : style_.ok);
    ImGui::TextUnformatted(game.c_str());
    ImGui::SameLine();
    ImGui::TextColored(style_.muted, "%s", site.c_str());
    if (g == HostGame::Original) ImGui::TextColored(style_.warn, "TM Original support is experimental.");
    if (host_.isOnline() || !host_.actionsAllowed())
        ImGui::TextColored(style_.warn, "Net play: the randomizer is paused and does nothing here.");
    ImGui::Dummy(ImVec2(0, 4 * sc));
}

// ------------------------------------------------------------------ pages

void Plugin::pageRun() {
    const float sc = style_.scale;
    const Session* s = run_.session();
    if (run_.active() && s) {
        ui::beginCard("Run in progress", Icon::Timer);
        panelTimer();
        ui::medalDot(kAuthor);
        ImGui::Text("AT %d", s->authorCount());
        ImGui::SameLine(0, 16 * sc);
        ui::medalDot(kGold);
        ImGui::Text("Gold %d", s->goldCount());
        ImGui::SameLine(0, 16 * sc);
        ImGui::Text("Skips %d", s->skipCount());
        ImGui::SameLine(0, 16 * sc);
        ImGui::TextColored(style_.muted, "%s", phaseName(run_.phase()));
        if (ImGui::Button("End run")) ImGui::OpenPopup("End run?");
        if (ImGui::BeginPopupModal("End run?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("End this run now? It is saved with the maps played so far.");
            if (ImGui::Button("End run", ImVec2(120 * sc, 0))) {
                run_.stop();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Keep playing", ImVec2(120 * sc, 0))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ui::endCard();
        float half = (ImGui::GetContentRegionAvail().x - 12 * sc) * 0.5f;
        ImGui::BeginGroup();
        ImGui::PushID("left");
        ImGui::BeginChild("##lr_run_left", ImVec2(half, 0), ImGuiChildFlags_AutoResizeY);
        ui::beginCard("Current map", Icon::Map);
        std::shared_ptr<PreparedMap> m = run_.displayMap();
        if (m) drawMapCard(m.get(), ImGui::GetContentRegionAvail().x, true);
        else ImGui::TextColored(run_.fetchFailing() ? style_.bad : style_.muted, "%s", run_.fetchStatus().c_str());
        ui::endCard();
        ImGui::EndChild();
        ImGui::PopID();
        ImGui::EndGroup();
        ImGui::SameLine(0, 12 * sc);
        ImGui::BeginChild("##lr_run_right", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
        ui::beginCard("Medals", Icon::Medal);
        panelMedals();
        ui::endCard();
        ui::beginCard("Controls", Icon::Controls);
        panelControls();
        ui::endCard();
        ImGui::EndChild();
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
    Site site = siteFor(settings_.site, host_.game());
    ui::beginCard("Start a run", Icon::Play);
    ImGui::TextColored(style_.text2, "%s", settings_.summary(site).c_str());
    ImGui::Dummy(ImVec2(0, 6 * sc));
    bool blocked = host_.isOnline() || !host_.actionsAllowed();
    static std::string startError;
    if (ui::bigButton("Start run", Icon::Play, ImVec2(240 * sc, 48 * sc), !blocked)) {
        if (dirty_) {
            settings_.save(host_);
            dirty_ = false;
        }
        startError.clear();
        if (!run_.start(&startError)) host_.log(LogWarn, "start: " + startError);
    }
    if (blocked) ImGui::TextColored(style_.warn, "Not available in net play.");
    else if (!startError.empty()) ImGui::TextColored(style_.bad, "%s", startError.c_str());
    else ImGui::TextColored(style_.muted, "Mode and goal: Rules. Which maps: Filters. The first map starts from the main menu.");
    ui::endCard();

    ui::beginCard("Last run", Icon::History);
    if (history_.runs().empty()) {
        ImGui::TextColored(style_.muted, "No run yet.");
    } else {
        const RunRecord& r = history_.runs().front();
        std::string when = r.started;
        for (char& c : when)
            if (c == 'T') c = ' ';
        ImGui::TextColored(style_.text2, "%s, goal %s, %s", modeTitle(r.mode), goalTitle(Session::goalFromKey(r.goal.c_str())),
                           when.c_str());
        ui::medalDot(kAuthor);
        ImGui::Text("AT %d", r.authors);
        ImGui::SameLine(0, 16 * sc);
        ui::medalDot(kGold);
        ImGui::Text("Gold %d", r.golds);
        ImGui::SameLine(0, 16 * sc);
        ImGui::Text("Skips %d", r.skips);
        ImGui::SameLine(0, 16 * sc);
        ImGui::Text("Maps %d", static_cast<int>(r.maps.size()));
        if (ImGui::Button("Open in History")) {
            histSel_ = 0;
            panels_.select("history");
        }
    }
    ui::endCard();
}

void Plugin::pageFilters() {
    const float sc = style_.scale;
    Site site = siteFor(settings_.site, host_.game());
    ui::beginCard("Source", Icon::Map);
    int sc0 = static_cast<int>(settings_.site);
    if (ImGui::Combo("TMX site", &sc0, kSiteChoices, 6)) {
        settings_.site = static_cast<SiteChoice>(sc0);
        settings_.envMask = 0;
        markDirty();
    }
    ui::infoTip("Auto takes the TMX site of the game you play: ESWC nations, Sunrise sunrise, Original original, "
                "TMNF tmnf.exchange, TMUF tmuf.exchange.");
    ui::endCard();

    ui::beginCard("Maps", Icon::Filter);
    if (sliderSeconds("Author time from", &settings_.atMinS, 600, "any")) markDirty();
    if (sliderSeconds("Author time up to", &settings_.atMaxS, 600, "any")) markDirty();
    const int* envs = nullptr;
    int nEnv = siteEnvironments(site, &envs);
    ui::label("Environments");
    if (nEnv > 1) {
        for (int i = 0; i < nEnv; ++i) {
            if (i) ImGui::SameLine();
            int bit = 1 << (envs[i] - 1);
            if (ui::chip(environmentName(envs[i]), (settings_.envMask & bit) != 0)) {
                settings_.envMask ^= bit;
                markDirty();
            }
        }
        ui::infoTip("None selected = all. With several, one environment is picked first for each map (evenly).");
    } else {
        ImGui::TextColored(style_.text2, "Stadium only on %s", siteHost(site));
    }
    ui::label("Difficulty");
    int diff = settings_.difficulty + 1;
    if (ui::segmented("diff", &diff, kDifficulty, 5)) {
        settings_.difficulty = diff - 1;
        markDirty();
    }
    ui::label("Mood");
    for (int i = 0; i < 4; ++i) {
        if (i) ImGui::SameLine();
        if (ui::chip(kMoods[i], (settings_.moodMask & (1 << i)) != 0)) {
            settings_.moodMask ^= 1 << i;
            markDirty();
        }
    }
    ui::infoTip("None selected = any mood.");
    ui::label("Routes");
    for (int i = 0; i < 3; ++i) {
        if (i) ImGui::SameLine();
        if (ui::chip(kRoutes[i], (settings_.routesMask & (1 << i)) != 0)) {
            settings_.routesMask ^= 1 << i;
            markDirty();
        }
    }
    ImGui::Dummy(ImVec2(0, 4 * sc));
    const char* tagName = settings_.tag >= 0 && settings_.tag < kTagCount ? kTags[settings_.tag] : "Any";
    if (ImGui::BeginCombo("Style", tagName)) {
        if (ImGui::Selectable("Any", settings_.tag < 0)) { settings_.tag = -1; markDirty(); }
        for (int i = 0; i < kTagCount; ++i)
            if (ImGui::Selectable(kTags[i], settings_.tag == i)) { settings_.tag = i; markDirty(); }
        ImGui::EndCombo();
    }
    if (ui::toggle("Exclude Unlimiter maps", &settings_.excludeUnlimiter)) markDirty();
    bool needAt = settings_.goal == Goal::Author;
    bool reqAt = settings_.requireAuthorTime || needAt;
    ImGui::BeginDisabled(needAt);
    if (ui::toggle("Only maps with an author time", &reqAt)) {
        settings_.requireAuthorTime = reqAt;
        markDirty();
    }
    ImGui::EndDisabled();
    ui::infoTip("Always on with the goal Author: without an author time the goal can't be reached.");
    ui::endCard();

    if (ImGui::CollapsingHeader("Advanced")) {
        ui::beginCard("TMX search", Icon::Sliders);
        if (inputString("Name", settings_.name, "part of the map name")) markDirty();
        if (inputString("Author", settings_.author, "uploader, exact name")) markDirty();
        int lb = settings_.lbType + 1;
        if (ImGui::Combo("Leaderboard", &lb, kLbTypes, 7)) {
            settings_.lbType = lb - 1;
            markDirty();
        }
        if (inputString("Uploaded after", settings_.uploadedAfter, "YYYY-MM-DD")) markDirty();
        if (inputString("Uploaded before", settings_.uploadedBefore, "YYYY-MM-DD")) markDirty();
        if (site == Site::TMUF) {
            const char* cars[] = {"Any", "Snow", "Desert", "Rally", "Island", "Coast", "Bay", "Stadium"};
            if (ImGui::Combo("Vehicle", &settings_.vehicle, cars, 8)) markDirty();
            ui::infoTip("TMUF only: maps driven with this environment's car (environment mixes).");
        }
        ImGui::Dummy(ImVec2(0, 4 * sc));
        for (int i = 0; i < FlagCount; ++i)
            if (ui::triState(kFlagNames[i], &settings_.flags[i])) markDirty();
        ImGui::TextColored(style_.muted, "Map type: Race only. Stunts, Platform and Puzzle are planned.");
        ui::endCard();
    }
}

void Plugin::pageRules() {
    const float sc = style_.scale;
    ui::beginCard("Mode", Icon::Timer);
    static const char* const modes[] = {"Challenge", "Survival", "Free play"};
    int mode = static_cast<int>(settings_.mode);
    if (ui::segmented("mode", &mode, modes, 3)) {
        settings_.mode = static_cast<Mode>(mode);
        markDirty();
    }
    ui::infoTip("Challenge (RMC): as many goal medals as possible before the timer runs out. Survival (RMS): medals add time, "
                "skips take time; the run ends at 0. Free play: no timer.");
    if (settings_.mode == Mode::RMC &&
        ImGui::SliderInt("Time limit", &settings_.rmcMinutes, 5, 180, "%d min", ImGuiSliderFlags_AlwaysClamp))
        markDirty();
    if (settings_.mode == Mode::RMS &&
        ImGui::SliderInt("Start time", &settings_.rmsStartMinutes, 1, 60, "%d min", ImGuiSliderFlags_AlwaysClamp))
        markDirty();
    ui::endCard();

    ui::beginCard("Goal", Icon::Medal);
    static const char* const goals[] = {"Author", "Gold", "Silver", "Bronze", "Finished"};
    int goal = static_cast<int>(settings_.goal);
    if (ui::segmented("goal", &goal, goals, 5)) {
        settings_.goal = static_cast<Goal>(goal);
        markDirty();
    }
    ui::infoTip("The medal that completes a map (RandomizerTMF's auto-skip mode). A better medal counts too. "
                "Silver, Bronze and Finished count as 'finished' in the results.");
    ui::endCard();

    ui::beginCard("Skips", Icon::Skip);
    bool unlimited = settings_.freeSkips < 0;
    if (ui::toggle("Unlimited free skips", &unlimited)) {
        settings_.freeSkips = unlimited ? -1 : 1;
        markDirty();
    }
    if (!unlimited && ImGui::SliderInt("Free skips", &settings_.freeSkips, 0, 10, "%d", ImGuiSliderFlags_AlwaysClamp)) markDirty();
    ImGui::BeginDisabled(settings_.goal != Goal::Author);
    if (ui::toggle("Gold skip", &settings_.goldSkip)) markDirty();
    ImGui::EndDisabled();
    ui::infoTip("Goal Author only: skip a map you have gold on; it counts as a gold medal.");
    ui::endCard();

    ui::beginCard("Map choice", Icon::Filter);
    if (ui::toggle("Equal environment distribution", &settings_.equalEnvironments)) markDirty();
    ui::infoTip("Pick the environment first (evenly), then a map of it. Without it, environments come as often as TMX has maps of them.");
    if (ui::toggle("Exclude maps from past runs", &settings_.excludePlayed)) markDirty();
    ui::infoTip("Every map in your saved runs on the same TMX site.");
    if (ui::toggle("Avoid maps you skipped", &settings_.avoidSkipped)) markDirty();
    ui::infoTip("Maps you skipped in saved runs.");
    if (ui::toggle("Never a map you finished before", &settings_.excludeFinished)) markDirty();
    ui::infoTip("Reads the map of every replay in the game's Autosaves folder (Tracks > Replays > Autosaves) once and keeps a "
                "cache. A map with an autosave was finished before.");
    ui::endCard();

    ui::beginCard("Between maps", Icon::Flag);
    if (ui::toggle("Next map automatically after a goal", &settings_.autoAdvance)) markDirty();
    ImGui::BeginDisabled(!settings_.autoAdvance);
    if (ImGui::SliderInt("Delay", &settings_.advanceDelayS, 0, 15, "%d s", ImGuiSliderFlags_AlwaysClamp)) markDirty();
    ImGui::EndDisabled();
    ui::infoTip("On TMNF/TMUF the map is only left from the end-race screen (Competition Patch rule); "
                "a skip during a run there asks you to quit the map yourself.");
    bool canSave = host_.canSaveReplay();
    ImGui::BeginDisabled(!canSave);
    bool rs = settings_.replayAutosave && canSave;
    if (ui::toggle("Save the replay of every goal medal", &rs)) {
        settings_.replayAutosave = rs;
        markDirty();
    }
    ImGui::EndDisabled();
    ui::infoTip(canSave ? "Saved in the game's Replays folder; the run file lists them."
                        : "Not available yet: the Horizon host can't save replays for plugins (host 0.1.0). Replays saved by "
                          "ESWC Autosave are still listed in the run file.");
    ui::endCard();

    if (ImGui::CollapsingHeader("Advanced")) {
        ui::beginCard("Survival numbers", Icon::Sliders);
        if (ImGui::SliderInt("Bonus per author medal", &settings_.rmsAuthorBonusS, 0, 600, "%d s")) markDirty();
        if (ImGui::SliderInt("Bonus per gold / gold skip", &settings_.rmsGoldBonusS, 0, 600, "%d s")) markDirty();
        if (ImGui::SliderInt("Penalty per skip", &settings_.rmsSkipPenaltyS, 0, 600, "%d s")) markDirty();
        ui::endCard();
        ui::beginCard("Banned maps", Icon::Stop);
        char buf[2048];
        std::snprintf(buf, sizeof buf, "%s", settings_.banned.c_str());
        if (ImGui::InputTextMultiline("##banned", buf, sizeof buf, ImVec2(-1, 70 * sc))) {
            settings_.banned = buf;
            markDirty();
        }
        ImGui::TextColored(style_.muted, "%d TMX ids, separated by commas or spaces", static_cast<int>(parseIdList(settings_.banned).size()));
        ui::infoTip("These TMX ids are never picked (on any site).");
        ui::endCard();
    }
}

void Plugin::pageLayout() {
    const float sc = style_.scale;
    ui::beginCard("HUD", Icon::Layout);
    if (ui::toggle("Show the HUD during a run", &settings_.hudVisible)) markDirty();
    ui::label("Layout");
    static const char* const presets[] = {"Compact card", "Split"};
    int preset = static_cast<int>(settings_.hudPreset);
    if (ui::segmented("preset", &preset, presets, 2)) {
        settings_.hudPreset = static_cast<HudPreset>(preset);
        if (settings_.hudPreset == HudPreset::Split) panels_.applySplitPreset();
        else panels_.applyCompactPreset();
        markDirty();
    }
    ui::infoTip("Split: one panel per corner (timer, map, medals, controls, session). Compact card: everything in one card.");
    if (ui::toggle("Edit layout", &settings_.layoutEdit)) markDirty();
    ui::infoTip("While the menu is open, the panels get an outline and can be moved and resized. Places are saved as a share "
                "of the screen. Off: no title bars, fixed, and clicks go through to the game except on buttons.");
    ImGui::SameLine(0, 24 * sc);
    if (ImGui::Button("Reset layout")) {
        if (settings_.hudPreset == HudPreset::Split) panels_.applySplitPreset();
        else panels_.applyCompactPreset();
    }
    ui::endCard();

    ui::beginCard("Panels", Icon::Card);
    int n = 0;
    const PanelDef* defs = Panels::table(&n);
    if (ImGui::BeginTable("##lr_panels", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX)) {
        ImGui::TableSetupColumn("Panel", ImGuiTableColumnFlags_WidthStretch, 1.4f);
        ImGui::TableSetupColumn("Show", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn("Opacity", ImGuiTableColumnFlags_WidthStretch, 1.4f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        for (int i = 0; i < n; ++i) {
            const PanelDef& d = defs[i];
            if (!d.hud) continue;
            PanelState& w = panels_.state(d);
            ImGui::PushID(d.key);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            icon(d.icon, ui::col(style_.accent));
            ImGui::SameLine();
            ImGui::TextUnformatted(d.title);
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
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderInt("##op", &w.opacity, 20, 100, "%d%%")) panels_.markDirty();
            ImGui::TableNextColumn();
            if (w.detached) {
                if (ImGui::SmallButton("Back to the menu")) panels_.attach(d);
            } else if (ImGui::SmallButton("Pop out")) {
                panels_.detach(d);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ui::endCard();

    ui::beginCard("Thumbnail", Icon::Map);
    static const char* const tsrc[] = {"TMX screenshot", "Map file", "Off"};
    int ts = static_cast<int>(settings_.thumbSource);
    if (ui::segmented("thumb", &ts, tsrc, 3)) {
        settings_.thumbSource = static_cast<ThumbSource>(ts);
        markDirty();
    }
    ui::infoTip("TMX screenshot: one extra request per map, the map file's own thumbnail as fallback (TMNF/TMUF maps have one, "
                "ESWC maps don't). Map file: no extra request.");
    if (!host_.canCreateTextures()) ImGui::TextColored(style_.warn, "This host can't create textures: no thumbnails.");
    ui::endCard();

    ui::beginCard("Hotkeys", Icon::Controls);
    struct Hk { int h; const char* what; };
    const Hk hks[] = {{hkSkip_, "Skip map"}, {hkGoldSkip_, "Gold skip"}, {hkHud_, "Show / hide HUD"}, {hkNext_, "Next map after a goal"}};
    for (const Hk& k : hks) {
        std::string t = hotkey(k.h);
        float x0 = ImGui::GetCursorPosX();
        ImGui::TextUnformatted(k.what);
        ImVec4 c = t.empty() ? style_.muted : style_.accent;
        ImGui::SameLine(x0 + 200 * sc);
        ImGui::TextColored(c, "%s", t.empty() ? "(none)" : t.c_str());
    }
    ui::infoTip("Change keys in the Horizon menu under Hotkeys.");
    ui::endCard();
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
        ImGui::TextColored(style_.muted, "No saved runs yet. Every finished run is saved here.");
        return;
    }
    float listH = ImGui::GetContentRegionAvail().y * (histSel_ >= 0 ? 0.32f : 1.0f);
    ImGuiTableFlags f = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY |
                        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX;
    if (ImGui::BeginTable("##lr_runs", 7, f, ImVec2(0, listH))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Started", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Game", ImGuiTableColumnFlags_WidthStretch, 1.8f);
        ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthStretch, 2.4f);
        ImGui::TableSetupColumn("AT", ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupColumn("Gold", ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupColumn("Skips", ImGuiTableColumnFlags_WidthStretch, 0.5f);
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
            if (ImGui::Selectable(when.c_str(), histSel_ == i, ImGuiSelectableFlags_SpanAllColumns)) histSel_ = histSel_ == i ? -1 : i;
            ImGui::PopID();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(r.game.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s, %s", modeTitle(r.mode), goalTitle(Session::goalFromKey(r.goal.c_str())));
            int vals[] = {r.authors, r.golds, r.skips, static_cast<int>(r.maps.size())};
            for (int v : vals) {
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
    ui::beginCard("Legacy Randomizer", Icon::Info);
    ImGui::PushStyleColor(ImGuiCol_Text, style_.text2);
    ImGui::TextWrapped("Random maps from TrackMania Exchange inside the game: Random Map Challenge, Random Map Survival and "
                       "free play. The next map downloads while you drive the current one; every run is saved as a JSON file.");
    ImGui::Spacing();
    ImGui::TextWrapped("It never changes physics, timing or replays and does nothing in net play. On TMNF/TMUF it only acts in "
                       "the menus and on the end-race screen (Competition Patch 2.0).");
    ImGui::PopStyleColor();
    ImGui::TextColored(style_.muted, "Version " LR_PLUGIN_VERSION ", MIT licence. Map data and screenshots: TMX. Rules after RandomizerTMF.");
    ui::endCard();
    ui::beginCard("Files", Icon::List);
    ImGui::TextColored(style_.muted, "Data folder");
    ImGui::TextUnformatted(host_.dataFolder().c_str());
    ImGui::TextColored(style_.muted, "Saved runs: %d", static_cast<int>(history_.runs().size()));
    ui::endCard();
}

// ------------------------------------------------------------------ results / tables

void Plugin::drawMapsTable(const RunRecord& r, const char* id, float height) {
    ImGuiTableFlags f = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp |
                        ImGuiTableFlags_PadOuterX;
    if (height > 0) f |= ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable(id, 7, f, ImVec2(0, height))) return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Map", ImGuiTableColumnFlags_WidthStretch, 3.0f);
    ImGui::TableSetupColumn("Author", ImGuiTableColumnFlags_WidthStretch, 1.6f);
    ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthStretch, 1.6f);
    ImGui::TableSetupColumn("Best", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("On map", ImGuiTableColumnFlags_WidthStretch, 0.9f);
    ImGui::TableSetupColumn("TMX", ImGuiTableColumnFlags_WidthStretch, 0.9f);
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
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(m.author.c_str());
        ImGui::TableNextColumn();
        ui::medalDot(medalColor(medalFromKey(m.medal)));
        ImGui::TextUnformatted(resultTitle(m.result));
        auto right = [](const std::string& t, const ImVec4& c) {
            float rx = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(rx - ImGui::CalcTextSize(t.c_str()).x);
            ImGui::TextColored(c, "%s", t.c_str());
        };
        ImGui::TableNextColumn();
        right(m.bestMs > 0 ? formatMs(m.bestMs) : std::string("-"), m.bestMs > 0 ? style_.text : style_.muted);
        ImGui::TableNextColumn();
        right(formatClock(m.spentMs), style_.muted);
        ImGui::TableNextColumn();
        right(std::to_string(m.tmxId), style_.muted);
    }
    ImGui::EndTable();
}

void Plugin::drawResults(const RunRecord& r, bool live) {
    const float sc = style_.scale;
    ui::beginCard(live ? "Run finished" : "Run", Icon::Flag);
    if (r.timeLimitMs > 0 && r.mode == "rmc")
        ImGui::TextColored(style_.text2, "%s, %d min, goal %s", modeTitle(r.mode), r.timeLimitMs / 60000,
                           goalTitle(Session::goalFromKey(r.goal.c_str())));
    else
        ImGui::TextColored(style_.text2, "%s, goal %s", modeTitle(r.mode), goalTitle(Session::goalFromKey(r.goal.c_str())));
    ImGui::PushFont(titleFont());
    ui::medalDot(kAuthor);
    ImGui::Text("AT %d", r.authors);
    ImGui::SameLine(0, 22 * sc);
    ui::medalDot(kGold);
    ImGui::Text("Gold %d", r.golds);
    ImGui::SameLine(0, 22 * sc);
    ImGui::Text("Skips %d", r.skips);
    ImGui::SameLine(0, 22 * sc);
    ImGui::Text("Maps %d", static_cast<int>(r.maps.size()));
    ImGui::PopFont();
    const char* by = r.endedBy == "timer" ? "time ran out" : r.endedBy == "user" ? "ended by you" : r.endedBy.c_str();
    ImGui::TextColored(style_.muted, "Played %s, %s, %s, %s", formatClock(r.elapsedMs).c_str(), r.site.c_str(), by, r.started.c_str());
    if (live) {
        if (ImGui::Button("New run", ImVec2(140 * sc, 0))) run_.newRun();
        ImGui::SameLine();
        if (!run_.lastRunPath().empty()) {
            ImGui::TextColored(style_.muted, "Saved");
            ui::infoTip(run_.lastRunPath().c_str());
        } else if (!run_.error().empty()) {
            ImGui::TextColored(style_.bad, "%s", run_.error().c_str());
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
        float h = width * static_cast<float>(m->thumb.h) / static_cast<float>(m->thumb.w);
        if (h > width * 0.62f) h = width * 0.62f;
        ImGui::Image(m->tex, ImVec2(width, h));
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
    ImGui::TextColored(style_.muted, goal == Goal::Finished ? "Personal best (goal: finish)" : "Personal best");
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

void Plugin::drawControls(float width) {
    const Session* s = run_.session();
    float sp = ImGui::GetStyle().ItemSpacing.x;
    bool showGold = s && s->config().goal == Goal::Author && s->config().allowGoldSkip;
    float bw = showGold ? (width - sp) * 0.5f : width;
    if (!s || !run_.active()) {
        ImGui::BeginDisabled();
        ImGui::Button("Skip", ImVec2(bw, 0));
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
}

void Plugin::panelControls() {
    float w = ImGui::GetContentRegionAvail().x;
    drawControls(w);
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
        ImGui::TextColored(style_.muted, "The maps of the run appear here.");
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
        ImGui::TextColored(style_.muted, "All results: Horizon menu > Legacy Randomizer");
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
    panels_.saveIfDirty(host_);
}

} // namespace lr
