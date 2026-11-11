// The only file that knows the Horizon plugin ABI: implements lr::Host on top of HorizonHostApi and
// exports the HorizonPlugin_* entry points. Headers: src/plugin/horizon/ (copied verbatim from
// horizon-host, see docs/plugin.md for the pinned commit).
#define HORIZON_PLUGIN_BUILD
#include "horizon/horizon_plugin.h"

#include <cstddef>
#include <memory>
#include <vector>

#include <windows.h>
#include <d3d9.h>

#include "imgui.h"
#include "horizon/horizon_imgui.h"
#include "plugin.h"

namespace {

const HorizonHostApi* g_api = nullptr;
HorizonPluginHandle g_self = nullptr;

// true when the host's table is large enough to contain `field`
#define HOST_HAS(field) (g_api && g_api->size >= offsetof(HorizonHostApi, field) + sizeof(g_api->field) && g_api->field)
#define EVENT_HAS(e, field) ((e)->size >= offsetof(HorizonEvent, field) + sizeof((e)->field))
#define MAP_HAS(m, field) ((m)->size >= offsetof(HorizonMapInfo, field) + sizeof((m)->field))

ImVec4 col(const HorizonColor& c) { return ImVec4(c.r, c.g, c.b, c.a); }
const char* s(const char* p) { return p ? p : ""; }

void HORIZON_CALL runTrampoline(void* user) {
    std::unique_ptr<std::function<void()>> fn(static_cast<std::function<void()>*>(user));
    if (*fn) (*fn)();
}

void HORIZON_CALL hotkeyTrampoline(void* user) {
    auto* fn = static_cast<std::function<void()>*>(user);
    if (*fn) (*fn)();
}

class HorizonHost final : public lr::Host {
public:
    HorizonHost() {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        freq_ = static_cast<double>(f.QuadPart);
    }

    void log(lr::LogLevel level, const std::string& line) override {
        if (HOST_HAS(log)) g_api->log(g_self, static_cast<int32_t>(level), "%s", line.c_str());
    }

    std::string getString(const char* key, const std::string& fallback) override {
        if (!HOST_HAS(get_string)) return fallback;
        char buf[1024];
        int32_t n = g_api->get_string(g_self, key, fallback.c_str(), buf, sizeof buf);
        if (n < 0) return fallback;
        if (n > static_cast<int32_t>(sizeof buf) - 1) n = sizeof buf - 1;
        return std::string(buf, static_cast<size_t>(n));
    }
    void setString(const char* key, const std::string& value) override {
        if (HOST_HAS(set_string)) g_api->set_string(g_self, key, value.c_str());
    }
    int getInt(const char* key, int fallback) override {
        return HOST_HAS(get_int) ? g_api->get_int(g_self, key, fallback) : fallback;
    }
    void setInt(const char* key, int value) override {
        if (HOST_HAS(set_int)) g_api->set_int(g_self, key, value);
    }

    std::string dataFolder() override {
        std::string d = HOST_HAS(data_folder) ? s(g_api->data_folder(g_self)) : "";
        if (!d.empty() && d.back() != '\\') d += '\\';
        return d;
    }

    lr::HostGame game() override {
        const HorizonGameInfo* gi = HOST_HAS(game_info) ? g_api->game_info() : nullptr;
        if (!gi) return lr::HostGame::Unknown;
        switch (gi->game) {
        case HORIZON_GAME_ESWC: return lr::HostGame::Eswc;
        case HORIZON_GAME_SUNRISE: return lr::HostGame::Sunrise;
        case HORIZON_GAME_ORIGINAL: return lr::HostGame::Original;
        case HORIZON_GAME_TMNF: return lr::HostGame::Tmnf;
        case HORIZON_GAME_TMUF: return lr::HostGame::Tmuf;
        default: return lr::HostGame::Unknown;
        }
    }
    std::string gameName() override {
        const HorizonGameInfo* gi = HOST_HAS(game_info) ? g_api->game_info() : nullptr;
        return gi && gi->game_name ? gi->game_name : "unknown game";
    }
    lr::GameState gameState() override {
        if (!HOST_HAS(get_game_state)) return lr::GameState::Unknown;
        int32_t st = g_api->get_game_state();
        return st >= 0 && st <= HORIZON_STATE_LOADING ? static_cast<lr::GameState>(st) : lr::GameState::Unknown;
    }
    bool isOnline() override { return HOST_HAS(is_online) && g_api->is_online() != 0; }
    bool actionsAllowed() override { return !HOST_HAS(actions_allowed) || g_api->actions_allowed(g_self) != 0; }
    bool menuOpen() override { return HOST_HAS(menu_open) && g_api->menu_open() != 0; }

    lr::HostResult playMapFile(const std::string& path) override {
        if (!HOST_HAS(play_map_file)) return lr::HostResult::NotSupported;
        return static_cast<lr::HostResult>(g_api->play_map_file(g_self, path.c_str(), 0));
    }
    lr::HostResult returnToMenu() override {
        if (!HOST_HAS(return_to_menu)) return lr::HostResult::NotSupported;
        return static_cast<lr::HostResult>(g_api->return_to_menu(g_self));
    }
    void runOnGameThread(std::function<void()> fn) override {
        if (!HOST_HAS(run_on_game_thread)) return;
        g_api->run_on_game_thread(g_self, runTrampoline, new std::function<void()>(std::move(fn)));
    }

    int registerHotkey(const char* action, const char* name, const char* key, std::function<void()> cb) override {
        if (!HOST_HAS(register_hotkey)) return 0;
        hotkeys_.push_back(std::make_unique<std::function<void()>>(std::move(cb)));
        return g_api->register_hotkey(g_self, action, name, key, hotkeyTrampoline, hotkeys_.back().get());
    }
    std::string hotkeyText(int handle) override {
        return HOST_HAS(hotkey_text) ? s(g_api->hotkey_text(g_self, handle)) : "";
    }

    lr::UiStyle style() override {
        lr::UiStyle u;
        const HorizonStyle* hs = HOST_HAS(style) ? g_api->style() : nullptr;
        if (!hs || hs->size < offsetof(HorizonStyle, font_mono) + sizeof(hs->font_mono)) return u;
        u.accent = col(hs->accent);
        u.ok = col(hs->ok);
        u.warn = col(hs->warn);
        u.bad = col(hs->bad);
        u.text = col(hs->text);
        u.text2 = col(hs->text2);
        u.muted = col(hs->muted);
        u.surface = col(hs->surface);
        u.body = static_cast<ImFont*>(hs->font_body);
        u.title = static_cast<ImFont*>(hs->font_title);
        u.mono = static_cast<ImFont*>(hs->font_mono);
        u.scale = hs->scale > 0 ? hs->scale : 1.0f;
        return u;
    }

    double nowMs() override {
        LARGE_INTEGER c;
        QueryPerformanceCounter(&c);
        return static_cast<double>(c.QuadPart) * 1000.0 / freq_;
    }

    // ABI minor 1: the host creates and releases textures. Older hosts (minor 0): use the D3D9 device the
    // host hands out and create a managed texture ourselves (what the host's DX9 renderer takes).
    bool hostTextures() { return g_api && g_api->abi_minor >= 1 && HOST_HAS(create_texture_rgba) && HOST_HAS(release_texture); }
    IDirect3DDevice9* dev() { return HOST_HAS(device) ? static_cast<IDirect3DDevice9*>(g_api->device()) : nullptr; }
    bool canCreateTextures() override { return hostTextures() || dev() != nullptr; }
    ImTextureID createTextureRGBA(int w, int h, const void* rgba) override {
        if (w <= 0 || h <= 0) return 0;
        if (hostTextures()) {
            ImTextureID t = static_cast<ImTextureID>(g_api->create_texture_rgba(g_self, static_cast<uint32_t>(w), static_cast<uint32_t>(h), rgba));
            if (t) fromHost_.push_back(t);
            return t;
        }
        IDirect3DDevice9* d = dev();
        if (!d) return 0;
        IDirect3DTexture9* tex = nullptr;
        if (FAILED(d->CreateTexture(static_cast<UINT>(w), static_cast<UINT>(h), 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, nullptr)))
            return 0;
        D3DLOCKED_RECT lr;
        if (FAILED(tex->LockRect(0, &lr, nullptr, 0))) {
            tex->Release();
            return 0;
        }
        const unsigned char* src = static_cast<const unsigned char*>(rgba);
        for (int y = 0; y < h; ++y) {
            unsigned char* dst = static_cast<unsigned char*>(lr.pBits) + static_cast<size_t>(y) * lr.Pitch;
            for (int x = 0; x < w; ++x) {
                const unsigned char* p = src + (static_cast<size_t>(y) * w + x) * 4;
                dst[x * 4 + 0] = p[2];
                dst[x * 4 + 1] = p[1];
                dst[x * 4 + 2] = p[0];
                dst[x * 4 + 3] = p[3];
            }
        }
        tex->UnlockRect(0);
        return static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(tex));  // ImTextureID is ImU64 since 1.91.4
    }
    void releaseTexture(ImTextureID t) override {
        if (!t) return;
        for (size_t i = 0; i < fromHost_.size(); ++i)
            if (fromHost_[i] == t) {
                fromHost_.erase(fromHost_.begin() + static_cast<long>(i));
                if (HOST_HAS(release_texture)) g_api->release_texture(g_self, static_cast<uint64_t>(t));
                return;
            }
        reinterpret_cast<IDirect3DTexture9*>(static_cast<uintptr_t>(t))->Release();
    }

    // save_replay exists from ABI minor 1 but host 0.1.0 answers NOT_SUPPORTED everywhere: stays off.
    bool canSaveReplay() override { return false; }
    lr::HostResult saveReplay(const std::string&) override { return lr::HostResult::NotSupported; }

    std::string gameFolder() override {
        const HorizonGameInfo* gi = HOST_HAS(game_info) ? g_api->game_info() : nullptr;
        std::string d = gi ? s(gi->game_folder) : "";
        if (!d.empty() && d.back() != '\\') d += '\\';
        return d;
    }
    std::string userFolder() override {
        const HorizonGameInfo* gi = HOST_HAS(game_info) ? g_api->game_info() : nullptr;
        std::string d = gi ? s(gi->user_folder) : "";
        if (!d.empty() && d.back() != '\\') d += '\\';
        return d;
    }

private:
    double freq_ = 1.0;
    std::vector<std::unique_ptr<std::function<void()>>> hotkeys_;
    std::vector<ImTextureID> fromHost_;  // textures made by the host (released through it)
};

HorizonHost* g_host = nullptr;
lr::Plugin* g_plugin = nullptr;

void attachContext() {
    if (HOST_HAS(imgui_context)) ImGui::SetCurrentContext(static_cast<ImGuiContext*>(g_api->imgui_context()));
}

lr::GameEvent convert(const HorizonEvent* e) {
    lr::GameEvent g;
    g.type = static_cast<lr::EventType>(e->type);
    if (EVENT_HAS(e, time_ms)) g.timeMs = e->time_ms;
    if (EVENT_HAS(e, checkpoint_index)) g.checkpoint = e->checkpoint_index;
    if (EVENT_HAS(e, respawns)) g.respawns = e->respawns;
    if (EVENT_HAS(e, is_local_player)) g.isLocalPlayer = e->is_local_player != 0;
    if (EVENT_HAS(e, is_pb)) g.isPb = e->is_pb != 0;
    if (EVENT_HAS(e, end_screen)) g.endScreen = e->end_screen != 0;
    if (EVENT_HAS(e, is_online)) g.online = e->is_online != 0;
    if (EVENT_HAS(e, game_state)) g.state = static_cast<lr::GameState>(e->game_state);
    if (EVENT_HAS(e, play_mode))
        g.solo = e->play_mode != HORIZON_MODE_NET && e->play_mode != HORIZON_MODE_EDITOR_TEST && e->play_mode != HORIZON_MODE_REPLAY;
    if (EVENT_HAS(e, map) && e->map) {
        const HorizonMapInfo* m = e->map;
        g.hasMap = true;
        if (MAP_HAS(m, path)) g.mapPath = s(m->path);
        if (MAP_HAS(m, uid)) g.mapUid = s(m->uid);
        if (MAP_HAS(m, name)) g.mapName = s(m->name);
        if (MAP_HAS(m, author)) g.mapAuthor = s(m->author);
        if (MAP_HAS(m, bronze_time)) {
            g.authorMs = m->author_time;
            g.goldMs = m->gold_time;
            g.silverMs = m->silver_time;
            g.bronzeMs = m->bronze_time;
        }
    }
    if (EVENT_HAS(e, path)) g.replayPath = s(e->path);
    if (EVENT_HAS(e, source)) g.replaySource = s(e->source);
    return g;
}

} // namespace

extern "C" {

HORIZON_EXPORT const HorizonPluginInfo* HORIZON_CALL HorizonPlugin_Info(void) {
    static const HorizonPluginInfo info = {
        HORIZON_ABI,
        "legacy-randomizer",
        "Legacy Randomizer",
        LR_PLUGIN_VERSION,
        HORIZON_GAME_ALL,  // Original: experimental (marked in the UI); the host refuses map loading there until supported
        HORIZON_CAP_MENU | HORIZON_CAP_OVERLAY | HORIZON_CAP_GAME_EVENTS | HORIZON_CAP_GAME_CONTROL,
        HORIZON_FLAG_SOLO_ONLY | HORIZON_FLAG_LIVE_TOGGLE,
        "TM Horizon",
        "Random Map Challenge and Survival with maps from TrackMania Exchange, inside the game.",
        HORIZON_ABI_MINOR,
    };
    return &info;
}

HORIZON_EXPORT int32_t HORIZON_CALL HorizonPlugin_Init(const HorizonHostApi* host) {
    if (!host || host->abi != HORIZON_ABI) return 1;
    g_api = host;
    g_self = host->plugin;
    if (!horizon_imgui_attach(host)) {
        if (HOST_HAS(log)) g_api->log(g_self, HORIZON_LOG_ERROR, "Dear ImGui of the host differs (plugin built with %s)", IMGUI_VERSION);
        g_api = nullptr;
        return 2;
    }
    g_host = new HorizonHost();
    g_plugin = new lr::Plugin(*g_host);
    g_plugin->init();
    if (HOST_HAS(subscribe))
        g_api->subscribe(g_self, HORIZON_EVENT_BIT(HORIZON_EVENT_MAP_LOADED) | HORIZON_EVENT_BIT(HORIZON_EVENT_RACE_START) |
                                     HORIZON_EVENT_BIT(HORIZON_EVENT_FINISH) | HORIZON_EVENT_BIT(HORIZON_EVENT_RACE_END) |
                                     HORIZON_EVENT_BIT(HORIZON_EVENT_MENU_ENTERED) | HORIZON_EVENT_BIT(HORIZON_EVENT_REPLAY_SAVED) |
                                     HORIZON_EVENT_BIT(HORIZON_EVENT_ONLINE_CHANGED));
    return 0;
}

HORIZON_EXPORT void HORIZON_CALL HorizonPlugin_Shutdown(void) {
    if (g_plugin) {
        attachContext();
        g_plugin->shutdown();
        delete g_plugin;
        g_plugin = nullptr;
    }
    delete g_host;
    g_host = nullptr;
    g_api = nullptr;
}

HORIZON_EXPORT void HORIZON_CALL HorizonPlugin_DrawMenu(void) {
    if (!g_plugin) return;
    attachContext();
    g_plugin->drawMenu();
}

HORIZON_EXPORT void HORIZON_CALL HorizonPlugin_DrawOverlay(void) {
    if (!g_plugin) return;
    attachContext();
    g_plugin->drawOverlay();
}

HORIZON_EXPORT void HORIZON_CALL HorizonPlugin_OnEvent(const HorizonEvent* e) {
    if (!g_plugin || !e || e->size < offsetof(HorizonEvent, type) + sizeof(e->type)) return;
    g_plugin->onEvent(convert(e));
}

HORIZON_EXPORT void HORIZON_CALL HorizonPlugin_SetEnabled(int32_t enabled) {
    // switched off from the menu: end a running run (it is saved), the host stops calling us
    if (g_plugin && !enabled) g_plugin->run().stop();
}

} // extern "C"
