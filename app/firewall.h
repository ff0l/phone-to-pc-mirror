#pragma once

#include <windows.h>

enum FirewallStatus {
    FirewallUnknown = 0,
    FirewallAllowed = 1,
    FirewallDenied = 2
};

FirewallStatus FirewallEnsureInbound(const wchar_t *exe_path);
