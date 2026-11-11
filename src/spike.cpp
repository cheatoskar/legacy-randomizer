// legacy_randomizer.dll, test harness for the gamectl and reflect modules (the parts the Horizon host vendors).
// Test folders only, loaded by the test dinput8.dll. Keys (TMVS uses F5-F8/F12, eswc-autosave F9):
//   F11        main menu: start a map file in solo (ini [spike] MapFile=, else the first map found)
//   Ctrl+F11   solo race: press the race's quit button (back to the menu)
//   Shift+F11  log game, state, current map, reflection self-check, write legacy_randomizer_reflection.json
// Supported: TMN ESWC 1.80, TM Sunrise Win10 repack, TMNF, TMUF; anything else only logs (discovery).
#include <windows.h>
#include <shlobj.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

#include "gamectl/gamectl.h"
#include "reflect/reflect.h"

namespace {

HANDLE g_log = INVALID_HANDLE_VALUE;
DWORD g_mainThread = 0;
HHOOK g_msgHook = nullptr;
bool g_ok = false;

void logLine(const char* fmt, ...) {
    if (g_log == INVALID_HANDLE_VALUE) return;
    char buf[1024];
    SYSTEMTIME t;
    GetLocalTime(&t);
    int len = _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%02u:%02u:%02u.%03u  ", t.wHour, t.wMinute, t.wSecond,
                          t.wMilliseconds);
    va_list ap;
    va_start(ap, fmt);
    int m = _vsnprintf_s(buf + len, sizeof(buf) - len - 2, _TRUNCATE, fmt, ap);
    va_end(ap);
    len = (m < 0) ? (int)strlen(buf) : len + m;
    buf[len++] = '\r';
    buf[len++] = '\n';
    DWORD w = 0;
    WriteFile(g_log, buf, (DWORD)len, &w, nullptr);
}

bool gameDir(wchar_t* out, size_t cap) {
    DWORD n = GetModuleFileNameW(nullptr, out, (DWORD)cap);
    if (n == 0 || n >= cap) return false;
    wchar_t* slash = wcsrchr(out, L'\\');
    if (!slash) return false;
    slash[1] = 0;
    return true;
}

// First *.Challenge.Gbx below dir (depth 2), for the spike's default test map.
bool findMap(const wchar_t* dir, wchar_t* out, size_t cap, int depth) {
    wchar_t pat[MAX_PATH];
    swprintf_s(pat, L"%s*", dir);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    bool found = false;
    do {
        if (fd.cFileName[0] == L'.') continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (depth > 0) {
                wchar_t sub[MAX_PATH];
                swprintf_s(sub, L"%s%s\\", dir, fd.cFileName);
                found = findMap(sub, out, cap, depth - 1);
            }
        } else {
            size_t n = wcslen(fd.cFileName);
            if (n > 14 && _wcsicmp(fd.cFileName + n - 14, L".Challenge.Gbx") == 0) {
                swprintf_s(out, cap, L"%s%s", dir, fd.cFileName);
                found = true;
            }
        }
    } while (!found && FindNextFileW(h, &fd));
    FindClose(h);
    return found;
}

void gamectlLog(const char* line) { logLine("%s", line); }

const char* stateName(gamectl::State s) {
    switch (s) {
    case gamectl::State::Loading: return "loading";
    case gamectl::State::MainMenu: return "main menu";
    case gamectl::State::SoloRace: return "solo race";
    case gamectl::State::NetRace: return "net race";
    case gamectl::State::Editor: return "editor";
    default: return "unknown";
    }
}

void logState() {
    logLine("state: %s, game %s, online %d", stateName(gamectl::state()), gamectl::gameName(), gamectl::isOnline() ? 1 : 0);
    gamectl::MapInfo m;
    if (gamectl::currentMap(&m))
        logLine("map: %s by %s, uid %s, AT %d gold %d silver %d bronze %d", m.name, m.author, m.uid, m.authorTime,
                m.goldTime, m.silverTime, m.bronzeTime);
    if (!reflect::ready()) {
        logLine("reflection: registry empty or unknown exe");
        return;
    }
    logLine("reflection: CTrackMania.ChallengeType=0x%X MenuManager=0x%X Races=0x%X Editor=0x%X",
            reflect::offsetOf("CTrackMania", "ChallengeType"), reflect::offsetOf("CTrackMania", "MenuManager"),
            reflect::offsetOf("CTrackMania", "Races"), reflect::offsetOf("CTrackMania", "Editor"));
    logLine("reflection: CTrackManiaRace.ButtonQuitOnClick=%p", reflect::methodOf("CTrackManiaRace", "ButtonQuitOnClick"));
    wchar_t path[MAX_PATH];
    if (gameDir(path, MAX_PATH)) {
        wcscat_s(path, L"legacy_randomizer_reflection.json");
        logLine("reflection: %d classes written to legacy_randomizer_reflection.json", reflect::dumpJson(path));
    }
}

bool testMap(wchar_t* src) {
    wchar_t dir[MAX_PATH], ini[MAX_PATH], search[MAX_PATH];
    if (!gameDir(dir, MAX_PATH)) return false;
    swprintf_s(ini, L"%slegacy_randomizer.ini", dir);
    GetPrivateProfileStringW(L"spike", L"MapFile", L"", src, MAX_PATH, ini);
    if (src[0]) return true;
    const wchar_t* subs[] = {L"GameData\\Tracks\\Challenges\\Downloaded\\_NostalgicCompletionist\\",
                             L"GameData\\Tracks\\Challenges\\My Challenges\\", L"GameData\\Tracks\\Challenges\\"};
    for (const wchar_t* s : subs) {
        swprintf_s(search, L"%s%s", dir, s);
        if (findMap(search, src, MAX_PATH, 3)) return true;
    }
    wchar_t docs[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, docs))) {
        swprintf_s(search, L"%s\\TrackMania\\Tracks\\Challenges\\My Challenges\\", docs);
        if (findMap(search, src, MAX_PATH, 3)) return true;
        swprintf_s(search, L"%s\\TrackMania\\Tracks\\Challenges\\Downloaded\\", docs);
        if (findMap(search, src, MAX_PATH, 3)) return true;
    }
    return false;
}

void playTestMap() {
    wchar_t src[MAX_PATH] = {};
    if (!testMap(src)) {
        logLine("play: no MapFile in legacy_randomizer.ini and no map found");
        return;
    }
    logLine("play: %ls", src);
    logLine("play: result %d", static_cast<int>(gamectl::playMapFile(src)));
}

LRESULT CALLBACK getMsgProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && wParam == PM_REMOVE) {
        const MSG* msg = reinterpret_cast<const MSG*>(lParam);
        if (msg->message == WM_KEYDOWN && !(msg->lParam & (1 << 30)) && msg->wParam == VK_F11) {
            if (GetKeyState(VK_SHIFT) < 0)
                logState();
            else if (GetKeyState(VK_CONTROL) < 0)
                logLine("quit: result %d", static_cast<int>(gamectl::returnToMenu()));
            else
                playTestMap();
        }
    }
    return CallNextHookEx(g_msgHook, code, wParam, lParam);
}

} // namespace

extern "C" int __cdecl LegacyRandomizerInit(unsigned) {
    if (g_ok) return 1;
    wchar_t path[MAX_PATH];
    if (gameDir(path, MAX_PATH)) {
        wcscat_s(path, L"legacy_randomizer.log");
        g_log = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    logLine("---- legacy_randomizer test harness start ----");
    wchar_t ini[MAX_PATH] = {};
    bool experimental = false;
    if (gameDir(ini, MAX_PATH)) {
        wcscat_s(ini, L"legacy_randomizer.ini");
        experimental = GetPrivateProfileIntW(L"spike", L"Experimental", 1, ini) != 0;
    }
    gamectl::Game game = gamectl::init(gamectlLog, experimental);
    logLine("game: %s (%d)", gamectl::gameName(), static_cast<int>(game));
    g_mainThread = GetCurrentThreadId();
    g_msgHook = SetWindowsHookExW(WH_GETMESSAGE, getMsgProc, nullptr, g_mainThread);
    logLine(g_msgHook ? "ready: F11 = play a test map (main menu), Ctrl+F11 = quit the solo race, Shift+F11 = log state + reflection dump"
                      : "key hook failed");
    g_ok = g_msgHook != nullptr;
    return g_ok ? 1 : 0;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(inst);
    return TRUE;
}
