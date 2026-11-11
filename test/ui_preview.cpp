// ui_preview.exe: the randomizer's UI and run flow without the game. A fake Horizon host (settings in a
// text file, fake game events, game control that only logs and simulates) drives the real plugin code
// (src/plugin, everything except host_horizon.cpp). Maps come live from TMX.
//
//   ui_preview.exe                          interactive window: F8 menu, "Fake game" panel, Ctrl+F9/F10/F11
//   ui_preview.exe --shots <dir>            renders the pages to PNG (1920x1080) and exits (few live TMX requests)
//   options: --game eswc|sunrise|original|tmnf|tmuf (default eswc), --data <dir> (plugin data folder)
//
// The ImGui style here is TM Vibrant Shaders' (src/overlay.cpp), standing in for the Horizon host's style.
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>
#include <d3d9.h>
#include <wincodec.h>

#include "imgui.h"
#include "backends/imgui_impl_dx9.h"
#include "backends/imgui_impl_win32.h"

#include "../src/core/gbx.h"
#include "../src/plugin/plugin.h"
#include "../src/plugin/util.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using namespace lr;

namespace {

// ------------------------------------------------------------------ fake host

class FakeHost final : public Host {
public:
    HostGame game_ = HostGame::Eswc;
    GameState state_ = GameState::MainMenu;
    bool online_ = false;
    bool menuOpen_ = true;
    bool replays_ = true;
    std::string data_;
    Plugin* plugin_ = nullptr;
    IDirect3DDevice9* dev_ = nullptr;
    UiStyle style_;
    double timeOffset_ = 0;
    GbxMapHeader cur_;
    int pb_ = 0;
    std::vector<std::string> lines_;

    FakeHost(const std::string& data, HostGame g) : game_(g), data_(data) {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        freq_ = static_cast<double>(f.QuadPart);
        ensureDir(data_);
        std::string text;
        if (readFile(data_ + "fake_host_settings.txt", text)) {
            size_t p = 0;
            while (p < text.size()) {
                size_t e = text.find('\n', p);
                if (e == std::string::npos) e = text.size();
                std::string line = text.substr(p, e - p);
                size_t eq = line.find('=');
                if (eq != std::string::npos) settings_[line.substr(0, eq)] = line.substr(eq + 1);
                p = e + 1;
            }
        }
    }

    // --- fake game ---
    void schedule(double inMs, std::function<void()> fn) { sched_.push_back({nowMs() + inMs, std::move(fn)}); }
    void pumpGame() {
        double now = nowMs();
        for (size_t i = 0; i < sched_.size();) {
            if (sched_[i].at <= now) {
                std::function<void()> fn = std::move(sched_[i].fn);
                sched_.erase(sched_.begin() + static_cast<long>(i));
                fn();
                i = 0;
                now = nowMs();
            } else {
                ++i;
            }
        }
        std::vector<std::function<void()>> q;
        {
            std::lock_guard<std::mutex> l(qMu_);
            q.swap(queue_);
        }
        for (auto& fn : q) fn();
    }
    GameEvent ev(EventType t) {
        GameEvent e;
        e.type = t;
        e.online = online_;
        e.state = state_;
        if (cur_.ok) {
            e.hasMap = true;
            e.mapPath = curPath_;
            e.mapUid = cur_.uid;
            e.mapName = stripTmCodes(cur_.name);
            e.mapAuthor = cur_.author;
            e.authorMs = cur_.authorMs;
            e.goldMs = cur_.goldMs;
            e.silverMs = cur_.silverMs;
            e.bronzeMs = cur_.bronzeMs;
        }
        return e;
    }
    void emit(const GameEvent& e) {
        if (plugin_) plugin_->onEvent(e);
    }
    void finish(int ms) {
        if (state_ != GameState::SoloRace) return;
        GameEvent e = ev(EventType::Finish);
        e.timeMs = ms;
        e.isPb = pb_ == 0 || ms < pb_;
        if (e.isPb) pb_ = ms;
        log(LogInfo, "[game] finish " + formatMs(ms));
        emit(e);
        state_ = GameState::EndRace;
        GameEvent re = ev(EventType::RaceEnd);
        re.endScreen = true;
        emit(re);
    }
    void restart() {
        if (state_ != GameState::SoloRace && state_ != GameState::EndRace) return;
        state_ = GameState::SoloRace;
        emit(ev(EventType::RaceStart));
    }
    void quitToMenu() {
        if (state_ != GameState::SoloRace && state_ != GameState::EndRace) return;
        log(LogInfo, "[game] player quits the map");
        state_ = GameState::MainMenu;
        emit(ev(EventType::MenuEntered));
    }
    void setOnline(bool on) {
        online_ = on;
        GameEvent e = ev(EventType::OnlineChanged);
        e.online = on;
        emit(e);
    }
    void advance(double ms) { timeOffset_ += ms; }
    void pressKey(const std::string& keyText) {
        for (auto& h : hotkeys_)
            if (h.key == keyText && h.cb) h.cb();
    }

    // --- Host ---
    void log(LogLevel level, const std::string& line) override {
        static const char* lv[] = {"debug", "info", "warn", "error"};
        std::string l = std::string("[") + lv[level] + "] " + line;
        std::lock_guard<std::mutex> g(logMu_);
        std::printf("%s\n", l.c_str());
        std::fflush(stdout);
        lines_.push_back(l);
        if (lines_.size() > 200) lines_.erase(lines_.begin());
    }
    std::string getString(const char* key, const std::string& fallback) override {
        auto it = settings_.find(key);
        return it == settings_.end() ? fallback : it->second;
    }
    void setString(const char* key, const std::string& value) override {
        if (settings_[key] == value) return;
        settings_[key] = value;
        std::string t;
        for (auto& kv : settings_) t += kv.first + "=" + kv.second + "\n";
        writeFileAtomic(data_ + "fake_host_settings.txt", t);
    }
    int getInt(const char* key, int fallback) override {
        auto it = settings_.find(key);
        return it == settings_.end() ? fallback : std::atoi(it->second.c_str());
    }
    void setInt(const char* key, int value) override { setString(key, std::to_string(value)); }
    std::string dataFolder() override { return data_; }
    std::string gameFolder() override { return std::string(); }
    std::string userFolder() override { return std::string(); }
    HostGame game() override { return game_; }
    std::string gameName() override {
        switch (game_) {
        case HostGame::Eswc: return "TrackMania Nations ESWC";
        case HostGame::Sunrise: return "TrackMania Sunrise";
        case HostGame::Original: return "TrackMania Original";
        case HostGame::Tmnf: return "TrackMania Nations Forever";
        case HostGame::Tmuf: return "TrackMania United Forever";
        case HostGame::Unknown: break;
        }
        return "unknown game";
    }
    GameState gameState() override { return state_; }
    bool isOnline() override { return online_; }
    bool actionsAllowed() override { return !online_; }
    bool menuOpen() override { return menuOpen_; }
    HostResult playMapFile(const std::string& path) override {
        log(LogInfo, "[gamectl] PlayMapFile " + path);
        if (online_) return HostResult::BlockedOnline;
        if (state_ != GameState::MainMenu) return HostResult::WrongState;
        GbxMapHeader h = readChallengeHeaderFile(path);
        if (!h.ok) return HostResult::Failed;
        state_ = GameState::Loading;
        cur_ = h;
        curPath_ = path;
        pb_ = 0;
        schedule(700, [this] {
            state_ = GameState::SoloRace;
            emit(ev(EventType::MapLoaded));
        });
        schedule(900, [this] { emit(ev(EventType::RaceStart)); });
        return HostResult::Ok;
    }
    HostResult returnToMenu() override {
        log(LogInfo, "[gamectl] ReturnToMenu");
        if (state_ != GameState::SoloRace && state_ != GameState::EndRace) return HostResult::WrongState;
        bool forever = game_ == HostGame::Tmnf || game_ == HostGame::Tmuf;
        if (forever && state_ == GameState::SoloRace) return HostResult::BlockedOnline;  // CP run gate
        schedule(400, [this] {
            state_ = GameState::MainMenu;
            emit(ev(EventType::MenuEntered));
        });
        return HostResult::Ok;
    }
    void runOnGameThread(std::function<void()> fn) override {
        std::lock_guard<std::mutex> l(qMu_);
        queue_.push_back(std::move(fn));
    }
    int registerHotkey(const char* action, const char* name, const char* key, std::function<void()> cb) override {
        hotkeys_.push_back({action, name, key ? key : "", std::move(cb)});
        return static_cast<int>(hotkeys_.size());
    }
    std::string hotkeyText(int h) override {
        return h >= 1 && h <= static_cast<int>(hotkeys_.size()) ? hotkeys_[h - 1].key : std::string();
    }
    UiStyle style() override { return style_; }
    double nowMs() override {
        LARGE_INTEGER c;
        QueryPerformanceCounter(&c);
        return static_cast<double>(c.QuadPart) * 1000.0 / freq_ + timeOffset_;
    }
    bool canCreateTextures() override { return dev_ != nullptr; }
    ImTextureID createTextureRGBA(int w, int h, const void* rgba) override {
        IDirect3DTexture9* tex = nullptr;
        if (!dev_ || FAILED(dev_->CreateTexture(w, h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, nullptr))) return 0;
        D3DLOCKED_RECT r;
        if (FAILED(tex->LockRect(0, &r, nullptr, 0))) {
            tex->Release();
            return 0;
        }
        const unsigned char* s = static_cast<const unsigned char*>(rgba);
        for (int y = 0; y < h; ++y) {
            unsigned char* d = static_cast<unsigned char*>(r.pBits) + static_cast<size_t>(y) * r.Pitch;
            for (int x = 0; x < w; ++x) {
                const unsigned char* p = s + (static_cast<size_t>(y) * w + x) * 4;
                d[x * 4] = p[2];
                d[x * 4 + 1] = p[1];
                d[x * 4 + 2] = p[0];
                d[x * 4 + 3] = p[3];
            }
        }
        tex->UnlockRect(0);
        log(LogDebug, "[host] texture " + std::to_string(w) + "x" + std::to_string(h));
        return static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(tex));
    }
    void releaseTexture(ImTextureID t) override {
        if (t) reinterpret_cast<IDirect3DTexture9*>(static_cast<uintptr_t>(t))->Release();
    }
    bool canSaveReplay() override { return replays_; }
    HostResult saveReplay(const std::string& name) override {
        log(LogInfo, "[host] SaveReplay " + name);
        std::string path = "Replays\\" + name;
        schedule(300, [this, path] {
            GameEvent e = ev(EventType::ReplaySaved);
            e.replayPath = path;
            e.replaySource = "legacy-randomizer";
            emit(e);
        });
        return HostResult::Ok;
    }

private:
    struct Sched { double at; std::function<void()> fn; };
    struct Hk { std::string action, name, key; std::function<void()> cb; };
    double freq_ = 1;
    std::string curPath_;
    std::map<std::string, std::string> settings_;
    std::vector<Sched> sched_;
    std::vector<std::function<void()>> queue_;
    std::mutex qMu_, logMu_;
    std::vector<Hk> hotkeys_;
};

// ------------------------------------------------------------------ window / device

HWND g_hwnd = nullptr;
IDirect3D9* g_d3d = nullptr;
IDirect3DDevice9* g_dev = nullptr;
D3DPRESENT_PARAMETERS g_pp = {};
UINT g_resizeW = 0, g_resizeH = 0;
bool g_shots = false;
FakeHost* g_host = nullptr;

LRESULT WINAPI wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return 1;
    switch (msg) {
    case WM_SIZE:
        if (wp != SIZE_MINIMIZED && !g_shots) {
            g_resizeW = LOWORD(lp);
            g_resizeH = HIWORD(lp);
        }
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (g_host && wp >= VK_F1 && wp <= VK_F12 && !(lp & (1 << 30))) {
            if (wp == VK_F8) {
                g_host->menuOpen_ = !g_host->menuOpen_;
                return 0;
            }
            std::string k;
            if (GetKeyState(VK_CONTROL) & 0x8000) k += "Ctrl+";
            if (GetKeyState(VK_SHIFT) & 0x8000) k += "Shift+";
            k += "F" + std::to_string(wp - VK_F1 + 1);
            g_host->pressKey(k);
            return 0;
        }
        break;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool createDevice(UINT w, UINT h) {
    g_d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!g_d3d) return false;
    g_pp.Windowed = TRUE;
    g_pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    g_pp.BackBufferWidth = w;
    g_pp.BackBufferHeight = h;
    g_pp.EnableAutoDepthStencil = TRUE;
    g_pp.AutoDepthStencilFormat = D3DFMT_D16;
    g_pp.PresentationInterval = g_shots ? D3DPRESENT_INTERVAL_IMMEDIATE : D3DPRESENT_INTERVAL_ONE;
    g_pp.hDeviceWindow = g_hwnd;
    if (SUCCEEDED(g_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, g_hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_pp, &g_dev)))
        return true;
    return SUCCEEDED(g_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, g_hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &g_pp, &g_dev));
}

void resetDevice() {
    ImGui_ImplDX9_InvalidateDeviceObjects();
    if (g_dev->Reset(&g_pp) == D3DERR_INVALIDCALL) IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

// The Horizon host's menu style (horizon-host src/host/ui/ui_style.cpp at 895610c: TM Horizon launcher
// tokens, accent #ff6b1a, radii 10/14/20, Segoe UI 16 + Bahnschrift 28), so the screenshots show the
// plugin as it looks inside the host.
UiStyle applyHostStyle(float s) {
    ImGuiStyle& st = ImGui::GetStyle();
    st = ImGuiStyle();
    st.WindowPadding = ImVec2(16.0f * s, 16.0f * s);
    st.FramePadding = ImVec2(12.0f * s, 8.0f * s);
    st.CellPadding = ImVec2(12.0f * s, 8.0f * s);
    st.ItemSpacing = ImVec2(12.0f * s, 8.0f * s);
    st.ItemInnerSpacing = ImVec2(8.0f * s, 4.0f * s);
    st.IndentSpacing = 24.0f * s;
    st.ScrollbarSize = 12.0f * s;
    st.GrabMinSize = 14.0f * s;
    st.WindowBorderSize = 1.0f;
    st.ChildBorderSize = 0.0f;
    st.PopupBorderSize = 1.0f;
    st.FrameBorderSize = 0.0f;
    st.TabBorderSize = 0.0f;
    st.WindowRounding = 20.0f * s;
    st.ChildRounding = 14.0f * s;
    st.FrameRounding = 10.0f * s;
    st.PopupRounding = 10.0f * s;
    st.ScrollbarRounding = 12.0f * s;
    st.GrabRounding = 10.0f * s;
    st.TabRounding = 10.0f * s;
    st.WindowTitleAlign = ImVec2(0.0f, 0.5f);

    const ImVec4 accent(1.0f, 0.42f, 0.10f, 1.0f), accent2(1.0f, 0.54f, 0.27f, 1.0f), accent3(1.0f, 0.69f, 0.37f, 1.0f);
    const ImVec4 bg(0.039f, 0.047f, 0.067f, 0.97f), bg2(0.063f, 0.075f, 0.102f, 1.0f), surface(0.106f, 0.118f, 0.141f, 1.0f);
    const ImVec4 text(0.933f, 0.941f, 0.953f, 1.0f), text2(0.765f, 0.788f, 0.820f, 1.0f), muted(0.557f, 0.592f, 0.639f, 1.0f);
    auto a = [](ImVec4 c, float alpha) { return ImVec4(c.x, c.y, c.z, alpha); };
    ImVec4* c = st.Colors;
    ImGui::StyleColorsDark();
    c[ImGuiCol_Text] = text;
    c[ImGuiCol_TextDisabled] = muted;
    c[ImGuiCol_WindowBg] = bg;
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = a(bg2, 0.98f);
    c[ImGuiCol_Border] = ImVec4(1, 1, 1, 0.08f);
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = ImVec4(1, 1, 1, 0.07f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(1, 1, 1, 0.11f);
    c[ImGuiCol_FrameBgActive] = ImVec4(1, 1, 1, 0.15f);
    c[ImGuiCol_TitleBg] = bg2;
    c[ImGuiCol_TitleBgActive] = bg2;
    c[ImGuiCol_TitleBgCollapsed] = a(bg2, 0.8f);
    c[ImGuiCol_MenuBarBg] = bg2;
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(1, 1, 1, 0.14f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1, 1, 1, 0.24f);
    c[ImGuiCol_ScrollbarGrabActive] = a(accent2, 0.85f);
    c[ImGuiCol_CheckMark] = accent2;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = accent2;
    c[ImGuiCol_Button] = surface;
    c[ImGuiCol_ButtonHovered] = ImVec4(0.153f, 0.169f, 0.204f, 1.0f);
    c[ImGuiCol_ButtonActive] = a(accent, 0.35f);
    c[ImGuiCol_Header] = a(accent, 0.16f);
    c[ImGuiCol_HeaderHovered] = a(accent, 0.28f);
    c[ImGuiCol_HeaderActive] = a(accent, 0.40f);
    c[ImGuiCol_Separator] = ImVec4(1, 1, 1, 0.08f);
    c[ImGuiCol_SeparatorHovered] = a(accent2, 0.6f);
    c[ImGuiCol_SeparatorActive] = accent2;
    c[ImGuiCol_ResizeGrip] = ImVec4(1, 1, 1, 0.08f);
    c[ImGuiCol_ResizeGripHovered] = a(accent2, 0.6f);
    c[ImGuiCol_ResizeGripActive] = accent2;
    c[ImGuiCol_TabHovered] = a(accent, 0.4f);
    c[ImGuiCol_Tab] = surface;
    c[ImGuiCol_TabSelected] = a(accent, 0.3f);
    c[ImGuiCol_TabSelectedOverline] = accent;
    c[ImGuiCol_TableHeaderBg] = bg2;
    c[ImGuiCol_TableBorderStrong] = ImVec4(1, 1, 1, 0.08f);
    c[ImGuiCol_TableBorderLight] = ImVec4(1, 1, 1, 0.05f);
    c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(1, 1, 1, 0.025f);
    c[ImGuiCol_TextSelectedBg] = a(accent, 0.35f);
    c[ImGuiCol_PlotHistogram] = accent;
    c[ImGuiCol_PlotLines] = accent2;
    c[ImGuiCol_PlotHistogramHovered] = accent3;
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.55f);

    ImGuiIO& io = ImGui::GetIO();
    char win[MAX_PATH] = {};
    GetWindowsDirectoryA(win, MAX_PATH);
    std::string fonts = std::string(win) + "\\Fonts\\";
    static const ImWchar ranges[] = {0x0020, 0x024F, 0x0370, 0x03FF, 0x0400, 0x04FF, 0x2000, 0x206F, 0};
    auto add = [&](const char* file, float px) -> ImFont* {
        std::string p = fonts + file;
        if (GetFileAttributesA(p.c_str()) == INVALID_FILE_ATTRIBUTES) return nullptr;
        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 2;
        return io.Fonts->AddFontFromFileTTF(p.c_str(), px, &cfg, ranges);
    };
    UiStyle u;
    u.scale = s;
    u.body = add("segoeui.ttf", 16.0f * s);
    if (!u.body) u.body = io.Fonts->AddFontDefault();
    u.title = add("bahnschrift.ttf", 28.0f * s);
    if (!u.title) u.title = add("seguisb.ttf", 16.0f * s);
    u.mono = add("consola.ttf", 14.0f * s);
    io.FontDefault = u.body;
    u.accent = accent;
    u.ok = ImVec4(0.24f, 0.86f, 0.52f, 1.0f);
    u.warn = ImVec4(0.95f, 0.76f, 0.31f, 1.0f);
    u.bad = ImVec4(1.0f, 0.36f, 0.36f, 1.0f);
    u.text = text;
    u.text2 = text2;
    u.muted = muted;
    u.surface = surface;
    return u;
}

// A plain "game" behind the UI: sky, hills, grass, road.
void drawBackdrop(bool dim) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImVec2 sz = ImGui::GetIO().DisplaySize;
    float hz = sz.y * 0.52f;
    dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(sz.x, hz), IM_COL32(44, 86, 160, 255), IM_COL32(44, 86, 160, 255),
                                IM_COL32(140, 182, 224, 255), IM_COL32(140, 182, 224, 255));
    ImVec2 hills[] = {{0, hz}, {sz.x * 0.14f, hz - sz.y * 0.12f}, {sz.x * 0.28f, hz - 10}, {sz.x * 0.42f, hz - sz.y * 0.15f},
                      {sz.x * 0.58f, hz - 20}, {sz.x * 0.74f, hz - sz.y * 0.11f}, {sz.x * 0.9f, hz - 30}, {sz.x, hz - sz.y * 0.08f},
                      {sz.x, hz}};
    for (int i = 0; i + 1 < 9; ++i)
        dl->AddTriangleFilled(hills[i], hills[i + 1], ImVec2((hills[i].x + hills[i + 1].x) * 0.5f, hz), IM_COL32(92, 120, 140, 255));
    dl->AddRectFilled(ImVec2(0, hz - 30), ImVec2(sz.x, hz), IM_COL32(92, 120, 140, 255));
    dl->AddRectFilledMultiColor(ImVec2(0, hz), ImVec2(sz.x, sz.y), IM_COL32(78, 110, 58, 255), IM_COL32(78, 110, 58, 255),
                                IM_COL32(38, 58, 30, 255), IM_COL32(38, 58, 30, 255));
    float cx = sz.x * 0.5f;
    dl->AddQuadFilled(ImVec2(cx - 60, hz + 20), ImVec2(cx + 60, hz + 20), ImVec2(cx + sz.x * 0.36f, sz.y), ImVec2(cx - sz.x * 0.36f, sz.y),
                      IM_COL32(52, 54, 62, 255));
    dl->AddQuadFilled(ImVec2(cx - 60, hz + 20), ImVec2(cx - 52, hz + 20), ImVec2(cx - sz.x * 0.30f, sz.y), ImVec2(cx - sz.x * 0.36f, sz.y),
                      IM_COL32(214, 76, 44, 255));
    dl->AddQuadFilled(ImVec2(cx + 52, hz + 20), ImVec2(cx + 60, hz + 20), ImVec2(cx + sz.x * 0.36f, sz.y), ImVec2(cx + sz.x * 0.30f, sz.y),
                      IM_COL32(230, 230, 230, 255));
    dl->AddRectFilled(ImVec2(cx - 160, sz.y - 250), ImVec2(cx + 160, sz.y - 120), IM_COL32(200, 44, 46, 255), 24.0f);
    if (dim) dl->AddRectFilled(ImVec2(0, 0), sz, IM_COL32(8, 10, 16, 150));
}

// The host's menu shell, reduced to one page: the plugin draws inside the child.
void drawShell(FakeHost& host, Plugin& plugin) {
    if (!host.menuOpen_) return;
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    float sc = host.style_.scale;
    ImVec2 size(1180 * sc, 900 * sc);
    if (size.y > ds.y - 40) size.y = ds.y - 40;
    ImGui::SetNextWindowPos(ImVec2(ds.x * 0.5f, ds.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
    if (ImGui::Begin("Horizon (preview host)  >  Legacy Randomizer", &host.menuOpen_, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::BeginChild("##plugin_legacy-randomizer");
        plugin.drawMenu();
        ImGui::EndChild();
    }
    ImGui::End();
}

void drawFakeGamePanel(FakeHost& host, Plugin& plugin) {
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(ds.x - 12, ds.y - 12), ImGuiCond_FirstUseEver, ImVec2(1, 1));
    ImGui::SetNextWindowSize(ImVec2(440, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Fake game (preview only)", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }
    static const char* states[] = {"unknown", "main menu", "solo race", "end-race screen", "net race", "editor", "replay", "loading"};
    ImGui::Text("%s, state: %s%s", host.gameName().c_str(), states[static_cast<int>(host.state_)], host.online_ ? ", ONLINE" : "");
    if (host.cur_.ok) ImGui::Text("Map: %s  AT %s  Gold %s  PB %s", stripTmCodes(host.cur_.name).c_str(), formatMs(host.cur_.authorMs).c_str(),
                                   formatMs(host.cur_.goldMs).c_str(), formatMs(host.pb_).c_str());
    ImGui::Text("Run: %s", phaseName(plugin.run().phase()));
    bool race = host.state_ == GameState::SoloRace;
    ImGui::BeginDisabled(!race || !host.cur_.ok);
    if (ImGui::Button("Finish: AT")) host.finish(host.cur_.authorMs > 0 ? host.cur_.authorMs - 7 : host.cur_.goldMs);
    ImGui::SameLine();
    if (ImGui::Button("Finish: gold")) host.finish(host.cur_.goldMs);
    ImGui::SameLine();
    if (ImGui::Button("Finish: silver")) host.finish(host.cur_.silverMs);
    ImGui::SameLine();
    if (ImGui::Button("Finish: slow")) host.finish(host.cur_.bronzeMs + 5000);
    ImGui::EndDisabled();
    if (ImGui::Button("Restart (RaceStart)")) host.restart();
    ImGui::SameLine();
    if (ImGui::Button("Esc > Quit")) host.quitToMenu();
    ImGui::SameLine();
    if (ImGui::Button(host.online_ ? "Go offline" : "Go online")) host.setOnline(!host.online_);
    if (ImGui::Button("+1 min")) host.advance(60000);
    ImGui::SameLine();
    if (ImGui::Button("+10 min")) host.advance(600000);
    ImGui::SameLine();
    ImGui::Checkbox("host can save replays", &host.replays_);
    int g = static_cast<int>(host.game_) - 1;
    const char* games[] = {"ESWC", "Sunrise", "Original", "TMNF", "TMUF"};
    if (ImGui::Combo("game", &g, games, 5)) host.game_ = static_cast<HostGame>(g + 1);
    ImGui::TextDisabled("F8 menu, Ctrl+F9 skip, Ctrl+F10 gold skip, Ctrl+F11 HUD");
    ImGui::Separator();
    ImGui::BeginChild("##log");
    for (const std::string& l : host.lines_) ImGui::TextUnformatted(l.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    ImGui::End();
}

// ------------------------------------------------------------------ frame + capture

struct App {
    FakeHost* host;
    Plugin* plugin;
    bool showPanel = true;

    void frame(bool present) {
        if (g_resizeW && g_resizeH) {
            g_pp.BackBufferWidth = g_resizeW;
            g_pp.BackBufferHeight = g_resizeH;
            g_resizeW = g_resizeH = 0;
            resetDevice();
        }
        host->pumpGame();
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        if (g_shots) ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(g_pp.BackBufferWidth), static_cast<float>(g_pp.BackBufferHeight));
        ImGui::NewFrame();
        drawBackdrop(host->menuOpen_);
        plugin->drawOverlay();
        drawShell(*host, *plugin);
        if (!g_shots && showPanel) drawFakeGamePanel(*host, *plugin);
        ImGui::EndFrame();
        g_dev->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        g_dev->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(20, 22, 28, 255), 1.0f, 0);
        if (g_dev->BeginScene() >= 0) {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_dev->EndScene();
        }
        if (present) {
            HRESULT r = g_dev->Present(nullptr, nullptr, nullptr, nullptr);
            if (r == D3DERR_DEVICELOST && g_dev->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) resetDevice();
        }
    }
    void frames(int n) {
        for (int i = 0; i < n; ++i) {
            pumpMessages();
            frame(false);
        }
    }
    static void pumpMessages() {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    bool waitUntil(const std::function<bool()>& done, int timeoutMs) {
        DWORD start = GetTickCount();
        while (!done()) {
            if (static_cast<int>(GetTickCount() - start) > timeoutMs) return false;
            pumpMessages();
            frame(false);
            Sleep(15);
        }
        return true;
    }
};

bool savePng(const std::wstring& path) {
    IDirect3DSurface9* rt = nullptr;
    IDirect3DSurface9* sys = nullptr;
    if (FAILED(g_dev->GetRenderTarget(0, &rt))) return false;
    D3DSURFACE_DESC d;
    rt->GetDesc(&d);
    bool ok = SUCCEEDED(g_dev->CreateOffscreenPlainSurface(d.Width, d.Height, d.Format, D3DPOOL_SYSTEMMEM, &sys, nullptr)) &&
              SUCCEEDED(g_dev->GetRenderTargetData(rt, sys));
    std::vector<unsigned char> bgr;
    UINT stride = d.Width * 3;
    if (ok) {
        D3DLOCKED_RECT lr;
        ok = SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY));
        if (ok) {
            bgr.resize(static_cast<size_t>(stride) * d.Height);
            for (UINT y = 0; y < d.Height; ++y) {
                const unsigned char* s = static_cast<const unsigned char*>(lr.pBits) + static_cast<size_t>(y) * lr.Pitch;
                unsigned char* o = bgr.data() + static_cast<size_t>(y) * stride;
                for (UINT x = 0; x < d.Width; ++x) {
                    o[x * 3] = s[x * 4];
                    o[x * 3 + 1] = s[x * 4 + 1];
                    o[x * 3 + 2] = s[x * 4 + 2];
                }
            }
            sys->UnlockRect();
        }
    }
    if (sys) sys->Release();
    rt->Release();
    if (!ok) return false;
    IWICImagingFactory* f = nullptr;
    IWICStream* st = nullptr;
    IWICBitmapEncoder* enc = nullptr;
    IWICBitmapFrameEncode* fr = nullptr;
    IPropertyBag2* props = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f));
    if (SUCCEEDED(hr)) hr = f->CreateStream(&st);
    if (SUCCEEDED(hr)) hr = st->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
    if (SUCCEEDED(hr)) hr = f->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc);
    if (SUCCEEDED(hr)) hr = enc->Initialize(st, WICBitmapEncoderNoCache);
    if (SUCCEEDED(hr)) hr = enc->CreateNewFrame(&fr, &props);
    if (SUCCEEDED(hr)) hr = fr->Initialize(props);
    if (SUCCEEDED(hr)) hr = fr->SetSize(d.Width, d.Height);
    WICPixelFormatGUID fmt = GUID_WICPixelFormat24bppBGR;
    if (SUCCEEDED(hr)) hr = fr->SetPixelFormat(&fmt);
    if (SUCCEEDED(hr)) hr = fr->WritePixels(d.Height, stride, static_cast<UINT>(bgr.size()), bgr.data());
    if (SUCCEEDED(hr)) hr = fr->Commit();
    if (SUCCEEDED(hr)) hr = enc->Commit();
    if (props) props->Release();
    if (fr) fr->Release();
    if (enc) enc->Release();
    if (st) st->Release();
    if (f) f->Release();
    return SUCCEEDED(hr);
}

int g_shotFails = 0;

void shot(App& app, const std::string& dir, const char* name) {
    app.frames(8);
    std::string path = dir + name;
    bool ok = savePng(toWide(path));
    std::printf("%s %s\n", ok ? "wrote" : "FAILED", path.c_str());
    if (!ok) ++g_shotFails;
}

int runShots(App& app, std::string dir) {
    if (!dir.empty() && dir.back() != '\\' && dir.back() != '/') dir += '\\';
    ensureDir(dir);
    FakeHost& host = *app.host;
    Plugin& plugin = *app.plugin;
    RunController& run = plugin.run();
    auto fail = [&](const char* what) {
        std::printf("shots: %s (run phase %s, fetch: %s)\n", what, phaseName(run.phase()), run.fetchStatus().c_str());
        return 1;
    };

    Panels& panels = plugin.panels();
    host.menuOpen_ = true;
    panels.select("run");
    shot(app, dir, "01-start.png");
    panels.select("filters");
    plugin.settings().atMinS = 30;  // show the author time range with values; the run below uses any length
    plugin.settings().atMaxS = 90;
    shot(app, dir, "02-filters.png");
    plugin.settings().atMinS = 0;
    plugin.settings().atMaxS = 0;
    panels.select("rules");
    shot(app, dir, "03-rules.png");
    panels.select("layout");
    plugin.settings().layoutEdit = true;
    shot(app, dir, "04-layout-editor.png");
    plugin.settings().layoutEdit = false;
    panels.select("run");
    app.frames(2);
    host.menuOpen_ = false;

    std::string why;
    if (!run.start(&why)) return fail(why.c_str());
    if (!app.waitUntil([&] { return run.phase() == Phase::Playing; }, 120000)) return fail("first map did not start");
    long long first = run.displayMap()->tmx.id;
    app.waitUntil([&] { return run.fetcher().hasReady(); }, 60000);  // the next map, prefetched

    // map 1: a gold finish, then the author medal -> next (prefetched) map
    MapMedals m1 = run.displayMap()->medals;
    host.advance(6 * 60000 + 31000);
    app.frames(2);
    int goldTime = m1.goldMs > m1.authorMs ? (m1.goldMs + m1.authorMs) / 2 : m1.goldMs;
    host.finish(goldTime);
    app.frames(2);
    host.restart();
    app.frames(2);
    host.advance(95000);
    host.finish(m1.authorMs - 13);
    if (!app.waitUntil([&] { return run.phase() == Phase::Playing && run.displayMap() && run.displayMap()->tmx.id != first; }, 120000))
        return fail("second map did not start");
    long long second = run.displayMap()->tmx.id;
    // map 2: free skip
    host.advance(41000);
    app.frames(2);
    run.skip();
    if (!app.waitUntil([&] { return run.phase() == Phase::Playing && run.displayMap() && run.displayMap()->tmx.id != second; }, 120000))
        return fail("third map did not start");
    // map 3: a gold finish, back on the track: the HUD shots
    MapMedals m3 = run.displayMap()->medals;
    host.advance(4 * 60000 + 36000);
    app.frames(2);
    host.finish(m3.goldMs > m3.authorMs ? (m3.goldMs + m3.authorMs) / 2 : m3.goldMs);
    app.frames(2);
    host.restart();
    app.waitUntil([&] { return run.fetcher().hasReady(); }, 60000);
    shot(app, dir, "05-hud-split.png");
    panels.applyCompactPreset();
    shot(app, dir, "06-hud-compact.png");
    panels.applySplitPreset();
    app.frames(2);
    // gold skip, end the run
    run.goldSkip();
    app.frames(4);
    run.stop();
    app.frames(4);
    host.menuOpen_ = true;
    panels.select("run");
    shot(app, dir, "07-results.png");
    panels.select("history");
    plugin.selectHistoryRun(0);
    shot(app, dir, "08-history.png");
    return g_shotFails ? 1 : 0;
}

} // namespace

int main(int argc, char** argv) {
    std::string shotsDir, data;
    HostGame game = HostGame::Eswc;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--shots") && i + 1 < argc) shotsDir = argv[++i];
        else if (!std::strcmp(argv[i], "--data") && i + 1 < argc) data = argv[++i];
        else if (!std::strcmp(argv[i], "--game") && i + 1 < argc) {
            std::string g = argv[++i];
            game = g == "sunrise" ? HostGame::Sunrise : g == "original" ? HostGame::Original : g == "tmnf" ? HostGame::Tmnf
                 : g == "tmuf" ? HostGame::Tmuf : HostGame::Eswc;
        }
    }
    g_shots = !shotsDir.empty();
    if (data.empty()) {
        if (g_shots) {
            // fresh data folder: the screenshots show exactly one run
            wchar_t tmp[MAX_PATH];
            GetTempPathW(MAX_PATH, tmp);
            data = toUtf8(tmp) + "lr_ui_preview_shots\\";
            for (const std::string& f : listFiles(data + "runs\\", "*.json")) deleteFile(data + "runs\\" + f);
            deleteFile(data + "fake_host_settings.txt");
        } else {
            char exe[MAX_PATH];
            GetModuleFileNameA(nullptr, exe, MAX_PATH);
            std::string d = exe;
            d = d.substr(0, d.find_last_of('\\') + 1);
            data = d + "ui_preview_data\\";
        }
    }
    if (data.back() != '\\') data += '\\';
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ImGui_ImplWin32_EnableDpiAwareness();

    const UINT W = g_shots ? 1920 : 1600, H = g_shots ? 1080 : 900;
    WNDCLASSEXW wc = {sizeof wc, CS_CLASSDC, wndProc, 0, 0, GetModuleHandleW(nullptr), nullptr, nullptr, nullptr, nullptr, L"LrUiPreview", nullptr};
    RegisterClassExW(&wc);
    DWORD style = g_shots ? WS_POPUP : WS_OVERLAPPEDWINDOW;
    RECT rc = {0, 0, static_cast<LONG>(W), static_cast<LONG>(H)};
    AdjustWindowRect(&rc, style, FALSE);
    g_hwnd = CreateWindowW(wc.lpszClassName, L"Legacy Randomizer - UI preview (fake host)", style, 60, 40, rc.right - rc.left,
                           rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr);
    if (!g_hwnd || !createDevice(W, H)) {
        std::printf("cannot create the window / Direct3D 9 device\n");
        return 1;
    }
    if (!g_shots) {
        ShowWindow(g_hwnd, SW_SHOWDEFAULT);
        UpdateWindow(g_hwnd);
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    FakeHost host(data, game);
    host.style_ = applyHostStyle(1.0f);
    host.dev_ = g_dev;
    g_host = &host;
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX9_Init(g_dev);

    Plugin plugin(host);
    host.plugin_ = &plugin;
    plugin.init();
    App app{&host, &plugin};

    int rc2 = 0;
    if (g_shots) {
        rc2 = runShots(app, shotsDir);
    } else {
        bool done = false;
        while (!done) {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
                if (msg.message == WM_QUIT) done = true;
            }
            if (done) break;
            app.frame(true);
        }
    }
    plugin.shutdown();
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_dev->Release();
    g_d3d->Release();
    DestroyWindow(g_hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    CoUninitialize();
    return rc2;
}
