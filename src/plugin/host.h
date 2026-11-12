// What the randomizer needs from its host, as a small C++ interface. The plugin logic and UI only see
// this; host_horizon.cpp implements it on top of the Horizon plugin ABI (src/plugin/horizon/), and
// test/ui_preview.cpp implements it with a fake game. Switching ABI versions = editing host_horizon.cpp.
//
// Threading: every method is called on the game's main thread (the thread that draws ImGui), except
// log() and runOnGameThread(), which may be called from any thread.
#pragma once
#include <functional>
#include <string>

#include "imgui.h"

namespace lr {

enum class HostGame { Unknown, Eswc, Sunrise, Original, Tmnf, Tmuf };

// Mirrors HorizonGameState.
enum class GameState { Unknown = 0, MainMenu = 1, SoloRace = 2, EndRace = 3, NetRace = 4, Editor = 5, Replay = 6, Loading = 7 };

// Mirrors HorizonResult.
enum class HostResult { Ok = 0, NotSupported = -1, WrongState = -2, BlockedOnline = -3, InvalidArg = -4, Failed = -5, Busy = -6 };

enum class EventType {
    MapLoaded = 1, RaceStart = 2, Respawn = 3, Checkpoint = 4, Finish = 5, RaceEnd = 6, MenuEntered = 7,
    ReplaySaved = 8, OnlineChanged = 9, GameStateChanged = 10
};

enum LogLevel { LogDebug = 0, LogInfo = 1, LogWarn = 2, LogError = 3 };

struct GameEvent {
    EventType type = EventType::MapLoaded;
    int timeMs = -1;         // Checkpoint, Finish
    int checkpoint = -1;
    int respawns = -1;
    bool isLocalPlayer = true;
    bool isPb = false;
    bool endScreen = false;  // RaceEnd
    bool online = false;     // host's IsOnline() at the time
    GameState state = GameState::Unknown;  // GameStateChanged
    bool solo = true;        // play mode is solo (false: net, editor test, replay)
    bool hasMap = false;
    std::string mapPath, mapUid, mapName /* without $ codes */, mapAuthor;
    int authorMs = -1, goldMs = -1, silverMs = -1, bronzeMs = -1;
    std::string replayPath, replaySource;  // ReplaySaved
};

// The host's shared look. Fonts may be null (then the current font is used).
struct UiStyle {
    ImVec4 accent{1.0f, 0.66f, 0.28f, 1.0f};
    ImVec4 ok{0.40f, 0.85f, 0.45f, 1.0f};
    ImVec4 warn{1.0f, 0.78f, 0.30f, 1.0f};
    ImVec4 bad{0.95f, 0.35f, 0.30f, 1.0f};
    ImVec4 text{1, 1, 1, 1};
    ImVec4 text2{0.85f, 0.86f, 0.88f, 1.0f};
    ImVec4 muted{0.55f, 0.57f, 0.62f, 1.0f};
    ImVec4 surface{0.10f, 0.11f, 0.14f, 0.92f};
    ImFont* body = nullptr;
    ImFont* title = nullptr;
    ImFont* mono = nullptr;
    float scale = 1.0f;
};

// Sizes from the host (Horizon ABI minor 4, ui_metrics). Pixels; 0 = not known.
struct UiMetrics {
    float scale = 0;         // = UiStyle::scale (screen height / 1080 x content scale)
    float contentScale = 0;  // the player's content scale, 0.8 .. 1.25
    float pageW = 0, pageH = 0;  // room for the plugin's page in the container that calls drawMenu (docked or popped out)
};

class Host {
public:
    virtual ~Host() = default;

    virtual void log(LogLevel level, const std::string& line) = 0;

    // settings, stored by the host per plugin
    virtual std::string getString(const char* key, const std::string& fallback) = 0;
    virtual void setString(const char* key, const std::string& value) = 0;
    virtual int getInt(const char* key, int fallback) = 0;
    virtual void setInt(const char* key, int value) = 0;

    virtual std::string dataFolder() = 0;  // UTF-8, trailing backslash, exists
    virtual std::string gameFolder() = 0;  // folder of the exe, trailing backslash ("" unknown)
    virtual std::string userFolder() = 0;  // the game's user data folder (Documents\TrackMania...), "" unknown

    virtual HostGame game() = 0;
    virtual std::string gameName() = 0;
    virtual GameState gameState() = 0;
    virtual bool isOnline() = 0;
    virtual bool actionsAllowed() = 0;  // false: online gate or Competition Patch run gate
    virtual bool menuOpen() = 0;

    // game control (main menu / end screen rules are enforced by the caller AND the host)
    virtual HostResult playMapFile(const std::string& utf8Path) = 0;
    virtual HostResult returnToMenu() = 0;
    virtual void runOnGameThread(std::function<void()> fn) = 0;

    // hotkeys: returns a handle > 0, 0 on error; the callback runs on the game thread
    virtual int registerHotkey(const char* action, const char* name, const char* defaultKey, std::function<void()> cb) = 0;
    virtual std::string hotkeyText(int handle) = 0;

    virtual UiStyle style() = 0;
    // false on hosts without ui_metrics (Horizon ABI minor < 4): then the plugin uses the ImGui region only
    virtual bool uiMetrics(UiMetrics* out) = 0;
    virtual double nowMs() = 0;  // monotonic clock

    // --- not in Horizon ABI 1.0 (see docs/plugin.md, "What the host must add") ---
    // RGBA8 texture usable as ImTextureID by the host's ImGui renderer; nullptr when unavailable.
    virtual bool canCreateTextures() = 0;
    virtual ImTextureID createTextureRGBA(int w, int h, const void* rgba) = 0;
    virtual void releaseTexture(ImTextureID tex) = 0;
    // Save the replay of the run that just ended (valid on RACE_END / end screen). Path UTF-8.
    virtual bool canSaveReplay() = 0;
    virtual HostResult saveReplay(const std::string& utf8Path) = 0;
};

} // namespace lr
