// legacy_randomizer.dll, spike: can a mod start a map file in solo from the main menu? (ESWC 1.80, Sunrise repack)
//
// TrackMania Nations ESWC, unofficial 1.80 patch (exe 0.1.7.9, PE timestamp 0x5ce7272f).
// Offsets per exe are in kGames; the ESWC values are explained here. What the game does for "Play Track > Solo > OK" (found with Ghidra, compared with TMNF symbols):
//   - CGameCtnApp is the global at 0xcaee50; the menus object keeps it at menus+0x374.
//   - CGameCtnChallengeInfo::s_ChallengeInfos is the buffer {count, items, capacity} at 0xcaed34.
//   - CGameCtnChallengeInfo::RefreshFromDisk(folders, 0, 0) (0x703990, cdecl) re-reads the track
//     folders (app+0x1f0) from disk and adds an info for every new .Challenge.Gbx;
//     SortByName (0x703b00) sorts the list like the game's own rescan (0x70eb80) does.
//   - MenuPlayChallenge_Solo (0x69f8b0): app+0x2e8 = 3 (challenge type), menus+0x3e4 = 0x13.
//   - MenuChooseChallenge_OnOk (0x6a0480): menus+0x3a4 = the chosen infos {count, items, capacity},
//     then menus+0x2e4 = 0xf. The menu loop returns that result and the game plays the challenge.
// Shift+F11 logs that state; F11 copies a map into Tracks\Challenges\Downloaded\_LegacyRandomizer and does
// the steps above. Only in the main menu. Nothing is patched; no hooks into game code.
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

#include "reflect/reflect.h"

namespace {

// Per exe: where the game keeps what the "Play Track > Solo > OK" path touches.
struct Game {
    const char* name;
    DWORD timestamp;          // PE header TimeDateStamp of the supported exe
    uintptr_t app;            // global CGameCtnApp* (stored by its constructor)
    uintptr_t challengeInfos; // CGameCtnChallengeInfo::s_ChallengeInfos {count, items, capacity}
    uintptr_t refreshFromDisk;
    uintptr_t sortByName;
    uintptr_t gameMalloc, gameFree;
    size_t appFolders, appChallengeType;
    size_t menusApp, menusResult, menusChosen, menusMode;
    int modeValue, resultPlay;
};

const Game kGames[] = {
    // TrackMania Nations ESWC, unofficial 1.80 (0.1.7.9); sha256 983bd127...
    {"TrackMania Nations ESWC 1.80", 0x5ce7272f, 0x00caee50, 0x00caed34, 0x00703990, 0x00703b00, 0x004044b0,
     0x004045b0, 0x1f0, 0x2e8, 0x374, 0x2e4, 0x3a4, 0x3e4, 0x13, 0xf},
    // TrackMania Sunrise, Win10 community repack TmSunrise.exe; sha256 e59dfc7e...
    // MenuPlayChallenge_Solo 0x7bb660, MenuPlayChallenge_OnOk 0x7bc230, SetChallengeType 0x824f00
    {"TrackMania Sunrise (Win10 repack)", 0x5cc8f0cf, 0x00bcce50, 0x00bcd1e8, 0x0085fce0, 0x0085fe50, 0x00402cc0,
     0x00402b90, 0x1b8, 0x2a0, 0x2e8, 0x258, 0x318, 0x354, 2, 1},
};
const Game* g = nullptr;

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

template <typename T> T& at(void* base, size_t off) { return *reinterpret_cast<T*>(static_cast<char*>(base) + off); }

struct Buffer {
    uint32_t count;
    void** items;
    uint32_t capacity;
};

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

void* findMenus(void* app) {
    void* found = nullptr;
    for (size_t off = 0; off < 0x1000; off += 4) {
        void* p = nullptr;
        __try {
            p = at<void*>(app, off);
            if (!p || reinterpret_cast<uintptr_t>(p) < 0x10000) continue;
            if (at<void*>(p, g->menusApp) == app) {
                logLine("  menus candidate: app+0x%03X = %p", (unsigned)off, p);
                if (!found) found = p;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    return found;
}

void dumpState() {
    __try {
        void* app = *reinterpret_cast<void**>(g->app);
        Buffer* infos = reinterpret_cast<Buffer*>(g->challengeInfos);
        logLine("dump: app=%p challengeInfos=%u", app, infos->count);
        if (!app) return;
        logLine("  app+0x2e8 (challenge type)=%d folders=%p", at<int>(app, g->appChallengeType), at<void*>(app, g->appFolders));
        void* menus = findMenus(app);
        if (menus) {
            Buffer& chosen = at<Buffer>(menus, g->menusChosen);
            logLine("  menus=%p result=%d mode=0x%X chosen={%u,%p,%u}", menus, at<int>(menus, g->menusResult),
                    at<int>(menus, g->menusMode), chosen.count, chosen.items, chosen.capacity);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        logLine("dump: exception 0x%08X while reading", GetExceptionCode());
    }
}

// Reflection self-check: the offsets found by hand must equal what the engine's own tables say by name.
void reflectionCheck() {
    if (!reflect::ready()) {
        logLine("reflection: registry empty or unknown exe");
        return;
    }
    logLine("reflection: CTrackMania.ChallengeType=0x%X (table 0x%X), MenuManager=0x%X, Races=0x%X",
            reflect::offsetOf("CTrackMania", "ChallengeType"), (unsigned)g->appChallengeType,
            reflect::offsetOf("CTrackMania", "MenuManager"), reflect::offsetOf("CTrackMania", "Races"));
    logLine("reflection: CTrackManiaRace.ButtonQuitOnClick=%p, CTrackManiaRace.Game=0x%X",
            reflect::methodOf("CTrackManiaRace", "ButtonQuitOnClick"), reflect::offsetOf("CTrackManiaRace", "Game"));
    wchar_t path[MAX_PATH];
    if (gameDir(path, MAX_PATH)) {
        wcscat_s(path, L"legacy_randomizer_reflection.json");
        logLine("reflection: %d classes written to legacy_randomizer_reflection.json", reflect::dumpJson(path));
    }
}

// Ctrl+F11 in a solo race: press the race's own "quit" button (CTrackManiaRace::ButtonQuitOnClick, found by name).
void quitRace() {
    using QuitFn = void(__thiscall*)(void*);
    void* fn = reflect::methodOf("CTrackManiaRace", "ButtonQuitOnClick");
    int racesOff = reflect::offsetOf("CTrackMania", "Races");
    __try {
        void* app = *reinterpret_cast<void**>(g->app);
        void* race = (app && racesOff >= 0) ? at<void*>(app, racesOff) : nullptr;
        logLine("quit: app=%p race=%p fn=%p", app, race, fn);
        if (!race || !fn) return;
        reinterpret_cast<QuitFn>(fn)(race);
        logLine("quit: ButtonQuitOnClick called");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        logLine("quit: exception 0x%08X", GetExceptionCode());
    }
}

// The game-side part of F11, in its own function so __try has no C++ objects to unwind.
int playInfo(void* app, void* menus, void* info) {
    __try {
        using MallocFn = void*(__cdecl*)(size_t);
        using FreeFn = void(__cdecl*)(void*);
        Buffer& chosen = at<Buffer>(menus, g->menusChosen);
        if (chosen.capacity < 1 || !chosen.items) {
            void** items = static_cast<void**>(reinterpret_cast<MallocFn>(g->gameMalloc)(4 * sizeof(void*)));
            if (!items) return 0;
            if (chosen.items) reinterpret_cast<FreeFn>(g->gameFree)(chosen.items);
            chosen.items = items;
            chosen.capacity = 4;
        }
        chosen.items[0] = info;
        chosen.count = 1;
        at<int>(app, g->appChallengeType) = 3;
        at<int>(menus, g->menusMode) = g->modeValue;
        at<int>(menus, g->menusResult) = g->resultPlay;
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        logLine("play: exception 0x%08X while setting the menu", GetExceptionCode());
        return 0;
    }
}

int refreshAndFindNew(void* app, void** newInfo) {
    __try {
        Buffer* infos = reinterpret_cast<Buffer*>(g->challengeInfos);
        const uint32_t before = infos->count;
        void** snapshot = static_cast<void**>(HeapAlloc(GetProcessHeap(), 0, (before + 1) * sizeof(void*)));
        if (!snapshot) return -1;
        for (uint32_t i = 0; i < before; i++) snapshot[i] = infos->items[i];
        using RefreshFn = void(__cdecl*)(void*, int, int);
        using SortFn = void(__cdecl*)();
        reinterpret_cast<RefreshFn>(g->refreshFromDisk)(at<void*>(app, g->appFolders), 0, 0);
        reinterpret_cast<SortFn>(g->sortByName)();
        int added = 0;
        for (uint32_t i = 0; i < infos->count; i++) {
            void* p = infos->items[i];
            bool old = false;
            for (uint32_t j = 0; j < before && !old; j++) old = snapshot[j] == p;
            if (!old) {
                added++;
                *newInfo = p;
            }
        }
        logLine("play: challenge infos %u -> %u, %d new", before, infos->count, added);
        HeapFree(GetProcessHeap(), 0, snapshot);
        return added;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        logLine("play: exception 0x%08X in RefreshFromDisk", GetExceptionCode());
        return -1;
    }
}

void playTestMap() {
    void* app = nullptr;
    __try {
        app = *reinterpret_cast<void**>(g->app);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    if (!app) {
        logLine("play: no app yet");
        return;
    }
    void* menus = findMenus(app);
    if (!menus) {
        logLine("play: menus object not found, nothing changed");
        return;
    }
    wchar_t dir[MAX_PATH], src[MAX_PATH], dst[MAX_PATH], ncDir[MAX_PATH];
    if (!gameDir(dir, MAX_PATH)) return;
    // map: ini MapFile=..., else the first map NostalgicCompletionist downloaded
    wchar_t ini[MAX_PATH];
    swprintf_s(ini, L"%slegacy_randomizer.ini", dir);
    GetPrivateProfileStringW(L"spike", L"MapFile", L"", src, MAX_PATH, ini);
    if (!src[0]) {
        swprintf_s(ncDir, L"%sGameData\\Tracks\\Challenges\\Downloaded\\_NostalgicCompletionist\\", dir);
        bool found = findMap(ncDir, src, MAX_PATH, 2);
        if (!found) {
            swprintf_s(ncDir, L"%sGameData\\Tracks\\Challenges\\My Challenges\\", dir);
            found = findMap(ncDir, src, MAX_PATH, 2);
        }
        if (!found) {
            logLine("play: no MapFile in the ini and no map below %ls", ncDir);
            return;
        }
    }
    wchar_t outDir[MAX_PATH];
    swprintf_s(outDir, L"%sGameData\\Tracks\\Challenges\\Downloaded\\_LegacyRandomizer\\", dir);
    CreateDirectoryW(outDir, nullptr);
    swprintf_s(dst, L"%sLR_%lu.Challenge.Gbx", outDir, GetTickCount());
    if (!CopyFileW(src, dst, TRUE)) {
        logLine("play: copy failed (%lu): %ls", GetLastError(), src);
        return;
    }
    logLine("play: copied %ls -> %ls", src, dst);
    void* info = nullptr;
    int added = refreshAndFindNew(app, &info);
    if (added != 1 || !info) {
        logLine("play: expected exactly 1 new challenge info, got %d - not starting", added);
        return;
    }
    if (playInfo(app, menus, info)) logLine("play: menu set to play info %p (result 0x%X)", info, g->resultPlay);
}

LRESULT CALLBACK getMsgProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && wParam == PM_REMOVE) {
        const MSG* msg = reinterpret_cast<const MSG*>(lParam);
        if (msg->message == WM_KEYDOWN && !(msg->lParam & (1 << 30))) {
            // F11: TM Vibrant Shaders uses F5-F8 and F12, eswc-autosave F9
            if (msg->wParam == VK_F11) {
                if (GetKeyState(VK_SHIFT) < 0) {
                    dumpState();
                    reflectionCheck();
                } else if (GetKeyState(VK_CONTROL) < 0) {
                    quitRace();
                } else {
                    playTestMap();
                }
            }
        }
    }
    return CallNextHookEx(g_msgHook, code, wParam, lParam);
}

// The supported game this process is, by PE timestamp (exe loaded at its preferred base), or nullptr.
const Game* detectGame(DWORD* timestamp) {
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(GetModuleHandleW(nullptr));
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(reinterpret_cast<char*>(dos) + dos->e_lfanew);
    *timestamp = nt->FileHeader.TimeDateStamp;
    if (reinterpret_cast<uintptr_t>(dos) != 0x400000 || nt->OptionalHeader.ImageBase != 0x400000) return nullptr;
    for (const Game& game : kGames)
        if (game.timestamp == *timestamp) return &game;
    return nullptr;
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
    logLine("---- legacy_randomizer spike start ----");
    DWORD timestamp = 0;
    g = detectGame(&timestamp);
    if (!g) {
        logLine("unsupported exe (PE timestamp 0x%08lX) - inactive", timestamp);
        return 0;
    }
    logLine("game: %s", g->name);
    if (!reflect::init(timestamp)) logLine("reflection: no registry address for this exe");
    g_mainThread = GetCurrentThreadId();
    g_msgHook = SetWindowsHookExW(WH_GETMESSAGE, getMsgProc, nullptr, g_mainThread);
    logLine(g_msgHook ? "ready: F11 = play a test map (main menu), Ctrl+F11 = quit the solo race, Shift+F11 = log state + reflection dump" : "key hook failed");
    g_ok = g_msgHook != nullptr;
    return g_ok ? 1 : 0;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(inst);
    return TRUE;
}
