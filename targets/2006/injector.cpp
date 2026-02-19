// dmc3 se 2006 injector
// finds dmc3se.exe, injects mod2006.dll via CreateRemoteThread

#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

static DWORD find_pid(const char* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32 pe; pe.dwSize = sizeof(pe);
    DWORD pid = 0;
    if (Process32First(snap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

int main() {
    printf("DMC3 SE 2006 - Injector\n\n");

    // resolve dll path (same folder as injector)
    char dll[MAX_PATH];
    GetModuleFileNameA(NULL, dll, MAX_PATH);
    char* sl = strrchr(dll, '\\');
    if (sl) *(sl + 1) = '\0';
    strcat(dll, "mod2006.dll");

    char full[MAX_PATH];
    GetFullPathNameA(dll, MAX_PATH, full, NULL);

    if (GetFileAttributesA(full) == INVALID_FILE_ATTRIBUTES) {
        printf("mod2006.dll not found next to injector.exe\n");
        printf("Press enter to exit...\n"); getchar();
        return 1;
    }
    printf("DLL: %s\n", full);

    // find process
    printf("Looking for dmc3se.exe...\n");
    DWORD pid = 0;
    for (int i = 0; i < 30 && !pid; i++) {
        pid = find_pid("dmc3se.exe");
        if (!pid) Sleep(1000);
    }
    if (!pid) {
        printf("dmc3se.exe not found. Start the game first.\n");
        printf("Press enter to exit...\n"); getchar();
        return 1;
    }
    printf("Found PID %d\n", pid);

    // inject
    HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!proc) {
        printf("OpenProcess failed (%d). Run as admin.\n", GetLastError());
        printf("Press enter to exit...\n"); getchar();
        return 1;
    }

    size_t pathlen = strlen(full) + 1;
    LPVOID rmem = VirtualAllocEx(proc, NULL, pathlen, MEM_COMMIT, PAGE_READWRITE);
    WriteProcessMemory(proc, rmem, full, pathlen, NULL);

    FARPROC loadlib = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    HANDLE t = CreateRemoteThread(proc, NULL, 0, (LPTHREAD_START_ROUTINE)loadlib, rmem, 0, NULL);

    if (!t) {
        printf("CreateRemoteThread failed (%d)\n", GetLastError());
    } else {
        WaitForSingleObject(t, 5000);
        DWORD code = 0; GetExitCodeThread(t, &code);
        if (code) printf("Injected OK. Check dmc3_mod.log\n");
        else      printf("LoadLibrary returned 0 - DLL failed to load in game\n");
        CloseHandle(t);
    }

    VirtualFreeEx(proc, rmem, 0, MEM_RELEASE);
    CloseHandle(proc);
    printf("Press enter to exit...\n"); getchar();
    return 0;
}
