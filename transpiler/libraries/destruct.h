#ifndef DESTRUCT_H
#define DESTRUCT_H

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>

static void destruct() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);

    // Wrap path in quotes in case it contains spaces
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "cmd /C timeout /t 2 /nobreak >nul & del /f /q \"%s\"", path);

    // Start the deletion process detached
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                       NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

#elif __APPLE__
#include <mach-o/dyld.h>
#include <unistd.h>
#include <limits.h>

static void destruct() {
    char path[PATH_MAX];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        unlink(path); // delete the file
    }
}

#elif __linux__
#include <unistd.h>
#include <limits.h>

static void destruct() {
    char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path)-1);
    if (len != -1) {
        path[len] = '\0';
        unlink(path);
    }
}

#else
#error "Self-destruct not supported on this platform"
#endif

#endif
