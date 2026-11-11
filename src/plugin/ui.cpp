// Menu page (start page, live run, results, history, settings, about) and the in-race HUD.
// Colours come from the host's style; the only own colours are the medal colours.
#include <cstdio>
#include <string>

#include "imgui.h"
#include "plugin.h"
#include "tmtext.h"
#include "util.h"

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
    return "Unfinished";
}

// small filled circle at the cursor, vertically centred on a text line
void medalDot(const ImVec4& c) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = ImGui::GetTextLineHeight();
    float r = h * 0.22f;
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(p.x + r + 1, p.y + h * 0.5f), r, ImGui::ColorConvertFloat4ToU32(c), 16);
    ImGui::Dummy(ImVec2(r * 2 + 2, h));
    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
}

struct EnvItem { int id; const char* name; };
const EnvItem kEnvTmuf[] = {{1, "Snow"}, {2, "Desert"}, {3, "Rally"}, {4, "Island"}, {5, "Coast"}, {6, "Bay"}, {7, "Stadium"}};
const EnvItem kEnvSunrise[] = {{4, "Island"}, {5, "Coast"}, {6, "Bay"}};
const EnvItem kEnvOriginal[] = {{1, "Snow"}, {2, "Desert"}, {3, "Rally"}};

const EnvItem* envList(Site s, int& n) {
    switch (s) {
    case Site::TMUF: n = 7; return kEnvTmuf;
    case Site::TMS: n = 3; return kEnvSunrise;
    case Site::TMO: n = 3; return kEnvOriginal;
    case Site::TMNF:
    case Site::TMN_ESWC: break;
    }
    n = 0;
    return nullptr;  // Stadium only
}

const char* const kDifficulty[] = {"Any", "Beginner", "Intermediate", "Expert", "Lunatic"};
// TMX tag ids, identical on all five sites (checked /api/meta/tags 2026-11-11)
const char* const kTags[] = {"Race", "Stunt", "Maze", "Offroad", "Multilap", "FullSpeed", "LOL", "Tech", "SpeedTech", "RPG",
                             "PressForward", "Trial", "Grass", "Story", "Nascar", "Speedfun", "Endurance", "Altered Nadeo",
                             "Transitional"};
const int kTagCount = static_cast<int>(sizeof kTags / sizeof kTags[0]);

const char* const kSiteChoices[] = {"Auto (from game)", "tmnf.exchange", "tmuf.exchange", "nations.tm-exchange.com",
                                    "sunrise.tm-exchange.com", "original.tm-exchange.com"};

std::string clockText(int seconds) {
    char b[32];
    std::snprintf(b, sizeof b, "%d:%02d", seconds / 60, seconds % 60);
    return b;
}

// slider in seconds shown as m:ss; 0 shows offText
bool sliderSeconds(const char* label, int* v, int maxS, const char* offText) {
    std::string fmt = *v == 0 ? std::string(offText) : clockText(*v);
    return ImGui::SliderInt(label, v, 0, maxS, fmt.c_str(), ImGuiSliderFlags_AlwaysClamp);
}

void textRight(const char* text, float rightX, const ImVec4* col = nullptr) {
    float w = ImGui::CalcTextSize(text).x;
    ImGui::SameLine(rightX - w);
    if (col) ImGui::TextColored(*col, "%s", text);
    else ImGui::TextUnformatted(text);
}

} // namespace

// ------------------------------------------------------------------ helpers

ImFont* Plugin::titleFont() { return style_.title ? style_.title : ImGui::GetFont(); }

void Plugin::section(const char* title) {
    ImGui::Spacing();
    ImGui::SeparatorText(title);
}

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

// ------------------------------------------------------------------ menu

void Plugin::drawMenu() {
    style_ = host_.style();
    drawHeader();
    if (ImGui::BeginTabBar("##lr_tabs")) {
        const char* names[] = {"Run", "History", "Settings", "About"};
        for (int i = 0; i < 4; ++i) {
            ImGuiTabItemFlags f = forceTab_ == i ? ImGuiTabItemFlags_SetSelected : 0;
            if (!ImGui::BeginTabItem(names[i], nullptr, f)) continue;
            ImGui::BeginChild("##lr_page", ImVec2(0, 0), ImGuiChildFlags_None);
            switch (i) {
            case 0:
                if (run_.active()) drawLivePage();
                else if (run_.phase() == Phase::Over) drawResults(run_.lastRun(), true);
                else drawStartPage();
                break;
            case 1: drawHistoryPage(); break;
            case 2: drawSettingsPage(); break;
            default: drawAboutPage(); break;
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    forceTab_ = -1;
    if (dirty_ && !ImGui::IsAnyItemActive()) {
        settings_.save(host_);
        dirty_ = false;
    }
}

void Plugin::drawHeader() {
    ImGui::PushFont(titleFont());
    ImGui::TextColored(style_.accent, "LEGACY RANDOMIZER");
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextColored(style_.muted, "v" LR_PLUGIN_VERSION);
    HostGame g = host_.game();
    medalDot(g == HostGame::Unknown ? style_.bad : style_.ok);
    ImGui::TextUnformatted(host_.gameName().c_str());
    ImGui::SameLine();
    ImGui::TextColored(style_.muted, "maps from %s", siteHost(siteFor(settings_.site, g)));
    if (g == HostGame::Original) {
        ImGui::SameLine();
        ImGui::TextColored(style_.warn, "experimental");
    }
    if (host_.isOnline() || !host_.actionsAllowed())
        ImGui::TextColored(style_.warn, "Net play: the randomizer is paused and does nothing here.");
}

void Plugin::drawStartPage() {
    const float sc = style_.scale;
    ImGui::PushItemWidth(320 * sc);

    section("Mode");
    int mode = static_cast<int>(settings_.mode);
    if (ImGui::RadioButton("Random Map Challenge", mode == 0)) mode = 0;
    ImGui::SameLine();
    if (ImGui::RadioButton("Random Map Survival", mode == 1)) mode = 1;
    ImGui::SameLine();
    if (ImGui::RadioButton("Free play", mode == 2)) mode = 2;
    if (mode != static_cast<int>(settings_.mode)) {
        settings_.mode = static_cast<Mode>(mode);
        markDirty();
    }
    switch (settings_.mode) {
    case Mode::RMC:
        if (ImGui::SliderInt("Time limit", &settings_.rmcMinutes, 5, 180, "%d min", ImGuiSliderFlags_AlwaysClamp)) markDirty();
        ImGui::PushStyleColor(ImGuiCol_Text, style_.muted);
        ImGui::TextWrapped("Hit as many goal medals as you can before the timer runs out. A skip costs no extra time, "
                           "but the time spent on the map is gone; free skips (for broken maps) give it back.");
        ImGui::PopStyleColor();
        break;
    case Mode::RMS:
        if (ImGui::SliderInt("Start time", &settings_.rmsStartMinutes, 1, 60, "%d min", ImGuiSliderFlags_AlwaysClamp)) markDirty();
        if (sliderSeconds("Bonus per author medal", &settings_.rmsAuthorBonusS, 600, "none")) markDirty();
        if (sliderSeconds("Bonus per gold / gold skip", &settings_.rmsGoldBonusS, 600, "none")) markDirty();
        if (sliderSeconds("Penalty per skip", &settings_.rmsSkipPenaltyS, 600, "none")) markDirty();
        ImGui::PushStyleColor(ImGuiCol_Text, style_.muted);
        ImGui::TextWrapped("Survive as long as you can: medals add time, skips take time away. The run ends at 0.");
        ImGui::PopStyleColor();
        break;
    case Mode::FreePlay:
        ImGui::PushStyleColor(ImGuiCol_Text, style_.muted);
        ImGui::TextWrapped("No timer: random maps for as long as you like. Medals and skips are still counted.");
        ImGui::PopStyleColor();
        break;
    }

    section("Goal medal");
    int goal = static_cast<int>(settings_.goal);
    if (ImGui::RadioButton("Author", goal == 0)) goal = 0;
    ImGui::SameLine();
    if (ImGui::RadioButton("Gold", goal == 1)) goal = 1;
    if (goal != static_cast<int>(settings_.goal)) {
        settings_.goal = static_cast<Goal>(goal);
        markDirty();
    }

    section("Skips");
    bool unlimited = settings_.freeSkips < 0;
    if (ImGui::Checkbox("Unlimited free skips", &unlimited)) {
        settings_.freeSkips = unlimited ? -1 : 1;
        markDirty();
    }
    if (!unlimited && ImGui::SliderInt("Free skips", &settings_.freeSkips, 0, 10, "%d", ImGuiSliderFlags_AlwaysClamp)) markDirty();
    ImGui::BeginDisabled(settings_.goal != Goal::Author);
    if (ImGui::Checkbox("Gold skip", &settings_.goldSkip)) markDirty();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextColored(style_.muted, "skip a map you have gold on; it counts as a gold medal");

    section("Map filters");
    Site site = siteFor(settings_.site, host_.game());
    int sc0 = static_cast<int>(settings_.site);
    if (ImGui::Combo("TMX site", &sc0, kSiteChoices, 6)) {
        settings_.site = static_cast<SiteChoice>(sc0);
        settings_.environment = 0;
        markDirty();
    }
    if (sliderSeconds("Author time from", &settings_.atMinS, 600, "any")) markDirty();
    if (sliderSeconds("Author time up to", &settings_.atMaxS, 600, "any")) markDirty();
    int nEnv = 0;
    const EnvItem* envs = envList(site, nEnv);
    if (envs) {
        const char* cur = "Any";
        for (int i = 0; i < nEnv; ++i)
            if (envs[i].id == settings_.environment) cur = envs[i].name;
        if (ImGui::BeginCombo("Environment", cur)) {
            if (ImGui::Selectable("Any", settings_.environment == 0)) { settings_.environment = 0; markDirty(); }
            for (int i = 0; i < nEnv; ++i)
                if (ImGui::Selectable(envs[i].name, settings_.environment == envs[i].id)) {
                    settings_.environment = envs[i].id;
                    markDirty();
                }
            ImGui::EndCombo();
        }
    } else {
        ImGui::BeginDisabled();
        int zero = 0;
        const char* st[] = {"Stadium"};
        ImGui::Combo("Environment", &zero, st, 1);
        ImGui::EndDisabled();
    }
    int diff = settings_.difficulty + 1;
    if (ImGui::Combo("Difficulty", &diff, kDifficulty, 5)) {
        settings_.difficulty = diff - 1;
        markDirty();
    }
    const char* tagName = settings_.tag >= 0 && settings_.tag < kTagCount ? kTags[settings_.tag] : "Any";
    if (ImGui::BeginCombo("Style", tagName)) {
        if (ImGui::Selectable("Any", settings_.tag < 0)) { settings_.tag = -1; markDirty(); }
        for (int i = 0; i < kTagCount; ++i)
            if (ImGui::Selectable(kTags[i], settings_.tag == i)) { settings_.tag = i; markDirty(); }
        ImGui::EndCombo();
    }
    if (ImGui::Checkbox("Exclude Unlimiter maps", &settings_.excludeUnlimiter)) markDirty();
    bool needAt = settings_.goal == Goal::Author;
    bool reqAt = settings_.requireAuthorTime || needAt;
    ImGui::BeginDisabled(needAt);
    if (ImGui::Checkbox("Only maps with an author time", &reqAt)) {
        settings_.requireAuthorTime = reqAt;
        markDirty();
    }
    ImGui::EndDisabled();
    if (needAt) {
        ImGui::SameLine();
        ImGui::TextColored(style_.muted, "always on with goal Author");
    }
    if (ImGui::Checkbox("Exclude maps from past runs", &settings_.excludePlayed)) markDirty();
    ImGui::SameLine();
    ImGui::TextColored(style_.muted, "%d maps on %s", static_cast<int>(history_.playedIds(siteHost(site)).size()), siteHost(site));
    ImGui::PopItemWidth();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    bool blocked = host_.isOnline() || !host_.actionsAllowed();
    ImGui::BeginDisabled(blocked);
    static std::string startError;
    if (ImGui::Button("Start run", ImVec2(180 * sc, 0))) {
        if (dirty_) { settings_.save(host_); dirty_ = false; }
        startError.clear();
        if (!run_.start(&startError)) host_.log(LogWarn, "start: " + startError);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (blocked) ImGui::TextColored(style_.warn, "Not available in net play.");
    else if (!startError.empty()) ImGui::TextColored(style_.bad, "%s", startError.c_str());
    else ImGui::TextColored(style_.muted, "Changes are saved automatically. The first map starts from the main menu.");
}

void Plugin::drawLivePage() {
    const Session* s = run_.session();
    if (!s) return;
    const float sc = style_.scale;
    int ms = s->countsDown() ? s->remainingMs() : s->elapsedMs();
    ImGui::PushFont(titleFont());
    ImGui::SetWindowFontScale(1.6f);
    ImGui::TextColored(s->countsDown() && ms < 60000 ? style_.bad : style_.accent, "%s", formatClock(ms).c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextColored(style_.muted, "%s, goal %s, %s", modeShort(s->config().mode),
                       s->config().goal == Goal::Author ? "Author" : "Gold", siteHost(run_.site()));
    medalDot(kAuthor);
    ImGui::Text("AT %d", s->authorCount());
    ImGui::SameLine(0, 16 * sc);
    medalDot(kGold);
    ImGui::Text("Gold %d", s->goldCount());
    ImGui::SameLine(0, 16 * sc);
    int left = s->freeSkipsLeft();
    if (left < 0) ImGui::Text("Skips %d (free)", s->skipCount());
    else ImGui::Text("Skips %d, %d free left", s->skipCount(), left);
    ImGui::SameLine(0, 16 * sc);
    ImGui::TextColored(style_.muted, "%s", phaseName(run_.phase()));

    section("Current map");
    std::shared_ptr<PreparedMap> m = run_.displayMap();
    if (m) {
        ImGui::BeginGroup();
        drawHudMapCard(m.get(), 360 * sc, false);
        ImGui::EndGroup();
    } else {
        ImGui::TextColored(run_.fetchFailing() ? style_.bad : style_.muted, "%s", run_.fetchStatus().c_str());
    }
    if (!run_.notice().empty()) ImGui::TextColored(style_.warn, "%s", run_.notice().c_str());
    if (!run_.error().empty()) ImGui::TextColored(style_.bad, "%s", run_.error().c_str());
    if (m) ImGui::TextColored(run_.fetchFailing() ? style_.bad : style_.muted, "Next map: %s", run_.fetchStatus().c_str());

    ImGui::Spacing();
    ImGui::BeginDisabled(!run_.canSkip());
    if (ImGui::Button(s->freeSkipsLeft() != 0 ? "Free skip" : "Skip")) run_.skip();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!run_.canGoldSkip());
    if (ImGui::Button("Gold skip")) run_.goldSkip();
    ImGui::EndDisabled();
    if (run_.phase() == Phase::Leaving) {
        ImGui::SameLine();
        if (ImGui::Button("Next map")) run_.nextNow();
    }
    if (run_.leftMap()) {
        ImGui::SameLine();
        if (ImGui::Button("Resume map")) run_.resume();
    }
    if (run_.phase() == Phase::Loading && run_.pendingLoadAgeMs() > 15000) {
        ImGui::SameLine();
        if (ImGui::Button("Take another map")) run_.dropPendingMap();
    }
    ImGui::SameLine(0, 24 * sc);
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

    RunRecord snap = run_.snapshot();
    if (!snap.maps.empty()) {
        section("Maps so far");
        drawMapsTable(snap, "##lr_live_maps", 0);
    }
}

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
    ImGui::TableSetupColumn("On map", ImGuiTableColumnFlags_WidthStretch, 1.0f);
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
        Medal med = medalFromKey(m.medal);
        bool skipped = m.result == "skipped" || m.result == "free_skip" || m.result == "unfinished";
        medalDot(skipped && med == Medal::None ? kNoMedal : medalColor(med));
        ImGui::TextUnformatted(resultTitle(m.result));
        ImGui::TableNextColumn();
        if (m.bestMs > 0) ImGui::TextUnformatted(formatMs(m.bestMs).c_str());
        else ImGui::TextColored(style_.muted, "-");
        ImGui::TableNextColumn();
        ImGui::TextColored(style_.muted, "%s", formatClock(m.spentMs).c_str());
        ImGui::TableNextColumn();
        ImGui::TextColored(style_.muted, "%lld", m.tmxId);
    }
    ImGui::EndTable();
}

void Plugin::drawResults(const RunRecord& r, bool live) {
    const float sc = style_.scale;
    ImGui::PushFont(titleFont());
    ImGui::TextColored(style_.accent, live ? "RUN FINISHED" : "RUN");
    ImGui::PopFont();
    ImGui::SameLine();
    if (r.timeLimitMs > 0 && r.mode == "rmc")
        ImGui::TextColored(style_.muted, "%s, %d min", modeTitle(r.mode), r.timeLimitMs / 60000);
    else
        ImGui::TextColored(style_.muted, "%s", modeTitle(r.mode));

    section("Totals");
    medalDot(kAuthor);
    ImGui::Text("AT %d", r.authors);
    ImGui::SameLine(0, 18 * sc);
    medalDot(kGold);
    ImGui::Text("Gold %d", r.golds);
    ImGui::SameLine(0, 18 * sc);
    ImGui::Text("Skips %d", r.skips);
    ImGui::SameLine(0, 18 * sc);
    ImGui::Text("Maps played %d", static_cast<int>(r.maps.size()));
    const char* by = r.endedBy == "timer" ? "time ran out" : r.endedBy == "user" ? "ended by you" : r.endedBy.c_str();
    ImGui::TextColored(style_.muted, "Played %s, goal %s, %s, %s, %s", formatClock(r.elapsedMs).c_str(),
                       r.goal == "gold" ? "Gold" : "Author", r.site.c_str(), by, r.started.c_str());

    section("Maps");
    float footer = live ? ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y * 2 : 0;
    float h = ImGui::GetContentRegionAvail().y - footer;
    drawMapsTable(r, live ? "##lr_results" : "##lr_hist_maps", h > 120 * sc ? h : 0);
    if (live) {
        ImGui::Spacing();
        if (ImGui::Button("New run", ImVec2(140 * sc, 0))) run_.newRun();
        ImGui::SameLine();
        if (!run_.lastRunPath().empty()) ImGui::TextColored(style_.muted, "Saved to %s", run_.lastRunPath().c_str());
        else if (!run_.error().empty()) ImGui::TextColored(style_.bad, "%s", run_.error().c_str());
    }
}

void Plugin::drawHistoryPage() {
    const auto& runs = history_.runs();
    if (ImGui::Button("Reload")) {
        history_.reload();
        histSel_ = -1;
    }
    ImGui::SameLine();
    ImGui::TextColored(style_.muted, "%d runs in %s", static_cast<int>(runs.size()), history_.runsFolder().c_str());
    if (runs.empty()) {
        ImGui::TextColored(style_.muted, "No saved runs yet. Every finished run is saved here.");
        return;
    }
    float listH = ImGui::GetContentRegionAvail().y * (histSel_ >= 0 ? 0.35f : 1.0f);
    ImGuiTableFlags f = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY |
                        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX;
    if (ImGui::BeginTable("##lr_runs", 7, f, ImVec2(0, listH))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Started", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Game", ImGuiTableColumnFlags_WidthStretch, 1.6f);
        ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthStretch, 2.2f);
        ImGui::TableSetupColumn("AT", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupColumn("Gold", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupColumn("Skips", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupColumn("Maps", ImGuiTableColumnFlags_WidthStretch, 0.6f);
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
            ImGui::Text("%s, %s", modeTitle(r.mode), r.goal == "gold" ? "Gold" : "Author");
            ImGui::TableNextColumn();
            ImGui::Text("%d", r.authors);
            ImGui::TableNextColumn();
            ImGui::Text("%d", r.golds);
            ImGui::TableNextColumn();
            ImGui::Text("%d", r.skips);
            ImGui::TableNextColumn();
            ImGui::Text("%d", static_cast<int>(r.maps.size()));
        }
        ImGui::EndTable();
    }
    if (histSel_ >= 0 && histSel_ < static_cast<int>(runs.size())) {
        ImGui::Spacing();
        drawResults(runs[histSel_], false);
    }
}

void Plugin::drawSettingsPage() {
    const float sc = style_.scale;
    ImGui::PushItemWidth(260 * sc);
    section("Between maps");
    if (ImGui::Checkbox("Go to the next map automatically after a goal medal", &settings_.autoAdvance)) markDirty();
    ImGui::BeginDisabled(!settings_.autoAdvance);
    if (ImGui::SliderInt("Delay", &settings_.advanceDelayS, 0, 15, "%d s", ImGuiSliderFlags_AlwaysClamp)) markDirty();
    ImGui::EndDisabled();
    ImGui::PushStyleColor(ImGuiCol_Text, style_.muted);
    ImGui::TextWrapped("On TMNF/TMUF the randomizer only leaves a map from the end-race screen (Competition Patch rule). "
                       "A skip during a run there asks you to quit the map yourself.");
    ImGui::PopStyleColor();

    section("Replays");
    bool canSave = host_.canSaveReplay();
    ImGui::BeginDisabled(!canSave);
    if (ImGui::Checkbox("Save the replay of every goal medal", &settings_.replayAutosave)) markDirty();
    ImGui::EndDisabled();
    ImGui::PushStyleColor(ImGuiCol_Text, style_.muted);
    if (canSave)
        ImGui::TextWrapped("Saved to Replays\\LegacyRandomizer\\ in the game's user folder; the run file lists them.");
    else
        ImGui::TextWrapped("This Horizon host can't save replays for plugins yet. Replays saved by another plugin "
                           "(e.g. ESWC Autosave) are still listed in the run file.");
    ImGui::PopStyleColor();

    section("HUD");
    if (ImGui::Checkbox("Show the HUD during a run", &settings_.hudVisible)) markDirty();
    if (ImGui::Checkbox("Compact HUD", &settings_.hudCompact)) markDirty();
    int corner = static_cast<int>(settings_.hudCorner);
    const char* corners[] = {"Top left", "Top right"};
    if (ImGui::Combo("Position", &corner, corners, 2)) {
        settings_.hudCorner = static_cast<HudCorner>(corner);
        markDirty();
    }
    int ts = static_cast<int>(settings_.thumbSource);
    const char* tsrc[] = {"TMX screenshot (map file as fallback)", "Map file thumbnail only (no extra request)", "Off"};
    if (ImGui::Combo("Thumbnail", &ts, tsrc, 3)) {
        settings_.thumbSource = static_cast<ThumbSource>(ts);
        markDirty();
    }
    if (!host_.canCreateTextures())
        ImGui::TextColored(style_.warn, "This Horizon host can't create textures for plugins yet: no thumbnails.");

    section("Hotkeys");
    struct Hk { int h; const char* what; };
    const Hk hks[] = {{hkSkip_, "Skip map"}, {hkGoldSkip_, "Gold skip"}, {hkHud_, "Show / hide HUD"}, {hkNext_, "Next map after a goal"}};
    if (ImGui::BeginTable("##lr_hk", 2, ImGuiTableFlags_SizingFixedFit)) {
        for (const Hk& k : hks) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            std::string t = hotkey(k.h);
            if (t.empty()) ImGui::TextColored(style_.muted, "(none)");
            else ImGui::TextColored(style_.accent, "%s", t.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(k.what);
        }
        ImGui::EndTable();
    }
    ImGui::TextColored(style_.muted, "Change keys in the Horizon menu under Hotkeys.");

    section("Storage");
    ImGui::TextColored(style_.muted, "Data folder: %s", host_.dataFolder().c_str());
    ImGui::TextColored(style_.muted, "Saved runs: %d", static_cast<int>(history_.runs().size()));
    ImGui::PopItemWidth();
    ImGui::Spacing();
    if (ImGui::Button("Reset to defaults")) ImGui::OpenPopup("Reset settings?");
    if (ImGui::BeginPopupModal("Reset settings?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Reset every randomizer setting to its default?");
        if (ImGui::Button("Reset", ImVec2(110 * sc, 0))) {
            settings_.resetToDefaults();
            markDirty();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(110 * sc, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void Plugin::drawAboutPage() {
    ImGui::PushStyleColor(ImGuiCol_Text, style_.text2);
    ImGui::TextWrapped("Legacy Randomizer plays random maps from TrackMania Exchange inside the game: Random Map "
                       "Challenge (timer, as many goal medals as possible), Random Map Survival (medals add time, skips "
                       "cost time) and free play.");
    ImGui::Spacing();
    ImGui::TextWrapped("Maps come from the TMX site of the game you play (tmnf/tmuf.exchange, nations, sunrise and "
                       "original.tm-exchange.com). The next map is downloaded while you drive the current one. Each run "
                       "is saved as a JSON file in the plugin's data folder.");
    ImGui::Spacing();
    ImGui::TextWrapped("The randomizer never changes game physics, timing or replays and does nothing in net play. "
                       "On TMNF/TMUF it only acts in the menus and on the end-race screen (Competition Patch 2.0).");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::TextColored(style_.muted, "Version " LR_PLUGIN_VERSION ", MIT licence. Map data and screenshots: TMX.");
}

// ------------------------------------------------------------------ HUD

void Plugin::drawHudMapCard(PreparedMap* m, float width, bool compact) {
    ensureTexture(m);
    if (!compact && m->tex && m->thumb.w > 0) {
        float h = width * static_cast<float>(m->thumb.h) / static_cast<float>(m->thumb.w);
        if (h > width * 0.62f) h = width * 0.62f;
        ImGui::Image(m->tex, ImVec2(width, h));
    }
    TmTextStyle ts;
    ts.font = style_.body;
    ts.maxWidth = width;
    ts.defaultColor = ImGui::ColorConvertFloat4ToU32(style_.text);
    tmText(m->name, ts);
    if (m->tmx.id > 0) ImGui::TextColored(style_.muted, "by %s, TMX %lld", tmPlain(m->author).c_str(), m->tmx.id);
    else ImGui::TextColored(style_.muted, "by %s", tmPlain(m->author).c_str());

    const Session* s = run_.session();
    Goal goal = s ? s->config().goal : Goal::Author;
    bool isCur = run_.displayMap().get() == m && run_.phase() != Phase::Loading && run_.phase() != Phase::Preparing;
    Medal best = isCur ? run_.bestMedal() : Medal::None;
    int pb = isCur ? run_.bestMs() : 0;
    Medal goalMedal = goal == Goal::Author ? Medal::Author : Medal::Gold;

    if (compact) {
        medalDot(medalColor(goalMedal));
        ImGui::Text("%s %s", medalShort(goalMedal), formatMs(medalTime(m->medals, goalMedal)).c_str());
        ImGui::SameLine(0, 12 * style_.scale);
        if (pb > 0) {
            medalDot(medalColor(best));
            ImGui::Text("PB %s", formatMs(pb).c_str());
        } else {
            ImGui::TextColored(style_.muted, "no finish yet");
        }
        return;
    }

    ImGui::Spacing();
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
            dl->AddRectFilled(ImVec2(p.x - 4, p.y - 1), ImVec2(p.x + width + 4, p.y + lineH - 1),
                              ImGui::ColorConvertFloat4ToU32(bg), 4.0f * style_.scale);
        }
        medalDot(medalColor(md));
        if (reached) ImGui::TextColored(style_.text, "%s", medalShort(md));
        else ImGui::TextColored(style_.text2, "%s", medalShort(md));
        if (md == goalMedal) {
            ImGui::SameLine();
            ImGui::TextColored(style_.accent, "goal");
        }
        std::string tt = t > 0 ? formatMs(t) : std::string("unknown");
        ImVec4 tc = reached ? medalColor(md) : style_.text2;
        textRight(tt.c_str(), x0 + width, &tc);
    }
    float x0 = ImGui::GetCursorPosX();
    ImGui::TextColored(style_.muted, "Personal best");
    if (pb > 0) {
        ImVec4 c = best != Medal::None ? medalColor(best) : style_.text;
        textRight(formatMs(pb).c_str(), x0 + width, &c);
    } else {
        ImVec4 c = style_.muted;
        textRight("no finish yet", x0 + width, &c);
    }
}

void Plugin::drawHud() {
    const Session* s = run_.session();
    if (!s) return;
    const float sc = style_.scale;
    const bool compact = settings_.hudCompact;
    const float W = (compact ? 290.0f : 340.0f) * sc;
    const ImGuiStyle& st = ImGui::GetStyle();
    const float inner = W - st.WindowPadding.x * 2;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float margin = 16 * sc;
    ImVec2 pos(vp->WorkPos.x + margin, vp->WorkPos.y + margin);
    if (settings_.hudCorner == HudCorner::TopRight) pos.x = vp->WorkPos.x + vp->WorkSize.x - margin - W;
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(W, 0), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.86f);
    const ImGuiWindowFlags hudFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                                      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;
    ImVec2 btnPos;
    if (ImGui::Begin("##lr_hud", nullptr, hudFlags)) {
        // timer + mode
        int ms = s->countsDown() ? s->remainingMs() : s->elapsedMs();
        float x0 = ImGui::GetCursorPosX();
        ImGui::PushFont(titleFont());
        ImGui::SetWindowFontScale(compact ? 1.25f : 1.6f);
        ImGui::TextColored(s->countsDown() && ms < 60000 ? style_.bad : style_.accent, "%s", formatClock(ms).c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopFont();
        char modeText[64];
        std::snprintf(modeText, sizeof modeText, "%s, %s", modeShort(s->config().mode),
                      s->config().goal == Goal::Author ? "Author" : "Gold");
        ImVec4 mc = style_.muted;
        textRight(modeText, x0 + inner, &mc);

        std::shared_ptr<PreparedMap> m = run_.displayMap();
        Phase ph = run_.phase();
        if (ph == Phase::Preparing && !m) {
            ImGui::TextColored(style_.text2, "Getting the first map");
            ImGui::PushTextWrapPos(x0 + inner);
            ImGui::TextColored(run_.fetchFailing() ? style_.bad : style_.muted, "%s", run_.fetchStatus().c_str());
            ImGui::PopTextWrapPos();
        } else if (m) {
            drawHudMapCard(m.get(), inner, compact);
        }

        if (run_.goalJustReached()) {
            Medal md = run_.bestMedal();
            ImGui::PushFont(titleFont());
            ImGui::TextColored(medalColor(md), md == Medal::Author ? "AUTHOR MEDAL!" : "GOLD MEDAL!");
            ImGui::PopFont();
        }

        ImGui::Separator();
        medalDot(kAuthor);
        ImGui::Text("AT %d", s->authorCount());
        ImGui::SameLine(0, 12 * sc);
        medalDot(kGold);
        ImGui::Text("Gold %d", s->goldCount());
        ImGui::SameLine(0, 12 * sc);
        int left = s->freeSkipsLeft();
        if (left < 0) ImGui::Text("Skips %d", s->skipCount());
        else ImGui::Text("Skips %d (%d free)", s->skipCount(), left);

        ImGui::PushTextWrapPos(x0 + inner);
        if (!run_.notice().empty()) ImGui::TextColored(style_.warn, "%s", run_.notice().c_str());
        if (!run_.error().empty()) ImGui::TextColored(style_.bad, "%s", run_.error().c_str());
        if (ph == Phase::Loading) ImGui::TextColored(style_.muted, "Loading the map");
        else if (ph != Phase::Preparing && (!compact || run_.fetchFailing() || ph == Phase::WaitingMap))
            ImGui::TextColored(run_.fetchFailing() ? style_.bad : style_.muted, "Next map: %s", run_.fetchStatus().c_str());
        ImGui::PopTextWrapPos();

        btnPos = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(inner, ImGui::GetFrameHeight()));
        if (!compact) {
            std::string hs = hotkey(hkSkip_), hg = hotkey(hkGoldSkip_), hh = hotkey(hkHud_);
            std::string hint;
            if (!hs.empty()) hint += hs + " skip   ";
            if (!hg.empty()) hint += hg + " gold skip   ";
            if (!hh.empty()) hint += hh + " hide";
            if (!hint.empty()) ImGui::TextColored(style_.muted, "%s", hint.c_str());
        }
    }
    ImGui::End();

    // Buttons live in their own small window: the HUD itself lets every click through to the game.
    ImGui::SetNextWindowPos(btnPos, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    const ImGuiWindowFlags btnFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                      ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin("##lr_hud_buttons", nullptr, btnFlags)) {
        float sp = st.ItemSpacing.x;
        float toggleW = ImGui::GetFrameHeight() * 1.4f;
        bool showGold = s->config().goal == Goal::Author && s->config().allowGoldSkip;
        float bw = showGold ? (inner - toggleW - sp * 2) * 0.5f : inner - toggleW - sp;
        if (run_.waitingForUser() || (run_.phase() == Phase::Leaving && !run_.notice().empty())) {
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
        ImGui::SameLine(0, sp);
        if (ImGui::Button(compact ? "+##lr_hud_toggle" : "-##lr_hud_toggle", ImVec2(toggleW, 0))) {
            settings_.hudCompact = !settings_.hudCompact;
            markDirty();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(compact ? "Expand the HUD" : "Compact HUD");
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void Plugin::drawResultsToast() {
    const RunRecord& r = run_.lastRun();
    const float sc = style_.scale;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + 24 * sc), ImGuiCond_Always,
                            ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.92f);
    const ImGuiWindowFlags f = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                               ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("##lr_toast", nullptr, f)) {
        ImGui::PushFont(titleFont());
        ImGui::TextColored(style_.accent, "RUN FINISHED");
        ImGui::PopFont();
        medalDot(kAuthor);
        ImGui::Text("AT %d", r.authors);
        ImGui::SameLine(0, 14 * sc);
        medalDot(kGold);
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
    Phase before = run_.phase();
    run_.update();
    if (run_.phase() == Phase::Over && before != Phase::Over) toastDismissed_ = false;
    if (run_.phase() != Phase::Over) toastDismissed_ = true;
    if (dirty_ && !ImGui::IsAnyItemActive()) {
        settings_.save(host_);
        dirty_ = false;
    }
    if (run_.phase() == Phase::Over && !toastDismissed_ && !host_.menuOpen()) drawResultsToast();
    if (!run_.active() || !settings_.hudVisible) return;
    drawHud();
}

} // namespace lr
