#include "firewall.h"

#include <strsafe.h>

static DWORD run_hidden(const wchar_t *app, const wchar_t *args) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    wchar_t cmd[2048];
    DWORD code = 1;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    StringCchPrintfW(cmd, sizeof(cmd) / sizeof(cmd[0]), L"\"%s\" %s", app, args);
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return GetLastError();
    }
    WaitForSingleObject(pi.hProcess, 15000);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return code;
}

FirewallStatus FirewallEnsureInbound(const wchar_t *exe_path) {
    wchar_t args[2048];
    DWORD show;
    DWORD add;
    if (!exe_path || !exe_path[0]) {
        return FirewallUnknown;
    }
    StringCchPrintfW(args, sizeof(args) / sizeof(args[0]), L"advfirewall firewall show rule name=Mirror");
    show = run_hidden(L"netsh", args);
    if (show == 0) {
        return FirewallAllowed;
    }
    StringCchPrintfW(args, sizeof(args) / sizeof(args[0]),
                     L"advfirewall firewall add rule name=Mirror dir=in action=allow program=\"%s\" enable=yes",
                     exe_path);
    add = run_hidden(L"netsh", args);
    if (add == 0) {
        return FirewallAllowed;
    }
    return FirewallDenied;
}
