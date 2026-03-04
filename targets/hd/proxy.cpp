// dinput8.dll proxy for DMC3 HD Collection (Steam)
// drop this as dinput8.dll in the game folder
// forwards all dinput8 calls to the real system dll, loads modhd.dll on attach

#include <windows.h>
#include <stdio.h>

typedef HRESULT(WINAPI* DI8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);

static HMODULE g_real = NULL;
static HMODULE g_mod  = NULL;
static DI8Create_t g_real_create = NULL;

static bool load_real() {
    char sys[MAX_PATH];
    GetSystemDirectoryA(sys, MAX_PATH);
    char path[MAX_PATH];
    _snprintf(path, MAX_PATH, "%s\\dinput8.dll", sys);
    g_real = LoadLibraryA(path);
    if(!g_real) return false;
    g_real_create = (DI8Create_t)GetProcAddress(g_real, "DirectInput8Create");
    return g_real_create != NULL;
}

static void load_mod() {
    // find our own directory and load modhd.dll from there
    char dir[MAX_PATH];
    HMODULE self;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)load_mod, &self);
    GetModuleFileNameA(self, dir, MAX_PATH);
    char* sl = strrchr(dir, '\\');
    if(sl) *(sl+1) = '\0';
    char path[MAX_PATH];
    _snprintf(path, MAX_PATH, "%smodhd.dll", dir);
    if(GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES)
        g_mod = LoadLibraryA(path);
}

extern "C" __declspec(dllexport) HRESULT WINAPI DirectInput8Create(
    HINSTANCE hinst, DWORD ver, REFIID riid, LPVOID* out, LPUNKNOWN outer) {
    if(!g_real_create) return E_FAIL;
    return g_real_create(hinst, ver, riid, out, outer);
}

extern "C" __declspec(dllexport) HRESULT WINAPI DllCanUnloadNow() {
    typedef HRESULT(WINAPI*fn)();
    fn f = (fn)GetProcAddress(g_real, "DllCanUnloadNow");
    return f ? f() : S_FALSE;
}

extern "C" __declspec(dllexport) HRESULT WINAPI DllGetClassObject(REFCLSID c, REFIID r, LPVOID* p) {
    typedef HRESULT(WINAPI*fn)(REFCLSID,REFIID,LPVOID*);
    fn f = (fn)GetProcAddress(g_real, "DllGetClassObject");
    return f ? f(c,r,p) : E_FAIL;
}

extern "C" __declspec(dllexport) HRESULT WINAPI DllRegisterServer() {
    typedef HRESULT(WINAPI*fn)();
    fn f = (fn)GetProcAddress(g_real, "DllRegisterServer");
    return f ? f() : E_FAIL;
}

extern "C" __declspec(dllexport) HRESULT WINAPI DllUnregisterServer() {
    typedef HRESULT(WINAPI*fn)();
    fn f = (fn)GetProcAddress(g_real, "DllUnregisterServer");
    return f ? f() : E_FAIL;
}

BOOL APIENTRY DllMain(HMODULE hmod, DWORD reason, LPVOID) {
    if(reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hmod);
        if(!load_real()) return FALSE;
        load_mod();
    }
    if(reason == DLL_PROCESS_DETACH) {
        // dont FreeLibrary here - loader lock makes it unsafe
        // process teardown cleans up anyway
    }
    return TRUE;
}
