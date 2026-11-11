// dinput8.dll (test loader for the ESWC spike only): the eswc-autosave loader plus one extra DLL.
//
// Passes every DirectInput export to the system dinput8.dll. On the game's first DirectInput8Create
// it loads, from this DLL's folder, eswc_autosave.dll (EswcAutosaveInit, loader api 1, as the
// released loader does) and legacy_randomizer.dll (LegacyRandomizerInit). Either may be missing.
// Replaced later by the Horizon Legacy Loader.
#include <windows.h>

namespace {
constexpr unsigned kLoaderApi = 1;

HMODULE g_real = nullptr;
FARPROC g_procs[6] = {};
const char* const kNames[6] = {"DirectInput8Create", "DllCanUnloadNow",     "DllGetClassObject",
                               "DllRegisterServer",  "DllUnregisterServer", "GetdfDIJoystick"};
HMODULE g_self = nullptr;
volatile LONG g_modsStarted = 0;

bool loadReal() {
    if (g_real) return true;
    wchar_t path[MAX_PATH];
    UINT n = GetSystemDirectoryW(path, MAX_PATH);
    if (n == 0 || n > MAX_PATH - 16) return false;
    lstrcatW(path, L"\\dinput8.dll");
    HMODULE real = LoadLibraryW(path);
    if (!real) return false;
    for (int i = 0; i < 6; i++) g_procs[i] = GetProcAddress(real, kNames[i]);
    g_real = real;
    return true;
}

FARPROC proc(int i) { return loadReal() ? g_procs[i] : nullptr; }

void startOne(const wchar_t* file, const char* initName) {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(g_self, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return;
    wchar_t* slash = nullptr;
    for (wchar_t* p = path; *p; p++)
        if (*p == L'\\' || *p == L'/') slash = p;
    if (!slash || (slash + 1 - path) + lstrlenW(file) + 1 >= MAX_PATH) return;
    lstrcpyW(slash + 1, file);
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) return;
    HMODULE mod = LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!mod) return;
    using InitFn = int(__cdecl*)(unsigned);
    InitFn init = reinterpret_cast<InitFn>(GetProcAddress(mod, initName));
    if (init) init(kLoaderApi);
}

void startMods() {
    if (InterlockedExchange(&g_modsStarted, 1) != 0) return;
    startOne(L"eswc_autosave.dll", "EswcAutosaveInit");
    startOne(L"legacy_randomizer.dll", "LegacyRandomizerInit");
}
} // namespace

extern "C" {

HRESULT WINAPI Proxy_DirectInput8Create(HINSTANCE inst, DWORD version, REFIID riid, LPVOID* out, void* outer) {
    startMods();
    using Fn = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, void*);
    Fn fn = reinterpret_cast<Fn>(proc(0));
    return fn ? fn(inst, version, riid, out, outer) : E_FAIL;
}

HRESULT WINAPI Proxy_DllCanUnloadNow() {
    using Fn = HRESULT(WINAPI*)();
    Fn fn = reinterpret_cast<Fn>(proc(1));
    return fn ? fn() : S_FALSE;
}

HRESULT WINAPI Proxy_DllGetClassObject(REFCLSID clsid, REFIID riid, LPVOID* out) {
    using Fn = HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*);
    Fn fn = reinterpret_cast<Fn>(proc(2));
    return fn ? fn(clsid, riid, out) : CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT WINAPI Proxy_DllRegisterServer() {
    using Fn = HRESULT(WINAPI*)();
    Fn fn = reinterpret_cast<Fn>(proc(3));
    return fn ? fn() : E_FAIL;
}

HRESULT WINAPI Proxy_DllUnregisterServer() {
    using Fn = HRESULT(WINAPI*)();
    Fn fn = reinterpret_cast<Fn>(proc(4));
    return fn ? fn() : E_FAIL;
}

LPCVOID WINAPI Proxy_GetdfDIJoystick() {
    using Fn = LPCVOID(WINAPI*)();
    Fn fn = reinterpret_cast<Fn>(proc(5));
    return fn ? fn() : nullptr;
}

} // extern "C"

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = inst;
        DisableThreadLibraryCalls(inst);
    }
    return TRUE;
}
