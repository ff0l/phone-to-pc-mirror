#pragma once

#include <windows.h>

struct Receiver;

Receiver *ReceiverCreate(HWND notify);
void ReceiverDestroy(Receiver *rx);
bool ReceiverStart(Receiver *rx, HWND video, const wchar_t *airplay_name);
void ReceiverStop(Receiver *rx);

enum {
    WM_MIRROR_READY = WM_APP + 1,
    WM_MIRROR_CLIENT = WM_APP + 2,
    WM_MIRROR_IDLE = WM_APP + 3,
    WM_MIRROR_SIZE = WM_APP + 4
};
